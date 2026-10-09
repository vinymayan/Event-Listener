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

	enum class RuleTarget : std::uint8_t
	{
		kEventActor,
		kPlayer,
		kCombatTarget
	};

	enum class CooldownScope : std::uint8_t
	{
		kGlobal,
		kPerActor
	};

	enum class ConditionType : std::uint8_t
	{
		kRace,
		kKeyword,
		kFaction,
		kEquippedWeapon,
		kActorValue
	};

	enum class ComparisonOperator : std::uint8_t
	{
		kGreater,
		kGreaterOrEqual,
		kLess,
		kLessOrEqual,
		kEqual,
		kBetween
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

	struct RuleCondition
	{
		ConditionType type{ ConditionType::kRace };
		RE::FormID formID{ 0 };
		bool negate{ false };
		std::string actorValue{ "Stamina" };
		ComparisonOperator comparison{ ComparisonOperator::kGreaterOrEqual };
		float value{ 0.0F };
		float maximumValue{ 0.0F };
	};

	struct Rule
	{
		bool enabled{ true };
		std::string name{ "New Rule" };
		std::string eventName;
		bool affectPlayer{ false };
		RE::FormID requiredPerk{ 0 };
		RuleTarget target{ RuleTarget::kEventActor };
		float chance{ 100.0F };
		ValueSource chanceSource{ ValueSource::kFixed };
		RE::FormID chanceGlobal{ 0 };
		std::string chanceActorValue{ "Block" };
		float cooldownSeconds{ 0.0F };
		ValueSource cooldownSource{ ValueSource::kFixed };
		RE::FormID cooldownGlobal{ 0 };
		std::string cooldownActorValue{ "Block" };
		CooldownScope cooldownScope{ CooldownScope::kPerActor };
		std::uint32_t maxActivations{ 0 };
		ValueSource maxActivationsSource{ ValueSource::kFixed };
		RE::FormID maxActivationsGlobal{ 0 };
		std::string maxActivationsActorValue{ "Block" };
		std::vector<RuleCondition> conditions;
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
