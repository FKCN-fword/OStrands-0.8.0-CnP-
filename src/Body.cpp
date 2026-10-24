#include "Body.h"

#include "Anatomy.h"
#include "Settings.h"
#include "fluid/Clusters.h"

#include <bit>
#include <atomic>
#include <cstring>
#include <format>
#include <numeric>
#include <unordered_set>

namespace OStrands::Body
{
	namespace
	{
		enum Bone : std::size_t
		{
			kPelvis,
			kSpine1,
			kSpine2,
			kNeck,
			kHead,
			kLThigh,
			kLCalf,
			kLFoot,
			kRThigh,
			kRCalf,
			kRFoot,
			kLUpperArm,
			kLForearm,
			kLHand,
			kRUpperArm,
			kRForearm,
			kRHand,
			kLButt,
			kRButt,
			kLBreast,
			kRBreast,
			kBelly,
			kBones
		};

		const std::array<std::vector<const char*>, kBones> kBoneNames{ { { "NPC Pelvis [Pelv]" }, { "NPC Spine1 [Spn1]" },
			{ "NPC Spine2 [Spn2]" }, { "NPC Neck [Neck]" }, { "NPC Head [Head]" }, { "NPC L Thigh [LThg]" }, { "NPC L Calf [LClf]" },
			{ "NPC L Foot [Lft ]" }, { "NPC R Thigh [RThg]" }, { "NPC R Calf [RClf]" }, { "NPC R Foot [Rft ]" }, { "NPC L UpperArm [LUar]" },
			{ "NPC L Forearm [LLar]" }, { "NPC L Hand [LHnd]" }, { "NPC R UpperArm [RUar]" }, { "NPC R Forearm [RLar]" },
			{ "NPC R Hand [RHnd]" }, { "NPC L Butt" }, { "NPC R Butt" }, { "L Breast02", "NPC L Breast01", "NPC L Breast" },
			{ "R Breast02", "NPC R Breast01", "NPC R Breast" }, { "NPC Belly" } } };

		struct Shape
		{
			Bone        a;
			Bone        b;
			float       from;
			float       to;
			float       radius;
			const char* name;
		};

		constexpr std::array<Shape, 18> kShapes{ {
			{ kPelvis, kSpine1, 0.0f, 1.0f, 6.5f, "waist" },
			{ kSpine1, kSpine2, 0.3f, 1.6f, 7.0f, "chest" },
			{ kNeck, kHead, 0.0f, 1.0f, 3.2f, "neck" },
			{ kLThigh, kLCalf, 0.0f, 0.5f, 5.8f, "left thigh" },
			{ kLThigh, kLCalf, 0.5f, 1.0f, 4.4f, "left knee" },
			{ kRThigh, kRCalf, 0.0f, 0.5f, 5.8f, "right thigh" },
			{ kRThigh, kRCalf, 0.5f, 1.0f, 4.4f, "right knee" },
			{ kLCalf, kLFoot, 0.0f, 1.0f, 3.6f, "left calf" },
			{ kRCalf, kRFoot, 0.0f, 1.0f, 3.6f, "right calf" },
			{ kLUpperArm, kLForearm, 0.0f, 1.0f, 2.9f, "left arm" },
			{ kRUpperArm, kRForearm, 0.0f, 1.0f, 2.9f, "right arm" },
			{ kLForearm, kLHand, 0.0f, 1.0f, 2.3f, "left forearm" },
			{ kRForearm, kRHand, 0.0f, 1.0f, 2.3f, "right forearm" },
			{ kLButt, kLButt, 0.0f, 0.0f, 5.0f, "left buttock" },
			{ kRButt, kRButt, 0.0f, 0.0f, 5.0f, "right buttock" },
			{ kLBreast, kLBreast, 0.0f, 0.0f, 4.0f, "left breast" },
			{ kRBreast, kRBreast, 0.0f, 0.0f, 4.0f, "right breast" },
			{ kBelly, kBelly, 0.0f, 0.0f, 5.5f, "belly" },
		} };

		constexpr float       kHeadRadius = 5.8f;
		constexpr float       kHeadUp = 4.0f;
		constexpr float       kHeadForward = 1.0f;
		constexpr float       kMaxSpeed = 500.0f;
		constexpr float       kForget = 30.0f;
		constexpr std::size_t kMinimumSamples = 24;
		constexpr float       kFitLow = 0.5f;
		constexpr float       kFitHigh = 2.5f;
		constexpr std::size_t kShaftMinimum = 12;
		constexpr float       kTipRing = 0.3f;
		constexpr float       kThinnest = 0.05f;
		constexpr float         kRecheck = 2.0f;
		constexpr std::size_t   kInfluences = 4;
		constexpr float         kWrapVoxel = 1.5f;
		constexpr float         kWrapOutward = 0.65f;
		constexpr float         kWrapPush = 2.0f;
		constexpr std::uint32_t kWrapSmallest = 30;
		constexpr std::size_t   kWrapMinimum = 500;
		constexpr std::uint32_t kWrapVertexBudget = 200000;
		constexpr std::size_t   kHeadPatches = 50;
		constexpr std::uint32_t kDynamicStride = 16;
		constexpr float         kDynamicDrift = 1.0f;
		constexpr float         kGridCell = 6.0f;

		struct Entry
		{
			RE::NiPointer<RE::NiAVObject>       root;
			std::array<RE::NiPointer<RE::NiAVObject>, kBones> bones{};
			std::array<float, kShapes.size()>   fitted{};
			std::vector<Capsule>                capsules;
			std::vector<RE::NiPoint3>           previous;
			std::uint64_t                       frame{ 0 };
			float                               seen{ 0.0f };
			std::string                         fitReport;
			bool                                reported{ false };
			const void*                         body{ nullptr };
			std::uint32_t                       vertices{ 0 };
			float                               checked{ 0.0f };
		};

		struct Influence
		{
			RE::NiPointer<RE::NiAVObject> bone;
			RE::NiTransform skinToBone;
			float           weight{ 0.0f };
		};

		struct Mark
		{
			RE::NiPoint3           point;
			std::vector<Influence> influences;
		};

		struct Piece
		{
			Mark  from;
			Mark  to;
			float radius{ 0.0f };
		};

		struct Schlong
		{
			RE::NiPointer<RE::NiAVObject>                       root;
			RE::NiPointer<RE::NiAVObject>                       tipBone;
			std::vector<std::pair<const void*, std::uint32_t>> signature;
			bool                                                measured{ false };
			Mark                                                tip;
			RE::NiPoint3                                        axis;
			std::vector<Piece>                                  pieces;
			std::vector<Capsule>                                capsules;
			std::vector<RE::NiPoint3>                           previous;
			std::uint64_t                                       frame{ 0 };
			float                                               seen{ 0.0f };
			float                                               checked{ 0.0f };
		};

		struct Sample
		{
			RE::NiPoint3                point;
			std::array<float, 4>        weights{};
			std::array<std::uint8_t, 4> bones{};
			std::size_t                 slot{ 0 };
		};

