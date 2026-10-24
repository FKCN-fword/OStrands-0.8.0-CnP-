#include "Arousal.h"

#include "Anatomy.h"
#include "FluidManager.h"
#include "Settings.h"
#include "StrandManager.h"

namespace OStrands::Arousal
{
	namespace
	{
		constexpr RE::FormID       kFaction = 0x3FC36;
		constexpr std::string_view kPlugin = "SexLabAroused.esm";
		constexpr const wchar_t*   kNGModule = L"SexlabArousedNG.dll";
		constexpr const char*      kNGExport = "SLA_GetArousal";
		constexpr float            kPoll = 1.0f;

		using GetArousal = float (*)(RE::Actor*);

		RE::TESFaction*                     faction{ nullptr };
		GetArousal                          getArousal{ nullptr };
		float                               wait{ 0.0f };
		std::vector<StrandManager::Aroused> current;

		float LevelOf(RE::Actor* a_actor)
		{
			if (!a_actor->IsInFaction(faction)) {
				return 0.0f;
			}
			return getArousal ? getArousal(a_actor) : static_cast<float>(a_actor->GetFactionRank(faction, a_actor->IsPlayer()));
		}

		std::string NameOf(RE::ActorHandle a_handle)
		{
			const auto actor = a_handle.get();
			return actor ? actor->GetDisplayFullName() : "?";
		}

		bool Listed(const std::vector<StrandManager::Aroused>& a_list, RE::ActorHandle a_actor)
		{
			return std::ranges::any_of(a_list, [&](const StrandManager::Aroused& a_entry) { return a_entry.actor == a_actor; });
		}
	}

	void Connect()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		faction = data ? data->LookupForm<RE::TESFaction>(kFaction, kPlugin) : nullptr;
		auto* module = faction ? REX::W32::GetModuleHandleW(kNGModule) : nullptr;
		getArousal = module ? reinterpret_cast<GetArousal>(REX::W32::GetProcAddress(module, kNGExport)) : nullptr;
		if (!faction) {
			logger::info("no arousal mod found; arousal drips stay off");
		} else if (getArousal) {
			logger::info("SLO Aroused NG found; aroused actors drip outside scenes, arousal read live from its plugin");
		} else {
			logger::info("arousal found in SexLabAroused.esm; aroused actors drip outside scenes");
		}
	}

	bool Installed()
	{
		return faction != nullptr;
	}

	bool Live()
	{
		return getArousal != nullptr;
	}

	void Update(float a_delta)
	{
		wait -= a_delta;
		if (wait > 0.0f) {
			return;
		}
		wait = kPoll;
		auto        settingsGuard = Settings::Lock();
		const auto& settings = Settings::Get();
		auto*       player = RE::PlayerCharacter::GetSingleton();

		std::vector<StrandManager::Aroused> next;
		if (faction && player && settings.enabled && settings.arousal && (settings.arousalWetness || settings.arousalPrecum)) {
			const auto origin = player->GetPosition();
			const auto consider = [&](RE::Actor* a_actor) {
				if (!a_actor || a_actor->IsDead() || a_actor->IsChild() || a_actor->IsGhost() || a_actor->IsSummoned() || !a_actor->Is3DLoaded() ||
					a_actor->GetPosition().GetDistance(origin) > settings.arousalRadius) {
					return;
				}
				if (!Anatomy::IsPerson(a_actor)) {
					return;
				}
				const auto handle = a_actor->GetHandle();
				if (FluidManager::Get().InScene(handle)) {
					return;
				}
				const float level = LevelOf(a_actor);
				if (level > 0.0f && level >= settings.arousalThreshold) {
					next.push_back({ handle, level });
				}
			};
			consider(player);
			if (auto* lists = RE::ProcessLists::GetSingleton()) {
				for (const auto& handle : lists->highActorHandles) {
					const auto actor = handle.get();
					consider(actor.get());
				}
			}
		}

		if (settings.debugLog) {
			for (const auto& entry : next) {
				if (!Listed(current, entry.actor)) {
					logger::info("{} is aroused ({:.0f})", NameOf(entry.actor), entry.level);
				}
			}
			for (const auto& entry : current) {
				if (!Listed(next, entry.actor)) {
					logger::info("{} is no longer dripping from arousal", NameOf(entry.actor));
				}
			}
		}
		current = next;
		StrandManager::Get().SetAroused(std::move(next));
	}

	void Reset()
	{
		current.clear();
		wait = 0.0f;
	}
}
