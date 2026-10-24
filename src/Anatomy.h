#pragma once

namespace OStrands
{
	enum class Act
	{
		kVaginal,
		kAnal,
		kFellatio,
		kCunnilingus,
		kAnilingus,
		kVaginalFingering,
		kAnalFingering,
		kTribbing,
		kVaginalToy,
		kAnalToy
	};

	std::string_view ActName(Act a_act);

	struct Pair
	{
		RE::ActorHandle giver;
		RE::ActorHandle receiver;
		Act             act{ Act::kVaginal };

		bool operator==(const Pair& a_rhs) const
		{
			return giver == a_rhs.giver && receiver == a_rhs.receiver && act == a_rhs.act;
		}
	};

	namespace Anatomy
	{
		enum class Opening
		{
			kVagina,
			kAnus,
			kMouth
		};

		struct Point
		{
			RE::NiAVObject* node{ nullptr };
			RE::NiPoint3    local;

			RE::NiPoint3 World() const { return node->world * local; }
		};

		struct Names
		{
			std::vector<RE::BSFixedString>   vaginaRim{ "NPC L Pussy02", "NPC R Pussy02", "VaginaB1", "Clitoral1" };
			std::vector<RE::BSFixedString>   anusRim{ "NPC LB Anus2", "NPC RB Anus2", "NPC RT Anus2", "NPC LT Anus2" };
			RE::BSFixedString                vaginaDeep{ "VaginaDeep1" };
			RE::BSFixedString                anusDeep{ "NPC Anus Deep1" };
			std::array<RE::BSFixedString, 6> shaft{ "NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
				"NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]" };
			RE::BSFixedString                genitalsBase{ "NPC GenitalsBase [GenBase]" };
			RE::BSFixedString                pelvis{ "NPC Pelvis [Pelv]" };
			RE::BSFixedString                scrotum{ "NPC GenitalsScrotum [GenScrot]" };
			RE::BSFixedString                head{ "NPC Head [Head]" };
			RE::BSFixedString                headMagic{ "NPC Head MagicNode [Hmag]" };
			std::array<RE::BSFixedString, 4> animObjects{ "AnimObjectA", "AnimObjectB", "AnimObjectL", "AnimObjectR" };
			std::array<std::array<RE::BSFixedString, 4>, 2> hands{ {
				{ "NPC R Hand [RHnd]", "NPC R Finger10 [RF10]", "NPC R Finger11 [RF11]", "NPC R Finger12 [RF12]" },
				{ "NPC L Hand [LHnd]", "NPC L Finger10 [LF10]", "NPC L Finger11 [LF11]", "NPC L Finger12 [LF12]" },
			} };
			std::array<std::array<RE::BSFixedString, 2>, 2> middleFingers{ {
				{ "NPC R Finger21 [RF21]", "NPC R Finger22 [RF22]" },
				{ "NPC L Finger21 [LF21]", "NPC L Finger22 [LF22]" },
			} };

			static const Names& Get();
		};

		Opening OpeningOf(Act a_act);
		bool    IsPenis(Act a_act);
		bool    IsFingers(Act a_act);
		bool    IsOralGiver(Act a_act);
		bool    IsToy(Act a_act);
		bool    HasShaft(Act a_act);
		bool    Allowed(Act a_act);

		bool        Collect(RE::NiAVObject* a_root, const std::vector<RE::BSFixedString>& a_names, std::vector<Point>& a_points, RE::NiPoint3& a_center);
		bool        MouthSite(RE::NiAVObject* a_root, std::vector<Point>& a_points, RE::NiPoint3& a_center, RE::NiPoint3& a_forward);
		bool        VulvaSite(RE::NiAVObject* a_root, std::vector<Point>& a_points, RE::NiPoint3& a_center);
		const char* FindOpening(RE::NiAVObject* a_root, Opening a_opening, std::vector<Point>& a_points, RE::NiPoint3& a_center, RE::NiPoint3& a_inward);

		struct ShaftState
		{
			bool schlong{ false };
			bool strapOn{ false };
		};

		void       UpdateShaftCache();
		void       ResetShaftCache();
		ShaftState GetShaftState(RE::Actor* a_actor);
		bool       HasSchlong(RE::Actor* a_actor);
		bool       IsPerson(RE::Actor* a_actor);
		RE::NiAVObject* CreatureTipNode(RE::NiAVObject* a_root);

		RE::TESObjectARMO*       BodyArmor(RE::Actor* a_actor);
		std::vector<std::string> WornModels(RE::Actor* a_actor);
		bool                     IsStrapWearer(RE::Actor* a_actor, const std::vector<std::string>& a_models);
	}
}
