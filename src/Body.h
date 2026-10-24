#pragma once

#include "fluid/Skin.h"

namespace OStrands::Body
{
	struct Capsule
	{
		RE::NiPoint3 a;
		RE::NiPoint3 b;
		float        radius{ 0.0f };
		RE::NiPoint3 velocityA;
		RE::NiPoint3 velocityB;
	};

	struct Hole
	{
		RE::NiPoint3 center;
		float        radius{ 0.0f };
	};

	struct Tip
	{
		RE::NiPoint3 point;
		RE::NiPoint3 axis;
	};

	struct Patch
	{
		RE::NiPoint3 position;
		RE::NiPoint3 normal;
		RE::NiPoint3 velocity;
	};

	struct Surface
	{
		std::span<const Patch>   patches;
		std::span<const Capsule> extras;
	};

	void                     BeginFrame(float a_delta);
	void                     RequestRefit();
	std::uint64_t            DebugMeshSignature(RE::Actor* a_actor);
	std::span<const Capsule> Of(RE::Actor* a_actor);
	std::span<const Capsule> ShaftOf(RE::Actor* a_actor);
	std::optional<Tip>       TipOf(RE::Actor* a_actor);
	Surface                  SurfaceOf(RE::Actor* a_actor);
	const Fluid::Skin*       GridOf(RE::Actor* a_actor);
	void                     Reset();

	RE::NiPoint3 Closest(const Capsule& a_capsule, const RE::NiPoint3& a_point, float* a_along = nullptr);
	bool         PushOut(std::span<const Capsule> a_capsules, std::span<const Hole> a_holes, RE::NiPoint3& a_point, float a_margin);
	bool         PushOut(const Fluid::Skin& a_skin, RE::NiPoint3& a_point, float a_margin);
}
