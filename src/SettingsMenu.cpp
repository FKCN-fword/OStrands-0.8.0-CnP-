#include "SettingsMenu.h"

#include "Arousal.h"
#include "Body.h"
#include "FluidManager.h"
#include "OStimBridge.h"
#include "Revealing.h"
#include "Settings.h"

#include "SKSEMenuFramework.h"

#include <cmath>

namespace OStrands::SettingsMenu
{
	namespace
	{
		namespace UI = ImGuiMCP;

		using Condition = std::function<bool()>;

		void Hint(const char* a_text)
		{
			if (a_text && *a_text && UI::IsItemHovered(UI::ImGuiHoveredFlags_AllowWhenDisabled)) {
				UI::SetTooltip("%s", a_text);
			}
		}

		void Persist()
		{
			auto guard = Settings::Lock();
			Settings::Get().Save();
		}

		struct Option
		{
			Option(const char* a_name, const char* a_tooltip, Condition a_condition) :
				name(a_name), tooltip(a_tooltip), condition(std::move(a_condition))
			{}

			virtual ~Option() = default;

			void Draw()
			{
				bool enabled = true;
				if (condition) {
					auto guard = Settings::Lock();
					enabled = condition();
				}
				UI::BeginDisabled(!enabled);
				Body();
				UI::EndDisabled();
			}

			virtual void Body() = 0;

			const char* name;
			const char* tooltip;
			Condition   condition;
		};

		struct Heading : Option
		{
			explicit Heading(const char* a_name) :
				Option(a_name, nullptr, {})
			{}

			void Body() override { UI::SeparatorText(name); }
		};

		struct Note : Option
		{
			explicit Note(std::function<std::string()> a_text) :
				Option("", nullptr, {}), text(std::move(a_text))
			{}

			void Body() override { UI::TextWrapped("%s", text().c_str()); }

			std::function<std::string()> text;
		};

		struct Toggle : Option
		{
			Toggle(const char* a_name, const char* a_tooltip, bool Settings::* a_field, Condition a_condition = {}) :
				Option(a_name, a_tooltip, std::move(a_condition)), field(a_field)
			{}

			void Body() override
			{
				bool value = false;
				{
					auto guard = Settings::Lock();
					value = Settings::Get().*field;
				}
				if (UI::Checkbox(name, &value)) {
					auto       guard = Settings::Lock();
					auto&      settings = Settings::Get();
					const bool previous = settings.*field;
					settings.*field = value;
					if (!settings.Save()) {
						settings.*field = previous;
					}
				}
				Hint(tooltip);
			}

			bool Settings::* field;
		};

		template <class T>
		struct Slider : Option
		{
			Slider(const char* a_name, const char* a_tooltip, T Settings::* a_field, float a_min, float a_max, float a_step,
				Condition a_condition = {}) :
				Option(a_name, a_tooltip, std::move(a_condition)), field(a_field), min(a_min), max(a_max), step(a_step)
			{
				const int digits = a_step >= 1.0f ? 0 : a_step >= 0.1f ? 1 : a_step >= 0.01f ? 2 : 3;
				format = std::format("%.{}f", digits);
			}

			void Body() override
			{
				float value = 0.0f;
				{
					auto guard = Settings::Lock();
					value = static_cast<float>(Settings::Get().*field);
				}
				if (UI::SliderFloat(name, &value, min, max, format.c_str()) && std::isfinite(value)) {
					value = std::clamp(std::round(value / step) * step, min, max);
					auto  guard = Settings::Lock();
					auto& settings = Settings::Get();
					if constexpr (std::is_integral_v<T>) {
						settings.*field = static_cast<T>(std::lround(value));
					} else {
						settings.*field = static_cast<T>(value);
					}
					settings.Validate();
				}
				if (UI::IsItemDeactivatedAfterEdit()) {
					Persist();
				}
				Hint(tooltip);
			}

			T Settings::* field;
			float         min;
			float         max;
			float         step;
			std::string   format;
		};

		struct Choice : Option
		{
			Choice(const char* a_name, const char* a_tooltip, int Settings::* a_field, std::vector<std::pair<const char*, int>> a_items,
				Condition a_condition = {}) :
				Option(a_name, a_tooltip, std::move(a_condition)), field(a_field), items(std::move(a_items))
			{
				for (const auto& item : items) {
					labels.push_back(item.first);
				}
			}

