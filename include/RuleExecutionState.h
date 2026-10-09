#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace Settings
{
	struct RuleRuntimeState
	{
		using Clock = std::chrono::steady_clock;

		std::uint64_t activations{ 0 };
		std::optional<float> activationDay;
		std::optional<Clock::time_point> lastGlobalActivation;
		std::unordered_map<std::uint32_t, Clock::time_point> lastActorActivations;

		bool TryActivate(float a_gameDays, float a_chance, float a_cooldown, std::uint32_t a_limit,
			bool a_globalCooldown, std::uint32_t a_actorID, Clock::time_point a_now, float a_roll)
		{
			if (!std::isfinite(a_gameDays)) {
				return false;
			}
			const auto day = std::floor(a_gameDays);
			if (!activationDay || *activationDay != day) {
				activationDay = day;
				activations = 0;
			}
			if (a_limit > 0 && activations >= a_limit) {
				return false;
			}

			const auto isCoolingDown = [&](Clock::time_point a_last) {
				return std::chrono::duration<float>(a_now - a_last).count() < a_cooldown;
			};
			if (a_cooldown > 0.0F) {
				if (a_globalCooldown) {
					if (lastGlobalActivation && isCoolingDown(*lastGlobalActivation)) {
						return false;
					}
				} else if (const auto found = lastActorActivations.find(a_actorID);
					found != lastActorActivations.end() && isCoolingDown(found->second)) {
					return false;
				}
			}
			if (a_chance <= 0.0F || (a_chance < 100.0F && a_roll >= a_chance)) {
				return false;
			}

			++activations;
			lastGlobalActivation = a_now;
			lastActorActivations[a_actorID] = a_now;
			return true;
		}
	};
}
