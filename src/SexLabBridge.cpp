#include "SexLabBridge.h"

#include "AutoPairs.h"
#include "FluidManager.h"
#include "Settings.h"
#include "StrandManager.h"

#include <algorithm>
#include <charconv>
#include <format>

namespace OStrands::SexLabBridge
{
	namespace
	{
		constexpr std::int32_t     kThreadBase = 0x10000;
		constexpr std::string_view kPlugin = "SexLab.esm";
		constexpr float            kPairHold = 2.0f;
		constexpr float            kConfirm = 0.5f;
		constexpr float            kGap = 0.75f;
		constexpr float            kFresh = 0.5f;
		constexpr float            kQuiet = 3.0f;
		constexpr float            kPoll = 0.5f;
		constexpr float            kAliasPoll = 0.35f;
		constexpr float            kPollTimeout = 5.0f;
		constexpr float            kOrgasmWindow = 3.0f;
		constexpr float            kRepeat = 10.0f;
		constexpr std::int32_t     kMaxCounterDelta = 16;
		constexpr const char*      kThreadScript = "sslThreadController";
		constexpr const char*      kCountFunction = "GetOrgasmCount";

		namespace PPA
		{
			constexpr std::uint32_t kVersion = 1;
			constexpr const wchar_t* kModule = L"AccuratePenetration.dll";
			constexpr const char*    kExport = "AccuratePenetration_GetAPI_V1";

			enum class Site : std::uint8_t
			{
				kNone,
				kMouth,
				kAnus,
				kVagina,
				kBoth,
				kHandL,
				kHandR,
				kHands
			};

			struct Partner
			{
				RE::ActorHandle actor;
				Site            site{ Site::kNone };
				std::uint8_t    position{ 0 };
				float           depth{ 0.0f };
				float           size{ 0.0f };
				float           girth{ 0.0f };
			};

			struct Update
			{
				std::uint32_t   apiVersion{ 0 };
				std::uint32_t   size{ 0 };
				RE::ActorHandle receiver;
				std::uint8_t    position{ 0 };
				std::uint32_t   context{ 0 };
				const Partner*  self{ nullptr };
				const Partner*  partners{ nullptr };
				std::uint32_t   partnerCount{ 0 };
				float           anusOpening{ 0.0f };
				float           vaginalOpening{ 0.0f };
				bool            ending{ false };
			};

			using Handle = std::uint64_t;
			using Callback = void(__cdecl*)(const Update*, void*);

			struct Interface
			{
				std::uint32_t version{ 0 };
				std::uint32_t size{ 0 };
				Handle(__cdecl* listen)(Callback, void*){ nullptr };
				bool(__cdecl* unlisten)(Handle){ nullptr };
			};

			using GetInterface = const Interface*(__cdecl*)();

			static_assert(sizeof(RE::ActorHandle) == 4);
			static_assert(sizeof(Partner) == 20);
			static_assert(offsetof(Update, context) == 16);
			static_assert(offsetof(Update, partners) == 32);
			static_assert(offsetof(Update, ending) == 52);
			static_assert(sizeof(Interface) == 24);
		}

		enum class Kind
		{
			kStart,
			kStage,
			kOrgasm,
			kEnd
		};

		struct Event
		{
			Kind         kind{ Kind::kStart };
			std::int32_t tid{ 0 };
			RE::FormID   quest{ 0 };
		};

		struct Sighting
		{
			RE::ActorHandle    receiver;
			RE::ActorHandle    giver;
			std::optional<Act> act;
			bool               ending{ false };
			float              depth{ 0.0f };
		};

		struct Sighted
		{
			Pair  pair;
			float first{ 0.0f };
			float last{ 0.0f };

			bool Confirmed() const { return last - first >= kConfirm; }
		};

		struct Inside
		{
			Pair  pair;
			float depth{ 0.0f };
			float at{ 0.0f };
		};

		struct Count
		{
			std::int32_t    tid{ 0 };
			std::uint32_t   serial{ 0 };
			RE::ActorHandle actor;
			std::int32_t    count{ -1 };
			float           sent{ 0.0f };
			std::uint32_t   aliasRound{ 0 }; // 0 = native 0.8 thread-controller query
		};

