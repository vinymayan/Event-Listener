#include "Settings.h"

#include "ListManager.h"
#include "logger.h"

#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace ImGui = ImGuiMCP;

namespace
{
	constexpr const char* kModDir = "Data/Viny Mods/Event Listener";
	constexpr const char* kRulesDir = "Data/Viny Mods/Event Listener/Rules";
	constexpr const char* kLanguagePath = "Data/Viny Mods/Event Listener/Language.json";

	std::unordered_map<std::string, std::string> g_langMap;

	void EnsureMenuListsPopulated()
	{
		auto* manager = ListManager::GetSingleton();
		if (!manager->_isPopulated) {
			manager->PopulateAllLists();
		}
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
		ensureString("menu.affect_player", "Can Affect Player");
		ensureString("menu.required_perk", "Required Perk");
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

	bool DrawValueSourceDropdown(Settings::RuleEffect& a_effect)
	{
		bool changed = false;
		if (ImGui::BeginCombo(GetLoc("menu.value_source", "Value Source"), ValueSourceName(a_effect.valueSource))) {
			for (const auto source : { Settings::ValueSource::kFixed, Settings::ValueSource::kGlobal, Settings::ValueSource::kActorValue }) {
				const bool selected = a_effect.valueSource == source;
				if (ImGui::Selectable(ValueSourceName(source), selected)) {
					a_effect.valueSource = source;
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
			changed |= DrawValueSourceDropdown(a_effect);
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
		if (ImGui::Button(GetLoc("menu.add_rule", "Add Rule"))) {
			Settings::Rule rule;
			rule.name = MakeUniqueRuleName(rule.name, Settings::Rules.size());
			Settings::Rules.push_back(rule);
			changed = true;
		}

		ImGui::Separator();

		for (std::size_t i = 0; i < Settings::Rules.size();) {
			auto& rule = Settings::Rules[i];
			ImGui::PushID(static_cast<int>(i));
			const std::string header = rule.name + "##rule_" + std::to_string(i);
			if (ImGui::CollapsingHeader(header.c_str())) {
				ImGui::Indent();
				changed |= ImGui::Checkbox(GetLoc("menu.enabled", "Enabled"), &rule.enabled);
				changed |= DrawStringInput(GetLoc("menu.rule_name", "Rule Name"), rule.name, 128);
				changed |= DrawStringInput(GetLoc("menu.event_name", "Event"), rule.eventName, 128);
				changed |= ImGui::Checkbox(GetLoc("menu.affect_player", "Can Affect Player"), &rule.affectPlayer);
				changed |= DrawDropdown(GetLoc("menu.required_perk", "Required Perk"), "Perk", rule.requiredPerk);

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
				logger::debug("[DebugMenu] Reload Data clicked: forcing PopulateAllLists after dynamic form update.");
				logger::debug("[DebugMenu] PopulateAllLists BEGIN reason=manual_reload_data");
				manager->PopulateAllLists(true);
				logger::debug("[DebugMenu] PopulateAllLists END reason=manual_reload_data");
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
}

namespace Settings
{
	void LoadSettings()
	{
		std::unique_lock lock(RuleMutex);
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

		for (const auto& rule : matchingRules) {
			if (a_actor->IsPlayerRef() && !rule.affectPlayer) {
				continue;
			}

			if (rule.requiredPerk != 0) {
				auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(rule.requiredPerk);
				if (!perk || !a_actor->HasPerk(perk)) {
					continue;
				}
			}

			for (const auto& effect : rule.effects) {
				ApplyEffect(a_actor, effect);
			}
		}
	}

}
