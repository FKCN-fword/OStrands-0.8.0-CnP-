#include "OCumBridge.h"

#include "FluidManager.h"
#include "Settings.h"

namespace OStrands::OCumBridge
{
	namespace
	{
		class Listener : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			static Listener* Get()
			{
				static Listener listener;
				return &listener;
			}

			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (!a_event || !a_event->sender) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (std::string_view(a_event->eventName.c_str()) != "ocum_play_cum_shoot_effect") {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (auto* actor = a_event->sender->As<RE::Actor>()) {
					FluidManager::Get().Load(actor->GetHandle(), a_event->numArg);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Connect()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		const bool installed = data && data->LookupModByName("OCum.esp");
		FluidManager::Get().SetOCum(installed);
		if (auto* source = SKSE::GetModCallbackEventSource()) {
			source->AddEventSink(Listener::Get());
		} else {
			logger::warn("SKSE mod events are unavailable; OCum loads are ignored");
		}
		auto settingsGuard = Settings::Lock();
		const int mode = Settings::Get().semen;
		logger::info("OCum {}; semen {}", installed ? "found" : "not found",
			mode > 0 ? "on" : mode == 0 ? "off" : installed ? "on (auto, OCum found)" : "off in OStim scenes (auto, no OCum)");
	}
}