		struct Scene
		{
			std::int32_t                        tid{ 0 };
			std::vector<RE::ActorHandle>        actors;
			AutoPairs::Detector                 detector;
			std::vector<Sighted>                seen;
			std::vector<Inside>                 inside;
			std::vector<Pair>                   pairs;
			float                               heard{ -1.0f };
			bool                                fromPPA{ false };
			std::uint32_t                       serial{ 0 };
			RE::FormID                         quest{ 0 };
			bool                               aliasMode{ false };
			std::uint32_t                      aliasRound{ 0 };
			float                              aliasRetryAfter{ 0.0f };
			RE::BSTSmartPointer<RE::BSScript::Object> script;
			std::unordered_map<std::uint32_t, std::int32_t> orgasms;
			std::unordered_map<std::uint32_t, float>        climaxed;
			float                               climaxFrame{ -1.0f };
			float                               polled{ -kPoll };
			std::size_t                         waiting{ 0 };
			float                               orgasmAt{ -1.0f };
			bool                                answered{ false };
		};

		enum class Counting
		{
			kUnknown,
			kYes,
			kNo
		};

		std::mutex            lock;
		std::vector<Event>    pending;
		std::vector<Sighting> sightings;
		std::vector<Count>    counts;
		std::vector<Scene>    scenes;
		bool                  installed{ false };
		bool                  ppa{ false };
		float                 clock{ 0.0f };
		std::uint32_t         serials{ 0 };
		Counting              counting{ Counting::kUnknown };

		std::optional<Kind> KindOf(std::string_view a_name)
		{
			if (a_name == "AnimationStart") {
				return Kind::kStart;
			}
			if (a_name == "StageStart" || a_name == "PositionChange" || a_name == "AnimationChange" || a_name == "ActorChangeEnd" ||
				a_name == "ActorsRelocated") {
				return Kind::kStage;
			}
			if (a_name == "OrgasmStart") {
				return Kind::kOrgasm;
			}
			if (a_name == "AnimationEnd") {
				return Kind::kEnd;
			}
			return std::nullopt;
		}

		bool FromSexLab(const RE::TESForm* a_form)
		{
			const auto* file = a_form ? a_form->GetFile(0) : nullptr;
			return file && file->GetFilename() == kPlugin;
		}

		std::optional<Act> ActOf(PPA::Site a_site)
		{
			switch (a_site) {
			case PPA::Site::kVagina:
			case PPA::Site::kBoth:
				return Act::kVaginal;
			case PPA::Site::kAnus:
				return Act::kAnal;
			case PPA::Site::kMouth:
				return Act::kFellatio;
			default:
				return std::nullopt;
			}
		}

