#include "Anatomy.h"

#include "Settings.h"

#include <unordered_map>

namespace OStrands
{
	std::string_view ActName(Act a_act)
	{
		switch (a_act) {
		case Act::kVaginal:
			return "vaginal";
		case Act::kAnal:
			return "anal";
		case Act::kFellatio:
			return "blowjob";
		case Act::kCunnilingus:
			return "cunnilingus";
		case Act::kAnilingus:
			return "rimming";
		case Act::kVaginalFingering:
			return "vaginal fingering";
		case Act::kAnalFingering:
			return "anal fingering";
		case Act::kTribbing:
			return "scissoring";
		case Act::kVaginalToy:
			return "vaginal toy";
		case Act::kAnalToy:
			return "anal toy";
		}
		return "?";
	}

	namespace Anatomy
	{
		namespace
		{
			using CacheClock = std::chrono::steady_clock;

			struct ShaftEntry
			{
				RE::NiPointer<RE::NiAVObject> root;
				std::uint64_t               equipment{ 0 };
				CacheClock::time_point      checked{};
				CacheClock::time_point      seen{};
				ShaftState                  state;
			};

			std::unordered_map<std::uint32_t, ShaftEntry> shaftCache;
			CacheClock::time_point                        lastPruned{};

			std::uint64_t EquipmentSignature(RE::Actor* a_actor)
			{
				const auto biped = a_actor->GetBiped(false);
				std::uint64_t signature = 14695981039346656037ull;
				const auto mix = [&](const void* a_pointer) {
					signature = (signature ^ reinterpret_cast<std::uintptr_t>(a_pointer)) * 1099511628211ull;
				};
				mix(biped.get());
				if (biped) {
					for (const auto& object : biped->objects) {
						mix(object.item);
						mix(object.addon);
						mix(object.partClone.get());
					}
				}
				return signature;
			}

			bool ScanSchlong(RE::NiAVObject* a_root)
			{
				const auto&                          names = Names::Get();
				const std::array<RE::NiAVObject*, 3> shaft{ a_root->GetObjectByName(names.shaft[3]), a_root->GetObjectByName(names.shaft[4]),
					a_root->GetObjectByName(names.shaft[5]) };
				if (!shaft.back()) {
					return false;
				}
				bool found = false;
				RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
					auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
					if (!skin || !skin->bones || !skin->skinData) {
						return RE::BSVisit::BSVisitControl::kContinue;
					}
					for (std::uint32_t i = 0; i < skin->skinData->bones; ++i) {
						if (skin->bones[i] && std::ranges::find(shaft, skin->bones[i]) != shaft.end()) {
							found = true;
							return RE::BSVisit::BSVisitControl::kStop;
						}
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
				return found;
			}

			RE::NiPoint3 Lerp(const RE::NiPoint3& a, const RE::NiPoint3& b, float t) { return a + (b - a) * t; }

			RE::NiPoint3 Normalized(RE::NiPoint3 v, const RE::NiPoint3& fallback)
			{
				return v.Unitize() > 1e-5f ? v : fallback;
			}

			RE::NiPoint3 BodyUp(RE::NiAVObject* a_root, const RE::NiPoint3& a_inward)
			{
				static const RE::BSFixedString spine{ "NPC Spine [Spn0]" };
				const RE::NiPoint3             fallback = a_root->world.rotate * RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
				auto*                          pelvis = a_root->GetObjectByName(Names::Get().pelvis);
				auto*                          chest = a_root->GetObjectByName(spine);
				RE::NiPoint3                   up = pelvis && chest ? chest->world.translate - pelvis->world.translate : fallback;
				up -= a_inward * up.Dot(a_inward);
				return Normalized(up, fallback);
			}

			void Shift(std::vector<Point>& a_points, RE::NiPoint3& a_center, const RE::NiPoint3& a_offset)
			{
				if (a_offset.SqrLength() < 1e-8f) {
					return;
				}
				a_center += a_offset;
				for (auto& point : a_points) {
					const auto& world = point.node->world;
					point.local += world.rotate.Transpose() * a_offset / std::max(world.scale, 1e-4f);
				}
			}

			void Align(RE::NiAVObject* a_root, std::vector<Point>& a_points, RE::NiPoint3& a_center, const RE::NiPoint3& a_inward, float a_out, float a_up)
			{
				Shift(a_points, a_center, (BodyUp(a_root, a_inward) * a_up - a_inward * a_out) * a_root->world.scale);
			}
		}

