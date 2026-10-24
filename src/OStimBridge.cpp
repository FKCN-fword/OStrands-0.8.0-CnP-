#include "OStimBridge.h"

#include "FluidManager.h"
#include "Settings.h"
#include "StrandManager.h"

#include "PluginInterface/InterfaceExchangeMessage.h"
#include "PluginInterface/Threading/ThreadInterface.h"

namespace OStrands::OStimBridge
{
	namespace
	{
		RE::Actor* ActorAt(OStim::Thread* thread, std::int32_t position)
		{
			if (position < 0 || static_cast<std::uint32_t>(position) >= thread->getActorCount()) {
				return nullptr;
			}
			return static_cast<RE::Actor*>(thread->getActor(position)->getGameActor());
		}

		std::unordered_map<std::string, std::optional<Act>> actions;
		std::mutex                                          actionsLock;
		OStim::ThreadInterface*                             threadInterface{ nullptr };

		std::optional<std::optional<Act>> ParseAct(std::string_view a_name)
		{
			static constexpr std::array<std::pair<std::string_view, Act>, 10> names{ {
				{ "vaginal", Act::kVaginal },
				{ "anal", Act::kAnal },
				{ "blowjob", Act::kFellatio },
				{ "cunnilingus", Act::kCunnilingus },
				{ "rimming", Act::kAnilingus },
				{ "fingering", Act::kVaginalFingering },
				{ "analfingering", Act::kAnalFingering },
				{ "scissoring", Act::kTribbing },
				{ "toy", Act::kVaginalToy },
				{ "analtoy", Act::kAnalToy },
			} };
			if (a_name == "none") {
				return std::optional<Act>{};
			}
			for (const auto& [name, act] : names) {
				if (name == a_name) {
					return std::optional<Act>{ act };
				}
			}
			return std::nullopt;
		}

		void BuildActions()
		{
			auto settingsGuard = Settings::Lock();
			std::unordered_map<std::string, std::optional<Act>> table{
				{ "vaginalsex", Act::kVaginal },
				{ "analsex", Act::kAnal },
				{ "blowjob", Act::kFellatio },
				{ "deepthroating", Act::kFellatio },
				{ "penilelicking", Act::kFellatio },
				{ "lickingpenis", Act::kFellatio },
				{ "3pp_kissfellatio1", Act::kFellatio },
				{ "3pp_kissfellatio2", Act::kFellatio },
				{ "vulvaleating", Act::kCunnilingus },
				{ "vulvallicking", Act::kCunnilingus },
				{ "cunnilingus", Act::kCunnilingus },
				{ "lickingvagina", Act::kCunnilingus },
				{ "3pp_cunnilingus", Act::kCunnilingus },
				{ "rimjob", Act::kAnilingus },
				{ "anallicking", Act::kAnilingus },
				{ "anilingus", Act::kAnilingus },
				{ "vaginalfingering", Act::kVaginalFingering },
				{ "femalemasturbation", Act::kVaginalFingering },
				{ "3pp_femalemasturbation", Act::kVaginalFingering },
				{ "vulvalrubbing", Act::kVaginalFingering },
				{ "analfingering", Act::kAnalFingering },
				{ "tribbing", Act::kTribbing },
				{ "vaginaltoying", Act::kVaginalToy },
				{ "analtoying", Act::kAnalToy },
				{ "vaginalfisting", std::nullopt },
				{ "analfisting", std::nullopt },
			};
			for (const auto& [id, name] : Settings::Get().actions) {
				if (auto act = ParseAct(name)) {
					table[id] = *act;
				} else {
					logger::warn("[Actions] {} = {}: unknown strand type", id, name);
				}
			}
			logger::info("{} action types mapped", table.size());
			std::scoped_lock guard(actionsLock);
			actions = std::move(table);
		}

