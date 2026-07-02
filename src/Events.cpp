#include "Events.h"
#include "DelayedDispatcher.h"
#include "ListManager.h"
#include "Settings.h"

RE::BSEventNotifyControl NpcCycleSink::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
	if (!a_event || !a_event->holder) return RE::BSEventNotifyControl::kContinue;

	auto* actor = a_event->holder->As<RE::Actor>();
	if (!actor || actor->IsDead()) return RE::BSEventNotifyControl::kContinue;
	const std::string_view eventName = a_event->tag;
	Settings::ApplyRulesForEvent(actor, eventName);
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
{
	if (!a_event || !a_event->actor) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto actor = a_event->actor.get();
	auto* npc = actor->As<RE::Actor>();
	if (npc && npc != RE::PlayerCharacter::GetSingleton()) {
		switch (a_event->newState.get()) {
		case RE::ACTOR_COMBAT_STATE::kCombat:
			NpcCombatTracker::RegisterSink(npc);
			break;
		case RE::ACTOR_COMBAT_STATE::kNone:
			NpcCombatTracker::UnregisterSink(npc);
			break;
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

void NpcCombatTracker::RegisterSink(RE::Actor* a_actor)
{
	if (!a_actor) {
		return;
	}

	std::unique_lock lock(g_mutex);
	if (g_trackedNPCs.find(a_actor->GetFormID()) == g_trackedNPCs.end()) {
		a_actor->AddAnimationGraphEventSink(NpcCycleSink::GetSingleton());
		g_trackedNPCs.insert(a_actor->GetFormID());
	}
}

void NpcCombatTracker::UnregisterSink(RE::Actor* a_actor)
{
	if (!a_actor || a_actor->IsPlayerRef()) return;

	std::unique_lock lock(g_mutex);
	if (g_trackedNPCs.find(a_actor->GetFormID()) != g_trackedNPCs.end()) {
		a_actor->RemoveAnimationGraphEventSink(NpcCycleSink::GetSingleton());
		g_trackedNPCs.erase(a_actor->GetFormID());
	}
}

void NpcCombatTracker::RegisterSinksForExistingCombatants()
{
	auto* processLists = RE::ProcessLists::GetSingleton();
	if (!processLists) {
		SKSE::log::warn("[NpcCombatTracker] Não foi possível obter ProcessLists.");
		return;
	}

	// Itera sobre todos os atores que estão "ativos" no jogo
	for (auto& actorHandle : processLists->highActorHandles) {
		if (auto actor = actorHandle.get().get()) {
			// A função IsInCombat() nos diz se o ator já está em um estado de combate
			if (!actor->IsPlayerRef()) {
				if (actor->IsInCombat()) {
					SKSE::log::info("[NpcCombatTracker] Ator '{}' ({:08X}) já está em combate. Registrando sink...",
						actor->GetName(), actor->GetFormID());
					// Usamos a mesma função de registro que já existe!
					RegisterSink(actor);
				}
			}

		}
	}

	SKSE::log::info("[NpcCombatTracker] Verificação concluída.");
}

[[nodiscard]] RE::Actor* LookupActorByFormID(RE::FormID a_formID)
{
	if (a_formID == 0) {
		return nullptr;
	}

	auto* form = RE::TESForm::LookupByID(a_formID);
	return form ? form->As<RE::Actor>() : nullptr;
}

void ScheduleSinkRegistration(RE::Actor* actor, int attempts)
{
	if (attempts > 20) {
		SKSE::log::critical("[Actor3DLoadEventHandler] Desistindo após {} tentativas para o ator {:08X}.", attempts, actor->GetFormID());
		return;
	}

	const auto actorFormID = actor->GetFormID();

	Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(100), [actorFormID, attempts]() {
		SKSE::GetTaskInterface()->AddTask([actorFormID, attempts]() {
			auto* actor = LookupActorByFormID(actorFormID);
			if (!actor) return;

			RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
			actor->GetAnimationGraphManager(graphManager);

			if (graphManager) {
				NpcCombatTracker::UnregisterSink(actor);
				NpcCombatTracker::RegisterSink(actor);
			}
			else {
				// Graph ainda nulo, tenta de novo
				ScheduleSinkRegistration(actor, attempts + 1);
			}
			});
		});
}

RE::BSEventNotifyControl PC3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*)
{
	if (!a_event || !a_event->loaded) {
		return RE::BSEventNotifyControl::kContinue;
	}

	// Em vez de pegar o Player Singleton, buscamos o formulário pelo ID do evento
	auto* form = RE::TESForm::LookupByID(a_event->formID);
	if (!form) return RE::BSEventNotifyControl::kContinue;

	// Tentamos converter para Ator. Se não for ator (ex: uma parede), ignoramos.
	auto* actor = form->As<RE::Actor>();

	if (actor) {
		ScheduleSinkRegistration(actor, 0);
	}

	return RE::BSEventNotifyControl::kContinue;
}
