#include "Settings.h"
#include "AtomicFile.h"

#include <SimpleIni.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

namespace OStrands
{
	namespace
	{
		constexpr const char* kPath = "Data/SKSE/Plugins/OStrands.ini";
		constexpr const char* kUserPath = "Data/SKSE/Plugins/OStrands_User.ini";

		bool ReadIni(CSimpleIniA& ini, const char* a_path)
		{
			if (ini.LoadFile(a_path) >= 0) {
				return true;
			}
			if (std::filesystem::exists(a_path)) {
				throw std::runtime_error(std::format("Cannot read the existing {}", a_path));
			}
			return false;
		}

		bool Same(float a_value, float a_default)
		{
			return std::abs(a_value - a_default) <= 1e-5f * std::max(1.0f, std::abs(a_default));
		}

		template <class T>
		void Merge(std::vector<std::pair<std::string, T>>& a_list, std::string a_key, T a_value)
		{
			auto entry = std::ranges::find_if(a_list, [&](const auto& a_entry) { return a_entry.first == a_key; });
			if (entry != a_list.end()) {
				entry->second = std::move(a_value);
			} else {
				a_list.emplace_back(std::move(a_key), std::move(a_value));
			}
		}
	}

	std::unique_lock<std::recursive_mutex> Settings::Lock()
	{
		static std::recursive_mutex mutex;
		return std::unique_lock(mutex);
	}

	Settings& Settings::Get()
	{
		static Settings settings;
		return settings;
	}

	const Settings& Settings::Defaults()
	{
		static const Settings defaults;
		return defaults;
	}

	bool Settings::Save() const
	{
		auto guard = Lock();
		try {
			CSimpleIniA ini;
			ini.SetUnicode();
			const bool existed = ReadIni(ini, kUserPath);
			const auto& d = Defaults();
			bool        changed = false;

			auto keep = [&](const char* section, const char* key, bool differs, const std::string& value) {
				if (!differs) {
					ini.Delete(section, key, true);
					return;
				}
				if (ini.SetValue(section, key, value.c_str()) < 0) {
					throw std::runtime_error("Cannot serialize settings");
				}
				changed = true;
			};
			auto b = [&](const char* section, const char* key, bool Settings::* field) {
				keep(section, key, this->*field != d.*field, this->*field ? "true" : "false");
			};
			auto f = [&](const char* section, const char* key, float Settings::* field) {
				keep(section, key, !Same(this->*field, d.*field), std::format("{}", this->*field));
			};
			auto i = [&](const char* section, const char* key, int Settings::* field) {
				keep(section, key, this->*field != d.*field, std::to_string(this->*field));
			};

			b("General", "Enabled", &Settings::enabled);
			b("General", "DebugLog", &Settings::debugLog);
			f("Spawn", "Rate", &Settings::bridgeRate);
			f("Shape", "Transparency", &Settings::transparency);
			f("Shape", "Shine", &Settings::shine);
			f("Shape", "Gloss", &Settings::gloss);
			b("Glints", "Enabled", &Settings::glints);
			f("Glints", "Rate", &Settings::glintRate);
			f("Glints", "Size", &Settings::glintSize);
			f("Glints", "Brightness", &Settings::glintBrightness);
			i("Spawn", "MaxTotal", &Settings::maxTotal);
			b("Crotch", "Enabled", &Settings::crotch);
			f("Crotch", "Rate", &Settings::crotchRate);
			b("Tribbing", "Enabled", &Settings::tribbing);
			b("Oral", "Enabled", &Settings::oral);
			b("Fingering", "Enabled", &Settings::fingering);
			b("Toys", "Enabled", &Settings::toys);
			b("Drips", "Enabled", &Settings::drips);
			b("Drips", "StrapWearer", &Settings::wearerDrips);
			f("Drips", "Interval", &Settings::dripInterval);
			f("Drips", "Linger", &Settings::dripLinger);
			f("Alignment", "VaginaOut", &Settings::vaginaOut);
			f("Alignment", "VaginaUp", &Settings::vaginaUp);
			f("Alignment", "AnusOut", &Settings::anusOut);
			f("Alignment", "AnusUp", &Settings::anusUp);
			f("Alignment", "MouthOut", &Settings::mouthOut);
			f("Alignment", "MouthUp", &Settings::mouthUp);
			b("Arousal", "Enabled", &Settings::arousal);
			b("Arousal", "Wetness", &Settings::arousalWetness);
			b("Arousal", "Precum", &Settings::arousalPrecum);
			f("Arousal", "Threshold", &Settings::arousalThreshold);
			f("Arousal", "Interval", &Settings::arousalInterval);
			f("Arousal", "Radius", &Settings::arousalRadius);
			keep("Semen", "Enabled", semen != d.semen, semen < 0 ? "auto" : semen > 0 ? "true" : "false");
			f("Semen", "Amount", &Settings::semenAmount);
			f("Semen", "Strength", &Settings::semenStrength);
			i("Semen", "Pulses", &Settings::semenPulses);
			f("Semen", "Opacity", &Settings::semenOpacity);
			f("Semen", "Cohesion", &Settings::semenCohesion);
			f("Semen", "Viscosity", &Settings::semenViscosity);
			f("Semen", "Stretch", &Settings::semenStretch);
			f("Semen", "Stickiness", &Settings::semenStickiness);
			f("Semen", "Overflow", &Settings::semenOverflow);
			f("Semen", "OCumWait", &Settings::ocumWait);
			b("Collision", "Enabled", &Settings::collision);
			f("Collision", "Thickness", &Settings::collisionThickness);
			b("Collision", "Show", &Settings::showShapes);
			f("Fluid", "Lifetime", &Settings::fluidLifetime);
			f("Fluid", "FadeTime", &Settings::fluidFade);
			i("Fluid", "MaxParticles", &Settings::fluidMaxParticles);
			i("Fluid", "TestKey", &Settings::fluidTestKey);
			i("Drips", "RevealingKey", &Settings::revealingKey);
			ini.Delete("RevealingArmor", nullptr, true);
			for (const auto& [id, name] : revealingArmor) {
				if (ini.SetValue("RevealingArmor", id.c_str(), name.c_str()) < 0) {
					throw std::runtime_error("Cannot serialize settings");
				}
				changed = true;
			}

			if (!existed && !changed) {
				return true;
			}
			std::string serialized;
			if (ini.Save(serialized, false) < 0) {
				throw std::runtime_error("Cannot serialize settings");
			}
			WriteFileAtomically(kUserPath, serialized);
			if (!existed) {
				logger::info("your changed settings are now kept in OStrands_User.ini");
			}
			return true;
		} catch (const std::exception& error) {
			logger::warn("could not save {}: {} (previous file preserved)", kUserPath, error.what());
			return false;
		}
	}