			void Body() override
			{
				int value = 0;
				{
					auto guard = Settings::Lock();
					value = Settings::Get().*field;
				}
				int index = 0;
				for (std::size_t i = 0; i < items.size(); ++i) {
					if (items[i].second == value) {
						index = static_cast<int>(i);
					}
				}
				if (UI::Combo(name, &index, labels.data(), static_cast<int>(labels.size())) && index >= 0 &&
					static_cast<std::size_t>(index) < items.size()) {
					auto      guard = Settings::Lock();
					auto&     settings = Settings::Get();
					const int previous = settings.*field;
					settings.*field = items[index].second;
					settings.Validate();
					if (!settings.Save()) {
						settings.*field = previous;
					}
				}
				Hint(tooltip);
			}

			int Settings::*                            field;
			std::vector<std::pair<const char*, int>>   items;
			std::vector<const char*>                   labels;
		};

		struct Key : Option
		{
			static constexpr int kEscape = 0x01;
			static constexpr int kBackspace = 0x0E;
			static constexpr int kDelete = 0xD3;

			Key(const char* a_name, const char* a_tooltip, int Settings::* a_field, Condition a_condition = {}) :
				Option(a_name, a_tooltip, std::move(a_condition)), field(a_field)
			{}

			static const std::uint8_t* State()
			{
				auto* input = RE::BSInputDeviceManager::GetSingleton();
				auto* keyboard = input ? input->GetKeyboard() : nullptr;
				return keyboard ? keyboard->GetRuntimeData().curState : nullptr;
			}

			const std::string& NameOf(int a_code)
			{
				if (a_code != named) {
					named = a_code;
					RE::BSFixedString text;
					auto*             input = RE::BSInputDeviceManager::GetSingleton();
					if (a_code <= 0) {
						label = "Unbound";
					} else if (input && input->GetButtonNameFromID(RE::INPUT_DEVICE::kKeyboard, a_code, text) && !text.empty()) {
						label = text.c_str();
					} else {
						label = std::format("Key {}", a_code);
					}
				}
				return label;
			}

			int Pressed()
			{
				const auto* state = State();
				if (!state) {
					return -1;
				}
				for (std::size_t code = 1; code < held.size(); ++code) {
					const bool down = (state[code] & 0x80) != 0;
					if (down && !held[code]) {
						return static_cast<int>(code);
					}
					held[code] = held[code] && down;
				}
				return -1;
			}

			void Listen()
			{
				const auto* state = State();
				for (std::size_t code = 0; code < held.size(); ++code) {
					held[code] = state && (state[code] & 0x80) != 0;
				}
				listening = true;
			}

			void Body() override
			{
				int value = 0;
				{
					auto guard = Settings::Lock();
					value = Settings::Get().*field;
				}
				if (listening) {
					if (const int code = Pressed(); code > 0) {
						listening = false;
						if (code != kEscape) {
							auto  guard = Settings::Lock();
							auto& settings = Settings::Get();
							settings.*field = code == kBackspace || code == kDelete ? 0 : code;
							settings.Validate();
							settings.Save();
							value = settings.*field;
						}
					}
				}
				const std::string text = listening ? std::format("Press a key... (Esc cancels, Delete unbinds)###{}", name) :
				                                     std::format("{}: {}###{}", name, NameOf(value), name);
				if (UI::Button(text.c_str())) {
					if (listening) {
						listening = false;
					} else {
						Listen();
					}
				}
				Hint(tooltip);
			}

			int Settings::*        field;
			bool                   listening{ false };
			std::array<bool, 256>  held{};
			int                    named{ -1 };
			std::string            label;
		};

		struct Button : Option
		{
			Button(const char* a_name, const char* a_tooltip, std::function<void()> a_action, Condition a_condition = {}) :
				Option(a_name, a_tooltip, std::move(a_condition)), action(std::move(a_action))
			{}

			void Body() override
			{
				if (UI::Button(name)) {
					action();
				}
				Hint(tooltip);
			}

			std::function<void()> action;
		};

		using Page = std::vector<std::unique_ptr<Option>>;

		Page strands;
		Page semen;
		Page arousal;
		Page alignment;

		void Draw(Page& a_page)
		{
			for (auto& option : a_page) {
				option->Draw();
			}
		}