		struct Source
		{
			RE::NiSkinInstance* skin{ nullptr };
			std::uint32_t       tipIndex{ 0 };
			std::vector<Sample> samples;
		};

		struct Joint
		{
			RE::NiPointer<RE::NiAVObject> bone;
			RE::NiTransform               skinToBone;
		};

		struct WrapPoint
		{
			RE::NiPoint3                 position;
			RE::NiPoint3                 normal;
			std::array<std::uint16_t, 4> joints{};
			std::array<float, 4>         weights{};
		};

		struct Dynamic
		{
			RE::NiPointer<RE::BSGeometry>                    geometry;
			std::vector<std::pair<std::uint32_t, std::uint32_t>> points;
		};

		struct Wrap
		{
			RE::NiPointer<RE::NiAVObject>                       root;
			std::vector<std::pair<const void*, std::uint32_t>> signature;
			std::vector<Joint>                                  joints;
			std::vector<WrapPoint>                              points;
			std::vector<Dynamic>                                dynamics;
			std::vector<Patch>                                  patches;
			std::vector<Capsule>                                extras;
			Fluid::Skin                                         grid;
			std::uint64_t                                       gridFrame{ 0 };
			std::vector<RE::NiPoint3>                           previous;
			bool                                                head{ false };
			bool                                                moving{ false };
			std::uint64_t                                       frame{ 0 };
			float                                               seen{ 0.0f };
			float                                               checked{ 0.0f };
		};

		struct Mesh
		{
			std::vector<RE::NiPoint3>                positions;
			std::vector<RE::NiPoint3>                normals;
			std::vector<std::array<float, 4>>        weights;
			std::vector<std::array<std::uint8_t, 4>> bones;
			bool                                     dynamic{ false };
		};

		std::unordered_map<std::uint32_t, Entry>   entries;
		std::unordered_map<std::uint32_t, Schlong> schlongs;
		std::unordered_map<std::uint32_t, Wrap>    wraps;
		std::uint64_t                              frame{ 1 };
		float                                    elapsed{ 0.0f };
		float                                    clock{ 0.0f };
		std::atomic<bool>                         refitRequested{ false };

		float Half(std::uint16_t a_value)
		{
			const std::uint32_t sign = static_cast<std::uint32_t>(a_value & 0x8000u) << 16;
			std::uint32_t       exponent = (a_value >> 10) & 0x1Fu;
			std::uint32_t       mantissa = a_value & 0x3FFu;
			std::uint32_t       bits = sign;
			if (exponent == 0) {
				if (mantissa != 0) {
					exponent = 113;
					while (!(mantissa & 0x400u)) {
						mantissa <<= 1;
						--exponent;
					}
					bits |= (exponent << 23) | ((mantissa & 0x3FFu) << 13);
				}
			} else if (exponent == 31) {
				bits |= 0x7F800000u | (mantissa << 13);
			} else {
				bits |= ((exponent + 112) << 23) | (mantissa << 13);
			}
			return std::bit_cast<float>(bits);
		}

		float SegmentDistance(const RE::NiPoint3& a_point, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
		{
			const RE::NiPoint3 span = a_to - a_from;
			const float        length = span.SqrLength();
			const float        t = length > 1e-8f ? std::clamp((a_point - a_from).Dot(span) / length, 0.0f, 1.0f) : 0.0f;
			return a_point.GetDistance(a_from + span * t);
		}

		std::uint32_t StrideOf(const RE::BSGraphics::VertexDesc& a_desc)
		{
			return static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(a_desc) & 0xF) * 4;
		}

		bool FullPrecision(const RE::BSGraphics::VertexDesc& a_desc)
		{
			std::uint32_t position = StrideOf(a_desc);
			for (std::uint8_t attribute = RE::BSGraphics::Vertex::VA_TEXCOORD0; attribute < RE::BSGraphics::Vertex::VA_COUNT; ++attribute) {
				const auto flag = static_cast<RE::BSGraphics::Vertex::Flags>(1u << attribute);
				if (a_desc.HasFlag(flag)) {
					position = std::min(position, a_desc.GetAttributeOffset(static_cast<RE::BSGraphics::Vertex::Attribute>(attribute)));
				}
			}
			return position >= 12;
		}

		RE::NiPoint3 PositionOf(const std::uint8_t* a_vertex, bool a_full)
		{
			if (a_full) {
				std::array<float, 3> xyz{};
				std::memcpy(xyz.data(), a_vertex, sizeof(xyz));
				return { xyz[0], xyz[1], xyz[2] };
			}
			std::array<std::uint16_t, 3> xyz{};
			std::memcpy(xyz.data(), a_vertex, sizeof(xyz));
			return { Half(xyz[0]), Half(xyz[1]), Half(xyz[2]) };
		}

		RE::NiPoint3 Unit(RE::NiPoint3 a_vector, const RE::NiPoint3& a_fallback)
		{
			return a_vector.Unitize() > 1e-5f ? a_vector : a_fallback;
		}

		std::uint32_t VerticesOf(RE::BSGeometry* a_geometry)
		{
			auto* skin = a_geometry ? a_geometry->GetGeometryRuntimeData().skinInstance.get() : nullptr;
			auto* partition = skin ? skin->skinPartition.get() : nullptr;
			return partition ? partition->vertexCount : 0u;
		}