	bool Settings::Load()
	{
		auto guard = Lock();
		try {
			Settings replacement = Defaults();
			if (!replacement.ReadFile(kPath)) {
				logger::info("OStrands.ini not found; using built-in defaults");
			}
			const bool custom = replacement.ReadFile(kUserPath);
			replacement.Validate();
			*this = std::move(replacement);
			Report();
			if (custom) {
				logger::info("loaded your changes from OStrands_User.ini");
			} else {
				Save();
			}
			return true;
		} catch (const std::exception& error) {
			logger::warn("could not reload settings: {} (current settings preserved)", error.what());
			return false;
		}
	}

	void Settings::Report() const
	{
		for (const auto& [path, value] : shaftRadiusOverrides) {
			logger::info("shaft radius {} for meshes matching {}", value, path);
		}
		logger::info("settings: bridges {}/s; crotch {}; tribbing {}; oral {}; fingering {}; drips {} every ~{}s; max {} total",
			bridgeRate, crotch, tribbing, oral, fingering, drips, dripInterval, maxTotal);
		logger::info("semen {}, amount {}, {} pulses, lifetime {}s, test key {}", semen < 0 ? "auto" : semen ? "on" : "off", semenAmount,
			semenPulses, fluidLifetime, fluidTestKey);
		logger::info("arousal drips {}: wetness {}, precum {}, from arousal {}, every ~{}s at full arousal, within {} units", arousal ? "on" : "off",
			arousalWetness, arousalPrecum, arousalThreshold, arousalInterval, arousalRadius);
	}