		std::optional<Act> ActFromTags(OStim::ActionType* a_type)
		{
			if (a_type->hasTag("fisting")) {
				return std::nullopt;
			}
			if (a_type->hasTag("toying") || a_type->hasTag("toy")) {
				if (a_type->hasTag("vaginalpenetration")) {
					return Act::kVaginalToy;
				}
				if (a_type->hasTag("analpenetration")) {
					return Act::kAnalToy;
				}
				return std::nullopt;
			}
			if (a_type->hasTag("vaginalpenetration")) {
				return Act::kVaginal;
			}
			if (a_type->hasTag("analpenetration")) {
				return Act::kAnal;
			}
			if (a_type->hasTag("fellatio")) {
				return Act::kFellatio;
			}
			if (a_type->hasTag("cunnilingus")) {
				return Act::kCunnilingus;
			}
			if (a_type->hasTag("anilingus")) {
				return Act::kAnilingus;
			}
			if (a_type->hasTag("fingering")) {
				return a_type->hasTag("analstimulation") ? Act::kAnalFingering : Act::kVaginalFingering;
			}
			return std::nullopt;
		}

		std::optional<Pair> PairFor(OStim::Thread* thread, OStim::Action* action)
		{
			auto*              type = action->getType();
			std::optional<Act> act;
			bool               mapped = false;
			{
				std::scoped_lock guard(actionsLock);
				if (auto it = actions.find(type->getActionID()); it != actions.end()) {
					act = it->second;
					mapped = true;
				}
			}
			if (!mapped) {
				act = ActFromTags(type);
			}
			if (!act || !Anatomy::Allowed(*act)) {
				return std::nullopt;
			}
			auto* actor = ActorAt(thread, action->getActor(OStim::ACTOR));
			auto* target = ActorAt(thread, action->getActor(OStim::TARGET));
			if (!actor) {
				return std::nullopt;
			}
			if (!target) {
				target = actor;
			}
			if (!Anatomy::IsPerson(actor) || !Anatomy::IsPerson(target)) {
				return std::nullopt;
			}
			if (*act == Act::kFellatio) {
				std::swap(actor, target);
			}
			return Pair{ actor->GetHandle(), target->GetHandle(), *act };
		}

		std::vector<Pair> PairsOf(OStim::Thread* thread)
		{
			std::vector<Pair> pairs;
			auto*             node = thread->getCurrentNode();
			if (!node) {
				return pairs;
			}
			for (std::uint32_t i = 0; i < node->getActionCount(); ++i) {
				if (auto pair = PairFor(thread, node->getAction(i))) {
					pairs.push_back(*pair);
				}
			}
			return pairs;
		}

		std::vector<RE::ActorHandle> ActorsOf(OStim::Thread* thread)
		{
			std::vector<RE::ActorHandle> result;
			for (std::uint32_t i = 0; i < thread->getActorCount(); ++i) {
				auto* actor = ActorAt(thread, static_cast<std::int32_t>(i));
				if (actor && Anatomy::IsPerson(actor)) {
					result.push_back(actor->GetHandle());
				}
			}
			return result;
		}

		struct SceneChanged : OStim::ThreadEventListener
		{
			void listen(OStim::Thread* thread) override
			{
				auto settingsGuard = Settings::Lock();
				auto* node = thread->getCurrentNode();
				if (!node) {
					return;
				}
				std::vector<Pair> pairs;
				for (std::uint32_t i = 0; i < node->getActionCount(); ++i) {
					auto* action = node->getAction(i);
					auto  pair = PairFor(thread, action);
					if (!pair) {
						continue;
					}
					pairs.push_back(*pair);
					if (Settings::Get().debugLog) {
						logger::info("thread {} scene {}: {} - {} gives {} to {}", thread->getThreadID(), node->getNodeID(),
							action->getType()->getActionID(), pair->giver.get()->GetDisplayFullName(), ActName(pair->act),
							pair->receiver.get()->GetDisplayFullName());
					}
				}
				if (pairs.empty() && node->getActionCount() > 0 && Settings::Get().debugLog) {
					std::string ids;
					for (std::uint32_t i = 0; i < node->getActionCount(); ++i) {
						ids += (i ? ", " : "") + std::string(node->getAction(i)->getType()->getActionID());
					}
					logger::info("thread {} scene {}: no strands for {}", thread->getThreadID(), node->getNodeID(), ids);
				}
				StrandManager::Get().SetPairs(thread->getThreadID(), std::move(pairs));
				FluidManager::Get().Scene(thread->getThreadID(), ActorsOf(thread));
			}
		};