		void __cdecl OnPenetration(const PPA::Update* a_update, void*)
		{
			if (!a_update || a_update->apiVersion != PPA::kVersion || a_update->size < sizeof(PPA::Update)) {
				return;
			}
			std::scoped_lock guard(lock);
			if (a_update->ending) {
				sightings.push_back({ a_update->receiver, {}, std::nullopt, true });
				return;
			}
			sightings.push_back({ a_update->receiver, {}, std::nullopt, false });
			for (std::uint32_t i = 0; a_update->partners && i < a_update->partnerCount; ++i) {
				const auto& partner = a_update->partners[i];
				if (const auto act = ActOf(partner.site)) {
					sightings.push_back({ a_update->receiver, partner.actor, *act, false, partner.depth });
				}
			}
		}

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
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto kind = KindOf(a_event->eventName.c_str());
				if (!kind || !a_event->sender || a_event->sender->GetFormType() != RE::FormType::Quest || !FromSexLab(a_event->sender)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const std::string_view text = a_event->strArg.c_str();
				std::int32_t           tid = -1;
				if (std::from_chars(text.data(), text.data() + text.size(), tid).ec != std::errc{} || tid < 0) {
					return RE::BSEventNotifyControl::kContinue;
				}
				std::scoped_lock guard(lock);
				pending.push_back({ *kind, tid, a_event->sender->GetFormID() });
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		std::vector<RE::ActorHandle> ActorsOf(RE::FormID a_quest)
		{
			std::vector<RE::ActorHandle> actors;
			auto*                        quest = RE::TESForm::LookupByID<RE::TESQuest>(a_quest);
			if (!quest) {
				return actors;
			}
			for (const auto& entry : quest->refAliasMap) {
				const auto reference = entry.second.get();
				auto*      actor = reference ? reference->As<RE::Actor>() : nullptr;
				if (actor && std::ranges::find(actors, actor->GetHandle()) == actors.end()) {
					actors.push_back(actor->GetHandle());
				}
			}
			return actors;
		}

		std::string Names(const std::vector<RE::ActorHandle>& a_actors)
		{
			std::string text;
			for (const auto& handle : a_actors) {
				const auto actor = handle.get();
				text += std::format("{}{}", text.empty() ? "" : ", ", actor ? actor->GetDisplayFullName() : "?");
			}
			return text;
		}

		std::string Describe(const std::vector<Pair>& a_pairs)
		{
			std::string text;
			for (const auto& pair : a_pairs) {
				const auto giver = pair.giver.get();
				const auto receiver = pair.receiver.get();
				text += std::format("{}{} gives {} to {}", text.empty() ? "" : "; ", giver ? giver->GetDisplayFullName() : "?", ActName(pair.act),
					receiver ? receiver->GetDisplayFullName() : "?");
			}
			return text.empty() ? std::string("nothing detected") : text;
		}

		void ConnectPenetration()
		{
			auto* module = REX::W32::GetModuleHandleW(PPA::kModule);
			auto* get = module ? reinterpret_cast<PPA::GetInterface>(REX::W32::GetProcAddress(module, PPA::kExport)) : nullptr;
			const auto* api = get ? get() : nullptr;
			if (!api || api->version != PPA::kVersion || api->size < sizeof(PPA::Interface) || !api->listen || !api->unlisten) {
				logger::info("PPA {}; SexLab acts are read from the animation", module ? "has no usable API" : "not found");
				return;
			}
			ppa = api->listen(OnPenetration, nullptr) != 0;
			logger::info("{}", ppa ? "PPA found; SexLab penetration comes from PPA" : "PPA refused the listener; SexLab acts are read from the animation");
		}

		void Absorb()
		{
			std::vector<Sighting> batch;
			{
				std::scoped_lock guard(lock);
				batch.swap(sightings);
			}
			for (const auto& sighting : batch) {
				for (auto& scene : scenes) {
					if (std::ranges::find(scene.actors, sighting.receiver) == scene.actors.end()) {
						continue;
					}
					if (sighting.ending) {
						std::erase_if(scene.seen, [&](const Sighted& a_entry) { return a_entry.pair.receiver == sighting.receiver; });
						std::erase_if(scene.inside, [&](const Inside& a_entry) { return a_entry.pair.receiver == sighting.receiver; });
						continue;
					}
					scene.heard = clock;
					if (!sighting.act || std::ranges::find(scene.actors, sighting.giver) == scene.actors.end() || !Anatomy::Allowed(*sighting.act)) {
						continue;
					}
					const Pair pair{ sighting.giver, sighting.receiver, *sighting.act };
					if (Anatomy::IsPenis(pair.act)) {
						auto deepest = std::ranges::find_if(scene.inside, [&](const Inside& a_entry) { return a_entry.pair.giver == pair.giver; });
						if (deepest == scene.inside.end()) {
							scene.inside.push_back({ pair, sighting.depth, clock });
						} else if (deepest->at != clock || sighting.depth > deepest->depth) {
							*deepest = { pair, sighting.depth, clock };
						}
					}
					auto entry = std::ranges::find_if(scene.seen, [&](const Sighted& a_entry) { return a_entry.pair == pair; });
					if (entry == scene.seen.end()) {
						scene.seen.push_back({ pair, clock, clock });
						continue;
					}
					if (!entry->Confirmed() && clock - entry->last > kGap) {
						entry->first = clock;
					}
					entry->last = clock;
				}
			}
		}

		std::vector<Pair> ClimaxPairs(const Scene& a_scene, RE::ActorHandle a_actor, bool a_debug)
		{
			auto       pairs = a_scene.pairs;
			const auto live = std::ranges::find_if(a_scene.inside, [&](const Inside& a_entry) {
				return a_entry.pair.giver == a_actor && a_entry.depth > 0.0f && clock - a_entry.at <= kFresh;
			});
			if (live == a_scene.inside.end()) {
				return pairs;
			}
			std::erase_if(pairs, [&](const Pair& a_pair) { return a_pair.giver == a_actor && Anatomy::IsPenis(a_pair.act); });
			pairs.insert(pairs.begin(), live->pair);
			if (a_debug) {
				const auto receiver = live->pair.receiver.get();
				logger::info("SexLab thread {}: PPA puts the climax {:.1f} deep, {} into {}", a_scene.tid, live->depth, ActName(live->pair.act),
					receiver ? receiver->GetDisplayFullName() : "?");
			}
			return pairs;
		}

		class CountCallback : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			CountCallback(std::int32_t a_tid, std::uint32_t a_serial, RE::ActorHandle a_actor, float a_sent, std::uint32_t a_aliasRound = 0) :
				_answer{ a_tid, a_serial, a_actor, -1, a_sent, a_aliasRound }
			{}

			void operator()(RE::BSScript::Variable a_result) override
			{
				auto answer = _answer;
				if (a_result.IsInt()) {
					answer.count = a_result.GetSInt();
				}
				std::scoped_lock guard(lock);
				counts.push_back(answer);
			}

			void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

		private:
			Count _answer;
		};