		RE::BSGeometry* FindBody(Entry& a_entry)
		{
			RE::BSGeometry* body = nullptr;
			std::uint32_t   most = 0;
			RE::BSVisit::TraverseScenegraphGeometries(a_entry.root.get(), [&](RE::BSGeometry* a_geometry) {
				auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
				if (!skin || !skin->skinData || !skin->bones || !skin->skinPartition) {
					return RE::BSVisit::BSVisitControl::kContinue;
				}
				bool thigh = false;
				for (std::uint32_t i = 0; i < skin->skinData->bones && !thigh; ++i) {
					thigh = skin->bones[i] && skin->bones[i] == a_entry.bones[kLThigh].get();
				}
				if (thigh && skin->skinPartition->vertexCount > most) {
					most = skin->skinPartition->vertexCount;
					body = a_geometry;
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return body;
		}

		void Fit(Entry& a_entry)
		{
			auto*      body = a_entry.bones[kLThigh] ? FindBody(a_entry) : nullptr;
			auto*      skin = body ? body->GetGeometryRuntimeData().skinInstance.get() : nullptr;
			auto*      partition = skin ? skin->skinPartition.get() : nullptr;
			a_entry.body = body;
			a_entry.vertices = partition ? partition->vertexCount : 0u;
			a_entry.fitted = {};
			a_entry.fitReport = std::format("mesh='{}', vertices={}; ", body && body->name.c_str() ? body->name.c_str() : "<none>",
				partition ? partition->vertexCount : 0u);
			if (!partition || partition->numPartitions == 0 || !partition->partitions[0].buffData ||
				!partition->partitions[0].buffData->rawVertexData) {
				a_entry.fitReport += "no readable vertex buffer; using default body shapes";
				return;
			}
			const auto&         first = partition->partitions[0];
			const std::uint32_t stride = StrideOf(first.vertexDesc);
			const bool          full = FullPrecision(first.vertexDesc);
			const std::uint8_t* data = first.buffData->rawVertexData;
			if (stride < 8 || stride > 128) {
				a_entry.fitReport += std::format("unexpected vertex stride {}; using default body shapes", stride);
				return;
			}

			const auto skinned = [&](const RE::NiAVObject* a_bone) {
				for (std::uint32_t i = 0; i < skin->skinData->bones; ++i) {
					if (skin->bones[i] == a_bone) {
						return true;
					}
				}
				return false;
			};
			for (std::size_t b = 0; b < kBones; ++b) {
				if (kBoneNames[b].size() < 2 || (a_entry.bones[b] && skinned(a_entry.bones[b].get()))) {
					continue;
				}
				for (const char* name : kBoneNames[b]) {
					auto* bone = a_entry.root->GetObjectByName(name);
					if (bone && skinned(bone)) {
						a_entry.bones[b].reset(bone);
						break;
					}
				}
			}

			std::array<std::optional<RE::NiPoint3>, kBones> bind;
			for (std::uint32_t i = 0; i < skin->skinData->bones; ++i) {
				for (std::size_t b = 0; b < kBones; ++b) {
					if (skin->bones[i] && skin->bones[i] == a_entry.bones[b].get()) {
						bind[b] = skin->skinData->GetBoneDataSkinToBone(i).Invert().translate;
					}
				}
			}
			struct Segment
			{
				RE::NiPoint3 from;
				RE::NiPoint3 to;
				bool         valid{ false };
			};
			std::array<Segment, kShapes.size()> segments{};
			for (std::size_t k = 0; k < kShapes.size(); ++k) {
				const auto& shape = kShapes[k];
				if (bind[shape.a] && bind[shape.b]) {
					const RE::NiPoint3 a = *bind[shape.a];
					const RE::NiPoint3 b = *bind[shape.b];
					segments[k] = { a + (b - a) * shape.from, a + (b - a) * shape.to, true };
				}
			}

			std::array<std::vector<float>, kShapes.size()> samples;
			for (std::uint32_t v = 0; v < partition->vertexCount; ++v) {
				const RE::NiPoint3 point = PositionOf(data + static_cast<std::size_t>(v) * stride, full);
				std::size_t        best = kShapes.size();
				float       bestGap = std::numeric_limits<float>::max();
				float       bestDistance = 0.0f;
				for (std::size_t k = 0; k < kShapes.size(); ++k) {
					if (!segments[k].valid) {
						continue;
					}
					const float distance = SegmentDistance(point, segments[k].from, segments[k].to);
					const float gap = distance - kShapes[k].radius;
					if (gap < bestGap) {
						bestGap = gap;
						bestDistance = distance;
						best = k;
					}
				}
				if (best < kShapes.size() && bestGap < kShapes[best].radius) {
					samples[best].push_back(bestDistance);
				}
			}

			std::size_t fitted = 0;
			std::string report;
			for (std::size_t k = 0; k < kShapes.size(); ++k) {
				auto& list = samples[k];
				if (!report.empty()) {
					report += "; ";
				}
				if (!segments[k].valid) {
					report += std::format("{}: default r={:.2f}, missing bind bone", kShapes[k].name, kShapes[k].radius);
					continue;
				}
				if (list.size() < kMinimumSamples) {
					report += std::format("{}: default r={:.2f}, samples={} (<{})", kShapes[k].name, kShapes[k].radius, list.size(), kMinimumSamples);
					continue;
				}
				const auto middle = list.begin() + static_cast<std::ptrdiff_t>(list.size() / 2);
				std::nth_element(list.begin(), middle, list.end());
				a_entry.fitted[k] = std::clamp(*middle, kShapes[k].radius * kFitLow, kShapes[k].radius * kFitHigh);
				++fitted;
				report += std::format("{}: r={:.2f}, median={:.2f}, samples={}{}", kShapes[k].name, a_entry.fitted[k], *middle,
					list.size(), a_entry.fitted[k] != *middle ? " (clamped)" : "");
			}
			a_entry.fitReport += std::format("fitted {}/{}; bind-space radii before actor scale and Thickness: {}; head uses its fixed default",
				fitted, kShapes.size(), report);
		}

		void Track(std::vector<Capsule>& a_capsules, std::vector<RE::NiPoint3>& a_previous)
		{
			const bool moving = a_previous.size() == a_capsules.size() * 2 && elapsed > 0.0f;
			a_previous.resize(a_capsules.size() * 2);
			for (std::size_t k = 0; k < a_capsules.size(); ++k) {
				auto& capsule = a_capsules[k];
				if (moving) {
					capsule.velocityA = (capsule.a - a_previous[2 * k]) / elapsed;
					capsule.velocityB = (capsule.b - a_previous[2 * k + 1]) / elapsed;
					if (capsule.velocityA.Length() > kMaxSpeed || capsule.velocityB.Length() > kMaxSpeed) {
						capsule.velocityA = capsule.velocityB = {};
					}
				}
				a_previous[2 * k] = capsule.a;
				a_previous[2 * k + 1] = capsule.b;
			}
		}

		void Build(Entry& a_entry, float a_thickness)
		{
			std::vector<Capsule> next;
			next.reserve(kShapes.size() + 1);
			for (std::size_t k = 0; k < kShapes.size(); ++k) {
				const auto& shape = kShapes[k];
				auto*       a = a_entry.bones[shape.a].get();
				auto*       b = a_entry.bones[shape.b].get();
				if (!a || !b) {
					continue;
				}
				const RE::NiPoint3 from = a->world.translate;
				const RE::NiPoint3 to = b->world.translate;
				const float        radius = (a_entry.fitted[k] > 0.0f ? a_entry.fitted[k] : shape.radius) * a->world.scale * a_thickness;
				next.push_back({ from + (to - from) * shape.from, from + (to - from) * shape.to, radius });
			}
			if (auto* head = a_entry.bones[kHead].get()) {
				const RE::NiPoint3 center = head->world.translate +
				                            head->world.rotate * RE::NiPoint3{ 0.0f, kHeadForward, kHeadUp } * head->world.scale;
				next.push_back({ center, center, kHeadRadius * head->world.scale * a_thickness });
			}
			Track(next, a_entry.previous);
			a_entry.capsules = std::move(next);
		}

		RE::NiPoint3 Place(const Mark& a_mark)
		{
			RE::NiPoint3 sum;
			for (const auto& influence : a_mark.influences) {
				sum += (influence.bone->world * (influence.skinToBone * a_mark.point)) * influence.weight;
			}
			return sum;
		}

		RE::NiPoint3 Turn(const Mark& a_mark, const RE::NiPoint3& a_direction)
		{
			RE::NiPoint3 sum;
			for (const auto& influence : a_mark.influences) {
				sum += (influence.bone->world.rotate * (influence.skinToBone.rotate * a_direction)) * influence.weight;
			}
			return sum;
		}

		float ScaleOf(const Mark& a_mark)
		{
			float sum = 0.0f;
			for (const auto& influence : a_mark.influences) {
				sum += influence.bone->world.scale * influence.skinToBone.scale * influence.weight;
			}
			return sum;
		}

		std::vector<RE::BSGeometry*> Candidates(RE::NiAVObject* a_root, RE::NiAVObject* a_tipBone)
		{
			std::vector<RE::BSGeometry*> found;
			RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
				auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
				if (!skin || !skin->skinData || !skin->bones) {
					return RE::BSVisit::BSVisitControl::kContinue;
				}
				for (std::uint32_t i = 0; i < skin->skinData->bones; ++i) {
					if (skin->bones[i] == a_tipBone) {
						found.push_back(a_geometry);
						break;
					}
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return found;
		}

		bool Decode(RE::BSGeometry* a_geometry, const std::array<RE::NiAVObject*, 6>& a_shaft, Source& a_out)
		{
			auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			auto* partition = skin ? skin->skinPartition.get() : nullptr;
			if (!skin || !skin->skinData || !skin->bones || !partition || partition->numPartitions == 0 ||
				!partition->partitions[0].buffData || !partition->partitions[0].buffData->rawVertexData) {
				return false;
			}
			const auto&         first = partition->partitions[0];
			const std::uint32_t stride = StrideOf(first.vertexDesc);
			const std::uint32_t skinning = first.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
			if (stride < 8 || stride > 128 || !first.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED) || skinning + 12 > stride) {
				return false;
			}
			const std::uint32_t count = skin->skinData->bones;
			std::vector<int>    slots(count, -1);
			bool                tip = false;
			for (std::uint32_t i = 0; i < count; ++i) {
				for (std::size_t k = 0; k < a_shaft.size(); ++k) {
					if (skin->bones[i] && skin->bones[i] == a_shaft[k]) {
						slots[i] = static_cast<int>(k);
						if (k + 1 == a_shaft.size()) {
							a_out.tipIndex = i;
							tip = true;
						}
					}
				}
			}
			if (!tip) {
				return false;
			}
			a_out.skin = skin;
			a_out.samples.clear();
			const bool          full = FullPrecision(first.vertexDesc);
			const std::uint8_t* data = first.buffData->rawVertexData;
			for (std::uint32_t v = 0; v < partition->vertexCount; ++v) {
				const std::uint8_t* vertex = data + static_cast<std::size_t>(v) * stride;
				Sample              sample;
				sample.point = PositionOf(vertex, full);
				std::array<std::uint16_t, 4> halves{};
				std::memcpy(halves.data(), vertex + skinning, sizeof(halves));
				std::memcpy(sample.bones.data(), vertex + skinning + 8, sizeof(sample.bones));
				std::size_t dominant = 0;
				for (std::size_t j = 0; j < halves.size(); ++j) {
					sample.weights[j] = Half(halves[j]);
					if (sample.weights[j] > sample.weights[dominant]) {
						dominant = j;
					}
				}
				const std::uint8_t bone = sample.bones[dominant];
				if (!(sample.weights[dominant] > 0.0f) || bone >= count || slots[bone] < 0) {
					continue;
				}
				sample.slot = static_cast<std::size_t>(slots[bone]);
				a_out.samples.push_back(sample);
			}
			return true;
		}

		Mark MarkOf(const Source& a_source, std::span<const Sample* const> a_members, const RE::NiPoint3& a_point)
		{
			const std::uint32_t count = a_source.skin->skinData->bones;
			std::vector<float>  sum(count, 0.0f);
			for (const auto* sample : a_members) {
				for (std::size_t j = 0; j < sample->weights.size(); ++j) {
					if (sample->bones[j] < count && sample->weights[j] > 0.0f) {
						sum[sample->bones[j]] += sample->weights[j];
					}
				}
			}
			std::vector<std::uint32_t> order(count);
			std::iota(order.begin(), order.end(), 0u);
			const auto top = order.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(kInfluences, count));
			std::partial_sort(order.begin(), top, order.end(), [&](std::uint32_t a_left, std::uint32_t a_right) { return sum[a_left] > sum[a_right]; });
			Mark  mark{ a_point, {} };
			float total = 0.0f;
			for (auto it = order.begin(); it != top; ++it) {
				if (sum[*it] > 0.0f && a_source.skin->bones[*it]) {
					mark.influences.push_back({ RE::NiPointer<RE::NiAVObject>(a_source.skin->bones[*it]), a_source.skin->skinData->GetBoneDataSkinToBone(*it), sum[*it] });
					total += sum[*it];
				}
			}
			for (auto& influence : mark.influences) {
				influence.weight /= total;
			}
			return mark;
		}

		void Measure(RE::Actor* a_actor, Schlong& a_schlong, const std::array<RE::NiAVObject*, 6>& a_shaft, std::span<RE::BSGeometry* const> a_candidates)
		{
			a_schlong.measured = false;
			a_schlong.pieces.clear();
			a_schlong.capsules.clear();
			a_schlong.previous.clear();
			const auto& settings = Settings::Get();
			Source      best;
			Source      current;
			for (auto* geometry : a_candidates) {
				if (Decode(geometry, a_shaft, current) && current.samples.size() > best.samples.size()) {
					std::swap(best, current);
				}
			}
			if (best.samples.size() < 2 * kShaftMinimum) {
				if (settings.debugLog) {
					logger::info("{}: no readable schlong mesh; the tip sits {} past the last genital bone", a_actor->GetDisplayFullName(),
						settings.tipExtension);
				}
				return;
			}

			const auto&        toTip = best.skin->skinData->GetBoneDataSkinToBone(best.tipIndex);
			const RE::NiPoint3 axis = Unit({ toTip.rotate.entry[1][0], toTip.rotate.entry[1][1], toTip.rotate.entry[1][2] }, { 0.0f, 1.0f, 0.0f });
			float              top = -std::numeric_limits<float>::max();
			for (const auto& sample : best.samples) {
				top = std::max(top, sample.point.Dot(axis));
			}
			std::vector<const Sample*> ring;
			RE::NiPoint3               center;
			for (const auto& sample : best.samples) {
				if (sample.point.Dot(axis) > top - kTipRing) {
					ring.push_back(&sample);
					center += sample.point;
				}
			}
			center = center / static_cast<float>(ring.size());
			const RE::NiPoint3 tip = center + axis * (top - center.Dot(axis));
			a_schlong.tip = MarkOf(best, ring, tip);
			a_schlong.axis = axis;
			if (a_schlong.tip.influences.empty()) {
				return;
			}

			for (std::size_t k = 0; k < a_shaft.size(); ++k) {
				std::vector<const Sample*> group;
				RE::NiPoint3               centroid;
				for (const auto& sample : best.samples) {
					if (sample.slot == k) {
						group.push_back(&sample);
						centroid += sample.point;
					}
				}
				if (group.size() < kShaftMinimum) {
					continue;
				}
				centroid = centroid / static_cast<float>(group.size());
				std::vector<float> along;
				std::vector<float> lateral;
				for (const auto* sample : group) {
					const RE::NiPoint3 offset = sample->point - centroid;
					along.push_back(sample->point.Dot(axis));
					lateral.push_back((offset - axis * offset.Dot(axis)).Length());
				}
				std::ranges::sort(along);
				const auto middle = lateral.begin() + static_cast<std::ptrdiff_t>(lateral.size() / 2);
				std::nth_element(lateral.begin(), middle, lateral.end());
				const float radius = *middle;
				if (!(radius > kThinnest)) {
					continue;
				}
				const float  low = along[along.size() * 5 / 100];
				const float  high = std::min(along[along.size() * 95 / 100], top);
				const float  mid = centroid.Dot(axis);
				RE::NiPoint3 from = centroid + axis * (0.5f * (low + high) - mid);
				RE::NiPoint3 to = from;
				if (high - low >= 2.0f * radius) {
					from = centroid + axis * (low + radius - mid);
					to = centroid + axis * (high - radius - mid);
				}
				Mark weights = MarkOf(best, group, from);
				if (weights.influences.empty()) {
					continue;
				}
				Piece piece{ weights, weights, radius };
				piece.to.point = to;
				a_schlong.pieces.push_back(std::move(piece));
			}
			a_schlong.measured = true;
			if (settings.debugLog) {
				const RE::NiPoint3 bind = toTip.Invert().translate;
				const float        past = (tip - bind).Dot(axis);
				const float        off = ((tip - bind) - axis * past).Length();
				logger::info("{}: measured the schlong from {} vertices: tip {:.1f} past the last genital bone{}, {} shaft shapes",
					a_actor->GetDisplayFullName(), best.samples.size(), past, off > 0.5f ? std::format(" and {:.1f} off its axis", off) : std::string{},
					a_schlong.pieces.size());
			}
		}

		Schlong* SchlongOf(RE::Actor* a_actor)
		{
			auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
			if (!root) {
				return nullptr;
			}
			auto& schlong = schlongs[a_actor->GetHandle().native_handle()];
			schlong.seen = clock;
			const bool fresh = schlong.root.get() != root;
			if (!fresh && clock - schlong.checked < kRecheck) {
				return &schlong;
			}
			if (fresh) {
				schlong = Schlong{};
				schlong.root.reset(root);
				schlong.seen = clock;
			}
			schlong.checked = clock;
			const auto&                    names = Anatomy::Names::Get();
			std::array<RE::NiAVObject*, 6> shaft{};
			for (std::size_t k = 0; k < shaft.size(); ++k) {
				shaft[k] = root->GetObjectByName(names.shaft[k]);
			}
			schlong.tipBone.reset(shaft.back());
			if (!schlong.tipBone && !Anatomy::IsPerson(a_actor)) {
				schlong.tipBone.reset(Anatomy::CreatureTipNode(root));
			}
			if (!schlong.tipBone) {
				schlong.measured = false;
				schlong.signature.clear();
				return &schlong;
			}
			if (!shaft.back()) {
				// Alternative creature tip: use for emission only, never human six-bone measurement.
				schlong.measured = false;
				schlong.signature.clear();
				return &schlong;
			}
			const auto                                          candidates = Candidates(root, schlong.tipBone.get());
			std::vector<std::pair<const void*, std::uint32_t>> signature;
			for (auto* geometry : candidates) {
				auto* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
				signature.emplace_back(geometry, skin && skin->skinPartition ? skin->skinPartition->vertexCount : 0u);
			}
			if (fresh || signature != schlong.signature) {
				schlong.signature = std::move(signature);
				Measure(a_actor, schlong, shaft, candidates);
			}
			return &schlong;
		}

		bool Hidden(RE::NiAVObject* a_object, RE::NiAVObject* a_root)
		{
			for (auto* node = a_object; node && node != a_root; node = node->parent) {
				if (node->GetAppCulled()) {
					return true;
				}
			}
			return false;
		}

		const char* Excluded(RE::BSGeometry* a_geometry, RE::NiAVObject* a_root)
		{
			const auto& data = a_geometry->GetGeometryRuntimeData();
			auto*       skin = data.skinInstance.get();
			auto*       partition = skin ? skin->skinPartition.get() : nullptr;
			if (!skin || !skin->skinData || !skin->bones || !partition || partition->numPartitions == 0) {
				return "not skinned";
			}
			if (partition->vertexCount < kWrapSmallest) {
				return "too small";
			}
			if (Hidden(a_geometry, a_root)) {
				return "hidden";
			}
			if (data.alphaProperty && data.alphaProperty->GetAlphaBlending()) {
				return "see-through";
			}
			auto* shader = data.shaderProperty.get();
			auto* material = shader ? shader->GetBaseMaterial() : nullptr;
			if (material && material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint) {
				return "hair";
			}
			return nullptr;
		}

		std::vector<RE::BSGeometry*> WrapGeometries(RE::NiAVObject* a_root)
		{
			std::vector<RE::BSGeometry*> found;
			RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
				if (!Excluded(a_geometry, a_root)) {
					found.push_back(a_geometry);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return found;
		}

		bool ReadMesh(RE::BSGeometry* a_geometry, Mesh& a_out)
		{
			auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			auto* partition = skin ? skin->skinPartition.get() : nullptr;
			if (!skin || !skin->skinData || !skin->bones || !partition || !partition->numPartitions ||
				partition->vertexCount > kWrapVertexBudget) {
				return false;
			}
			const auto& first = partition->partitions[0];
			if (!first.buffData || !first.buffData->rawVertexData) {
				return false;
			}
			const std::uint32_t stride = StrideOf(first.vertexDesc);
			const std::uint32_t skinning = first.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
			if (stride < 8 || stride > 128 || !first.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED) || skinning + 12 > stride) {
				return false;
			}
			const std::uint32_t count = partition->vertexCount;
			const bool          stored = first.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
			const bool          full = FullPrecision(first.vertexDesc);
			const std::uint8_t* data = first.buffData->rawVertexData;
			a_out.positions.resize(count);
			a_out.weights.resize(count);
			a_out.bones.resize(count);
			a_out.dynamic = false;
			for (std::uint32_t v = 0; v < count; ++v) {
				const std::uint8_t* vertex = data + static_cast<std::size_t>(v) * stride;
				a_out.positions[v] = stored ? PositionOf(vertex, full) : RE::NiPoint3{};
				std::array<std::uint16_t, 4> halves{};
				std::memcpy(halves.data(), vertex + skinning, sizeof(halves));
				std::memcpy(a_out.bones[v].data(), vertex + skinning + 8, sizeof(a_out.bones[v]));
				for (std::size_t j = 0; j < halves.size(); ++j) {
					a_out.weights[v][j] = Half(halves[j]);
				}
			}
			if (auto* dynamic = a_geometry->AsDynamicTriShape()) {
				std::vector<RE::NiPoint3> moved(count);
				bool                      read = false;
				{
					auto&                runtime = dynamic->GetDynamicTrishapeRuntimeData();
					RE::BSSpinLockGuard guard(runtime.lock);
					if (runtime.dynamicData && runtime.dataSize >= count * kDynamicStride) {
						const auto* source = static_cast<const std::uint8_t*>(runtime.dynamicData);
						for (std::uint32_t v = 0; v < count; ++v) {
							std::array<float, 3> xyz{};
							std::memcpy(xyz.data(), source + static_cast<std::size_t>(v) * kDynamicStride, sizeof(xyz));
							moved[v] = { xyz[0], xyz[1], xyz[2] };
						}
						read = true;
					}
				}
				float drift = 0.0f;
				for (std::uint32_t v = 0; read && stored && v < count; ++v) {
					drift += moved[v].GetDistance(a_out.positions[v]);
				}
				if (read && (!stored || drift < kDynamicDrift * static_cast<float>(count))) {
					a_out.positions = std::move(moved);
					a_out.dynamic = true;
				}
			}
			if (!stored && !a_out.dynamic) {
				return false;
			}
			a_out.normals.assign(count, RE::NiPoint3{});
			for (std::uint32_t p = 0; p < partition->numPartitions; ++p) {
				const auto& part = partition->partitions[p];
				if (!part.triList) {
					continue;
				}
				std::uint32_t highest = 0;
				for (std::uint32_t i = 0; i < part.triangles * 3u; ++i) {
					highest = std::max<std::uint32_t>(highest, part.triList[i]);
				}
				const bool local = part.vertexMap && highest < part.vertices && part.vertices < count;
				const auto index = [&](std::uint32_t a_corner) -> std::uint32_t {
					const std::uint32_t value = part.triList[a_corner];
					return local ? part.vertexMap[value] : value;
				};
				for (std::uint32_t t = 0; t < part.triangles; ++t) {
					const std::uint32_t a = index(3 * t);
					const std::uint32_t b = index(3 * t + 1);
					const std::uint32_t c = index(3 * t + 2);
					if (a >= count || b >= count || c >= count) {
						continue;
					}
					const RE::NiPoint3 normal = (a_out.positions[b] - a_out.positions[a]).Cross(a_out.positions[c] - a_out.positions[a]);
					a_out.normals[a] += normal;
					a_out.normals[b] += normal;
					a_out.normals[c] += normal;
				}
			}
			return true;
		}

		void Track(std::vector<Patch>& a_patches, std::vector<RE::NiPoint3>& a_previous, bool& a_moving)
		{
			const bool moving = a_moving && a_previous.size() == a_patches.size() && elapsed > 0.0f;
			a_previous.resize(a_patches.size());
			for (std::size_t k = 0; k < a_patches.size(); ++k) {
				auto& patch = a_patches[k];
				patch.velocity = {};
				if (moving) {
					const RE::NiPoint3 velocity = (patch.position - a_previous[k]) / elapsed;
					if (velocity.Length() <= kMaxSpeed) {
						patch.velocity = velocity;
					}
				}
				a_previous[k] = patch.position;
			}
			a_moving = true;
		}

		void Enwrap(RE::Actor* a_actor, Wrap& a_wrap, std::span<RE::BSGeometry* const> a_geometries)
		{
			a_wrap.joints.clear();
			a_wrap.points.clear();
			a_wrap.dynamics.clear();
			a_wrap.patches.clear();
			a_wrap.previous.clear();
			a_wrap.moving = false;
			a_wrap.head = false;
			const auto& settings = Settings::Get();
			auto*       head = a_wrap.root->GetObjectByName(Anatomy::Names::Get().head);
			std::unordered_set<std::uint64_t> taken;
			std::size_t                       headPoints = 0;
			std::string                       report;
			Mesh                              mesh;
			const auto                        cell = [](float a_value) {
				return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(a_value / kWrapVoxel)) + (1 << 19)) & 0xFFFFF;
			};
			for (auto* geometry : a_geometries) {
				const char* name = geometry->name.c_str() ? geometry->name.c_str() : "?";
				if (!ReadMesh(geometry, mesh)) {
					report += std::format("{}{}: unreadable", report.empty() ? "" : ", ", name);
					continue;
				}
				auto* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
				if (!skin || !skin->skinData || !skin->bones) {
					report += std::format("{}{}: unreadable", report.empty() ? "" : ", ", name);
					continue;
				}
				const std::uint32_t bones = skin->skinData->bones;
				std::vector<RE::NiPoint3> binds(bones);
				for (std::uint32_t i = 0; i < bones; ++i) {
					binds[i] = skin->skinData->GetBoneDataSkinToBone(i).Invert().translate;
				}
				std::size_t outward = 0;
				std::size_t facing = 0;
				for (std::size_t v = 0; v < mesh.positions.size(); ++v) {
					const auto&       weights = mesh.weights[v];
					const std::size_t dominant = static_cast<std::size_t>(std::ranges::max_element(weights) - weights.begin());
					const std::uint8_t bone = mesh.bones[v][dominant];
					if (bone >= bones || mesh.normals[v].SqrLength() < 1e-12f) {
						continue;
					}
					++facing;
					outward += mesh.normals[v].Dot(mesh.positions[v] - binds[bone]) > 0.0f;
				}
				const float share = facing ? static_cast<float>(outward) / static_cast<float>(facing) : 0.0f;
				if (share < kWrapOutward) {
					report += std::format("{}{}: inner surface ({:.0f}% outward)", report.empty() ? "" : ", ", name, share * 100.0f);
					continue;
				}
				const std::size_t base = a_wrap.joints.size();
				for (std::uint32_t i = 0; i < bones; ++i) {
					a_wrap.joints.push_back({ RE::NiPointer<RE::NiAVObject>(skin->bones[i]), skin->skinData->GetBoneDataSkinToBone(i) });
				}
				Dynamic     dynamic;
				std::size_t kept = 0;
				for (std::uint32_t v = 0; v < mesh.positions.size(); ++v) {
					RE::NiPoint3 normal = mesh.normals[v];
					if (normal.Unitize() < 1e-6f) {
						continue;
					}
					const auto& position = mesh.positions[v];
					if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
						continue;
					}
					const std::uint64_t key = (cell(position.x) << 43) | (cell(position.y) << 23) | (cell(position.z) << 3) |
					                          (normal.x > 0.0f ? 4u : 0u) | (normal.y > 0.0f ? 2u : 0u) | (normal.z > 0.0f ? 1u : 0u);
					if (!taken.insert(key).second) {
						continue;
					}
					WrapPoint point{ position, normal, {}, {} };
					float     total = 0.0f;
					std::size_t dominant = 0;
					for (std::size_t j = 0; j < 4; ++j) {
						const std::uint8_t bone = mesh.bones[v][j];
						const float        weight = mesh.weights[v][j];
						if (bone < bones && skin->bones[bone] && std::isfinite(weight) && weight > 0.0f) {
							point.joints[j] = static_cast<std::uint16_t>(base + bone);
							point.weights[j] = weight;
							total += weight;
							if (weight > point.weights[dominant]) {
								dominant = j;
							}
						}
					}
					if (!(total > 0.0f)) {
						continue;
					}
					for (auto& weight : point.weights) {
						weight /= total;
					}
					if (head && a_wrap.joints[point.joints[dominant]].bone.get() == head) {
						++headPoints;
					}
					if (mesh.dynamic) {
						dynamic.points.emplace_back(static_cast<std::uint32_t>(a_wrap.points.size()), v);
					}
					a_wrap.points.push_back(point);
					++kept;
				}
				if (!dynamic.points.empty()) {
					dynamic.geometry.reset(geometry);
					a_wrap.dynamics.push_back(std::move(dynamic));
				}
				report += std::format("{}{}: {} of {} vertices{}", report.empty() ? "" : ", ", name, kept, mesh.positions.size(), mesh.dynamic ? " (animated face)" : "");
			}
			a_wrap.head = headPoints >= kHeadPatches;
			if (a_wrap.points.size() < kWrapMinimum) {
				a_wrap.points.clear();
				a_wrap.joints.clear();
				a_wrap.dynamics.clear();
			}
			if (settings.debugLog) {
				logger::info("{}: skin wrap {} points{}: {}", a_actor->GetDisplayFullName(), a_wrap.points.size(),
					a_wrap.points.empty() ? " (too few, using body shapes)" : a_wrap.head ? "" : " (no head mesh, using a head sphere)",
					report.empty() ? "no meshes" : report);
			}
		}

