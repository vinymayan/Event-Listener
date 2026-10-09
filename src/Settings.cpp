#include "Settings.h"

#include "ListManager.h"
#include "RuleExecutionState.h"
#include "logger.h"

#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace ImGui = ImGuiMCP;

namespace
{
	constexpr const char* kModDir = "Data/Viny Mods/Event Listener";
	constexpr const char* kRulesDir = "Data/Viny Mods/Event Listener/Rules";
	constexpr const char* kLanguagePath = "Data/Viny Mods/Event Listener/Language.json";

	std::unordered_map<std::string, std::string> g_langMap;

	std::mutex g_runtimeMutex;
	std::unordered_map<std::string, Settings::RuleRuntimeState> g_ruleRuntime;

	void EnsureMenuListsPopulated()
	{
	}

	[[nodiscard]] const char* GetLoc(const std::string& a_key, const char* a_default)
	{
		const auto it = g_langMap.find(a_key);
		return it != g_langMap.end() ? it->second.c_str() : a_default;
	}

	void WriteDefaultLanguageFile()
	{
		rapidjson::Document doc;
		doc.SetObject();

		if (std::filesystem::exists(kLanguagePath)) {
			FILE* readFile = nullptr;
			fopen_s(&readFile, kLanguagePath, "rb");
			if (readFile) {
				char readBuffer[65536];
				rapidjson::FileReadStream stream(readFile, readBuffer, sizeof(readBuffer));
				doc.ParseStream(stream);
				fclose(readFile);

				if (doc.HasParseError() || !doc.IsObject()) {
					logger::warn("[EventListener] Existing Language.json is invalid; rebuilding defaults.");
					doc.SetObject();
				}
			}
		}

		std::filesystem::create_directories(kModDir);
		auto& allocator = doc.GetAllocator();

		auto ensureString = [&](const char* key, const char* value) {
			if (doc.HasMember(key)) {
				return;
			}

			rapidjson::Value name;
			name.SetString(key, allocator);
			rapidjson::Value text;
			text.SetString(value, allocator);
			doc.AddMember(name, text, allocator);
		};

		ensureString("menu.rules", "Rules");
		ensureString("menu.debug", "Debug");
		ensureString("menu.add_rule", "Add Rule");
		ensureString("menu.enabled", "Enabled");
		ensureString("menu.rule_name", "Rule Name");
		ensureString("menu.event_name", "Event");
		ensureString("menu.search_rule_name", "Search Rule Name");
		ensureString("menu.search_event", "Search Event");
		ensureString("menu.affect_player", "Can Affect Player");
		ensureString("menu.required_perk", "Required Perk");
		ensureString("menu.rule_target", "Target");
		ensureString("menu.target_event_actor", "Event Actor");
		ensureString("menu.target_player", "Player");
		ensureString("menu.target_combat_target", "Combat Target");
		ensureString("menu.execution", "Execution");
		ensureString("menu.chance", "Chance (%)");
		ensureString("menu.cooldown", "Cooldown (seconds)");
		ensureString("menu.cooldown_scope", "Cooldown Scope");
		ensureString("menu.cooldown_global", "Global");
		ensureString("menu.cooldown_per_actor", "Per Actor");
		ensureString("menu.max_activations_per_day", "Maximum Activations per Day (0 = unlimited)");
		ensureString("menu.conditions", "Conditions");
		ensureString("menu.add_condition", "Add Condition");
		ensureString("menu.condition_type", "Condition");
		ensureString("menu.condition_race", "Race");
		ensureString("menu.condition_keyword", "Keyword");
		ensureString("menu.condition_faction", "Faction");
		ensureString("menu.condition_equipped_weapon", "Equipped Weapon");
		ensureString("menu.condition_actor_value", "Actor Value");
		ensureString("menu.invert_condition", "Invert Condition");
		ensureString("menu.race", "Race");
		ensureString("menu.keyword", "Keyword");
		ensureString("menu.faction", "Faction");
		ensureString("menu.weapon", "Weapon");
		ensureString("menu.comparison", "Comparison");
		ensureString("menu.comparison_greater", "Greater Than");
		ensureString("menu.comparison_greater_equal", "Greater Than or Equal");
		ensureString("menu.comparison_less", "Less Than");
		ensureString("menu.comparison_less_equal", "Less Than or Equal");
		ensureString("menu.comparison_equal", "Equal");
		ensureString("menu.comparison_between", "Between");
		ensureString("menu.comparison_value", "Value");
		ensureString("menu.minimum_value", "Minimum Value");
		ensureString("menu.maximum_value", "Maximum Value");
		ensureString("menu.invalid_actor_value", "Invalid Actor Value name");
		ensureString("menu.effects", "Effects");
		ensureString("menu.add_effect", "Add Effect");
		ensureString("menu.delete_rule", "Delete Rule");
		ensureString("menu.effect_type", "Effect");
		ensureString("menu.spell", "Spell");
		ensureString("menu.require_spell_cost", "Only Cast If Resources Are Available");
		ensureString("menu.perk", "Perk");
		ensureString("menu.sound", "Sound");
		ensureString("menu.shader_effect", "Shader Effect");
		ensureString("menu.duration", "Duration");
		ensureString("menu.operation", "Operation");
		ensureString("menu.resource", "Resource");
		ensureString("menu.value_source", "Value Source");
		ensureString("menu.fixed_value", "Fixed Value");
		ensureString("menu.global", "Global");
		ensureString("menu.actor_value", "Actor Value");
		ensureString("menu.form_list_empty", "No forms loaded");
		ensureString("common.none", "None");
		ensureString("common.remove", "Remove");
		ensureString("debug.title", "Debug Tools");
		ensureString("debug.reload_data", "Reload Data");
		ensureString("debug.reload_data_hint", "Refreshes the internal form database. Use this after dynamic form mods have injected or updated forms.");

		FILE* writeFile = nullptr;
		fopen_s(&writeFile, kLanguagePath, "wb");
		if (!writeFile) {
			logger::warn("[EventListener] Could not write default language file to {}.", kLanguagePath);
			return;
		}

		char writeBuffer[65536];
		rapidjson::FileWriteStream stream(writeFile, writeBuffer, sizeof(writeBuffer));
		rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(stream);
		doc.Accept(writer);
		fclose(writeFile);
	}