	void Settings::Validate()
	{
		const auto& defaults = Defaults();
		auto bounded = [](float value, float fallback, float min, float max) {
			return std::isfinite(value) ? std::clamp(value, min, max) : fallback;
		};
		bridgeRate = bounded(bridgeRate, defaults.bridgeRate, kRateMin, kRateMax);
		transparency = bounded(transparency, defaults.transparency, kTransparencyMin, kTransparencyMax);
		shine = bounded(shine, defaults.shine, kShineMin, kShineMax);
		gloss = bounded(gloss, defaults.gloss, kGlossMin, kGlossMax);
		glintRate = bounded(glintRate, defaults.glintRate, kGlintRateMin, kGlintRateMax);
		glintSize = bounded(glintSize, defaults.glintSize, kGlintSizeMin, kGlintSizeMax);
		glintBrightness = bounded(glintBrightness, defaults.glintBrightness, kGlintBrightnessMin, kGlintBrightnessMax);
		glintLife = bounded(glintLife, defaults.glintLife, 0.05f, 2.0f);
		glintMax = std::clamp(glintMax, 0, 64);
		crotchRate = bounded(crotchRate, defaults.crotchRate, kRateMin, kRateMax);
		dripInterval = bounded(dripInterval, defaults.dripInterval, kIntervalMin, kIntervalMax);
		dripLinger = bounded(dripLinger, defaults.dripLinger, kLingerMin, kLingerMax);
		arousalThreshold = bounded(arousalThreshold, defaults.arousalThreshold, kThresholdMin, kThresholdMax);
		for (auto field : { &Settings::vaginaOut, &Settings::vaginaUp, &Settings::anusOut, &Settings::anusUp, &Settings::mouthOut, &Settings::mouthUp }) {
			this->*field = bounded(this->*field, 0.0f, kAlignMin, kAlignMax);
		}
		arousalInterval = bounded(arousalInterval, defaults.arousalInterval, kArousalIntervalMin, kArousalIntervalMax);
		arousalRadius = bounded(arousalRadius, defaults.arousalRadius, kReachMin, kReachMax);
		maxTotal = std::clamp(maxTotal, kCountMin, kCountMax);
		semen = std::clamp(semen, -1, 1);
		semenAmount = bounded(semenAmount, defaults.semenAmount, kAmountMin, kAmountMax);
		semenStrength = bounded(semenStrength, defaults.semenStrength, kStrengthMin, kStrengthMax);
		semenPulses = std::clamp(semenPulses, kPulsesMin, kPulsesMax);
		semenOpacity = bounded(semenOpacity, defaults.semenOpacity, kOpacityMin, kOpacityMax);
		semenCohesion = bounded(semenCohesion, defaults.semenCohesion, kCohesionMin, kCohesionMax);
		semenViscosity = bounded(semenViscosity, defaults.semenViscosity, kViscosityMin, kViscosityMax);
		semenStretch = bounded(semenStretch, defaults.semenStretch, kStretchMin, kStretchMax);
		semenStickiness = bounded(semenStickiness, defaults.semenStickiness, kStickinessMin, kStickinessMax);
		semenOverflow = bounded(semenOverflow, defaults.semenOverflow, kOverflowMin, kOverflowMax);
		ocumWait = bounded(ocumWait, defaults.ocumWait, kWaitMin, kWaitMax);
		collisionThickness = bounded(collisionThickness, defaults.collisionThickness, kThicknessMin, kThicknessMax);
		fluidLifetime = bounded(fluidLifetime, defaults.fluidLifetime, kLifetimeMin, kLifetimeMax);
		fluidFade = bounded(fluidFade, defaults.fluidFade, kFadeMin, kFadeMax);
		fluidMaxParticles = std::clamp(fluidMaxParticles, kParticlesMin, kParticlesMax);
		fluidTestKey = fluidTestKey > 0 && fluidTestKey <= kKeyMax ? fluidTestKey : 0;
		revealingKey = revealingKey > 0 && revealingKey <= kKeyMax ? revealingKey : 0;
	}

	bool Settings::ReadFile(const char* a_path)
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		if (!ReadIni(ini, a_path)) {
			return false;
		}