		Wrap* WrapOf(RE::Actor* a_actor)
		{
			auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
			if (!root) {
				return nullptr;
			}
			auto& wrap = wraps[a_actor->GetHandle().native_handle()];
			wrap.seen = clock;
			const bool fresh = wrap.root.get() != root;
			if (!fresh && clock - wrap.checked < kRecheck) {
				return &wrap;
			}
			if (fresh) {
				wrap = Wrap{};
				wrap.root.reset(root);
				wrap.seen = clock;
			}
			wrap.checked = clock;
			const auto                                          geometries = WrapGeometries(root);
			std::vector<std::pair<const void*, std::uint32_t>> signature;
			for (auto* geometry : geometries) {
				signature.emplace_back(geometry, VerticesOf(geometry));
			}
			if (fresh || signature != wrap.signature) {
				wrap.signature = std::move(signature);
				Enwrap(a_actor, wrap, geometries);
			}
			return &wrap;
		}

		void Pose(Wrap& a_wrap, RE::Actor* a_actor, float a_thickness);

		Wrap* Posed(RE::Actor* a_actor)
		{
			const auto& settings = Settings::Get();
			auto*       wrap = settings.collision ? WrapOf(a_actor) : nullptr;
			if (!wrap || wrap->points.empty()) {
				return nullptr;
			}
			if (wrap->frame != frame) {
				wrap->frame = frame;
				Pose(*wrap, a_actor, settings.collisionThickness);
			}
			return wrap;
		}

