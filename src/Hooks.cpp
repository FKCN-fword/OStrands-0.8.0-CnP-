#include "Hooks.h"

#include "Arousal.h"
#include "Body.h"
#include "FluidManager.h"
#include "OStimBridge.h"
#include "Revealing.h"
#include "SexLabBridge.h"
#include "Settings.h"
#include "StrandManager.h"

#include <atomic>

namespace OStrands::Hooks
{
	namespace
	{
		class FrameDriver : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static FrameDriver* Get()
			{
				static FrameDriver driver;
				return &driver;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const*, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				_frame.fetch_add(1, std::memory_order_acq_rel);
				auto* tasks = SKSE::GetTaskInterface();
				if (tasks && _pending.load(std::memory_order_acquire) < kMaxPending) {
					_pending.fetch_add(1, std::memory_order_acq_rel);
					tasks->AddTask([this]() {
						Update();
						_pending.fetch_sub(1, std::memory_order_acq_rel);
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			static constexpr int           kMaxPending = 2;
			static constexpr std::uint64_t kMaxCatchUp = 4;

			void Update()
			{
				const std::uint64_t frame = _frame.load(std::memory_order_acquire);
				if (frame == _handled) {
					return;
				}
				const std::uint64_t passed = frame - _handled;
				_handled = frame;
				Heartbeat(std::chrono::steady_clock::now(), passed);
				auto* ui = RE::UI::GetSingleton();
				const bool mainMenu = ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
				if (mainMenu != _mainMenu) {
					_mainMenu = mainMenu;
					if (mainMenu) {
						ResetSession();
					}
				}
				if (mainMenu) {
					return;
				}
				const bool editingBody = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
				if (editingBody != _editingBody) {
					auto guard = Settings::Lock();
					FluidManager::Get().WaitForBodyEditor(editingBody);
					Body::Reset();
					_editingBody = editingBody;
				}
				if (editingBody || (ui && ui->GameIsPaused())) {
					return;
				}
				const float delta = RE::GetSecondsSinceLastFrame() * static_cast<float>(std::min(passed, kMaxCatchUp));
				Body::BeginFrame(delta);
				SexLabBridge::Update(delta);
				Arousal::Update(delta);
				StrandManager::Get().Update(delta);
				FluidManager::Get().Update(delta);
				CheckKeys();
			}

			static bool Down(int a_key)
			{
				auto* input = RE::BSInputDeviceManager::GetSingleton();
				auto* keyboard = input ? input->GetKeyboard() : nullptr;
				if (a_key <= 0 || !keyboard) {
					return false;
				}
				const auto& state = keyboard->GetRuntimeData().curState;
				return static_cast<std::size_t>(a_key) < std::size(state) && (state[a_key] & 0x80) != 0;
			}

			void CheckKeys()
			{
				int test = 0;
				int revealing = 0;
				{
					auto settingsGuard = Settings::Lock();
					test = Settings::Get().fluidTestKey;
					revealing = Settings::Get().revealingKey;
				}
				const bool testDown = Down(test);
				const bool revealingDown = Down(revealing);
				const bool typing = Typing();
				if (testDown && !_testHeld && !typing && test == _testKey) {
					OStimBridge::TestClimax();
				}
				if (revealingDown && !_revealingHeld && !typing && revealing == _revealingKey) {
					Revealing::Toggle();
				}
				_testHeld = testDown;
				_revealingHeld = revealingDown;
				_testKey = test;
				_revealingKey = revealing;
			}

			static bool Typing()
			{
				auto* ui = RE::UI::GetSingleton();
				auto* controls = RE::ControlMap::GetSingleton();
				return (ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) || (controls && controls->GetRuntimeData().textEntryCount > 0);
			}

			void Heartbeat(std::chrono::steady_clock::time_point a_now, std::uint64_t a_frames)
			{
				++_calls;
				_frames += a_frames;
				if (a_now - _lastBeat >= 10s) {
					auto settingsGuard = Settings::Lock();
					if (Settings::Get().debugLog) {
						logger::info("frame driver alive: {} updates for {} frames in the last 10s", _calls, _frames);
					}
					_lastBeat = a_now;
					_calls = 0;
					_frames = 0;
				}
			}

			std::atomic<std::uint64_t>            _frame{ 0 };
			std::atomic<int>                      _pending{ 0 };
			std::uint64_t                         _handled{ 0 };
			std::chrono::steady_clock::time_point _lastBeat{ std::chrono::steady_clock::now() };
			int                                   _calls{ 0 };
			std::uint64_t                         _frames{ 0 };
			bool                                  _testHeld{ false };
			bool                                  _revealingHeld{ false };
			bool                                  _mainMenu{ false };
			int                                   _testKey{ 0 };
			int                                   _revealingKey{ 0 };
			bool                                  _editingBody{ false };
		};
	}

	void ResetSession()
	{
		StrandManager::Get().Reset();
		FluidManager::Get().Reset();
		Body::Reset();
		SexLabBridge::Reset();
		Arousal::Reset();
	}

	void Install()
	{
		if (!SKSE::GetTaskInterface()) {
			logger::error("no SKSE task interface; strands disabled");
			return;
		}
		auto* input = RE::BSInputDeviceManager::GetSingleton();
		if (!input) {
			logger::error("no input device manager; strands disabled");
			return;
		}
		input->PrependEventSink(FrameDriver::Get());
		logger::info("frame driver registered; updates run through the SKSE main-thread task queue");
	}
}