		auto b = [&](const char* section, const char* key, bool& value) { value = ini.GetBoolValue(section, key, value); };
		auto f = [&](const char* section, const char* key, float& value) {
			const double parsed = ini.GetDoubleValue(section, key, value);
			if (std::isfinite(parsed) && std::abs(parsed) <= std::numeric_limits<float>::max()) {
				value = static_cast<float>(parsed);
			} else {
				logger::warn("[{}] {} is not a finite float; using its default", section, key);
			}
		};
		auto i = [&](const char* section, const char* key, int& value) {
			value = static_cast<int>(ini.GetLongValue(section, key, value));
		};
		auto automatic = [&](const char* section, const char* key, int& value) {
			std::string text = ini.GetValue(section, key, "");
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (text == "auto") {
				value = -1;
			} else if (text == "true" || text == "1" || text == "yes" || text == "on") {
				value = 1;
			} else if (text == "false" || text == "0" || text == "no" || text == "off") {
				value = 0;
			} else if (!text.empty()) {
				logger::warn("[{}] {} should be auto, true or false; using its default", section, key);
			}
		};

		b("General", "Enabled", enabled);
		b("General", "DebugLog", debugLog);
		f("Spawn", "Rate", bridgeRate);
		i("Spawn", "MaxPerStroke", maxPerStroke);
		i("Spawn", "MaxPerPair", maxPerPair);
		i("Spawn", "MaxTotal", maxTotal);
		f("Spawn", "Cooldown", cooldown);
		f("Shape", "Radius", radius);
		f("Shape", "MinRadius", minRadius);
		f("Shape", "ShaftRadius", shaftRadius);
		f("Shape", "TipExtension", tipExtension);
		f("Shape", "Transparency", transparency);
		f("Shape", "Shine", shine);
		f("Shape", "Gloss", gloss);
		b("Glints", "Enabled", glints);
		f("Glints", "Rate", glintRate);
		f("Glints", "Size", glintSize);
		f("Glints", "Brightness", glintBrightness);
		f("Glints", "Life", glintLife);
		i("Glints", "Max", glintMax);

		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys("ShaftRadius", keys);
		for (const auto& key : keys) {
			std::string path = key.pItem;
			std::ranges::transform(path, path.begin(), [](unsigned char c) { return c == '/' ? '\\' : static_cast<char>(std::tolower(c)); });
			float value = shaftRadius;
			f("ShaftRadius", key.pItem, value);
			Merge(shaftRadiusOverrides, std::move(path), value);
		}