		void Pose(Wrap& a_wrap, RE::Actor* a_actor, float a_thickness)
		{
			for (auto& dynamic : a_wrap.dynamics) {
				auto*       shape = dynamic.geometry ? dynamic.geometry->AsDynamicTriShape() : nullptr;
				auto*       skin = shape ? shape->GetGeometryRuntimeData().skinInstance.get() : nullptr;
				auto*       partition = skin ? skin->skinPartition.get() : nullptr;
				if (!partition) {
					continue;
				}
				auto&                runtime = shape->GetDynamicTrishapeRuntimeData();
				RE::BSSpinLockGuard guard(runtime.lock);
				if (!runtime.dynamicData || runtime.dataSize < partition->vertexCount * kDynamicStride) {
					continue;
				}
				const auto* source = static_cast<const std::uint8_t*>(runtime.dynamicData);
				for (const auto& [point, vertex] : dynamic.points) {
					std::array<float, 3> xyz{};
					std::memcpy(xyz.data(), source + static_cast<std::size_t>(vertex) * kDynamicStride, sizeof(xyz));
					a_wrap.points[point].position = { xyz[0], xyz[1], xyz[2] };
				}
			}
			std::vector<RE::NiTransform> world(a_wrap.joints.size());
			for (std::size_t j = 0; j < a_wrap.joints.size(); ++j) {
				const auto& joint = a_wrap.joints[j];
				if (joint.bone) {
					world[j] = joint.bone->world * joint.skinToBone;
				}
			}
			const float push = (a_thickness - 1.0f) * kWrapPush;
			a_wrap.patches.resize(a_wrap.points.size());
			for (std::size_t k = 0; k < a_wrap.points.size(); ++k) {
				const auto&  point = a_wrap.points[k];
				RE::NiPoint3 position;
				RE::NiPoint3 normal;
				float        scale = 0.0f;
				for (std::size_t j = 0; j < 4; ++j) {
					const float weight = point.weights[j];
					if (weight <= 0.0f) {
						continue;
					}
					const auto& transform = world[point.joints[j]];
					position += (transform * point.position) * weight;
					normal += (transform.rotate * point.normal) * weight;
					scale += transform.scale * weight;
				}
				normal = Unit(normal, point.normal);
				a_wrap.patches[k].position = position + normal * (push * scale);
				a_wrap.patches[k].normal = normal;
			}
			Track(a_wrap.patches, a_wrap.previous, a_wrap.moving);

			std::vector<Capsule> extras;
			auto*                root = a_actor->Get3D(false);
			auto*                head = !a_wrap.head && root ? root->GetObjectByName(Anatomy::Names::Get().head) : nullptr;
			if (head) {
				const RE::NiPoint3 center = head->world.translate +
				                            head->world.rotate * RE::NiPoint3{ 0.0f, kHeadForward, kHeadUp } * head->world.scale;
				extras.push_back({ center, center, kHeadRadius * head->world.scale * a_thickness });
			}
			auto previous = std::vector<RE::NiPoint3>{};
			if (a_wrap.extras.size() == extras.size()) {
				for (const auto& capsule : a_wrap.extras) {
					previous.push_back(capsule.a);
					previous.push_back(capsule.b);
				}
			}
			Track(extras, previous);
			a_wrap.extras = std::move(extras);
		}
	}

	void BeginFrame(float a_delta)
	{
		if (refitRequested.exchange(false)) {
			Reset();
		}
		Anatomy::UpdateShaftCache();
		++frame;
		elapsed = a_delta;
		clock += a_delta;
		const auto stale = [](const auto& a_entry) {
			return clock - a_entry.second.seen > kForget || !a_entry.second.root || !a_entry.second.root->parent;
		};
		std::erase_if(entries, stale);
		std::erase_if(schlongs, stale);
		std::erase_if(wraps, stale);
	}

	void RequestRefit()
	{
		refitRequested.store(true);
	}

	std::uint64_t DebugMeshSignature(RE::Actor* a_actor)
	{
		auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
		if (!root || !root->parent || root->GetAppCulled()) {
			return 0;
		}
		auto* thigh = root->GetObjectByName(kBoneNames[kLThigh].front());
		if (!thigh) {
			return 0;
		}
		std::uint64_t hash = 14695981039346656037ull;
		const auto    mix = [&](std::uint64_t a_value) { hash = (hash ^ a_value) * 1099511628211ull; };
		mix(a_actor->GetFormID());
		mix(reinterpret_cast<std::uintptr_t>(root));
		if (const auto* base = a_actor->GetActorBase()) {
			mix(static_cast<std::uint64_t>(base->GetSex()));
			mix(std::bit_cast<std::uint32_t>(base->weight));
		}
		bool body = false;
		for (auto* geometry : WrapGeometries(root)) {
			auto* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
			auto* partition = skin ? skin->skinPartition.get() : nullptr;
			if (!skin || !skin->skinData || !skin->bones || !partition || !partition->numPartitions) {
				continue;
			}
			const auto& first = partition->partitions[0];
			mix(reinterpret_cast<std::uintptr_t>(geometry));
			mix(reinterpret_cast<std::uintptr_t>(skin));
			mix(reinterpret_cast<std::uintptr_t>(partition));
			mix(partition->vertexCount);
			if (!first.buffData || !first.buffData->rawVertexData) {
				continue;
			}
			mix(reinterpret_cast<std::uintptr_t>(first.buffData->rawVertexData));
			if (partition->vertexCount >= kWrapMinimum) {
				for (std::uint32_t i = 0; !body && i < skin->skinData->bones; ++i) {
					body = skin->bones[i] == thigh;
				}
			}
		}
		return body ? (hash ? hash : 1) : 0;
	}

	std::span<const Capsule> Of(RE::Actor* a_actor)
	{
		const auto& settings = Settings::Get();
		auto*       root = a_actor && settings.collision ? a_actor->Get3D(false) : nullptr;
		if (!root) {
			return {};
		}
		auto& entry = entries[a_actor->GetHandle().native_handle()];
		entry.seen = clock;
		if (entry.root.get() != root) {
			entry = Entry{};
			entry.root.reset(root);
			entry.seen = clock;
			for (std::size_t b = 0; b < kBones; ++b) {
				for (const char* name : kBoneNames[b]) {
					entry.bones[b].reset(root->GetObjectByName(name));
					if (entry.bones[b]) {
						break;
					}
				}
			}
			Fit(entry);
			entry.checked = clock;
		} else if (clock - entry.checked > kRecheck) {
			entry.checked = clock;
			auto*               body = entry.bones[kLThigh] ? FindBody(entry) : nullptr;
			auto*               skin = body ? body->GetGeometryRuntimeData().skinInstance.get() : nullptr;
			const std::uint32_t vertices = skin && skin->skinPartition ? skin->skinPartition->vertexCount : 0u;
			if (body != entry.body || vertices != entry.vertices) {
				Fit(entry);
				entry.reported = false;
			}
		}
		if (settings.debugLog && !entry.reported) {
			logger::info("{}: body collision fit: {}", a_actor->GetDisplayFullName(), entry.fitReport);
		}
		entry.reported = settings.debugLog;
		if (entry.frame != frame) {
			entry.frame = frame;
			Build(entry, settings.collisionThickness);
		}
		return entry.capsules;
	}

	std::span<const Capsule> ShaftOf(RE::Actor* a_actor)
	{
		const auto& settings = Settings::Get();
		auto*       schlong = settings.collision ? SchlongOf(a_actor) : nullptr;
		if (!schlong || !schlong->measured) {
			return {};
		}
		if (schlong->frame != frame) {
			schlong->frame = frame;
			std::vector<Capsule> next;
			next.reserve(schlong->pieces.size());
			for (const auto& piece : schlong->pieces) {
				next.push_back({ Place(piece.from), Place(piece.to), piece.radius * ScaleOf(piece.from) * settings.collisionThickness });
			}
			Track(next, schlong->previous);
			schlong->capsules = std::move(next);
		}
		return schlong->capsules;
	}

	std::optional<Tip> TipOf(RE::Actor* a_actor)
	{
		auto* schlong = SchlongOf(a_actor);
		if (!schlong || !schlong->tipBone) {
			return std::nullopt;
		}
		const auto&        world = schlong->tipBone->world;
		const RE::NiPoint3 boneAxis = Unit(world.rotate * RE::NiPoint3{ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
		if (schlong->measured) {
			return Tip{ Place(schlong->tip), Unit(Turn(schlong->tip, schlong->axis), boneAxis) };
		}
		return Tip{ world.translate + boneAxis * (Settings::Get().tipExtension * world.scale), boneAxis };
	}

	Surface SurfaceOf(RE::Actor* a_actor)
	{
		auto* wrap = Posed(a_actor);
		return wrap ? Surface{ wrap->patches, wrap->extras } : Surface{};
	}

	const Fluid::Skin* GridOf(RE::Actor* a_actor)
	{
		auto* wrap = Posed(a_actor);
		if (!wrap) {
			return nullptr;
		}
		if (wrap->gridFrame != frame) {
			wrap->gridFrame = frame;
			auto& patches = wrap->grid.Patches();
			patches.clear();
			patches.reserve(wrap->patches.size());
			for (const auto& patch : wrap->patches) {
				patches.push_back({ { patch.position.x, patch.position.y, patch.position.z }, { patch.normal.x, patch.normal.y, patch.normal.z },
					{ patch.velocity.x, patch.velocity.y, patch.velocity.z } });
			}
			wrap->grid.Build(kGridCell);
		}
		return &wrap->grid;
	}

	void Reset()
	{
		Anatomy::ResetShaftCache();
		entries.clear();
		schlongs.clear();
		wraps.clear();
	}

	RE::NiPoint3 Closest(const Capsule& a_capsule, const RE::NiPoint3& a_point, float* a_along)
	{
		const RE::NiPoint3 span = a_capsule.b - a_capsule.a;
		const float        length = span.SqrLength();
		const float        t = length > 1e-8f ? std::clamp((a_point - a_capsule.a).Dot(span) / length, 0.0f, 1.0f) : 0.0f;
		if (a_along) {
			*a_along = t;
		}
		return a_capsule.a + span * t;
	}

	bool PushOut(const Fluid::Skin& a_skin, RE::NiPoint3& a_point, float a_margin)
	{
		const std::size_t nearest = a_skin.Nearest({ a_point.x, a_point.y, a_point.z });
		if (nearest == Fluid::kNone) {
			return false;
		}
		const auto&        patch = a_skin.Patches()[nearest];
		const RE::NiPoint3 normal{ patch.normal.x, patch.normal.y, patch.normal.z };
		const float        depth = (a_point - RE::NiPoint3{ patch.position.x, patch.position.y, patch.position.z }).Dot(normal);
		if (depth >= a_margin) {
			return false;
		}
		a_point += normal * (a_margin - depth);
		return true;
	}

	bool PushOut(std::span<const Capsule> a_capsules, std::span<const Hole> a_holes, RE::NiPoint3& a_point, float a_margin)
	{
		for (const auto& hole : a_holes) {
			if (a_point.GetDistance(hole.center) < hole.radius) {
				return false;
			}
		}
		bool moved = false;
		for (const auto& capsule : a_capsules) {
			const RE::NiPoint3 closest = Closest(capsule, a_point);
			RE::NiPoint3       offset = a_point - closest;
			const float        distance = offset.Length();
			const float        limit = capsule.radius + a_margin;
			if (distance >= limit) {
				continue;
			}
			const RE::NiPoint3 normal = distance > 1e-4f ? offset / distance : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
			a_point = closest + normal * limit;
			moved = true;
		}
		return moved;
	}
}