		bool HasFunction(const RE::BSScript::ObjectTypeInfo* a_type, const char* a_name)
		{
			for (; a_type; a_type = a_type->GetParent()) {
				const auto* functions = a_type->GetMemberFuncIter();
				for (std::uint32_t i = 0; functions && i < a_type->GetNumMemberFuncs(); ++i) {
					if (functions[i].func && _stricmp(functions[i].func->GetName().c_str(), a_name) == 0) {
						return true;
					}
				}
			}
			return false;
		}

		RE::BSTSmartPointer<RE::BSScript::Object> CountingScript(RE::FormID a_quest)
		{
			if (counting == Counting::kNo) {
				return {};
			}
			auto*                                     quest = RE::TESForm::LookupByID<RE::TESQuest>(a_quest);
			auto*                                     vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto*                                     policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			RE::BSTSmartPointer<RE::BSScript::Object> script;
			if (!quest || !policy || !vm->FindBoundObject(policy->GetHandleForObject(RE::TESQuest::FORMTYPE, quest), kThreadScript, script) || !script) {
				return {};
			}
			if (counting == Counting::kUnknown) {
				counting = HasFunction(script->GetTypeInfo(), kCountFunction) ? Counting::kYes : Counting::kNo;
				logger::info("{}", counting == Counting::kYes ? "SexLab thread-controller orgasm counter found" :
					                                               "SexLab thread-controller counter unavailable; sslActorAlias.GetOrgasmCount fallback will be attempted");
			}
			return counting == Counting::kYes ? script : RE::BSTSmartPointer<RE::BSScript::Object>{};
		}

		bool Climax(Scene& a_scene, RE::ActorHandle a_actor, bool a_debug)
		{
			const auto key = a_actor.native_handle();
			if (const auto last = a_scene.climaxed.find(key); last != a_scene.climaxed.end() && clock - last->second < kRepeat) {
				if (a_debug) {
					const auto actor = a_actor.get();
					logger::info("SexLab thread {}: {} climax request suppressed by the existing {:.0f}s repeat guard",
						a_scene.tid, actor ? actor->GetDisplayFullName() : "?", kRepeat);
				}
				return false;
			}
			a_scene.climaxed[key] = clock;
			a_scene.climaxFrame = clock;
			if (a_debug) {
				const auto actor = a_actor.get();
				logger::info("SexLab thread {}: {} climax request submitted", a_scene.tid, actor ? actor->GetDisplayFullName() : "?");
			}
			FluidManager::Get().Climax(kThreadBase + a_scene.tid, a_actor, ClimaxPairs(a_scene, a_actor, a_debug), a_scene.actors);
			return true;
		}

		void ClimaxEveryone(Scene& a_scene, bool a_debug)
		{
			a_scene.orgasmAt = -1.0f;
			for (const auto& handle : a_scene.actors) {
				Climax(a_scene, handle, a_debug);
			}
		}