		void __stdcall DrawStrands() { Draw(strands); }
		void __stdcall DrawSemen() { Draw(semen); }
		void __stdcall DrawArousal() { Draw(arousal); }
		void __stdcall DrawAlignment() { Draw(alignment); }

		void BuildStrands()
		{
			auto&      page = strands;
			const auto on = [] { return Settings::Get().enabled; };
			const auto drips = [] { return Settings::Get().enabled && Settings::Get().drips; };
			const auto glints = [] { return Settings::Get().enabled && Settings::Get().glints; };
			const auto collision = [] { return Settings::Get().enabled && Settings::Get().collision; };

			page.push_back(std::make_unique<Heading>("일반"));
			page.push_back(std::make_unique<Toggle>("사용", "OStim 및 SexLab 장면에서 체액 가닥, 방울, 정액 효과를 표시합니다.", &Settings::enabled));
			page.push_back(std::make_unique<Toggle>("디버그 로그", "장면과 가닥 효과의 상세 정보를 OStrands.log에 기록합니다.", &Settings::debugLog));

			page.push_back(std::make_unique<Heading>("가닥 효과 종류"));
			page.push_back(std::make_unique<Toggle>("가랑이 가닥 효과", "삽입 동작 후 몸이 떨어질 때 몸 사이에 가닥 효과를 만듭니다.", &Settings::crotch, on));
			page.push_back(std::make_unique<Toggle>("외음부 마찰 가닥 효과", "두 외음부 사이에 가닥 효과를 만듭니다. 다음 장면부터 적용됩니다.", &Settings::tribbing, on));
			page.push_back(std::make_unique<Toggle>("구강 가닥 효과", "구강 성교와 외음부 및 항문 애무에 가닥 효과를 만듭니다. 다음 장면부터 적용됩니다.",
				&Settings::oral, on));
			page.push_back(std::make_unique<Toggle>("손가락 애무 가닥 효과", "손가락 삽입과 문지르기에 가닥 효과를 만듭니다. 다음 장면부터 적용됩니다.",
				&Settings::fingering, on));
			page.push_back(std::make_unique<Toggle>("도구 가닥 효과", "딜도 등 성인용 도구에 가닥 효과를 만듭니다. 다음 장면부터 적용됩니다.", &Settings::toys, on));

			page.push_back(std::make_unique<Heading>("효과 생성"));
			page.push_back(std::make_unique<Slider<float>>("연결 가닥 생성 빈도", "개구부와 음경 사이에 초당 생성되는 최대 가닥 수입니다.",
				&Settings::bridgeRate, Settings::kRateMin, Settings::kRateMax, 0.1f, on));
			page.push_back(std::make_unique<Slider<float>>("가랑이 가닥 생성 빈도", "몸 사이에 초당 생성되는 최대 가닥 수입니다.", &Settings::crotchRate,
				Settings::kRateMin, Settings::kRateMax, 0.1f, on));
			page.push_back(std::make_unique<Slider<int>>("최대 가닥 수", "화면에 동시에 표시되는 최대 가닥 수입니다.", &Settings::maxTotal,
				static_cast<float>(Settings::kCountMin), static_cast<float>(Settings::kCountMax), 1.0f, on));

			page.push_back(std::make_unique<Heading>("방울 효과"));
			page.push_back(std::make_unique<Toggle>("방울 효과", "장면 도중과 종료 후 질에서 자연스럽게 방울이 떨어집니다.", &Settings::drips, on));
			page.push_back(std::make_unique<Toggle>("스트랩온 착용자 방울 효과", "스트랩온을 착용한 여성에게도 방울 효과를 적용합니다.", &Settings::wearerDrips, drips));
			page.push_back(std::make_unique<Slider<float>>("방울 생성 간격", "방울이 떨어지는 평균 간격입니다. 단위: 초.", &Settings::dripInterval,
				Settings::kIntervalMin, Settings::kIntervalMax, 0.1f, drips));
			page.push_back(std::make_unique<Slider<float>>("종료 후 방울 지속 시간", "장면 종료 후 방울이 점차 뜸해지면서 지속되는 시간입니다. 단위: 초. 0이면 비활성화합니다.",
				&Settings::dripLinger, Settings::kLingerMin, Settings::kLingerMax, 1.0f, drips));

			page.push_back(std::make_unique<Heading>("외형"));
			page.push_back(std::make_unique<Slider<float>>("투명도", "가닥의 투명도입니다. 0이면 불투명합니다.", &Settings::transparency,
				Settings::kTransparencyMin, Settings::kTransparencyMax, 0.05f, on));
			page.push_back(std::make_unique<Slider<float>>("광택 밝기", "가닥 표면의 젖은 반사광 밝기입니다.", &Settings::shine,
				Settings::kShineMin, Settings::kShineMax, 0.5f, on));
			page.push_back(std::make_unique<Slider<float>>("광택 선명도", "높을수록 반사광이 작고 선명해져 액체처럼 보입니다.",
				&Settings::gloss, Settings::kGlossMin, Settings::kGlossMax, 10.0f, on));
			page.push_back(std::make_unique<Toggle>("반짝임", "수면의 햇빛처럼 가닥을 따라 작은 별 모양의 반짝임을 표시합니다.",
				&Settings::glints, on));
			page.push_back(std::make_unique<Slider<float>>("반짝임 빈도", "각 가닥에 초당 발생하는 반짝임 수입니다.", &Settings::glintRate,
				Settings::kGlintRateMin, Settings::kGlintRateMax, 0.1f, glints));
			page.push_back(std::make_unique<Slider<float>>("반짝임 크기", "각 반짝임의 크기입니다.", &Settings::glintSize, Settings::kGlintSizeMin,
				Settings::kGlintSizeMax, 0.05f, glints));
			page.push_back(std::make_unique<Slider<float>>("반짝임 밝기", "각 반짝임의 밝기입니다.", &Settings::glintBrightness,
				Settings::kGlintBrightnessMin, Settings::kGlintBrightnessMax, 0.1f, glints));

			page.push_back(std::make_unique<Heading>("충돌"));
			page.push_back(std::make_unique<Toggle>("신체 충돌", "가닥, 방울, 정액이 몸을 통과하지 않고 표면에 닿게 합니다.",
				&Settings::collision, on));
			page.push_back(std::make_unique<Slider<float>>("신체 충돌 두께",
				"충돌 형상의 크기를 조절합니다. 체액이 피부 위에 떠 있으면 낮추고, 피부 안으로 들어가면 높이세요.", &Settings::collisionThickness,
				Settings::kThicknessMin, Settings::kThicknessMax, 0.05f, collision));
			page.push_back(std::make_unique<Toggle>("충돌 형상 표시",
				"콘솔로 선택한 액터, 장면 참여자 또는 플레이어의 충돌 지점을 표시합니다. 캐릭터 생성 후 점검용이며 새 게임 시작 시 자동으로 꺼집니다. 평소 플레이에서는 꺼두세요.",
				&Settings::showShapes, on));
			page.push_back(std::make_unique<Button>("충돌 형상 재측정",
				"몸이나 체형을 변경한 후 로드된 메시를 다시 측정합니다. 디버그 로그가 켜져 있으면 선택한 메시, 표본 수, 대체 반경을 기록합니다.",
				[] { Body::RequestRefit(); }));

			page.push_back(std::make_unique<Heading>("설정 파일"));
			page.push_back(std::make_unique<Note>([] {
				return std::string("변경 사항은 Data\\SKSE\\Plugins\\OStrands_User.ini에 저장됩니다. 업데이트는 이 파일을 덮어쓰지 않습니다. OStrands.ini에는 기본값이 있으며 업데이트 시 교체됩니다.");
			}));
			page.push_back(std::make_unique<Button>("설정 파일 다시 불러오기", "직접 수정한 OStrands.ini와 OStrands_User.ini를 다시 읽습니다.", [] {
				if (Settings::Get().Load()) {
					OStimBridge::ReloadActions();
				}
			}));
		}

