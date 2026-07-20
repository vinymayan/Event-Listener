#include "logger.h"
#include "Events.h"
#include "ListManager.h"
#include "Settings.h"
namespace {
    bool hasDFG = false;

    class DynamicFormsGeneratorListener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        static DynamicFormsGeneratorListener* GetSingleton()
        {
            static DynamicFormsGeneratorListener singleton;
            return &singleton;
        }

        void Register()
        {
            if (auto dispatcher = SKSE::GetModCallbackEventSource()) {
                dispatcher->AddEventSink(this);
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
        {
            if (!a_event) return RE::BSEventNotifyControl::kContinue;

            std::string_view eventName = a_event->eventName.c_str();
            if (eventName == "DynamicFormsGeneratorLoaded") {
                ListManager::GetSingleton()->PopulateAllLists();
                return RE::BSEventNotifyControl::kContinue;
            }
            if (eventName == "DynamicFormsGeneratorUpdated") {
                ListManager::GetSingleton()->PopulateAllLists(true);
                return RE::BSEventNotifyControl::kContinue;
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPostLoad) {
        hasDFG = GetModuleHandleA("DynamicFormsGenerator.dll") != nullptr;
        if (hasDFG) {
            logger::info("DynamicFormsGenerator.dll found");
        }
        Settings::RegisterMenu();
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Settings::LoadSettings();
        if (!hasDFG) {
            ListManager::GetSingleton()->PopulateAllLists();
        }

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
        Settings::LoadSettings();
        NpcCombatTracker::RegisterSinksForExistingCombatants();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    DynamicFormsGeneratorListener::GetSingleton()->Register();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
