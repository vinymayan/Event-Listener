#pragma once

#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Settings
{
	enum class EffectType : std::uint8_t
	{
		kCastSpell,
		kDispelSpell,
		kAddPerk,
		kRemovePerk,
		kPlaySound,
		kPlayShaderEffect,
		kModifyResource
	};

	enum class ResourceType : std::uint8_t
	{
		kHealth,
		kMagicka,
		kStamina
	};

	enum class ResourceOperation : std::uint8_t
	{
		kConsume,
		kGain
	};

	enum class ValueSource : std::uint8_t
	{
		kFixed,
		kGlobal,
		kActorValue
	};

	struct RuleEffect
	{
		EffectType type{ EffectType::kCastSpell };
		RE::FormID formID{ 0 };
		ResourceOperation resourceOperation{ ResourceOperation::kConsume };
		ResourceType resource{ ResourceType::kStamina };
		ValueSource valueSource{ ValueSource::kFixed };
		float fixedValue{ 0.0F };
		RE::FormID globalID{ 0 };
		std::string actorValue{ "Stamina" };
		float duration{ 1.0F };
		bool requireSpellCost{ false };
	};

	struct Rule
	{
		bool enabled{ true };
		std::string name{ "New Rule" };
		std::string eventName{ "AttackStart" };
		bool affectPlayer{ false };
		RE::FormID requiredPerk{ 0 };
		std::vector<RuleEffect> effects;
		std::string sourceFileName;
	};

	inline std::vector<Rule> Rules;
	inline std::shared_mutex RuleMutex;

	void LoadSettings();
	void SaveSettings();
	void RegisterMenu();
	void ApplyRulesForEvent(RE::Actor* a_actor, std::string_view a_eventName);
}