		void BuildSemen()
		{
			auto&      page = semen;
			const auto on = [] { return Settings::Get().enabled; };
			const auto active = [] { return Settings::Get().enabled && FluidManager::Get().SemenEnabled(); };

			page.push_back(std::make_unique<Choice>("정액", "자동 모드에서는 SexLab 장면에 정액 효과를 적용하고, OStim 장면에서는 OCum이 설치된 경우 활성화합니다.", &Settings::semen,
				std::vector<std::pair<const char*, int>>{ { "자동", -1 }, { "켜기", 1 }, { "끄기", 0 } }, on));
			page.push_back(std::make_unique<Note>([] {
				auto       guard = Settings::Lock();
				const bool ocum = FluidManager::Get().OCumInstalled();
				const bool sexlab = FluidManager::Get().SexLabInstalled();
				const int  mode = Settings::Get().semen;
				if (mode > 0) {
					return std::string("정액 효과가 켜져 있습니다.");
				}
				if (mode == 0) {
					return std::string("정액 효과가 꺼져 있습니다.");
				}
				if (ocum) {
					return std::string("자동: 켜짐. OCum이 감지되었습니다.");
				}
				return std::string(sexlab ? "자동: SexLab 장면에서 켜짐, OStim 장면에서 꺼짐(OCum 미감지)." : "자동: 꺼짐. OCum이 감지되지 않았습니다.");
			}));

			page.push_back(std::make_unique<Heading>("사정"));
			page.push_back(std::make_unique<Slider<float>>("사정량", "각 사정의 양입니다. 플레이어가 참여하는 장면에서는 OCum이 추가로 조절합니다.", &Settings::semenAmount,
				Settings::kAmountMin, Settings::kAmountMax, 0.05f, active));
			page.push_back(std::make_unique<Slider<float>>("분출 강도", "정액이 분출되는 강도입니다.", &Settings::semenStrength, Settings::kStrengthMin,
				Settings::kStrengthMax, 0.05f, active));
			page.push_back(std::make_unique<Slider<int>>("분출 횟수", "절정마다 마지막 흘러내림 전에 분출하는 횟수입니다.", &Settings::semenPulses,
				static_cast<float>(Settings::kPulsesMin), static_cast<float>(Settings::kPulsesMax), 1.0f, active));
			page.push_back(std::make_unique<Slider<float>>("삽입 중 유출 비율",
				"삽입 상태에서 분출된 정액이 음경 주변으로 새어 나오는 비율입니다. 나머지는 빼낸 뒤 흘러나옵니다.", &Settings::semenOverflow,
				Settings::kOverflowMin, Settings::kOverflowMax, 0.05f, active));

			page.push_back(std::make_unique<Heading>("유체"));
			page.push_back(std::make_unique<Slider<float>>("점성", "정액이 걸쭉하고 느리게 흐르는 정도입니다. 높을수록 길게 이어진 형태가 잘 유지됩니다.",
				&Settings::semenViscosity, Settings::kViscosityMin, Settings::kViscosityMax, 1.0f, active));
			page.push_back(std::make_unique<Slider<float>>("응집력", "방울들이 서로 끌어당겨 덩어리로 뭉치는 정도입니다.", &Settings::semenCohesion,
				Settings::kCohesionMin, Settings::kCohesionMax, 0.5f, active));
			page.push_back(std::make_unique<Slider<float>>("늘어남", "가닥이 끊어지기 전에 늘어나는 정도입니다. 높을수록 길고 가늘게 늘어납니다.",
				&Settings::semenStretch, Settings::kStretchMin, Settings::kStretchMax, 0.5f, active));
			page.push_back(std::make_unique<Slider<float>>("피부 부착력", "정액이 피부에 달라붙는 정도입니다. 낮을수록 쉽게 흘러내립니다.",
				&Settings::semenStickiness, Settings::kStickinessMin, Settings::kStickinessMax, 0.1f, active));
			page.push_back(std::make_unique<Slider<float>>("불투명도", "1이면 불투명한 흰색입니다.", &Settings::semenOpacity, Settings::kOpacityMin,
				Settings::kOpacityMax, 0.05f, active));

			page.push_back(std::make_unique<Heading>("유지 시간"));
			page.push_back(std::make_unique<Slider<float>>("유지 시간", "정액이 사라지기 시작하기 전까지 유지되는 시간입니다. 단위: 초.", &Settings::fluidLifetime,
				Settings::kLifetimeMin, Settings::kLifetimeMax, 5.0f, active));
			page.push_back(std::make_unique<Slider<float>>("소멸 시간", "유지 시간이 끝난 후 정액이 줄어들며 사라지는 데 걸리는 시간입니다. 단위: 초.", &Settings::fluidFade,
				Settings::kFadeMin, Settings::kFadeMax, 0.5f, active));
			page.push_back(std::make_unique<Slider<int>>("최대 입자 수", "한 영역의 최대 정액 입자 수입니다. 낮출수록 처리 부담이 줄어듭니다.", &Settings::fluidMaxParticles,
				static_cast<float>(Settings::kParticlesMin), static_cast<float>(Settings::kParticlesMax), 32.0f, active));

			page.push_back(std::make_unique<Heading>("OCum 및 테스트"));
			page.push_back(std::make_unique<Slider<float>>("OCum 응답 대기 시간",
				"플레이어 장면에서 OCum의 사정량을 기다리는 시간입니다. 시간이 지나면 기본량으로 분출합니다. 단위: 초.", &Settings::ocumWait, Settings::kWaitMin,
				Settings::kWaitMax, 0.1f, active));
			page.push_back(std::make_unique<Key>("테스트 키",
				"클릭한 뒤 키를 누르세요. 플레이어의 OStim 장면에서 이 키를 누르면 음경이 있는 모든 참여자가 절정에 도달합니다. 평소에는 미지정 상태로 두세요.",
				&Settings::fluidTestKey, active));
		}

