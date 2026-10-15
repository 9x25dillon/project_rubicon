#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconFixedStep.h"

#include <cmath>

namespace RubiconCore
{
	int FFixedStepClock::Advance(double DeltaMs, int TickMsIn)
	{
		if (TickMsIn <= 0) return 0;
		if (DeltaMs > 0 && std::isfinite(DeltaMs)) AccumulatorMs += DeltaMs;
		const double Tick = static_cast<double>(TickMsIn);
		int Steps = static_cast<int>(std::floor(AccumulatorMs / Tick));
		if (Steps > MaxStepsPerAdvance)
		{
			DroppedMs += (Steps - MaxStepsPerAdvance) * Tick;
			AccumulatorMs -= (Steps - MaxStepsPerAdvance) * Tick;
			Steps = MaxStepsPerAdvance;
		}
		AccumulatorMs -= Steps * Tick;
		if (AccumulatorMs < 0) AccumulatorMs = 0;
		return Steps;
	}
}
