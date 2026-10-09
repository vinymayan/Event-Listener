// Standalone check: cl /std:c++17 /EHsc tests\ExecutionChecks.cpp /Fe:build\ExecutionChecks.exe
// Then run build\ExecutionChecks.exe. No Skyrim or CommonLib dependency.
#include "../include/RuleExecutionState.h"

#include <cassert>
#include <limits>

int main()
{
	using State = Settings::RuleRuntimeState;
	using namespace std::chrono_literals;
	const auto now = State::Clock::time_point{};

	State daily;
	assert(daily.TryActivate(1.1F, 100, 0, 2, false, 1, now, 0));
	assert(daily.TryActivate(1.9F, 100, 0, 2, false, 2, now, 0));
	assert(!daily.TryActivate(1.99F, 100, 0, 2, false, 1, now, 0));
	assert(daily.TryActivate(2.0F, 100, 0, 2, false, 1, now, 0));
	assert(daily.activations == 1);
	assert(!daily.TryActivate(std::numeric_limits<float>::quiet_NaN(), 100, 0, 2, false, 1, now, 0));

	State chance;
	assert(!chance.TryActivate(1, 0, 10, 1, false, 1, now, 0));
	assert(!chance.TryActivate(1, 50, 10, 1, false, 1, now, 50));
	assert(chance.activations == 0 && !chance.lastGlobalActivation);
	assert(chance.TryActivate(1, 50, 10, 1, false, 1, now, 49));

	State cooldown;
	assert(cooldown.TryActivate(1, 100, 0, 0, false, 1, now, 0));
	assert(!cooldown.TryActivate(1, 100, 10, 0, false, 1, now + 1s, 0));
	assert(cooldown.TryActivate(1, 100, 10, 0, false, 2, now + 1s, 0));
	assert(!cooldown.TryActivate(1, 100, 10, 0, true, 3, now + 2s, 0));
	assert(cooldown.TryActivate(1, 100, 10, 0, true, 3, now + 11s, 0));
	assert(cooldown.TryActivate(1, 100, 1, 0, false, 3, now + 12s, 0));

	State dynamicLimit;
	assert(dynamicLimit.TryActivate(1, 100, 0, 1, false, 1, now, 0));
	assert(!dynamicLimit.TryActivate(1, 100, 0, 1, false, 1, now, 0));
	assert(dynamicLimit.TryActivate(1, 100, 0, 2, false, 1, now, 0));
	assert(dynamicLimit.TryActivate(1, 100, 0, 0, false, 1, now, 0));
}