		void BuildArousal()
		{
			auto&      page = arousal;
			const auto found = [] { return Settings::Get().enabled && Arousal::Installed(); };
			const auto active = [] { return Settings::Get().enabled && Arousal::Installed() && Settings::Get().arousal; };

			page.push_back(std::make_unique<Note>([] {
				return std::string(Arousal::Live()      ? "SLO Aroused NG가 감지되었습니다. 플러그인에서 흥분도를 실시간으로 읽습니다." :
				                   Arousal::Installed() ? "흥분도 모드가 감지되었습니다." :
				                                          "흥분도 모드를 찾지 못했습니다. 이 설정을 사용하려면 SLO Aroused NG, OSL Aroused 또는 SexLab Aroused의 흥분도 팩션을 유지하는 모드를 설치하세요.");
			}));
			page.push_back(std::make_unique<Toggle>("흥분에 따른 방울 효과", "장면 밖에서도 주변의 흥분한 나체 액터에게 방울 효과를 적용합니다.", &Settings::arousal, found));
			page.push_back(std::make_unique<Toggle>("애액", "흥분한 액터의 질에서 애액이 떨어집니다.", &Settings::arousalWetness, active));
			page.push_back(std::make_unique<Toggle>("쿠퍼액", "흥분한 액터의 음경 끝에서 쿠퍼액이 떨어집니다.", &Settings::arousalPrecum, active));
			page.push_back(std::make_unique<Slider<float>>("흥분도 기준", "방울 효과가 시작되는 흥분도입니다. 범위: 0~100.", &Settings::arousalThreshold,
				Settings::kThresholdMin, Settings::kThresholdMax, 1.0f, active));
			page.push_back(std::make_unique<Slider<float>>("흥분 방울 생성 간격",
				"최대 흥분도에서 방울이 떨어지는 평균 간격입니다. 단위: 초. 기준 흥분도에서는 간격이 4배로 길어집니다.", &Settings::arousalInterval,
				Settings::kArousalIntervalMin, Settings::kArousalIntervalMax, 0.5f, active));
			page.push_back(std::make_unique<Slider<float>>("흥분 방울 적용 거리", "플레이어를 중심으로 방울 효과를 적용하는 거리입니다. 단위: 게임 거리.", &Settings::arousalRadius,
				Settings::kReachMin, Settings::kReachMax, 128.0f, active));

			page.push_back(std::make_unique<Heading>("노출 의상"));
			page.push_back(std::make_unique<Note>([] {
				return std::string("몸통 슬롯에 장비를 착용하면 방울 효과가 멈춥니다. 아래에서 노출 의상으로 지정했거나 OStrands_Revealing 키워드가 있는 장비는 예외입니다.");
			}));
			page.push_back(std::make_unique<Button>("몸통 장비를 노출 의상으로 지정",
				"콘솔로 선택한 액터 또는 플레이어의 몸통 장비를 지정합니다. 이후 같은 장비를 입은 모든 액터에게 방울 효과가 적용됩니다.",
				[] { SKSE::GetTaskInterface()->AddTask([] { Revealing::Mark(true); }); }));
			page.push_back(std::make_unique<Button>("노출 의상 지정 해제",
				"콘솔로 선택한 액터 또는 플레이어의 몸통 장비 지정을 해제합니다. 해당 장비를 입으면 방울 효과가 다시 멈춥니다.",
				[] { SKSE::GetTaskInterface()->AddTask([] { Revealing::Mark(false); }); }));
			page.push_back(std::make_unique<Key>("노출 의상 지정 전환 키",
				"클릭한 뒤 키를 누르세요. 게임에서 이 키로 콘솔 선택 액터 또는 플레이어의 몸통 장비를 노출 의상으로 지정하거나 해제합니다.",
				&Settings::revealingKey));
			page.push_back(std::make_unique<Note>([] {
				std::size_t marked = 0;
				{
					auto guard = Settings::Lock();
					marked = Settings::Get().revealingArmor.size();
				}
				const auto status = Revealing::Status();
				return std::format("노출 의상 {}개 지정됨.{}{}", marked, status.empty() ? "" : " ", status);
			}));
		}
	}