		struct SpeedChanged : OStim::ThreadEventListener
		{
			void listen(OStim::Thread* thread) override
			{
				auto settingsGuard = Settings::Lock();
				StrandManager::Get().Recalibrate();
				if (Settings::Get().debugLog) {
					logger::info("thread {} speed changed", thread->getThreadID());
				}
			}
		};

		struct ThreadStopped : OStim::ThreadEventListener
		{
			void listen(OStim::Thread* thread) override
			{
				auto settingsGuard = Settings::Lock();
				StrandManager::Get().ClearThread(thread->getThreadID());
				FluidManager::Get().EndThread(thread->getThreadID());
				if (Settings::Get().debugLog) {
					logger::info("thread {} ended", thread->getThreadID());
				}
			}
		};

		struct Climaxed : OStim::ThreadActorEventListener
		{
			void listen(OStim::Thread* thread, OStim::ThreadActor* actor) override
			{
				auto  settingsGuard = Settings::Lock();
				auto* game = actor ? static_cast<RE::Actor*>(actor->getGameActor()) : nullptr;
				if (!game) {
					return;
				}
				if (Settings::Get().debugLog) {
					logger::info("thread {}: {} climaxed", thread->getThreadID(), game->GetDisplayFullName());
				}
				FluidManager::Get().Climax(thread->getThreadID(), game->GetHandle(), PairsOf(thread), ActorsOf(thread));
			}
		};

		SceneChanged  sceneChanged;
		SpeedChanged  speedChanged;
		ThreadStopped threadStopped;
		Climaxed      climaxed;
	}

	void Connect()
	{
		BuildActions();
		OStim::InterfaceExchangeMessage message;
		SKSE::GetMessagingInterface()->Dispatch(OStim::InterfaceExchangeMessage::MESSAGE_TYPE, &message,
			sizeof(message), "OStim");
		if (!message.interfaceMap) {
			logger::warn("OStim did not answer; is OStim Standalone installed? Strands stay off.");
			return;
		}

		auto* threads = static_cast<OStim::ThreadInterface*>(
			message.interfaceMap->queryInterface(OStim::ThreadInterface::NAME));
		if (!threads) {
			logger::warn("OStim has no thread interface; strands stay off.");
			return;
		}

		threads->registerNodeChangedListener(&sceneChanged);
		threads->registerSpeedChangedListener(&speedChanged);
		threads->registerThreadStopListener(&threadStopped);
		threads->registerClimaxListener(&climaxed);
		threadInterface = threads;
		logger::info("connected to OStim (interface version {:08X})", threads->getVersion());
	}

	void ReloadActions()
	{
		BuildActions();
	}

	void TestClimax()
	{
		auto* thread = threadInterface ? threadInterface->getThread(0) : nullptr;
		if (!thread) {
			logger::info("test key: no player scene is running");
			RE::SendHUDMessage::ShowHUDMessage("OStrands: start a scene to test the fluid.");
			return;
		}
		const auto pairs = PairsOf(thread);
		const auto actors = ActorsOf(thread);
		for (std::uint32_t i = 0; i < thread->getActorCount(); ++i) {
			auto* actor = ActorAt(thread, static_cast<std::int32_t>(i));
			if (!actor) {
				continue;
			}
			FluidManager::Get().Climax(0, actor->GetHandle(), pairs, actors);
		}
		logger::info("test key: climax sent to {} actor(s) in the player scene", thread->getActorCount());
	}
}