		const Names& Names::Get()
		{
			static const Names names;
			return names;
		}

		Opening OpeningOf(Act a_act)
		{
			switch (a_act) {
			case Act::kAnal:
			case Act::kAnilingus:
			case Act::kAnalFingering:
			case Act::kAnalToy:
				return Opening::kAnus;
			case Act::kFellatio:
				return Opening::kMouth;
			default:
				return Opening::kVagina;
			}
		}

		bool IsPenis(Act a_act) { return a_act == Act::kVaginal || a_act == Act::kAnal || a_act == Act::kFellatio; }
		bool IsFingers(Act a_act) { return a_act == Act::kVaginalFingering || a_act == Act::kAnalFingering; }
		bool IsOralGiver(Act a_act) { return a_act == Act::kCunnilingus || a_act == Act::kAnilingus; }
		bool IsToy(Act a_act) { return a_act == Act::kVaginalToy || a_act == Act::kAnalToy; }
		bool HasShaft(Act a_act) { return IsPenis(a_act) || IsFingers(a_act) || IsToy(a_act); }

		bool Collect(RE::NiAVObject* a_root, const std::vector<RE::BSFixedString>& a_names, std::vector<Point>& a_points, RE::NiPoint3& a_center)
		{
			a_points.clear();
			RE::NiPoint3 sum;
			for (const auto& name : a_names) {
				if (auto* node = a_root->GetObjectByName(name)) {
					a_points.push_back({ node, {} });
					sum += node->world.translate;
				}
			}
			if (a_points.empty()) {
				return false;
			}
			a_center = sum / static_cast<float>(a_points.size());
			return true;
		}

		bool MouthSite(RE::NiAVObject* a_root, std::vector<Point>& a_points, RE::NiPoint3& a_center, RE::NiPoint3& a_forward)
		{
			const auto& names = Names::Get();
			const auto& settings = Settings::Get();
			auto*       head = a_root->GetObjectByName(names.head);
			auto*       magic = a_root->GetObjectByName(names.headMagic);
			if (!head || !magic) {
				return false;
			}
			const RE::NiPoint3 headPos = head->world.translate;
			const RE::NiPoint3 magicPos = magic->world.translate;
			a_forward = Normalized(magicPos - headPos, head->world.rotate * RE::NiPoint3{ 0.0f, 1.0f, 0.0f });
			a_center = Lerp(headPos, magicPos, settings.mouthBlend);
			const RE::NiPoint3    up = head->world.rotate * RE::NiPoint3{ 0.0f, 0.0f, 1.0f } * head->world.scale;
			const RE::NiPoint3    right = head->world.rotate * RE::NiPoint3{ 1.0f, 0.0f, 0.0f } * head->world.scale;
			a_center += a_forward * (settings.mouthOut * head->world.scale) + up * settings.mouthUp;
			const RE::NiTransform toHead = head->world.Invert();
			a_points.clear();
			for (const RE::NiPoint3& offset : { up * settings.lipHeight, up * -settings.lipHeight, right * settings.lipWidth, right * -settings.lipWidth }) {
				a_points.push_back({ head, toHead * (a_center + offset) });
			}
			return true;
		}