		f("Physics", "Gravity", gravity);
		f("Physics", "Damping", damping);
		f("Physics", "Viscosity", viscosity);
		f("Physics", "MaxLength", maxLength);
		f("Physics", "MaxLife", maxLife);
		f("Physics", "FadeTime", fadeTime);
		b("Crotch", "Enabled", crotch);
		f("Crotch", "Rate", crotchRate);
		i("Crotch", "MaxPerContact", crotchMaxPerContact);
		i("Crotch", "MaxPerPair", crotchMaxPerPair);
		f("Crotch", "MaxGap", crotchMaxGap);
		f("Crotch", "Spread", crotchSpread);
		f("Crotch", "SkinOffset", crotchSkin);
		f("Crotch", "Radius", crotchRadius);
		f("Crotch", "MinRadius", crotchMinRadius);
		f("Crotch", "MaxLength", crotchMaxLength);
		f("Crotch", "MaxLife", crotchMaxLife);
		b("Tribbing", "Enabled", tribbing);
		f("Tribbing", "Rate", tribbingRate);
		i("Tribbing", "MaxPerContact", tribbingMaxPerContact);
		i("Tribbing", "MaxPerPair", tribbingMaxPerPair);
		f("Tribbing", "MaxGap", tribbingMaxGap);
		f("Tribbing", "Spread", tribbingSpread);
		b("Oral", "Enabled", oral);
		b("Oral", "Drips", oralDrips);
		f("Oral", "MouthBlend", mouthBlend);
		f("Oral", "LipWidth", lipWidth);
		f("Oral", "LipHeight", lipHeight);
		f("Oral", "Rate", oralRate);
		i("Oral", "MaxPerContact", oralMaxPerContact);
		i("Oral", "MaxPerPair", oralMaxPerPair);
		f("Oral", "MaxGap", oralMaxGap);
		f("Oral", "Spread", oralSpread);
		b("Fingering", "Enabled", fingering);
		f("Fingering", "Radius", fingerRadius);
		f("Fingering", "TipExtension", fingerTipExtension);
		b("Fingering", "Drips", fingerDrips);
		f("Fingering", "Rate", fingerRate);
		i("Fingering", "MaxPerContact", fingerMaxPerContact);
		i("Fingering", "MaxPerPair", fingerMaxPerPair);
		f("Fingering", "MaxGap", fingerMaxGap);
		f("Fingering", "Spread", fingerSpread);
		b("Toys", "Enabled", toys);
		f("Toys", "Radius", toyRadius);
		f("Toys", "MaxRadius", toyMaxRadius);
		keys.clear();
		ini.GetAllKeys("Actions", keys);
		for (const auto& key : keys) {
			std::string id = key.pItem;
			std::string act = ini.GetValue("Actions", key.pItem, "");
			std::ranges::transform(id, id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::ranges::transform(act, act.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			Merge(actions, std::move(id), std::move(act));
		}

		b("Drips", "Enabled", drips);
		b("Drips", "StrapWearer", wearerDrips);
		f("Drips", "Interval", dripInterval);
		f("Drips", "Linger", dripLinger);
		i("Drips", "MaxPerSource", dripMaxPerSource);
		f("Drips", "Radius", dripRadius);
		f("Drips", "MinRadius", dripMinRadius);
		f("Drips", "StretchTime", dripStretchTime);
		f("Drips", "DropWeight", dripWeight);
		f("Drips", "MaxLength", dripMaxLength);
		f("Drips", "MaxLife", dripMaxLife);
		f("Drips", "FadeTime", dripFadeTime);
		i("Drips", "RevealingKey", revealingKey);
		if (const char* list = ini.GetValue("Drips", "RevealingKeywords", nullptr)) {
			revealingKeywords.clear();
			std::string_view rest = list;
			while (!rest.empty()) {
				const auto       comma = rest.find(',');
				std::string_view item = rest.substr(0, comma);
				rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
				while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
					item.remove_prefix(1);
				}
				while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
					item.remove_suffix(1);
				}
				if (!item.empty()) {
					std::string keyword(item);
					std::ranges::transform(keyword, keyword.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					revealingKeywords.push_back(std::move(keyword));
				}
			}
		}
		keys.clear();
		ini.GetAllKeys("RevealingArmor", keys);
		for (const auto& key : keys) {
			std::string id = key.pItem;
			std::ranges::transform(id, id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			Merge(revealingArmor, std::move(id), std::string(ini.GetValue("RevealingArmor", key.pItem, "")));
		}

		f("Alignment", "VaginaOut", vaginaOut);
		f("Alignment", "VaginaUp", vaginaUp);
		f("Alignment", "AnusOut", anusOut);
		f("Alignment", "AnusUp", anusUp);
		f("Alignment", "MouthOut", mouthOut);
		f("Alignment", "MouthUp", mouthUp);

		b("Arousal", "Enabled", arousal);
		b("Arousal", "Wetness", arousalWetness);
		b("Arousal", "Precum", arousalPrecum);
		f("Arousal", "Threshold", arousalThreshold);
		f("Arousal", "Interval", arousalInterval);
		f("Arousal", "Radius", arousalRadius);

		automatic("Semen", "Enabled", semen);
		f("Semen", "Amount", semenAmount);
		f("Semen", "Strength", semenStrength);
		i("Semen", "Pulses", semenPulses);
		f("Semen", "Opacity", semenOpacity);
		f("Semen", "Cohesion", semenCohesion);
		f("Semen", "Viscosity", semenViscosity);
		f("Semen", "Stretch", semenStretch);
		f("Semen", "Stickiness", semenStickiness);
		f("Semen", "Overflow", semenOverflow);
		f("Semen", "OCumWait", ocumWait);
		b("Collision", "Enabled", collision);
		f("Collision", "Thickness", collisionThickness);
		b("Collision", "Show", showShapes);
		f("Fluid", "Lifetime", fluidLifetime);
		f("Fluid", "FadeTime", fluidFade);
		i("Fluid", "MaxParticles", fluidMaxParticles);
		i("Fluid", "TestKey", fluidTestKey);
		return true;
	}
}
