#include "AutoPairs.h"

#include "Body.h"
#include "Settings.h"

#include <format>

namespace OStrands::AutoPairs
{
	using namespace Anatomy;

	namespace
	{
		constexpr float kInterval = 0.05f;
		constexpr float kWarmup = 1.0f;
		constexpr float kEnter = 0.18f;
		constexpr float kStay = 0.08f;
		constexpr float kIncumbent = 1.25f;
		constexpr float kShaftReach = 5.0f;
		constexpr float kMouthReach = 5.0f;
		constexpr float kFingerReach = 3.0f;
		constexpr float kTribReach = 5.0f;

		struct Site
		{
			bool         valid{ false };
			RE::NiPoint3 center;
		};

		struct Figure
		{
			RE::NiPointer<RE::Actor>  actor;
			float                     scale{ 1.0f };
			std::vector<RE::NiPoint3> shaft;
			Site                      vagina;
			Site                      anus;
			Site                      mouth;
			std::array<Site, 2>       fingers;
		};

		float SegmentDistance(const RE::NiPoint3& a_point, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
		{
			const RE::NiPoint3 span = a_to - a_from;
			const float        length = span.SqrLength();
			const float        t = length > 1e-8f ? std::clamp((a_point - a_from).Dot(span) / length, 0.0f, 1.0f) : 0.0f;
			return a_point.GetDistance(a_from + span * t);
		}

		float LineDistance(const RE::NiPoint3& a_point, const std::vector<RE::NiPoint3>& a_line)
		{
			float best = std::numeric_limits<float>::max();
			for (std::size_t i = 0; i + 1 < a_line.size(); ++i) {
				best = std::min(best, SegmentDistance(a_point, a_line[i], a_line[i + 1]));
			}
			return best;
		}

		float Closeness(float a_distance, float a_reach)
		{
			return a_distance < a_reach ? 1.0f - a_distance / a_reach : 0.0f;
		}

		Figure Measure(RE::NiPointer<RE::Actor> a_actor)
		{
			Figure figure;
			const RE::NiPointer<RE::NiAVObject> model{ a_actor ? a_actor->Get3D(false) : nullptr };
			auto* root = model.get();
			if (!root) {
				return figure;
			}
			figure.actor = a_actor;
			figure.scale = std::max(root->world.scale, 0.1f);
			const auto& names = Names::Get();
			const auto shaft = GetShaftState(a_actor.get());
			if (shaft.schlong || shaft.strapOn) {
				for (const auto& name : names.shaft) {
					auto* node = root->GetObjectByName(name);
					if (!node) {
						figure.shaft.clear();
						break;
					}
					figure.shaft.push_back(node->world.translate);
				}
				if (!figure.shaft.empty()) {
					if (const auto tip = Body::TipOf(a_actor.get())) {
						figure.shaft.push_back(tip->point);
					}
				}
			}
			std::vector<Point> rim;
			RE::NiPoint3       center;
			RE::NiPoint3       inward;
			if (!FindOpening(root, Opening::kVagina, rim, center, inward)) {
				figure.vagina = { true, center };
			}
			if (!FindOpening(root, Opening::kAnus, rim, center, inward)) {
				figure.anus = { true, center };
			}
			if (!FindOpening(root, Opening::kMouth, rim, center, inward)) {
				figure.mouth = { true, center };
			}
			for (std::size_t h = 0; h < names.hands.size(); ++h) {
				auto* last = root->GetObjectByName(names.hands[h].back());
				auto* before = root->GetObjectByName(names.hands[h][names.hands[h].size() - 2]);
				if (!last || !before) {
					continue;
				}
				RE::NiPoint3 direction = last->world.translate - before->world.translate;
				if (direction.Unitize() < 1e-4f) {
					continue;
				}
				figure.fingers[h] = { true, last->world.translate + direction * (Settings::Get().fingerTipExtension * last->world.scale) };
			}
			return figure;
		}
	}

	void Detector::Begin(std::vector<RE::ActorHandle> a_actors)
	{
		_actors = std::move(a_actors);
		_candidates.clear();
		_clock = 0.0f;
		_pending = 0.0f;
		_samples = 0;
	}

	Detector::Candidate& Detector::Find(const Pair& a_pair, std::uint32_t a_tool, std::uint32_t a_target, bool a_shaft)
	{
		for (auto& candidate : _candidates) {
			if (candidate.pair == a_pair && candidate.tool == a_tool && candidate.target == a_target) {
				return candidate;
			}
		}
		_candidates.push_back({ a_pair, a_tool, a_target, a_shaft });
		return _candidates.back();
	}

