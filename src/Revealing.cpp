#include "Revealing.h"

#include "Anatomy.h"
#include "Settings.h"

namespace OStrands::Revealing
{
	namespace
	{
		std::mutex  statusLock;
		std::string status;

		std::string Lower(std::string_view a_text)
		{
			std::string text(a_text);
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::string KeyOf(const RE::TESForm* a_form)
		{
			const auto* file = a_form ? a_form->GetFile(0) : nullptr;
			if (!file || a_form->IsDynamicForm()) {
				return {};
			}
			return std::format("{}|{:#x}", Lower(file->GetFilename()), a_form->GetLocalFormID());
		}

		bool Tagged(RE::TESObjectARMO* a_armor)
		{
			const auto& settings = Settings::Get();
			const auto  key = KeyOf(a_armor);
			if (!key.empty() && std::ranges::any_of(settings.revealingArmor, [&](const auto& a_entry) { return a_entry.first == key; })) {
				return true;
			}
			for (std::uint32_t i = 0; a_armor->keywords && i < a_armor->numKeywords; ++i) {
				const auto* keyword = a_armor->keywords[i];
				const char* id = keyword ? keyword->GetFormEditorID() : nullptr;
				if (id && *id && std::ranges::find(settings.revealingKeywords, Lower(id)) != settings.revealingKeywords.end()) {
					return true;
				}
			}
			return false;
		}

		void Report(std::string a_message)
		{
			logger::info("{}", a_message);
			RE::SendHUDMessage::ShowHUDMessage(std::format("OStrands: {}", a_message).c_str());
			std::scoped_lock guard(statusLock);
			status = std::move(a_message);
		}

		RE::Actor* Target()
		{
			const auto selected = RE::Console::GetSelectedRef();
			auto*      actor = selected ? selected->As<RE::Actor>() : nullptr;
			return actor ? actor : RE::PlayerCharacter::GetSingleton();
		}

		bool Marked(const std::string& a_key)
		{
			const auto& list = Settings::Get().revealingArmor;
			return !a_key.empty() && std::ranges::any_of(list, [&](const auto& a_entry) { return a_entry.first == a_key; });
		}
	}

	bool Exposed(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		if (!a_actor->GetParentCell() || !a_actor->Is3DLoaded()) {
			return false;
		}
		auto* armor = Anatomy::BodyArmor(a_actor);
		if (!armor) {
			return true;
		}
		auto guard = Settings::Lock();
		return Tagged(armor);
	}

	void Toggle()
	{
		auto* actor = Target();
		auto* armor = Anatomy::BodyArmor(actor);
		bool  marked = false;
		if (armor) {
			auto guard = Settings::Lock();
			marked = Marked(KeyOf(armor));
		}
		Mark(!marked);
	}

	void Mark(bool a_revealing)
	{
		auto* actor = Target();
		if (!actor) {
			return;
		}
		const std::string who = actor->GetDisplayFullName();
		auto*             armor = Anatomy::BodyArmor(actor);
		if (!armor) {
			Report(std::format("{} has nothing in the body slot.", who));
			return;
		}
		const auto key = KeyOf(armor);
		if (key.empty()) {
			Report(std::format("{}'s {} was made in-game and can't be marked.", who, armor->GetName()));
			return;
		}
		auto       guard = Settings::Lock();
		auto&      settings = Settings::Get();
		auto&      list = settings.revealingArmor;
		const auto entry = std::ranges::find_if(list, [&](const auto& a_entry) { return a_entry.first == key; });
		if (a_revealing == (entry != list.end())) {
			Report(std::format("{} ({}) already counts as {}.", armor->GetName(), who, a_revealing ? "revealing" : "covering"));
			return;
		}
		const auto previous = list;
		if (a_revealing) {
			list.emplace_back(key, armor->GetName());
		} else {
			list.erase(entry);
		}
		if (!settings.Save()) {
			list = previous;
			Report("Could not save OStrands_User.ini; see OStrands.log.");
			return;
		}
		Report(std::format("{} ({}) now counts as {}.", armor->GetName(), who, a_revealing ? "revealing" : "covering"));
	}

	std::string Status()
	{
		std::scoped_lock guard(statusLock);
		return status;
	}
}