		const char* FindOpening(RE::NiAVObject* a_root, Opening a_opening, std::vector<Point>& a_points, RE::NiPoint3& a_center, RE::NiPoint3& a_inward)
		{
			if (a_opening == Opening::kMouth) {
				RE::NiPoint3 forward;
				if (!MouthSite(a_root, a_points, a_center, forward)) {
					return "receiver skeleton has no NPC Head / NPC Head MagicNode";
				}
				a_inward = -forward;
				return nullptr;
			}
			const auto& names = Names::Get();
			const bool  vagina = a_opening == Opening::kVagina;
			if (!Collect(a_root, vagina ? names.vaginaRim : names.anusRim, a_points, a_center)) {
				return "receiver skeleton has none of the rim bones";
			}
			auto* deep = a_root->GetObjectByName(vagina ? names.vaginaDeep : names.anusDeep);
			if (!deep) {
				return vagina ? "receiver skeleton has no VaginaDeep1" : "receiver skeleton has no NPC Anus Deep1";
			}
			a_inward = deep->world.translate - a_center;
			if (a_inward.Unitize() < 1e-3f) {
				return "deep bone sits on the rim";
			}
			const auto& settings = Settings::Get();
			Align(a_root, a_points, a_center, a_inward, vagina ? settings.vaginaOut : settings.anusOut, vagina ? settings.vaginaUp : settings.anusUp);
			return nullptr;
		}

		bool VulvaSite(RE::NiAVObject* a_root, std::vector<Point>& a_points, RE::NiPoint3& a_center)
		{
			const auto& names = Names::Get();
			if (!Collect(a_root, names.vaginaRim, a_points, a_center)) {
				return false;
			}
			RE::NiPoint3 inward;
			if (auto* deep = a_root->GetObjectByName(names.vaginaDeep)) {
				inward = deep->world.translate - a_center;
				if (inward.Unitize() < 1e-3f) {
					inward = {};
				}
			}
			const auto& settings = Settings::Get();
			Align(a_root, a_points, a_center, inward, settings.vaginaOut, settings.vaginaUp);
			return true;
		}

		bool Allowed(Act a_act)
		{
			const auto& settings = Settings::Get();
			switch (a_act) {
			case Act::kFellatio:
			case Act::kCunnilingus:
			case Act::kAnilingus:
				return settings.oral;
			case Act::kVaginalFingering:
			case Act::kAnalFingering:
				return settings.fingering;
			case Act::kTribbing:
				return settings.tribbing;
			case Act::kVaginalToy:
			case Act::kAnalToy:
				return settings.toys;
			default:
				return true;
			}
		}

		RE::NiAVObject* CreatureTipNode(RE::NiAVObject* a_root)
		{
			if (!a_root) {
				return nullptr;
			}
			const auto& names = Names::Get();
			// Prefer the farthest available genital bone; creature rigs can have fewer than six.
			for (std::size_t i = names.shaft.size(); i-- > 0;) {
				if (auto* node = a_root->GetObjectByName(names.shaft[i])) {
					return node;
				}
			}
			// Common alternative genital-tip aliases (never fallback to arbitrary body parts).
			for (const char* name : { "NPC Penis Tip", "NPC PenisTip", "PenisTip", "Penis Tip", "Schlong Tip", "SchlongTip", "NPC Genitals Tip", "GenitalsTip", "NPC Genitals [Gen]", "NPC Penis [Penis]" }) {
				if (auto* node = a_root->GetObjectByName(RE::BSFixedString(name))) {
					return node;
				}
			}
			return nullptr;
		}

		bool IsPerson(RE::Actor* a_actor)
		{
			static auto* const person = RE::TESForm::LookupByID<RE::BGSKeyword>(0x13794);
			const auto*        race = a_actor ? a_actor->GetRace() : nullptr;
			if (!race) {
				return false;
			}
			if (!person || race->HasKeyword(person)) {
				return true;
			}
			return std::ranges::any_of(race->skeletonModels, [](const RE::TESModel& a_model) {
				const char* path = a_model.GetModel();
				return path && _strnicmp(path, "actors\\character\\", 17) == 0;
			});
		}