	bool Detector::Sample(float a_delta)
	{
		_clock += a_delta;
		_pending += a_delta;
		if (_pending < kInterval) {
			return false;
		}
		_pending = 0.0f;
		++_samples;

		std::vector<Figure> figures;
		figures.reserve(_actors.size());
		for (const auto& handle : _actors) {
			figures.push_back(Measure(handle.get()));
		}
		std::vector<std::pair<std::size_t, float>> seen;
		const auto record = [&](const Pair& a_pair, std::uint32_t a_tool, std::uint32_t a_target, bool a_shaft, float a_value) {
			if (a_value <= 0.0f) {
				return;
			}
			Find(a_pair, a_tool, a_target, a_shaft);
			for (std::size_t i = 0; i < _candidates.size(); ++i) {
				if (_candidates[i].pair == a_pair && _candidates[i].tool == a_tool && _candidates[i].target == a_target) {
					seen.emplace_back(i, a_value);
					break;
				}
			}
		};
		for (std::uint32_t g = 0; g < figures.size(); ++g) {
			const auto& giver = figures[g];
			if (!giver.actor) {
				continue;
			}
			for (std::uint32_t r = 0; r < figures.size(); ++r) {
				const auto& receiver = figures[r];
				if (!receiver.actor) {
					continue;
				}
				const float scale = 0.5f * (giver.scale + receiver.scale);
				const std::array<std::pair<const Site*, std::uint32_t>, 3> openings{ { { &receiver.vagina, 0 }, { &receiver.anus, 1 }, { &receiver.mouth, 2 } } };
				if (g != r && giver.shaft.size() >= 2) {
					for (const auto& [site, slot] : openings) {
						if (site->valid) {
							const Act act = slot == 0 ? Act::kVaginal : slot == 1 ? Act::kAnal : Act::kFellatio;
							record({ _actors[g], _actors[r], act }, g * 8, r * 4 + slot, true,
								Closeness(LineDistance(site->center, giver.shaft), kShaftReach * scale));
						}
					}
				}
				if (g != r && giver.mouth.valid) {
					for (std::uint32_t slot = 0; slot < 2; ++slot) {
						if (openings[slot].first->valid) {
							record({ _actors[g], _actors[r], slot == 0 ? Act::kCunnilingus : Act::kAnilingus }, g * 8 + 1, r * 4 + slot, false,
								Closeness(giver.mouth.center.GetDistance(openings[slot].first->center), kMouthReach * scale));
						}
					}
				}
				for (std::uint32_t h = 0; h < giver.fingers.size(); ++h) {
					if (!giver.fingers[h].valid) {
						continue;
					}
					for (std::uint32_t slot = 0; slot < 2; ++slot) {
						if (openings[slot].first->valid) {
							record({ _actors[g], _actors[r], slot == 0 ? Act::kVaginalFingering : Act::kAnalFingering }, g * 8 + 2 + h, r * 4 + slot, false,
								Closeness(giver.fingers[h].center.GetDistance(openings[slot].first->center), kFingerReach * scale));
						}
					}
				}
				if (g < r && giver.vagina.valid && receiver.vagina.valid) {
					record({ _actors[g], _actors[r], Act::kTribbing }, g * 8 + 4, r * 4, false,
						Closeness(giver.vagina.center.GetDistance(receiver.vagina.center), kTribReach * scale));
				}
			}
		}
		for (std::size_t i = 0; i < _candidates.size(); ++i) {
			float value = 0.0f;
			for (const auto& [index, sample] : seen) {
				if (index == i) {
					value = std::max(value, sample);
				}
			}
			_candidates[i].sum += value;
		}
		if (_clock < kWarmup) {
			return false;
		}
		const auto mean = [&](const Candidate& a_candidate) { return a_candidate.sum / static_cast<float>(std::max<std::uint32_t>(_samples, 1)); };
		const auto strength = [&](const Candidate& a_candidate) {
			return mean(a_candidate) * (std::ranges::find(_pairs, a_candidate.pair) != _pairs.end() ? kIncumbent : 1.0f);
		};
		for (auto& candidate : _candidates) {
			candidate.active = candidate.active ? mean(candidate) >= kStay : mean(candidate) >= kEnter;
		}
		const auto beaten = [&](const Candidate& a_candidate) {
			for (const auto& other : _candidates) {
				if (&other == &a_candidate || !other.active || strength(other) <= strength(a_candidate)) {
					continue;
				}
				if (other.tool == a_candidate.tool || (other.shaft && a_candidate.shaft && other.target == a_candidate.target)) {
					return true;
				}
				if (other.pair.act == Act::kFellatio && a_candidate.tool == (other.target / 4) * 8 + 1) {
					return true;
				}
			}
			return false;
		};
		std::vector<const Candidate*> winners;
		for (const auto& candidate : _candidates) {
			if (candidate.active && Allowed(candidate.pair.act) && !beaten(candidate)) {
				winners.push_back(&candidate);
			}
		}
		std::ranges::sort(winners, [](const Candidate* a_left, const Candidate* a_right) {
			return a_left->tool != a_right->tool ? a_left->tool < a_right->tool : a_left->target < a_right->target;
		});
		std::vector<Pair> pairs;
		for (const auto* winner : winners) {
			pairs.push_back(winner->pair);
		}
		if (pairs == _pairs) {
			return false;
		}
		_pairs = std::move(pairs);
		return true;
	}

	std::string Detector::Describe() const
	{
		std::string text;
		for (const auto& pair : _pairs) {
			const auto giver = pair.giver.get();
			const auto receiver = pair.receiver.get();
			text += std::format("{}{} gives {} to {}", text.empty() ? "" : "; ", giver ? giver->GetDisplayFullName() : "?", ActName(pair.act),
				receiver ? receiver->GetDisplayFullName() : "?");
		}
		return text.empty() ? std::string("nothing detected") : text;
	}
}