		void AbsorbCounts(bool a_debug)
		{
			std::vector<Count> batch;
			{
				std::scoped_lock guard(lock);
				batch.swap(counts);
			}
			for (const auto& answer : batch) {
				auto scene = std::ranges::find_if(scenes, [&](const Scene& a_scene) { return a_scene.tid == answer.tid && a_scene.serial == answer.serial; });
				if (scene == scenes.end()) {
					continue;
				}
				if (answer.aliasRound != 0 && (!scene->aliasMode || scene->aliasRound != answer.aliasRound)) {
					continue; // Discard a late Papyrus callback from an expired alias poll.
				}
				if (scene->waiting > 0) {
					--scene->waiting;
				}
				if (answer.count < 0) {
					// A dispatched Papyrus call can still fail to return an Int. Back off
					// alias polling in that case instead of retrying every 0.35 seconds.
					if (answer.aliasRound != 0) {
						if (clock >= scene->aliasRetryAfter) {
							logger::warn("SexLab thread {}: sslActorAlias.GetOrgasmCount returned no Int; retrying after a delay", scene->tid);
						}
						scene->aliasRetryAfter = std::max(scene->aliasRetryAfter, clock + kPollTimeout);
					}
					continue;
				}
				if (scene->orgasmAt >= 0.0f && answer.sent >= scene->orgasmAt) {
					scene->answered = true;
				}
				const auto key = answer.actor.native_handle();
				auto [entry, added] = scene->orgasms.try_emplace(key, answer.count);
				if (added) {
					// The first sample is a baseline. P+ can restore a saved orgasm
					// count when it initializes an alias, so replaying this value may
					// incorrectly create an effect for an earlier scene.
					if (a_debug) {
						const auto actor = answer.actor.get();
						logger::info("SexLab thread {}: established orgasm-count baseline {} for {}", scene->tid, answer.count,
							actor ? actor->GetDisplayFullName() : "?");
					}
					continue;
				}
				if (answer.count == entry->second) {
					continue;
				}
				if (answer.count < entry->second) {
					// Alias initialization/restore can reset or restore the counter.
					// Resync instead of interpreting a reset as a burst of new orgasms.
					const auto previous = entry->second;
					entry->second = answer.count;
					logger::info("SexLab thread {}: orgasm counter changed from {} to {}; resynchronized without spawning an effect",
						scene->tid, previous, answer.count);
					continue;
				}
				const auto delta = answer.count - entry->second;
				entry->second = answer.count;
				if (delta <= 0) {
					continue;
				}
				if (delta > kMaxCounterDelta) {
					logger::warn("SexLab thread {}: orgasm counter jumped by {}; resynchronized without spawning a burst",
						scene->tid, delta);
					continue;
				}
				const bool submitted = Climax(*scene, answer.actor, a_debug);
				if (a_debug && delta > 1) {
					const auto actor = answer.actor.get();
					logger::info("SexLab thread {}: orgasm counter advanced by {} for {}; {} effect request submitted (multiple increments are coalesced; the {:.0f}s repeat guard remains active)",
						scene->tid, delta, actor ? actor->GetDisplayFullName() : "?", submitted ? "one" : "no", kRepeat);
				}
			}
		}

