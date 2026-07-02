#include "logger.h"
#include "Events.h"
#include "ListManager.h"
#include "Settings.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPostLoad) {
        Settings::RegisterMenu();
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Settings::LoadSettings();

        if (auto* eventSource = RE::ScriptEventSourceHolder::GetSingleton()) {
            eventSource->AddEventSink(NpcCombatTracker::GetSingleton());
            eventSource->AddEventSink(PC3DLoadEventHandler::GetSingleton());
        }

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            player->AddAnimationGraphEventSink(NpcCycleSink::GetSingleton());
        }

        NpcCombatTracker::RegisterSinksForExistingCombatants();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        auto* manager = ListManager::GetSingleton();
        if (manager && !manager->_isPopulated) {
            manager->PopulateAllLists();
        }
        Settings::LoadSettings();
        NpcCombatTracker::RegisterSinksForExistingCombatants();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
