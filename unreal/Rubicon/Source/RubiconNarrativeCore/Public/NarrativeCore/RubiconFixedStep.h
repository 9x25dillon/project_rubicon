// Fixed-step clock: converts variable render frame deltas into a whole number of 10 ms core ticks.
// Simulation results depend only on the per-tick input stream, never on how frames were chunked.

#pragma once

#include <cstdint>

namespace RubiconCore
{
	struct FFixedStepClock
	{
		double AccumulatorMs = 0;
		int MaxStepsPerAdvance = 25; // spiral-of-death guard: at most 250 ms of simulation per frame
		double DroppedMs = 0;        // wall time discarded by the guard (hitches, breakpoints)

		/** Add a frame's delta and return how many fixed ticks to run now. */
		int Advance(double DeltaMs, int TickMsIn);
	};
}