		// Some SexLab builds, including the supplied SexLab P+ 2.19.0 source,
		// expose per-actor counts on sslActorAlias rather than sslThreadController.
		// This fallback reads the existing public Papyrus function. It does not require
		// a standalone SLSO.esp, install a low-level hook, or overwrite any PEX file.
		void PollAliases(Scene& a_scene)
		{
			if (!a_scene.aliasMode || clock < a_scene.aliasRetryAfter ||
				(a_scene.waiting > 0 && clock - a_scene.polled < kPollTimeout) ||
				(a_scene.orgasmAt < 0.0f && clock - a_scene.polled < kAliasPoll)) {
				return;
			}
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(a_scene.quest);
			if (!quest || !policy) {
				return;
			}
			a_scene.polled = clock;
			a_scene.waiting = 0;
			a_scene.aliasRound = a_scene.aliasRound == UINT32_MAX ? 1 : a_scene.aliasRound + 1;
			bool eligible = false;
			for (auto* baseAlias : quest->aliases) {
				auto* alias = (baseAlias && baseAlias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID) ?
					static_cast<RE::BGSRefAlias*>(baseAlias) : nullptr;
				auto* actor = alias ? alias->GetActorReference() : nullptr;
				if (!actor || std::ranges::find(a_scene.actors, actor->GetHandle()) == a_scene.actors.end()) {
					continue;
				}
				const auto shaft = Anatomy::GetShaftState(actor);
				if (!shaft.schlong || shaft.strapOn) {
					continue;
				}
				eligible = true;
				const auto aliasHandle = policy->GetHandleForObject(alias->GetVMTypeID(), alias);
				if (aliasHandle == policy->EmptyHandle()) {
					continue;
				}
				RE::BSTSmartPointer<RE::BSScript::Object> script;
				if (!vm->FindBoundObject(aliasHandle, "sslActorAlias", script) || !script) {
					continue;
				}
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb(
					new CountCallback(a_scene.tid, a_scene.serial, actor->GetHandle(), clock, a_scene.aliasRound));
				if (vm->DispatchMethodCall(script, kCountFunction, RE::MakeFunctionArguments(), cb)) {
					++a_scene.waiting;
				}
			}
			if (!eligible && a_scene.orgasmAt >= 0.0f) {
				a_scene.answered = true;
			}
			if (eligible && a_scene.waiting == 0) {
				a_scene.aliasRetryAfter = clock + 5.0f; // Avoid hammering a missing/incompatible Papyrus method.
			}
		}