	namespace
	{
		void BuildAlignment()
		{
			auto&      page = alignment;
			const auto on = [] { return Settings::Get().enabled; };
			const auto slider = [&](const char* a_name, const char* a_tooltip, float Settings::* a_field) {
				page.push_back(std::make_unique<Slider<float>>(a_name, a_tooltip, a_field, Settings::kAlignMin, Settings::kAlignMax, 0.1f, on));
			};

			page.push_back(std::make_unique<Note>([] {
				return std::string("스켈레톤과 신체 개구부 위치가 맞지 않을 때 체액 가닥, 방울 및 정액 효과의 시작 위치를 보정합니다. 충돌 형상 표시를 켜면 조절하면서 개구부 표시 위치를 확인할 수 있습니다. 값의 단위는 게임 거리입니다.");
			}));
			page.push_back(std::make_unique<Toggle>("충돌 형상 표시",
				"콘솔에서 선택한 액터, 장면 참여자 또는 플레이어의 충돌 지점과 개구부 표시를 보여줍니다. 평소 플레이에서는 꺼두세요.", &Settings::showShapes, on));

			page.push_back(std::make_unique<Heading>("질"));
			slider("질 앞뒤 위치", "양수면 몸 바깥쪽으로, 음수면 몸 안쪽으로 시작점을 이동합니다.", &Settings::vaginaOut);
			slider("질 상하 위치", "양수면 머리 방향으로, 음수면 발 방향으로 이동합니다.", &Settings::vaginaUp);

			page.push_back(std::make_unique<Heading>("항문"));
			slider("항문 앞뒤 위치", "양수면 몸 바깥쪽으로, 음수면 몸 안쪽으로 시작점을 이동합니다.", &Settings::anusOut);
			slider("항문 상하 위치", "양수면 머리 방향으로, 음수면 발 방향으로 이동합니다.", &Settings::anusUp);

			page.push_back(std::make_unique<Heading>("입"));
			slider("입 앞뒤 위치", "양수면 얼굴 앞쪽으로, 음수면 입 안쪽으로 이동합니다.", &Settings::mouthOut);
			slider("입 상하 위치", "양수면 코 방향으로, 음수면 턱 방향으로 이동합니다.", &Settings::mouthUp);

			page.push_back(std::make_unique<Button>("위치 보정 초기화", "이 페이지의 모든 위치 보정값을 0으로 초기화합니다.", [] {
				auto  guard = Settings::Lock();
				auto& settings = Settings::Get();
				for (auto field : { &Settings::vaginaOut, &Settings::vaginaUp, &Settings::anusOut, &Settings::anusUp, &Settings::mouthOut, &Settings::mouthUp }) {
					settings.*field = 0.0f;
				}
				settings.Save();
			}));
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled() || !GetMenuFrameworkModule()) {
			logger::info("SKSE Menu Framework not found; change settings in OStrands_User.ini");
			return;
		}
		if (strands.empty()) {
			BuildStrands();
			BuildSemen();
			BuildArousal();
			BuildAlignment();
		}
		SKSEMenuFramework::SetSection("OStrands");
		SKSEMenuFramework::AddSectionItem("가닥 효과", DrawStrands);
		SKSEMenuFramework::AddSectionItem("정액", DrawSemen);
		SKSEMenuFramework::AddSectionItem("흥분도", DrawArousal);
		SKSEMenuFramework::AddSectionItem("위치 보정", DrawAlignment);
		logger::info("settings added to SKSE Menu Framework (API {})", SKSEMenuFramework::GetMenuFrameworkAPIVersion());
	}
}