		void UpdateShaftCache()
		{
			const auto now = CacheClock::now();
			if (now - lastPruned >= 1s) {
				std::erase_if(shaftCache, [&](const auto& a_entry) {
					return now - a_entry.second.seen >= 30s || !a_entry.second.root || !a_entry.second.root->parent;
				});
				lastPruned = now;
			}
		}

		void ResetShaftCache()
		{
			shaftCache.clear();
			lastPruned = {};
		}

		ShaftState GetShaftState(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return {};
			}
			const auto handle = a_actor->GetHandle().native_handle();
			auto* root = a_actor->Get3D(false);
			if (!root) {
				shaftCache.erase(handle);
				return {};
			}
			const auto now = CacheClock::now();
			const auto equipment = EquipmentSignature(a_actor);
			auto [it, inserted] = shaftCache.try_emplace(handle);
			auto& entry = it->second;
			entry.seen = now;
			if (inserted || entry.root.get() != root || entry.equipment != equipment || now - entry.checked >= 500ms) {
				entry.root.reset(root);
				entry.equipment = equipment;
				entry.state.schlong = ScanSchlong(entry.root.get());
				// Baka ABC / MNC creatures may have tip bones without the human skinning layout.
				if (!entry.state.schlong && !IsPerson(a_actor)) {
					entry.state.schlong = CreatureTipNode(entry.root.get()) != nullptr;
				}
				const auto* base = a_actor->GetActorBase();
				entry.state.strapOn = base && base->GetSex() == RE::SEX::kFemale && IsStrapWearer(a_actor, WornModels(a_actor));
				entry.checked = now;
			}
			return entry.state;
		}

		bool HasSchlong(RE::Actor* a_actor)
		{
			return GetShaftState(a_actor).schlong;
		}

		RE::TESObjectARMO* BodyArmor(RE::Actor* a_actor)
		{
			if (!a_actor || !a_actor->GetParentCell()) {
				return nullptr;
			}
			const auto& biped = a_actor->GetBiped(false);
			auto*       item = biped ? biped->objects[RE::BIPED_OBJECT::kBody].item : nullptr;
			auto*       armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
			return armor && armor != a_actor->GetSkin() ? armor : nullptr;
		}

		std::vector<std::string> WornModels(RE::Actor* a_actor)
		{
			std::vector<std::string> paths;
			const auto&              biped = a_actor && a_actor->GetParentCell() ? a_actor->GetBiped(false) : RE::BSTSmartPointer<RE::BipedAnim>{};
			if (!biped) {
				return paths;
			}
			auto*                           skin = a_actor->GetSkin();
			std::vector<RE::TESObjectARMO*> seen;
			for (const auto& object : biped->objects) {
				auto* armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
				if (!armor || armor == skin || std::ranges::find(seen, armor) != seen.end()) {
					continue;
				}
				seen.push_back(armor);
				for (auto* addon : armor->armorAddons) {
					if (!addon) {
						continue;
					}
					for (const auto& model : addon->bipedModels) {
						std::string path = model.GetModel() ? model.GetModel() : "";
						std::ranges::transform(path, path.begin(), [](unsigned char c) { return c == '/' ? '\\' : static_cast<char>(std::tolower(c)); });
						if (!path.empty() && std::ranges::find(paths, path) == paths.end()) {
							paths.push_back(std::move(path));
						}
					}
				}
			}
			return paths;
		}

		bool IsStrapWearer(RE::Actor* a_actor, const std::vector<std::string>& a_models)
		{
			auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
			if (!base || base->GetSex() != RE::SEX::kFemale) {
				return false;
			}
			static constexpr std::array<std::string_view, 5> names{ "strapon", "strap-on", "strap_on", "strap on", "dildo" };
			return std::ranges::any_of(a_models, [](const std::string& path) {
				return std::ranges::any_of(names, [&](std::string_view name) { return path.find(name) != std::string::npos; });
			});
		}
	}
}