		void Poll(Scene& a_scene)
		{
			if (!a_scene.script || (a_scene.waiting > 0 && clock - a_scene.polled < kPollTimeout) ||
				(a_scene.orgasmAt < 0.0f && clock - a_scene.polled < kPoll)) {
				return;
			}
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			a_scene.polled = clock;
			a_scene.waiting = 0;
			bool eligible = false;
			for (const auto& handle : a_scene.actors) {
				const auto actor = handle.get();
				const auto shaft = actor ? Anatomy::GetShaftState(actor.get()) : Anatomy::ShaftState{};
				if (!shaft.schlong || shaft.strapOn) {
					continue;
				}
				eligible = true;
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new CountCallback(a_scene.tid, a_scene.serial, handle, clock));
				if (vm->DispatchMethodCall(a_scene.script, kCountFunction, RE::MakeFunctionArguments(actor.get()), callback)) {
					++a_scene.waiting;
				}
			}
			if (!eligible && a_scene.orgasmAt >= 0.0f) {
				a_scene.answered = true;
			}
		}
	}

	void Connect()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		installed = data && data->LookupModByName(kPlugin);
		if (!installed) {
			logger::info("SexLab not found");
			return;
		}
		auto* source = SKSE::GetModCallbackEventSource();
		if (!source) {
			installed = false;
			logger::warn("SKSE mod events are unavailable; SexLab scenes are ignored");
			return;
		}
		source->AddEventSink(Listener::Get());
		FluidManager::Get().SetSexLab(true);
		logger::info("SexLab found; following its scenes (thread-controller counter first, sslActorAlias counter fallback when needed)");
		ConnectPenetration();
	}

	void Update(float a_delta)
	{
		if (!installed) {
			return;
		}
		clock += a_delta;
		std::vector<Event> events;
		{
			std::scoped_lock guard(lock);
			events.swap(pending);
		}
		if (ppa) {
			Absorb();
		}
		auto        settingsGuard = Settings::Lock();
		const bool  debug = Settings::Get().debugLog;
		const auto  find = [](std::int32_t a_tid) { return std::ranges::find_if(scenes, [&](const Scene& a_scene) { return a_scene.tid == a_tid; }); };
		AbsorbCounts(debug);
		std::vector<Event> deferred;
		for (const auto& event : events) {
			const std::int32_t thread = kThreadBase + event.tid;
			auto               scene = find(event.tid);
			switch (event.kind) {
			case Kind::kStart:
			case Kind::kStage:
				{
					if (scene == scenes.end()) {
						scenes.push_back({ event.tid });
						scene = std::prev(scenes.end());
						scene->serial = ++serials;
					}
					scene->quest = event.quest;
					if (!scene->script && !scene->aliasMode) {
						scene->script = CountingScript(event.quest);
						// P+ exposes the per-actor counter on sslActorAlias and does not ship SLSO.esp.
						// Try the alias fallback whenever the thread-controller counter is unavailable.
						scene->aliasMode = !scene->script;
						if (scene->aliasMode && debug) {
							logger::info("SexLab thread {}: sslActorAlias.GetOrgasmCount fallback enabled", event.tid);
						}
					}
					auto actors = ActorsOf(event.quest);
					if (debug && (event.kind == Kind::kStart || actors != scene->actors)) {
						logger::info("SexLab thread {} {}: {}", event.tid, event.kind == Kind::kStart ? "started" : "changed", Names(actors));
					}
					FluidManager::Get().Scene(thread, actors);
					scene->detector.Begin(actors);
					scene->actors = std::move(actors);
				}
				break;
			case Kind::kOrgasm:
				if (scene == scenes.end()) {
					break;
				}
				if (scene->script || scene->aliasMode) {
					scene->orgasmAt = clock;
					scene->answered = false;
				} else {
					ClimaxEveryone(*scene, debug);
				}
				break;
			case Kind::kEnd:
				if (scene != scenes.end()) {
					if (scene->orgasmAt >= 0.0f) {
						ClimaxEveryone(*scene, debug);
					}
					if (scene->climaxFrame == clock) {
						deferred.push_back(event);
						break;
					}
					scenes.erase(scene);
				}
				StrandManager::Get().ClearThread(thread);
				FluidManager::Get().EndThread(thread);
				if (debug) {
					logger::info("SexLab thread {} ended", event.tid);
				}
				break;
			}
		}
		if (!deferred.empty()) {
			std::scoped_lock guard(lock);
			pending.insert(pending.begin(), deferred.begin(), deferred.end());
		}
		for (auto& scene : scenes) {
			if (scene.orgasmAt >= 0.0f && clock - scene.orgasmAt >= kOrgasmWindow) {
				if (scene.answered) {
					scene.orgasmAt = -1.0f;
				} else {
					if (debug) {
						logger::info("SexLab thread {}: no orgasm counts in time; everyone climaxes", scene.tid);
					}
					ClimaxEveryone(scene, debug);
				}
			}
			if (scene.aliasMode) {
				PollAliases(scene);
			} else {
				Poll(scene);
			}
		}
		for (auto& scene : scenes) {
			const bool fromPPA = scene.heard >= 0.0f && clock - scene.heard <= kQuiet;
			const bool sampled = scene.detector.Sample(a_delta);
			if (!sampled && !fromPPA && !scene.fromPPA) {
				continue;
			}
			std::vector<Pair> next;
			if (fromPPA) {
				std::erase_if(scene.seen, [](const Sighted& a_entry) { return clock - a_entry.last > (a_entry.Confirmed() ? kPairHold : kGap); });
				for (const auto& entry : scene.seen) {
					if (entry.Confirmed()) {
						next.push_back(entry.pair);
					}
				}
				for (const auto& pair : scene.detector.Pairs()) {
					const bool mouthTaken = Anatomy::IsOralGiver(pair.act) &&
					                        std::ranges::any_of(next, [&](const Pair& a_pair) { return a_pair.act == Act::kFellatio && a_pair.receiver == pair.giver; });
					if (!Anatomy::IsPenis(pair.act) && !mouthTaken) {
						next.push_back(pair);
					}
				}
			} else {
				next = scene.detector.Pairs();
			}
			scene.fromPPA = fromPPA;
			if (next == scene.pairs) {
				continue;
			}
			scene.pairs = std::move(next);
			StrandManager::Get().SetPairs(kThreadBase + scene.tid, scene.pairs);
			if (debug) {
				logger::info("SexLab thread {}{}: {}", scene.tid, fromPPA ? " (PPA)" : "", Describe(scene.pairs));
			}
		}
	}

	void Reset()
	{
		{
			std::scoped_lock guard(lock);
			pending.clear();
			sightings.clear();
			counts.clear();
		}
		scenes.clear();
		counting = Counting::kUnknown;
	}

	bool Owns(std::int32_t a_thread)
	{
		return a_thread >= kThreadBase;
	}
}