	[[nodiscard]] std::string ToLower(std::string a_value)
	{
		std::transform(a_value.begin(), a_value.end(), a_value.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return a_value;
	}

	[[nodiscard]] std::string GetLocalFormIDString(const InternalFormInfo& a_info)
	{
		const auto localID = (a_info.formID & 0xFF000000) == 0xFE000000 ? a_info.formID & 0xFFF : a_info.formID & 0xFFFFFF;
		return std::format("{:X}", localID);
	}

	[[nodiscard]] std::string GetFormDropdownLabel(const InternalFormInfo& a_info)
	{
		std::string label;
		if (!a_info.name.empty()) {
			label = a_info.name;
		}
		if (!a_info.editorID.empty()) {
			if (!label.empty()) {
				label += " - ";
			}
			label += a_info.editorID;
		}
		if (label.empty()) {
			label = std::format("{:08X}", a_info.formID);
		}

		label += " (";
		label += a_info.pluginName.empty() ? "Unknown" : a_info.pluginName;
		label += "|";
		label += GetLocalFormIDString(a_info);
		label += ")";
		return label;
	}

	[[nodiscard]] const char* EffectTypeName(Settings::EffectType a_type)
	{
		switch (a_type) {
		case Settings::EffectType::kCastSpell:
			return "CastSpell";
		case Settings::EffectType::kDispelSpell:
			return "DispelSpell";
		case Settings::EffectType::kAddPerk:
			return "AddPerk";
		case Settings::EffectType::kRemovePerk:
			return "RemovePerk";
		case Settings::EffectType::kPlaySound:
			return "PlaySound";
		case Settings::EffectType::kPlayShaderEffect:
			return "PlayShaderEffect";
		case Settings::EffectType::kModifyResource:
			return "ModifyResource";
		default:
			return "CastSpell";
		}
	}

	[[nodiscard]] Settings::EffectType EffectTypeFromName(std::string_view a_name)
	{
		if (a_name == "DispelSpell") return Settings::EffectType::kDispelSpell;
		if (a_name == "AddPerk") return Settings::EffectType::kAddPerk;
		if (a_name == "RemovePerk") return Settings::EffectType::kRemovePerk;
		if (a_name == "PlaySound") return Settings::EffectType::kPlaySound;
		if (a_name == "PlayShaderEffect") return Settings::EffectType::kPlayShaderEffect;
		if (a_name == "ModifyResource") return Settings::EffectType::kModifyResource;
		return Settings::EffectType::kCastSpell;
	}

	[[nodiscard]] const char* ResourceTypeName(Settings::ResourceType a_type)
	{
		switch (a_type) {
		case Settings::ResourceType::kHealth:
			return "Health";
		case Settings::ResourceType::kMagicka:
			return "Magicka";
		case Settings::ResourceType::kStamina:
		default:
			return "Stamina";
		}
	}

	[[nodiscard]] Settings::ResourceType ResourceTypeFromName(std::string_view a_name)
	{
		if (a_name == "Health") return Settings::ResourceType::kHealth;
		if (a_name == "Magicka") return Settings::ResourceType::kMagicka;
		return Settings::ResourceType::kStamina;
	}

	[[nodiscard]] const char* ResourceOperationName(Settings::ResourceOperation a_operation)
	{
		return a_operation == Settings::ResourceOperation::kGain ? "Gain" : "Consume";
	}

	[[nodiscard]] Settings::ResourceOperation ResourceOperationFromName(std::string_view a_name)
	{
		return a_name == "Gain" ? Settings::ResourceOperation::kGain : Settings::ResourceOperation::kConsume;
	}

	[[nodiscard]] const char* ValueSourceName(Settings::ValueSource a_source)
	{
		switch (a_source) {
		case Settings::ValueSource::kGlobal:
			return "Global";
		case Settings::ValueSource::kActorValue:
			return "ActorValue";
		case Settings::ValueSource::kFixed:
		default:
			return "Fixed";
		}
	}

	[[nodiscard]] Settings::ValueSource ValueSourceFromName(std::string_view a_name)
	{
		if (a_name == "Global") return Settings::ValueSource::kGlobal;
		if (a_name == "ActorValue") return Settings::ValueSource::kActorValue;
		return Settings::ValueSource::kFixed;
	}

	[[nodiscard]] const char* RuleTargetName(Settings::RuleTarget a_target)
	{
		switch (a_target) {
		case Settings::RuleTarget::kPlayer:
			return "Player";
		case Settings::RuleTarget::kCombatTarget:
			return "CombatTarget";
		case Settings::RuleTarget::kEventActor:
		default:
			return "EventActor";
		}
	}

	[[nodiscard]] Settings::RuleTarget RuleTargetFromName(std::string_view a_name)
	{
		if (a_name == "Player") return Settings::RuleTarget::kPlayer;
		if (a_name == "CombatTarget") return Settings::RuleTarget::kCombatTarget;
		return Settings::RuleTarget::kEventActor;
	}

	[[nodiscard]] const char* CooldownScopeName(Settings::CooldownScope a_scope)
	{
		return a_scope == Settings::CooldownScope::kGlobal ? "Global" : "PerActor";
	}

	[[nodiscard]] Settings::CooldownScope CooldownScopeFromName(std::string_view a_name)
	{
		return a_name == "Global" ? Settings::CooldownScope::kGlobal : Settings::CooldownScope::kPerActor;
	}

	[[nodiscard]] const char* ConditionTypeName(Settings::ConditionType a_type)
	{
		switch (a_type) {
		case Settings::ConditionType::kKeyword:
			return "Keyword";
		case Settings::ConditionType::kFaction:
			return "Faction";
		case Settings::ConditionType::kEquippedWeapon:
			return "EquippedWeapon";
		case Settings::ConditionType::kActorValue:
			return "ActorValue";
		case Settings::ConditionType::kRace:
		default:
			return "Race";
		}
	}

	[[nodiscard]] Settings::ConditionType ConditionTypeFromName(std::string_view a_name)
	{
		if (a_name == "Keyword") return Settings::ConditionType::kKeyword;
		if (a_name == "Faction") return Settings::ConditionType::kFaction;
		if (a_name == "EquippedWeapon") return Settings::ConditionType::kEquippedWeapon;
		if (a_name == "ActorValue") return Settings::ConditionType::kActorValue;
		return Settings::ConditionType::kRace;
	}

	[[nodiscard]] const char* ComparisonOperatorName(Settings::ComparisonOperator a_comparison)
	{
		switch (a_comparison) {
		case Settings::ComparisonOperator::kGreater:
			return "Greater";
		case Settings::ComparisonOperator::kLess:
			return "Less";
		case Settings::ComparisonOperator::kLessOrEqual:
			return "LessOrEqual";
		case Settings::ComparisonOperator::kEqual:
			return "Equal";
		case Settings::ComparisonOperator::kBetween:
			return "Between";
		case Settings::ComparisonOperator::kGreaterOrEqual:
		default:
			return "GreaterOrEqual";
		}
	}

	[[nodiscard]] Settings::ComparisonOperator ComparisonOperatorFromName(std::string_view a_name)
	{
		if (a_name == "Greater") return Settings::ComparisonOperator::kGreater;
		if (a_name == "Less") return Settings::ComparisonOperator::kLess;
		if (a_name == "LessOrEqual") return Settings::ComparisonOperator::kLessOrEqual;
		if (a_name == "Equal") return Settings::ComparisonOperator::kEqual;
		if (a_name == "Between") return Settings::ComparisonOperator::kBetween;
		return Settings::ComparisonOperator::kGreaterOrEqual;
	}

	[[nodiscard]] RE::ActorValue ResourceToActorValue(Settings::ResourceType a_resource)
	{
		switch (a_resource) {
		case Settings::ResourceType::kHealth:
			return RE::ActorValue::kHealth;
		case Settings::ResourceType::kMagicka:
			return RE::ActorValue::kMagicka;
		case Settings::ResourceType::kStamina:
		default:
			return RE::ActorValue::kStamina;
		}
	}

	[[nodiscard]] std::string SanitizeRuleFileName(const std::string& a_ruleName)
	{
		std::string safeName;
		safeName.reserve(a_ruleName.size());

		for (const char ch : a_ruleName) {
			const auto c = static_cast<unsigned char>(ch);
			safeName.push_back(std::isalnum(c) || ch == ' ' || ch == '_' || ch == '-' ? ch : '_');
		}

		while (!safeName.empty() && (safeName.back() == ' ' || safeName.back() == '.')) {
			safeName.pop_back();
		}

		if (safeName.empty() || safeName == "." || safeName == "..") {
			safeName = "New Rule";
		}

		return safeName + ".json";
	}

	[[nodiscard]] std::string MakeUniqueRuleName(std::string a_name, const std::size_t a_currentIndex)
	{
		if (a_name.empty()) {
			a_name = "New Rule";
		}

		auto exists = [&](const std::string& candidate) {
			for (std::size_t i = 0; i < Settings::Rules.size(); ++i) {
				if (i != a_currentIndex && Settings::Rules[i].name == candidate) {
					return true;
				}
			}
			return false;
		};

		if (!exists(a_name)) {
			return a_name;
		}

		const auto baseName = a_name;
		for (int suffix = 2; suffix < 10000; ++suffix) {
			const auto candidate = baseName + " " + std::to_string(suffix);
			if (!exists(candidate)) {
				return candidate;
			}
		}

		return baseName + " " + std::to_string(Settings::Rules.size() + 1);
	}

	void NormalizeRuleNames()
	{
		for (std::size_t i = 0; i < Settings::Rules.size(); ++i) {
			Settings::Rules[i].name = MakeUniqueRuleName(Settings::Rules[i].name, i);
		}
	}

	[[nodiscard]] std::string SerializeFormID(RE::FormID a_formID)
	{
		if (a_formID == 0) {
			return "";
		}

		if (auto* form = RE::TESForm::LookupByID(a_formID)) {
			return FormUtil::NormalizeFormID(form);
		}

		return std::format("{:X}", a_formID);
	}

	[[nodiscard]] RE::FormID ReadFormID(const rapidjson::Value& a_parent, const char* a_name)
	{
		if (!a_parent.HasMember(a_name)) {
			return 0;
		}

		const auto& value = a_parent[a_name];
		if (value.IsString()) {
			const std::string formString = value.GetString();
			if (formString.empty()) {
				return 0;
			}
			try {
				return FormUtil::FormIDFromString(formString);
			} catch (...) {
				logger::warn("[EventListener] Invalid FormID string '{}'.", formString);
				return 0;
			}
		}

		return 0;
	}

	void WriteFormID(rapidjson::Value& a_parent, rapidjson::Document::AllocatorType& a_allocator, const char* a_name, RE::FormID a_formID)
	{
		const auto formString = SerializeFormID(a_formID);
		rapidjson::Value value;
		value.SetString(formString.c_str(), static_cast<rapidjson::SizeType>(formString.size()), a_allocator);
		a_parent.AddMember(rapidjson::Value(a_name, a_allocator).Move(), value, a_allocator);
	}

	void ReadExecutionValueSource(const rapidjson::Value& a_parent, const char* a_prefix,
		Settings::ValueSource& a_source, RE::FormID& a_global, std::string& a_actorValue)
	{
		const auto sourceKey = std::string(a_prefix) + "Source";
		const auto globalKey = std::string(a_prefix) + "Global";
		const auto actorValueKey = std::string(a_prefix) + "ActorValue";
		if (a_parent.HasMember(sourceKey.c_str()) && a_parent[sourceKey.c_str()].IsString()) {
			a_source = ValueSourceFromName(a_parent[sourceKey.c_str()].GetString());
		}
		a_global = ReadFormID(a_parent, globalKey.c_str());
		if (a_parent.HasMember(actorValueKey.c_str()) && a_parent[actorValueKey.c_str()].IsString()) {
			a_actorValue = a_parent[actorValueKey.c_str()].GetString();
		}
	}

	void WriteExecutionValueSource(rapidjson::Value& a_parent,
		rapidjson::Document::AllocatorType& a_allocator, const char* a_prefix,
		Settings::ValueSource a_source, RE::FormID a_global, const std::string& a_actorValue)
	{
		const auto sourceKey = std::string(a_prefix) + "Source";
		const auto globalKey = std::string(a_prefix) + "Global";
		const auto actorValueKey = std::string(a_prefix) + "ActorValue";
		a_parent.AddMember(rapidjson::Value(sourceKey.c_str(), a_allocator).Move(),
			rapidjson::Value(ValueSourceName(a_source), a_allocator).Move(), a_allocator);
		WriteFormID(a_parent, a_allocator, globalKey.c_str(), a_global);
		a_parent.AddMember(rapidjson::Value(actorValueKey.c_str(), a_allocator).Move(),
			rapidjson::Value(a_actorValue.c_str(), a_allocator).Move(), a_allocator);
	}

	void LoadLanguage()
	{
		g_langMap.clear();
		WriteDefaultLanguageFile();

		std::ifstream file(kLanguagePath, std::ios::binary);
		if (!file.is_open()) {
			return;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string json = buffer.str();
		if (json.size() >= 3 && static_cast<unsigned char>(json[0]) == 0xEF && static_cast<unsigned char>(json[1]) == 0xBB && static_cast<unsigned char>(json[2]) == 0xBF) {
			json.erase(0, 3);
		}

		rapidjson::Document doc;
		doc.Parse(json.c_str());
		if (!doc.IsObject()) {
			return;
		}

		for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
			if (it->value.IsObject()) {
				const std::string category = it->name.GetString();
				for (auto child = it->value.MemberBegin(); child != it->value.MemberEnd(); ++child) {
					if (child->value.IsString()) {
						g_langMap[category + "." + child->name.GetString()] = child->value.GetString();
					}
				}
			} else if (it->value.IsString()) {
				g_langMap[it->name.GetString()] = it->value.GetString();
			}
		}
	}

	[[nodiscard]] Settings::RuleCondition ReadCondition(const rapidjson::Value& a_value)
	{
		Settings::RuleCondition condition;
		if (!a_value.IsObject()) {
			return condition;
		}

		if (a_value.HasMember("type") && a_value["type"].IsString()) {
			condition.type = ConditionTypeFromName(a_value["type"].GetString());
		}
		condition.formID = ReadFormID(a_value, "form");
		if (a_value.HasMember("negate") && a_value["negate"].IsBool()) {
			condition.negate = a_value["negate"].GetBool();
		}
		if (a_value.HasMember("actorValue") && a_value["actorValue"].IsString()) {
			condition.actorValue = a_value["actorValue"].GetString();
		}
		if (a_value.HasMember("comparison") && a_value["comparison"].IsString()) {
			condition.comparison = ComparisonOperatorFromName(a_value["comparison"].GetString());
		}
		if (a_value.HasMember("value") && a_value["value"].IsNumber()) {
			condition.value = a_value["value"].GetFloat();
		}
		if (a_value.HasMember("maximumValue") && a_value["maximumValue"].IsNumber()) {
			condition.maximumValue = a_value["maximumValue"].GetFloat();
		}
		return condition;
	}

	void WriteCondition(
		rapidjson::Value& a_array,
		rapidjson::Document::AllocatorType& a_allocator,
		const Settings::RuleCondition& a_condition)
	{
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("type", rapidjson::Value(ConditionTypeName(a_condition.type), a_allocator).Move(), a_allocator);
		WriteFormID(value, a_allocator, "form", a_condition.formID);
		value.AddMember("negate", a_condition.negate, a_allocator);
		value.AddMember("actorValue", rapidjson::Value(a_condition.actorValue.c_str(), a_allocator).Move(), a_allocator);
		value.AddMember("comparison", rapidjson::Value(ComparisonOperatorName(a_condition.comparison), a_allocator).Move(), a_allocator);
		value.AddMember("value", a_condition.value, a_allocator);
		value.AddMember("maximumValue", a_condition.maximumValue, a_allocator);
		a_array.PushBack(value, a_allocator);
	}

	[[nodiscard]] Settings::RuleEffect ReadEffect(const rapidjson::Value& a_value)
	{
		Settings::RuleEffect effect;
		if (!a_value.IsObject()) {
			return effect;
		}

		if (a_value.HasMember("type") && a_value["type"].IsString()) {
			effect.type = EffectTypeFromName(a_value["type"].GetString());
		}
		effect.formID = ReadFormID(a_value, "form");
		effect.globalID = ReadFormID(a_value, "global");

		if (a_value.HasMember("operation") && a_value["operation"].IsString()) {
			effect.resourceOperation = ResourceOperationFromName(a_value["operation"].GetString());
		}
		if (a_value.HasMember("resource") && a_value["resource"].IsString()) {
			effect.resource = ResourceTypeFromName(a_value["resource"].GetString());
		}
		if (a_value.HasMember("valueSource") && a_value["valueSource"].IsString()) {
			effect.valueSource = ValueSourceFromName(a_value["valueSource"].GetString());
		}
		if (a_value.HasMember("fixedValue") && a_value["fixedValue"].IsNumber()) {
			effect.fixedValue = std::max(0.0F, a_value["fixedValue"].GetFloat());
		}
		if (a_value.HasMember("actorValue") && a_value["actorValue"].IsString()) {
			effect.actorValue = a_value["actorValue"].GetString();
		}
		if (a_value.HasMember("duration") && a_value["duration"].IsNumber()) {
			effect.duration = std::max(0.0F, a_value["duration"].GetFloat());
		}
		if (a_value.HasMember("requireSpellCost") && a_value["requireSpellCost"].IsBool()) {
			effect.requireSpellCost = a_value["requireSpellCost"].GetBool();
		}

		return effect;
	}

	void WriteEffect(rapidjson::Value& a_array, rapidjson::Document::AllocatorType& a_allocator, const Settings::RuleEffect& a_effect)
	{
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("type", rapidjson::Value(EffectTypeName(a_effect.type), a_allocator).Move(), a_allocator);
		WriteFormID(value, a_allocator, "form", a_effect.formID);
		value.AddMember("operation", rapidjson::Value(ResourceOperationName(a_effect.resourceOperation), a_allocator).Move(), a_allocator);
		value.AddMember("resource", rapidjson::Value(ResourceTypeName(a_effect.resource), a_allocator).Move(), a_allocator);
		value.AddMember("valueSource", rapidjson::Value(ValueSourceName(a_effect.valueSource), a_allocator).Move(), a_allocator);
		value.AddMember("fixedValue", a_effect.fixedValue, a_allocator);
		WriteFormID(value, a_allocator, "global", a_effect.globalID);
		value.AddMember("actorValue", rapidjson::Value(a_effect.actorValue.c_str(), a_allocator).Move(), a_allocator);
		value.AddMember("duration", a_effect.duration, a_allocator);
		value.AddMember("requireSpellCost", a_effect.requireSpellCost, a_allocator);
		a_array.PushBack(value, a_allocator);
	}

	[[nodiscard]] std::optional<Settings::Rule> ReadRuleFile(const std::filesystem::path& a_path)
	{
		FILE* file = nullptr;
		fopen_s(&file, a_path.string().c_str(), "rb");
		if (!file) {
			return std::nullopt;
		}

		char readBuffer[65536];
		rapidjson::FileReadStream stream(file, readBuffer, sizeof(readBuffer));
		rapidjson::Document doc;
		doc.ParseStream(stream);
		fclose(file);

		if (!doc.IsObject()) {
			logger::warn("[EventListener] Rule file '{}' is not a JSON object.", a_path.string());
			return std::nullopt;
		}

		Settings::Rule rule;
		rule.sourceFileName = a_path.filename().string();

		if (doc.HasMember("enabled") && doc["enabled"].IsBool()) {
			rule.enabled = doc["enabled"].GetBool();
		}
		if (doc.HasMember("name") && doc["name"].IsString()) {
			rule.name = doc["name"].GetString();
		} else {
			rule.name = a_path.stem().string();
		}
		if (doc.HasMember("eventName") && doc["eventName"].IsString()) {
			rule.eventName = doc["eventName"].GetString();
		}
		if (doc.HasMember("affectPlayer") && doc["affectPlayer"].IsBool()) {
			rule.affectPlayer = doc["affectPlayer"].GetBool();
		}
		rule.requiredPerk = ReadFormID(doc, "requiredPerk");
		if (doc.HasMember("target") && doc["target"].IsString()) {
			rule.target = RuleTargetFromName(doc["target"].GetString());
		}
		if (doc.HasMember("chance") && doc["chance"].IsNumber()) {
			rule.chance = std::clamp(doc["chance"].GetFloat(), 0.0F, 100.0F);
		}
		if (doc.HasMember("cooldownSeconds") && doc["cooldownSeconds"].IsNumber()) {
			rule.cooldownSeconds = std::max(0.0F, doc["cooldownSeconds"].GetFloat());
		}
		if (doc.HasMember("cooldownScope") && doc["cooldownScope"].IsString()) {
			rule.cooldownScope = CooldownScopeFromName(doc["cooldownScope"].GetString());
		}
		if (doc.HasMember("maxActivations") && doc["maxActivations"].IsUint()) {
			rule.maxActivations = doc["maxActivations"].GetUint();
		}
		ReadExecutionValueSource(doc, "chance", rule.chanceSource, rule.chanceGlobal, rule.chanceActorValue);
		ReadExecutionValueSource(doc, "cooldown", rule.cooldownSource, rule.cooldownGlobal, rule.cooldownActorValue);
		ReadExecutionValueSource(doc, "maxActivations", rule.maxActivationsSource, rule.maxActivationsGlobal, rule.maxActivationsActorValue);

		if (doc.HasMember("conditions") && doc["conditions"].IsArray()) {
			for (const auto& condition : doc["conditions"].GetArray()) {
				rule.conditions.push_back(ReadCondition(condition));
			}
		}

		if (doc.HasMember("effects") && doc["effects"].IsArray()) {
			for (const auto& effect : doc["effects"].GetArray()) {
				rule.effects.push_back(ReadEffect(effect));
			}
		}

		return rule;
	}

	void WriteRuleFile(Settings::Rule& a_rule)
	{
		std::filesystem::create_directories(kRulesDir);

		const auto fileName = SanitizeRuleFileName(a_rule.name);
		const auto path = std::filesystem::path(kRulesDir) / fileName;

		if (!a_rule.sourceFileName.empty() && a_rule.sourceFileName != fileName) {
			std::error_code ec;
			std::filesystem::remove(std::filesystem::path(kRulesDir) / a_rule.sourceFileName, ec);
		}

		rapidjson::Document doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		doc.AddMember("enabled", a_rule.enabled, allocator);
		doc.AddMember("name", rapidjson::Value(a_rule.name.c_str(), allocator).Move(), allocator);
		doc.AddMember("eventName", rapidjson::Value(a_rule.eventName.c_str(), allocator).Move(), allocator);
		doc.AddMember("affectPlayer", a_rule.affectPlayer, allocator);
		WriteFormID(doc, allocator, "requiredPerk", a_rule.requiredPerk);
		doc.AddMember("target", rapidjson::Value(RuleTargetName(a_rule.target), allocator).Move(), allocator);
		doc.AddMember("chance", a_rule.chance, allocator);
		doc.AddMember("cooldownSeconds", a_rule.cooldownSeconds, allocator);
		doc.AddMember("cooldownScope", rapidjson::Value(CooldownScopeName(a_rule.cooldownScope), allocator).Move(), allocator);
		doc.AddMember("maxActivations", a_rule.maxActivations, allocator);
		WriteExecutionValueSource(doc, allocator, "chance", a_rule.chanceSource, a_rule.chanceGlobal, a_rule.chanceActorValue);
		WriteExecutionValueSource(doc, allocator, "cooldown", a_rule.cooldownSource, a_rule.cooldownGlobal, a_rule.cooldownActorValue);
		WriteExecutionValueSource(doc, allocator, "maxActivations", a_rule.maxActivationsSource, a_rule.maxActivationsGlobal, a_rule.maxActivationsActorValue);

		rapidjson::Value conditions(rapidjson::kArrayType);
		for (const auto& condition : a_rule.conditions) {
			WriteCondition(conditions, allocator, condition);
		}
		doc.AddMember("conditions", conditions, allocator);

		rapidjson::Value effects(rapidjson::kArrayType);
		for (const auto& effect : a_rule.effects) {
			WriteEffect(effects, allocator, effect);
		}
		doc.AddMember("effects", effects, allocator);

		FILE* file = nullptr;
		fopen_s(&file, path.string().c_str(), "wb");
		if (!file) {
			logger::warn("[EventListener] Could not save rule '{}'.", path.string());
			return;
		}

		char writeBuffer[65536];
		rapidjson::FileWriteStream stream(file, writeBuffer, sizeof(writeBuffer));
		rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(stream);
		doc.Accept(writer);
		fclose(file);

		a_rule.sourceFileName = fileName;
	}

	void DeleteRuleFile(const Settings::Rule& a_rule)
	{
		if (a_rule.sourceFileName.empty()) {
			return;
		}

		std::error_code ec;
		std::filesystem::remove(std::filesystem::path(kRulesDir) / a_rule.sourceFileName, ec);
	}

	void SaveSettingsUnlocked()
	{
		std::filesystem::create_directories(kModDir);
		std::filesystem::create_directories(kRulesDir);
		NormalizeRuleNames();

		for (auto& rule : Settings::Rules) {
			WriteRuleFile(rule);
		}
	}

	[[nodiscard]] bool DrawStringInput(const char* a_label, std::string& a_value, std::size_t a_size = 128)
	{
		std::vector<char> buffer(a_size, '\0');
		strcpy_s(buffer.data(), buffer.size(), a_value.c_str());
		if (ImGui::InputText(a_label, buffer.data(), buffer.size())) {
			a_value = buffer.data();
			return true;
		}
		return false;
	}

	[[nodiscard]] bool DrawFloatSliderWithInput(const char* a_label, float& a_value, float a_min, float a_max)
	{
		bool changed = false;
		ImGui::PushID(a_label);
		ImGui::SetNextItemWidth(180.0F);
		if (ImGui::SliderFloat("##slider", &a_value, a_min, a_max, "%.2f")) {
			changed = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(180.0F);
		if (ImGui::InputFloat(a_label, &a_value, 0.0F, 0.0F, "%.2f")) {
			changed = true;
		}
		a_value = std::clamp(a_value, a_min, a_max);
		ImGui::PopID();
		return changed;
	}

	[[nodiscard]] bool DrawIntSliderWithInput(
		const char* a_label,
		std::uint32_t& a_value,
		int a_min,
		int a_max)
	{
		auto value = static_cast<int>(std::min<std::uint32_t>(a_value, static_cast<std::uint32_t>(a_max)));
		bool changed = false;
		ImGui::PushID(a_label);
		ImGui::SetNextItemWidth(180.0F);
		if (ImGui::SliderInt("##slider", &value, a_min, a_max)) {
			changed = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(180.0F);
		if (ImGui::InputInt(a_label, &value)) {
			changed = true;
		}
		value = std::clamp(value, a_min, a_max);
		a_value = static_cast<std::uint32_t>(value);
		ImGui::PopID();
		return changed;
	}

	[[nodiscard]] bool DrawFloatInput(const char* a_label, float& a_value)
	{
		ImGui::SetNextItemWidth(240.0F);
		return ImGui::InputFloat(a_label, &a_value, 0.0F, 0.0F, "%.2f");
	}

	bool DrawDropdown(const char* a_label, const std::string& a_category, RE::FormID& a_currentFormID, float a_customWidth = 560.0F)
	{
		const auto& fullList = ListManager::GetSingleton()->GetList(a_category);
		if (fullList.empty()) {
			ImGui::TextDisabled("%s: %s", a_label, GetLoc("menu.form_list_empty", "No forms loaded"));
			return false;
		}

		bool changed = false;
		std::vector<std::string> comboItems;
		std::vector<int> mapToFull;
		comboItems.reserve(fullList.size() + 1);
		mapToFull.reserve(fullList.size() + 1);

		comboItems.emplace_back(GetLoc("common.none", "None"));
		mapToFull.push_back(-1);

		int localSelection = 0;
		for (std::size_t i = 0; i < fullList.size(); ++i) {
			comboItems.push_back(GetFormDropdownLabel(fullList[i]));
			mapToFull.push_back(static_cast<int>(i));
			if (fullList[i].formID == a_currentFormID) {
				localSelection = static_cast<int>(i) + 1;
			}
		}

		ImGui::PushID(a_label);
		std::string displayLabel = a_label;
		if (const auto hashPos = displayLabel.find("##"); hashPos != std::string::npos) {
			displayLabel = displayLabel.substr(0, hashPos);
		}

		ImGui::Text("%s:", displayLabel.c_str());
		ImGui::SameLine();
		if (a_customWidth > 0.0F) {
			ImGui::SetNextItemWidth(a_customWidth);
		}

		if (ImGui::BeginCombo("##form_dropdown", comboItems[localSelection].c_str())) {
			static std::map<std::string, std::string> searchBuffers;
			char searchBuf[256] = "";
			if (const auto it = searchBuffers.find(a_label); it != searchBuffers.end()) {
				strcpy_s(searchBuf, it->second.c_str());
			}

			ImGui::SetNextItemWidth(-1.0F);
			if (ImGui::InputText("##form_search", searchBuf, sizeof(searchBuf))) {
				searchBuffers[a_label] = searchBuf;
			}
			ImGui::Separator();

			const std::string searchLower = ToLower(searchBuf);
			ImGui::BeginChild("##form_scroll", { 0, 200 }, false);
			for (int i = 0; i < static_cast<int>(comboItems.size()); ++i) {
				if (!searchLower.empty() && ToLower(comboItems[i]).find(searchLower) == std::string::npos) {
					continue;
				}

				const bool selected = localSelection == i;
				if (ImGui::Selectable(comboItems[i].c_str(), selected)) {
					const int originalIndex = mapToFull[i];
					a_currentFormID = originalIndex == -1 ? 0 : fullList[originalIndex].formID;
					searchBuffers[a_label].clear();
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndChild();
			ImGui::EndCombo();
		}

		ImGui::PopID();
		return changed;
	}

	[[nodiscard]] const char* RuleTargetLabel(Settings::RuleTarget a_target)
	{
		switch (a_target) {
		case Settings::RuleTarget::kPlayer:
			return GetLoc("menu.target_player", "Player");
		case Settings::RuleTarget::kCombatTarget:
			return GetLoc("menu.target_combat_target", "Combat Target");
		case Settings::RuleTarget::kEventActor:
		default:
			return GetLoc("menu.target_event_actor", "Event Actor");
		}
	}

	bool DrawRuleTargetDropdown(Settings::Rule& a_rule)
	{
		constexpr std::array targets = {
			Settings::RuleTarget::kEventActor,
			Settings::RuleTarget::kPlayer,
			Settings::RuleTarget::kCombatTarget
		};

		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.rule_target", "Target"), RuleTargetLabel(a_rule.target))) {
			for (const auto target : targets) {
				const bool selected = a_rule.target == target;
				if (ImGui::Selectable(RuleTargetLabel(target), selected)) {
					a_rule.target = target;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	[[nodiscard]] const char* CooldownScopeLabel(Settings::CooldownScope a_scope)
	{
		return a_scope == Settings::CooldownScope::kGlobal ?
			GetLoc("menu.cooldown_global", "Global") :
			GetLoc("menu.cooldown_per_actor", "Per Actor");
	}

	bool DrawCooldownScopeDropdown(Settings::Rule& a_rule)
	{
		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.cooldown_scope", "Cooldown Scope"), CooldownScopeLabel(a_rule.cooldownScope))) {
			for (const auto scope : { Settings::CooldownScope::kGlobal, Settings::CooldownScope::kPerActor }) {
				const bool selected = a_rule.cooldownScope == scope;
				if (ImGui::Selectable(CooldownScopeLabel(scope), selected)) {
					a_rule.cooldownScope = scope;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	[[nodiscard]] const char* ConditionTypeLabel(Settings::ConditionType a_type)
	{
		switch (a_type) {
		case Settings::ConditionType::kKeyword:
			return GetLoc("menu.condition_keyword", "Keyword");
		case Settings::ConditionType::kFaction:
			return GetLoc("menu.condition_faction", "Faction");
		case Settings::ConditionType::kEquippedWeapon:
			return GetLoc("menu.condition_equipped_weapon", "Equipped Weapon");
		case Settings::ConditionType::kActorValue:
			return GetLoc("menu.condition_actor_value", "Actor Value");
		case Settings::ConditionType::kRace:
		default:
			return GetLoc("menu.condition_race", "Race");
		}
	}

	bool DrawConditionTypeDropdown(Settings::RuleCondition& a_condition)
	{
		constexpr std::array types = {
			Settings::ConditionType::kRace,
			Settings::ConditionType::kKeyword,
			Settings::ConditionType::kFaction,
			Settings::ConditionType::kEquippedWeapon,
			Settings::ConditionType::kActorValue
		};

		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.condition_type", "Condition"), ConditionTypeLabel(a_condition.type))) {
			for (const auto type : types) {
				const bool selected = a_condition.type == type;
				if (ImGui::Selectable(ConditionTypeLabel(type), selected)) {
					a_condition.type = type;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	[[nodiscard]] const char* ComparisonOperatorLabel(Settings::ComparisonOperator a_comparison)
	{
		switch (a_comparison) {
		case Settings::ComparisonOperator::kGreater:
			return GetLoc("menu.comparison_greater", "Greater Than");
		case Settings::ComparisonOperator::kLess:
			return GetLoc("menu.comparison_less", "Less Than");
		case Settings::ComparisonOperator::kLessOrEqual:
			return GetLoc("menu.comparison_less_equal", "Less Than or Equal");
		case Settings::ComparisonOperator::kEqual:
			return GetLoc("menu.comparison_equal", "Equal");
		case Settings::ComparisonOperator::kBetween:
			return GetLoc("menu.comparison_between", "Between");
		case Settings::ComparisonOperator::kGreaterOrEqual:
		default:
			return GetLoc("menu.comparison_greater_equal", "Greater Than or Equal");
		}
	}

	bool DrawComparisonDropdown(Settings::RuleCondition& a_condition)
	{
		constexpr std::array comparisons = {
			Settings::ComparisonOperator::kGreater,
			Settings::ComparisonOperator::kGreaterOrEqual,
			Settings::ComparisonOperator::kLess,
			Settings::ComparisonOperator::kLessOrEqual,
			Settings::ComparisonOperator::kEqual,
			Settings::ComparisonOperator::kBetween
		};

		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.comparison", "Comparison"), ComparisonOperatorLabel(a_condition.comparison))) {
			for (const auto comparison : comparisons) {
				const bool selected = a_condition.comparison == comparison;
				if (ImGui::Selectable(ComparisonOperatorLabel(comparison), selected)) {
					a_condition.comparison = comparison;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	bool DrawCondition(Settings::RuleCondition& a_condition)
	{
		bool changed = DrawConditionTypeDropdown(a_condition);
		switch (a_condition.type) {
		case Settings::ConditionType::kRace:
			changed |= DrawDropdown(GetLoc("menu.race", "Race"), "Race", a_condition.formID);
			changed |= ImGui::Checkbox(GetLoc("menu.invert_condition", "Invert Condition"), &a_condition.negate);
			break;
		case Settings::ConditionType::kKeyword:
			changed |= DrawDropdown(GetLoc("menu.keyword", "Keyword"), "Keyword", a_condition.formID);
			changed |= ImGui::Checkbox(GetLoc("menu.invert_condition", "Invert Condition"), &a_condition.negate);
			break;
		case Settings::ConditionType::kFaction:
			changed |= DrawDropdown(GetLoc("menu.faction", "Faction"), "Faction", a_condition.formID);
			changed |= ImGui::Checkbox(GetLoc("menu.invert_condition", "Invert Condition"), &a_condition.negate);
			break;
		case Settings::ConditionType::kEquippedWeapon:
			changed |= DrawDropdown(GetLoc("menu.weapon", "Weapon"), "Weapon", a_condition.formID);
			changed |= ImGui::Checkbox(GetLoc("menu.invert_condition", "Invert Condition"), &a_condition.negate);
			break;
		case Settings::ConditionType::kActorValue:
			changed |= DrawStringInput(GetLoc("menu.actor_value", "Actor Value"), a_condition.actorValue, 64);
			changed |= DrawComparisonDropdown(a_condition);
			if (a_condition.comparison == Settings::ComparisonOperator::kBetween) {
				changed |= DrawFloatInput(GetLoc("menu.minimum_value", "Minimum Value"), a_condition.value);
				changed |= DrawFloatInput(GetLoc("menu.maximum_value", "Maximum Value"), a_condition.maximumValue);
			} else {
				changed |= DrawFloatInput(GetLoc("menu.comparison_value", "Value"), a_condition.value);
			}
			if (RE::ActorValueList::LookupActorValueByName(a_condition.actorValue.c_str()) == RE::ActorValue::kNone) {
				ImGui::TextColored({ 1.0F, 0.35F, 0.35F, 1.0F }, "%s", GetLoc("menu.invalid_actor_value", "Invalid Actor Value name"));
			}
			break;
		default:
			break;
		}
		return changed;
	}

	bool DrawEffectTypeDropdown(Settings::RuleEffect& a_effect)
	{
		constexpr std::array types = {
			Settings::EffectType::kCastSpell,
			Settings::EffectType::kDispelSpell,
			Settings::EffectType::kAddPerk,
			Settings::EffectType::kRemovePerk,
			Settings::EffectType::kPlaySound,
			Settings::EffectType::kPlayShaderEffect,
			Settings::EffectType::kModifyResource
		};

		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.effect_type", "Effect"), EffectTypeName(a_effect.type))) {
			for (const auto type : types) {
				const bool selected = a_effect.type == type;
				if (ImGui::Selectable(EffectTypeName(type), selected)) {
					a_effect.type = type;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	bool DrawResourceDropdown(Settings::RuleEffect& a_effect)
	{
		constexpr std::array resources = {
			Settings::ResourceType::kHealth,
			Settings::ResourceType::kMagicka,
			Settings::ResourceType::kStamina
		};

		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.resource", "Resource"), ResourceTypeName(a_effect.resource))) {
			for (const auto resource : resources) {
				const bool selected = a_effect.resource == resource;
				if (ImGui::Selectable(ResourceTypeName(resource), selected)) {
					a_effect.resource = resource;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	bool DrawOperationDropdown(Settings::RuleEffect& a_effect)
	{
		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.operation", "Operation"), ResourceOperationName(a_effect.resourceOperation))) {
			for (const auto operation : { Settings::ResourceOperation::kConsume, Settings::ResourceOperation::kGain }) {
				const bool selected = a_effect.resourceOperation == operation;
				if (ImGui::Selectable(ResourceOperationName(operation), selected)) {
					a_effect.resourceOperation = operation;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	bool DrawValueSourceDropdown(Settings::ValueSource& a_valueSource)
	{
		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.value_source", "Value Source"), ValueSourceName(a_valueSource))) {
			for (const auto source : { Settings::ValueSource::kFixed, Settings::ValueSource::kGlobal, Settings::ValueSource::kActorValue }) {
				const bool selected = a_valueSource == source;
				if (ImGui::Selectable(ValueSourceName(source), selected)) {
					a_valueSource = source;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	template <typename T>
	bool DrawExecutionValue(const char* a_label, T& a_fixedValue, T a_maximum,
		Settings::ValueSource& a_source, RE::FormID& a_global, std::string& a_actorValue)
	{
		ImGui::PushID(a_label);
		bool changed = DrawValueSourceDropdown(a_source);
		ImGui::BeginDisabled(a_source != Settings::ValueSource::kFixed);
		if constexpr (std::is_integral_v<T>) {
			changed |= DrawIntSliderWithInput(a_label, a_fixedValue, 0, static_cast<int>(a_maximum));
		} else {
			changed |= DrawFloatSliderWithInput(a_label, a_fixedValue, 0.0F, a_maximum);
		}
		ImGui::EndDisabled();
		if (a_source == Settings::ValueSource::kGlobal) {
			changed |= DrawDropdown(GetLoc("menu.global", "Global"), "Global", a_global);
		} else if (a_source == Settings::ValueSource::kActorValue) {
			changed |= DrawStringInput(GetLoc("menu.actor_value", "Actor Value"), a_actorValue, 64);
			if (RE::ActorValueList::LookupActorValueByName(a_actorValue.c_str()) == RE::ActorValue::kNone) {
				ImGui::TextColored({ 1.0F, 0.35F, 0.35F, 1.0F }, "%s", GetLoc("menu.invalid_actor_value", "Invalid Actor Value name"));
			}
		}
		ImGui::PopID();
		return changed;
	}

	bool DrawEffect(Settings::RuleEffect& a_effect)
	{
		bool changed = false;
		changed |= DrawEffectTypeDropdown(a_effect);

		switch (a_effect.type) {
		case Settings::EffectType::kCastSpell:
			changed |= DrawDropdown(GetLoc("menu.spell", "Spell"), "Spell", a_effect.formID);
			changed |= ImGui::Checkbox(GetLoc("menu.require_spell_cost", "Only Cast If Resources Are Available"), &a_effect.requireSpellCost);
			break;
		case Settings::EffectType::kDispelSpell:
			changed |= DrawDropdown(GetLoc("menu.spell", "Spell"), "Spell", a_effect.formID);
			break;
		case Settings::EffectType::kAddPerk:
		case Settings::EffectType::kRemovePerk:
			changed |= DrawDropdown(GetLoc("menu.perk", "Perk"), "Perk", a_effect.formID);
			break;
		case Settings::EffectType::kPlaySound:
			changed |= DrawDropdown(GetLoc("menu.sound", "Sound"), "SoundDescriptor", a_effect.formID);
			break;
		case Settings::EffectType::kPlayShaderEffect:
			changed |= DrawDropdown(GetLoc("menu.shader_effect", "Shader Effect"), "EffectShader", a_effect.formID);
			changed |= DrawFloatSliderWithInput(GetLoc("menu.duration", "Duration"), a_effect.duration, 0.0F, 60.0F);
			break;
		case Settings::EffectType::kModifyResource:
			changed |= DrawOperationDropdown(a_effect);
			changed |= DrawResourceDropdown(a_effect);
			changed |= DrawValueSourceDropdown(a_effect.valueSource);
			if (a_effect.valueSource == Settings::ValueSource::kFixed) {
				changed |= DrawFloatSliderWithInput(GetLoc("menu.fixed_value", "Fixed Value"), a_effect.fixedValue, 0.0F, 10000.0F);
			} else if (a_effect.valueSource == Settings::ValueSource::kGlobal) {
				changed |= DrawDropdown(GetLoc("menu.global", "Global"), "Global", a_effect.globalID);
			} else {
				changed |= DrawStringInput(GetLoc("menu.actor_value", "Actor Value"), a_effect.actorValue, 64);
			}
			break;
		default:
			break;
		}

		return changed;
	}

	void RenderRules()
	{
		std::unique_lock lock(Settings::RuleMutex);
		bool changed = false;
		EnsureMenuListsPopulated();
		static std::string ruleNameSearch;
		static std::string eventSearch;
		(void)DrawStringInput(GetLoc("menu.search_rule_name", "Search Rule Name"), ruleNameSearch);
		(void)DrawStringInput(GetLoc("menu.search_event", "Search Event"), eventSearch);
		const auto nameFilter = ToLower(ruleNameSearch);
		const auto eventFilter = ToLower(eventSearch);
		if (ImGui::Button(GetLoc("menu.add_rule", "Add Rule"))) {
			Settings::Rule rule;
			rule.name = MakeUniqueRuleName(rule.name, Settings::Rules.size());
			Settings::Rules.push_back(rule);
			changed = true;
		}

		ImGui::Separator();

		for (std::size_t i = 0; i < Settings::Rules.size();) {
			auto& rule = Settings::Rules[i];
			if (ToLower(rule.name).find(nameFilter) == std::string::npos ||
				ToLower(rule.eventName).find(eventFilter) == std::string::npos) {
				++i;
				continue;
			}
			ImGui::PushID(static_cast<int>(i));
			const std::string header = rule.name + "##rule_" + std::to_string(i);
			if (ImGui::CollapsingHeader(header.c_str())) {
				ImGui::Indent();
				changed |= ImGui::Checkbox(GetLoc("menu.enabled", "Enabled"), &rule.enabled);
				changed |= DrawStringInput(GetLoc("menu.rule_name", "Rule Name"), rule.name, 128);
				changed |= DrawStringInput(GetLoc("menu.event_name", "Event"), rule.eventName, 128);
				changed |= ImGui::Checkbox(GetLoc("menu.affect_player", "Can Affect Player"), &rule.affectPlayer);
				changed |= DrawDropdown(GetLoc("menu.required_perk", "Required Perk"), "Perk", rule.requiredPerk);
				changed |= DrawRuleTargetDropdown(rule);

				ImGui::Spacing();
				ImGui::TextColored({ 0.6F, 0.8F, 1.0F, 1.0F }, "%s", GetLoc("menu.execution", "Execution"));
				changed |= DrawExecutionValue(GetLoc("menu.chance", "Chance (%)"), rule.chance, 100.0F,
					rule.chanceSource, rule.chanceGlobal, rule.chanceActorValue);
				changed |= DrawExecutionValue(GetLoc("menu.cooldown", "Cooldown (seconds)"), rule.cooldownSeconds, 3600.0F,
					rule.cooldownSource, rule.cooldownGlobal, rule.cooldownActorValue);
				if (rule.cooldownSource != Settings::ValueSource::kFixed || rule.cooldownSeconds > 0.0F) {
					changed |= DrawCooldownScopeDropdown(rule);
				}
				changed |= DrawExecutionValue(GetLoc("menu.max_activations_per_day", "Maximum Activations per Day (0 = unlimited)"), rule.maxActivations, std::uint32_t{ 1000000 },
					rule.maxActivationsSource, rule.maxActivationsGlobal, rule.maxActivationsActorValue);

				ImGui::Spacing();
				ImGui::TextColored({ 0.6F, 0.8F, 1.0F, 1.0F }, "%s", GetLoc("menu.conditions", "Conditions"));
				if (ImGui::Button(GetLoc("menu.add_condition", "Add Condition"))) {
					rule.conditions.push_back({});
					changed = true;
				}

				for (std::size_t conditionIndex = 0; conditionIndex < rule.conditions.size();) {
					ImGui::PushID(static_cast<int>(conditionIndex));
					const std::string conditionHeader = std::string(ConditionTypeLabel(rule.conditions[conditionIndex].type)) +
						"##condition_" + std::to_string(conditionIndex);
					if (ImGui::TreeNode(conditionHeader.c_str())) {
						changed |= DrawCondition(rule.conditions[conditionIndex]);
						if (ImGui::Button(GetLoc("common.remove", "Remove"))) {
							rule.conditions.erase(rule.conditions.begin() + static_cast<std::ptrdiff_t>(conditionIndex));
							changed = true;
							ImGui::TreePop();
							ImGui::PopID();
							continue;
						}
						ImGui::TreePop();
					}
					ImGui::PopID();
					++conditionIndex;
				}

				ImGui::Spacing();
				ImGui::TextColored({ 0.6F, 0.8F, 1.0F, 1.0F }, "%s", GetLoc("menu.effects", "Effects"));
				if (ImGui::Button(GetLoc("menu.add_effect", "Add Effect"))) {
					rule.effects.push_back({});
					changed = true;
				}

				for (std::size_t effectIndex = 0; effectIndex < rule.effects.size();) {
					ImGui::PushID(static_cast<int>(effectIndex));
					const std::string effectHeader = std::string(EffectTypeName(rule.effects[effectIndex].type)) + "##effect_" + std::to_string(effectIndex);
					if (ImGui::TreeNode(effectHeader.c_str())) {
						changed |= DrawEffect(rule.effects[effectIndex]);
						if (ImGui::Button(GetLoc("common.remove", "Remove"))) {
							rule.effects.erase(rule.effects.begin() + static_cast<std::ptrdiff_t>(effectIndex));
							changed = true;
							ImGui::TreePop();
							ImGui::PopID();
							continue;
						}
						ImGui::TreePop();
					}
					ImGui::PopID();
					++effectIndex;
				}

				ImGui::Spacing();
				if (ImGui::Button(GetLoc("menu.delete_rule", "Delete Rule"))) {
					DeleteRuleFile(rule);
					Settings::Rules.erase(Settings::Rules.begin() + static_cast<std::ptrdiff_t>(i));
					changed = true;
					ImGui::Unindent();
					ImGui::PopID();
					continue;
				}
				ImGui::Unindent();
			}
			ImGui::PopID();
			++i;
		}

		if (changed) {
			SaveSettingsUnlocked();
		}
	}

	void RenderDebug()
	{
		auto* manager = ListManager::GetSingleton();

		ImGui::Text("%s", GetLoc("debug.title", "Debug Tools"));
		ImGui::Separator();
		ImGui::TextWrapped("%s", GetLoc("debug.reload_data_hint", "Refreshes the internal form database. Use this after dynamic form mods have injected or updated forms."));
		ImGui::Text("Form database populated: %s", manager && manager->IsPopulated() ? "yes" : "no");
		ImGui::Separator();

		if (ImGui::Button(GetLoc("debug.reload_data", "Reload Data"))) {
			if (manager) {
				const bool reloaded = manager->PopulateAllLists(true);
				logger::debug("[DebugMenu] Reload Data clicked; refresh result: {}.", reloaded);
			} else {
				logger::debug("[DebugMenu] Reload Data clicked but manager is null.");
			}
		}
	}

	[[nodiscard]] float ResolveEffectValue(RE::Actor* a_actor, const Settings::RuleEffect& a_effect)
	{
		switch (a_effect.valueSource) {
		case Settings::ValueSource::kGlobal:
			if (auto* global = RE::TESForm::LookupByID<RE::TESGlobal>(a_effect.globalID)) {
				return std::max(0.0F, global->value);
			}
			return 0.0F;
		case Settings::ValueSource::kActorValue:
			if (a_actor) {
				const auto actorValue = RE::ActorValueList::LookupActorValueByName(a_effect.actorValue.c_str());
				if (actorValue != RE::ActorValue::kNone) {
					if (const auto owner = a_actor->AsActorValueOwner()) {
						return std::max(0.0F, owner->GetActorValue(actorValue));
					}
				}
			}
			return 0.0F;
		case Settings::ValueSource::kFixed:
		default:
			return std::max(0.0F, a_effect.fixedValue);
		}
	}

	[[nodiscard]] float ResolveExecutionValue(float a_fixedValue, Settings::ValueSource a_source,
		RE::FormID a_global, const std::string& a_actorValue, RE::Actor* a_actor)
	{
		float value = a_fixedValue;
		if (a_source == Settings::ValueSource::kGlobal) {
			if (const auto* global = RE::TESForm::LookupByID<RE::TESGlobal>(a_global)) {
				value = global->value;
			}
		} else if (a_source == Settings::ValueSource::kActorValue && a_actor) {
			const auto actorValue = RE::ActorValueList::LookupActorValueByName(a_actorValue.c_str());
			if (const auto* owner = a_actor->AsActorValueOwner(); owner && actorValue != RE::ActorValue::kNone) {
				value = owner->GetActorValue(actorValue);
			}
		}
		if (!std::isfinite(value)) {
			value = std::isfinite(a_fixedValue) ? a_fixedValue : 0.0F;
		}
		return std::max(0.0F, value);
	}

	[[nodiscard]] bool SpendSpellCostIfAvailable(RE::Actor* a_actor, RE::SpellItem* a_spell)
	{
		if (!a_actor || !a_spell) {
			return false;
		}

		if (a_actor == RE::PlayerCharacter::GetSingleton() && RE::PlayerCharacter::IsGodMode()) {
			return true;
		}

		const auto cost = std::max(0.0F, a_spell->CalculateMagickaCost(a_actor));
		if (cost <= 0.0F) {
			return true;
		}

		const auto owner = a_actor->AsActorValueOwner();
		if (!owner) {
			return false;
		}

		if (owner->GetActorValue(RE::ActorValue::kMagicka) < cost) {
			if (a_actor == RE::PlayerCharacter::GetSingleton()) {
				RE::FlashHUDMeter(RE::ActorValue::kMagicka);
			}
			logger::debug("[EventListener] CastSpell blocked because actor {:08X} has insufficient magicka for cost {}.", a_actor->formID, cost);
			return false;
		}

		owner->DamageActorValue(RE::ActorValue::kMagicka, cost);
		return true;
	}

	[[nodiscard]] RE::Actor* ResolveRuleTarget(RE::Actor* a_eventActor, Settings::RuleTarget a_target)
	{
		if (!a_eventActor) {
			return nullptr;
		}

		switch (a_target) {
		case Settings::RuleTarget::kPlayer:
			return RE::PlayerCharacter::GetSingleton();
		case Settings::RuleTarget::kCombatTarget: {
			auto target = a_eventActor->GetActorRuntimeData().currentCombatTarget.get();
			return target ? target->As<RE::Actor>() : nullptr;
		}
		case Settings::RuleTarget::kEventActor:
		default:
			return a_eventActor;
		}
	}

	[[nodiscard]] bool CompareActorValue(
		float a_currentValue,
		const Settings::RuleCondition& a_condition)
	{
		switch (a_condition.comparison) {
		case Settings::ComparisonOperator::kGreater:
			return a_currentValue > a_condition.value;
		case Settings::ComparisonOperator::kLess:
			return a_currentValue < a_condition.value;
		case Settings::ComparisonOperator::kLessOrEqual:
			return a_currentValue <= a_condition.value;
		case Settings::ComparisonOperator::kEqual:
			return std::abs(a_currentValue - a_condition.value) <= 0.001F;
		case Settings::ComparisonOperator::kBetween: {
			const auto minimum = std::min(a_condition.value, a_condition.maximumValue);
			const auto maximum = std::max(a_condition.value, a_condition.maximumValue);
			return a_currentValue >= minimum && a_currentValue <= maximum;
		}
		case Settings::ComparisonOperator::kGreaterOrEqual:
		default:
			return a_currentValue >= a_condition.value;
		}
	}

	[[nodiscard]] bool MatchesCondition(RE::Actor* a_actor, const Settings::RuleCondition& a_condition)
	{
		if (!a_actor) {
			return false;
		}

		bool matches = false;
		switch (a_condition.type) {
		case Settings::ConditionType::kRace: {
			auto* race = RE::TESForm::LookupByID<RE::TESRace>(a_condition.formID);
			if (!race) {
				return false;
			}
			matches = a_actor->GetRace() == race;
			break;
		}
		case Settings::ConditionType::kKeyword: {
			auto* keyword = RE::TESForm::LookupByID<RE::BGSKeyword>(a_condition.formID);
			if (!keyword) {
				return false;
			}
			matches = a_actor->HasKeyword(keyword);
			break;
		}
		case Settings::ConditionType::kFaction: {
			auto* faction = RE::TESForm::LookupByID<RE::TESFaction>(a_condition.formID);
			if (!faction) {
				return false;
			}
			matches = a_actor->IsInFaction(faction);
			break;
		}
		case Settings::ConditionType::kEquippedWeapon: {
			auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(a_condition.formID);
			if (!weapon) {
				return false;
			}
			matches = a_actor->GetEquippedObject(false) == weapon || a_actor->GetEquippedObject(true) == weapon;
			break;
		}
		case Settings::ConditionType::kActorValue: {
			const auto actorValue = RE::ActorValueList::LookupActorValueByName(a_condition.actorValue.c_str());
			const auto* owner = a_actor->AsActorValueOwner();
			if (actorValue == RE::ActorValue::kNone || !owner) {
				return false;
			}
			return CompareActorValue(owner->GetActorValue(actorValue), a_condition);
		}
		default:
			return false;
		}

		return a_condition.negate ? !matches : matches;
	}

	[[nodiscard]] bool MatchesAllConditions(RE::Actor* a_actor, const Settings::Rule& a_rule)
	{
		return std::ranges::all_of(a_rule.conditions, [&](const Settings::RuleCondition& a_condition) {
			return MatchesCondition(a_actor, a_condition);
		});
	}

	[[nodiscard]] float GetRandomPercent()
	{
		static thread_local std::mt19937 generator{ std::random_device{}() };
		static thread_local std::uniform_real_distribution<float> distribution(0.0F, 100.0F);
		return distribution(generator);
	}

	[[nodiscard]] bool TryActivateRule(const Settings::Rule& a_rule, RE::Actor* a_target)
	{
		if (!a_target) {
			return false;
		}

		const auto chance = std::clamp(ResolveExecutionValue(a_rule.chance, a_rule.chanceSource,
			a_rule.chanceGlobal, a_rule.chanceActorValue, a_target), 0.0F, 100.0F);
		const auto cooldown = ResolveExecutionValue(a_rule.cooldownSeconds, a_rule.cooldownSource,
			a_rule.cooldownGlobal, a_rule.cooldownActorValue, a_target);
		const auto maxActivations = static_cast<std::uint32_t>(std::min<double>(
			ResolveExecutionValue(static_cast<float>(a_rule.maxActivations), a_rule.maxActivationsSource,
				a_rule.maxActivationsGlobal, a_rule.maxActivationsActorValue, a_target),
			std::numeric_limits<std::uint32_t>::max()));
		const auto* calendar = RE::Calendar::GetSingleton();
		if (!calendar) {
			return false;
		}

		std::scoped_lock lock(g_runtimeMutex);
		auto& state = g_ruleRuntime[a_rule.name];
		return state.TryActivate(calendar->GetDaysPassed(), chance, cooldown, maxActivations,
			a_rule.cooldownScope == Settings::CooldownScope::kGlobal, a_target->GetFormID(),
			std::chrono::steady_clock::now(), chance > 0.0F && chance < 100.0F ? GetRandomPercent() : 0.0F);
	}

	[[nodiscard]] bool IsDuplicateEffect(const Settings::RuleEffect& a_lhs, const Settings::RuleEffect& a_rhs)
	{
		if (a_lhs.type != a_rhs.type) {
			return false;
		}

		switch (a_lhs.type) {
		case Settings::EffectType::kCastSpell:
		case Settings::EffectType::kDispelSpell:
		case Settings::EffectType::kAddPerk:
		case Settings::EffectType::kRemovePerk:
		case Settings::EffectType::kPlaySound:
			return a_lhs.formID == a_rhs.formID;
		case Settings::EffectType::kPlayShaderEffect:
			return a_lhs.formID == a_rhs.formID && a_lhs.duration == a_rhs.duration;
		case Settings::EffectType::kModifyResource:
			if (a_lhs.resourceOperation != a_rhs.resourceOperation ||
				a_lhs.resource != a_rhs.resource ||
				a_lhs.valueSource != a_rhs.valueSource) {
				return false;
			}

			switch (a_lhs.valueSource) {
			case Settings::ValueSource::kGlobal:
				return a_lhs.globalID == a_rhs.globalID;
			case Settings::ValueSource::kActorValue:
				return a_lhs.actorValue == a_rhs.actorValue;
			case Settings::ValueSource::kFixed:
			default:
				return a_lhs.fixedValue == a_rhs.fixedValue;
			}
		default:
			return false;
		}
	}

	void ApplyEffect(RE::Actor* a_actor, const Settings::RuleEffect& a_effect)
	{
		if (!a_actor) {
			return;
		}

		switch (a_effect.type) {
		case Settings::EffectType::kCastSpell:
			if (auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_effect.formID)) {
				if (a_effect.requireSpellCost && !SpendSpellCostIfAvailable(a_actor, spell)) {
					break;
				}
				if (auto* caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
					caster->CastSpellImmediate(spell, false, nullptr, 1.0F, false, 0.0F, a_actor);
				}
			}
			break;
		case Settings::EffectType::kDispelSpell:
			if (auto* spell = RE::TESForm::LookupByID<RE::MagicItem>(a_effect.formID)) {
				if (auto* target = a_actor->AsMagicTarget()) {
					auto casterHandle = a_actor->CreateRefHandle();
					target->DispelEffect(spell, casterHandle);
				}
			}
			break;
		case Settings::EffectType::kAddPerk:
			if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(a_effect.formID)) {
				a_actor->AddPerk(perk);
			}
			break;
		case Settings::EffectType::kRemovePerk:
			if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(a_effect.formID)) {
				a_actor->RemovePerk(perk);
			}
			break;
		case Settings::EffectType::kPlaySound:
			if (auto* sound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(a_effect.formID)) {
				if (auto* audio = RE::BSAudioManager::GetSingleton()) {
					RE::BSSoundHandle handle;
					if (audio->GetSoundHandle(handle, sound)) {
						if (auto* node = a_actor->Get3D()) {
							handle.SetObjectToFollow(node);
						} else {
							handle.SetPosition(a_actor->GetPosition());
						}
						handle.Play();
					}
				}
			}
			break;
		case Settings::EffectType::kPlayShaderEffect:
			if (auto* shader = RE::TESForm::LookupByID<RE::TESEffectShader>(a_effect.formID)) {
				a_actor->ApplyEffectShader(shader, a_effect.duration, nullptr, false, false);
			}
			break;
		case Settings::EffectType::kModifyResource: {
			const auto amount = ResolveEffectValue(a_actor, a_effect);
			if (amount <= 0.0F) {
				break;
			}
			const auto owner = a_actor->AsActorValueOwner();
			if (!owner) {
				break;
			}
			const auto actorValue = ResourceToActorValue(a_effect.resource);
			if (a_effect.resourceOperation == Settings::ResourceOperation::kGain) {
				owner->RestoreActorValue(actorValue, amount);
			} else {
				owner->DamageActorValue(actorValue, amount);
			}
			break;
		}
		default:
			break;
		}
	}

	void ApplyResolvedEffects(RE::Actor* a_actor, const std::vector<Settings::RuleEffect>& a_requestedEffects)
	{
		std::unordered_set<RE::FormID> spellsToDispel;
		std::unordered_set<RE::FormID> perksToRemove;
		for (const auto& effect : a_requestedEffects) {
			if (effect.type == Settings::EffectType::kDispelSpell) {
				spellsToDispel.insert(effect.formID);
			} else if (effect.type == Settings::EffectType::kRemovePerk) {
				perksToRemove.insert(effect.formID);
			}
		}

		std::vector<Settings::RuleEffect> resolvedEffects;
		resolvedEffects.reserve(a_requestedEffects.size());
		for (const auto& effect : a_requestedEffects) {
			if ((effect.type == Settings::EffectType::kCastSpell && spellsToDispel.contains(effect.formID)) ||
				(effect.type == Settings::EffectType::kAddPerk && perksToRemove.contains(effect.formID))) {
				continue;
			}

			const auto duplicate = std::ranges::find_if(resolvedEffects, [&](const Settings::RuleEffect& a_existing) {
				return IsDuplicateEffect(a_existing, effect);
			});
			if (duplicate == resolvedEffects.end()) {
				resolvedEffects.push_back(effect);
			} else if (effect.type == Settings::EffectType::kCastSpell) {
				duplicate->requireSpellCost = duplicate->requireSpellCost && effect.requireSpellCost;
			}
		}

		for (const auto& effect : resolvedEffects) {
			ApplyEffect(a_actor, effect);
		}
	}
}

namespace Settings
{
	void LoadSettings()
	{
		std::scoped_lock runtimeLock(g_runtimeMutex);
		g_ruleRuntime.clear();

		std::unique_lock ruleLock(RuleMutex);
		Rules.clear();

		std::filesystem::create_directories(kRulesDir);
		for (const auto& entry : std::filesystem::directory_iterator(kRulesDir)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".json") {
				continue;
			}

			if (auto rule = ReadRuleFile(entry.path())) {
				Rules.push_back(std::move(*rule));
			}
		}

		NormalizeRuleNames();
	}

	void SaveSettings()
	{
		std::unique_lock lock(RuleMutex);
		SaveSettingsUnlocked();
	}

	void RegisterMenu()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("[EventListener] SKSE Menu Framework not found; menu will not be rendered.");
			return;
		}

		LoadLanguage();
		LoadSettings();
		SKSEMenuFramework::SetSection("Event Listener");
		SKSEMenuFramework::AddSectionItem(GetLoc("menu.rules", "Rules"), RenderRules);
		SKSEMenuFramework::AddSectionItem(GetLoc("menu.debug", "Debug"), RenderDebug);
	}

	void ApplyRulesForEvent(RE::Actor* a_actor, std::string_view a_eventName)
	{
		if (!a_actor || a_actor->IsDead() || !a_actor->Is3DLoaded()) {
			return;
		}

		std::vector<Rule> matchingRules;
		{
			std::shared_lock lock(RuleMutex);
			for (const auto& rule : Rules) {
				if (rule.enabled && rule.eventName == a_eventName) {
					matchingRules.push_back(rule);
				}
			}
		}

		struct TargetEffectBatch
		{
			RE::Actor* target{ nullptr };
			std::vector<RuleEffect> effects;
		};

		std::vector<TargetEffectBatch> batches;
		for (const auto& rule : matchingRules) {
			auto* target = ResolveRuleTarget(a_actor, rule.target);
			if (!target || target->IsDead() || !target->Is3DLoaded()) {
				continue;
			}

			if (target->IsPlayerRef() && !rule.affectPlayer) {
				continue;
			}

			if (rule.requiredPerk != 0) {
				auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(rule.requiredPerk);
				if (!perk || !target->HasPerk(perk)) {
					continue;
				}
			}

			if (!MatchesAllConditions(target, rule) || !TryActivateRule(rule, target)) {
				continue;
			}

			auto batch = std::ranges::find_if(batches, [&](const TargetEffectBatch& a_batch) {
				return a_batch.target == target;
			});
			if (batch == batches.end()) {
				batches.push_back({ target, rule.effects });
			} else {
				batch->effects.insert(batch->effects.end(), rule.effects.begin(), rule.effects.end());
			}
		}

		for (const auto& batch : batches) {
			ApplyResolvedEffects(batch.target, batch.effects);
		}
	}

}
