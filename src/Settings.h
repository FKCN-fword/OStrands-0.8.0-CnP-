#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace OStrands
{
	struct Settings
	{
		bool  enabled{ true };
		bool  debugLog{ false };

		float bridgeRate{ 2.1f };
		int   maxPerStroke{ 2 };
		int   maxPerPair{ 5 };
		int   maxTotal{ 20 };
		float cooldown{ 0.25f };

		float radius{ 0.10f };
		float minRadius{ 0.035f };
		float shaftRadius{ 1.0f };
		std::vector<std::pair<std::string, float>> shaftRadiusOverrides;
		float tipExtension{ 2.9f };
		float transparency{ 0.8f };
		float shine{ 20.0f };
		float gloss{ 100.0f };

		bool  glints{ true };
		float glintRate{ 1.5f };
		float glintSize{ 1.3f };
		float glintBrightness{ 5.0f };
		float glintLife{ 0.25f };
		int   glintMax{ 24 };

		float gravity{ 350.0f };
		float damping{ 0.9f };
		float viscosity{ 4.0f };
		float maxLength{ 14.0f };
		float maxLife{ 4.0f };
		float fadeTime{ 0.4f };

		bool  crotch{ true };
		float crotchRate{ 2.1f };
		int   crotchMaxPerContact{ 3 };
		int   crotchMaxPerPair{ 5 };
		float crotchMaxGap{ 10.0f };
		float crotchSpread{ 1.5f };
		float crotchSkin{ 1.0f };
		float crotchRadius{ 0.09f };
		float crotchMinRadius{ 0.03f };
		float crotchMaxLength{ 12.0f };
		float crotchMaxLife{ 4.0f };

		bool  tribbing{ true };
		float tribbingRate{ 1.0f };
		int   tribbingMaxPerContact{ 3 };
		int   tribbingMaxPerPair{ 5 };
		float tribbingMaxGap{ 6.0f };
		float tribbingSpread{ 1.5f };

		bool  oral{ true };
		bool  oralDrips{ true };
		float mouthBlend{ 0.7f };
		float lipWidth{ 0.9f };
		float lipHeight{ 0.45f };
		float oralRate{ 0.8f };
		int   oralMaxPerContact{ 3 };
		int   oralMaxPerPair{ 5 };
		float oralMaxGap{ 6.0f };
		float oralSpread{ 1.0f };

		bool  fingering{ true };
		float fingerRadius{ 0.35f };
		float fingerTipExtension{ 0.9f };
		bool  fingerDrips{ true };
		float fingerRate{ 0.8f };
		int   fingerMaxPerContact{ 2 };
		int   fingerMaxPerPair{ 4 };
		float fingerMaxGap{ 4.0f };
		float fingerSpread{ 0.8f };

		bool  toys{ true };
		float toyRadius{ 0.8f };
		float toyMaxRadius{ 20.0f };

		std::vector<std::pair<std::string, std::string>> actions;

		bool  drips{ true };
		bool  wearerDrips{ true };
		float dripInterval{ 4.0f };
		float dripLinger{ 15.0f };
		int   dripMaxPerSource{ 3 };
		float dripRadius{ 0.09f };
		float dripMinRadius{ 0.025f };
		float dripStretchTime{ 1.0f };
		float dripWeight{ 2.5f };
		float dripMaxLength{ 18.0f };
		float dripMaxLife{ 7.0f };
		float dripFadeTime{ 0.9f };

		float vaginaOut{ 0.0f };
		float vaginaUp{ 0.0f };
		float anusOut{ 0.0f };
		float anusUp{ 0.0f };
		float mouthOut{ 0.0f };
		float mouthUp{ 0.0f };

		std::vector<std::string>                         revealingKeywords{ "ostrands_revealing" };
		std::vector<std::pair<std::string, std::string>> revealingArmor;

		bool  arousal{ true };
		bool  arousalWetness{ true };
		bool  arousalPrecum{ true };
		float arousalThreshold{ 50.0f };
		float arousalInterval{ 3.0f };
		float arousalRadius{ 2048.0f };

		int   semen{ -1 };
		float semenAmount{ 1.5f };
		float semenStrength{ 1.0f };
		int   semenPulses{ 5 };
		float semenOpacity{ 0.9f };
		float semenCohesion{ 15.0f };
		float semenViscosity{ 50.0f };
		float semenStretch{ 5.0f };
		float semenStickiness{ 0.4f };
		float semenOverflow{ 0.5f };
		float ocumWait{ 0.5f };

		bool  collision{ true };
		float collisionThickness{ 1.0f };
		bool  showShapes{ false };

		float fluidLifetime{ 30.0f };
		float fluidFade{ 6.0f };
		int   fluidMaxParticles{ 1024 };
		int   fluidTestKey{ 0 };
		int   revealingKey{ 0 };

		static Settings&       Get();
		static const Settings& Defaults();
		static std::unique_lock<std::recursive_mutex> Lock();
		static constexpr float kRateMin = 0.0f, kRateMax = 3.0f;
		static constexpr float kIntervalMin = 0.2f, kIntervalMax = 6.0f;
		static constexpr int kCountMin = 0, kCountMax = 32;
		static constexpr float kTransparencyMin = 0.0f, kTransparencyMax = 0.95f;
		static constexpr float kShineMin = 0.0f, kShineMax = 20.0f;
		static constexpr float kGlossMin = 10.0f, kGlossMax = 1000.0f;
		static constexpr float kGlintRateMin = 0.0f, kGlintRateMax = 6.0f;
		static constexpr float kGlintSizeMin = 0.1f, kGlintSizeMax = 3.0f;
		static constexpr float kGlintBrightnessMin = 0.0f, kGlintBrightnessMax = 5.0f;
		static constexpr float kLingerMin = 0.0f, kLingerMax = 30.0f;
		static constexpr float kThresholdMin = 0.0f, kThresholdMax = 100.0f;
		static constexpr float kAlignMin = -5.0f, kAlignMax = 5.0f;
		static constexpr float kArousalIntervalMin = 0.5f, kArousalIntervalMax = 20.0f;
		static constexpr float kReachMin = 256.0f, kReachMax = 8192.0f;
		static constexpr float kAmountMin = 0.0f, kAmountMax = 3.0f;
		static constexpr float kStrengthMin = 0.2f, kStrengthMax = 2.0f;
		static constexpr int kPulsesMin = 1, kPulsesMax = 12;
		static constexpr float kOverflowMin = 0.0f, kOverflowMax = 1.0f;
		static constexpr float kViscosityMin = 0.0f, kViscosityMax = 100.0f;
		static constexpr float kCohesionMin = 0.0f, kCohesionMax = 20.0f;
		static constexpr float kStretchMin = 0.0f, kStretchMax = 30.0f;
		static constexpr float kStickinessMin = 0.0f, kStickinessMax = 5.0f;
		static constexpr float kOpacityMin = 0.05f, kOpacityMax = 1.0f;
		static constexpr float kLifetimeMin = 5.0f, kLifetimeMax = 300.0f;
		static constexpr float kFadeMin = 0.5f, kFadeMax = 30.0f;
		static constexpr int kParticlesMin = 64, kParticlesMax = 2000;
		static constexpr float kWaitMin = 0.0f, kWaitMax = 2.0f;
		static constexpr float kThicknessMin = 0.5f, kThicknessMax = 2.0f;
		static constexpr int kKeyMax = 255;
		bool Load();
		bool Save() const;
		void Validate();

	private:
		bool ReadFile(const char* a_path);
		void Report() const;
	};
}
