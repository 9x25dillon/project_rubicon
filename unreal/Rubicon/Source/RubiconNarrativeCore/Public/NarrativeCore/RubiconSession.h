// The session reducer. Port of src/session.ts. Pure, deterministic, fixed-timestep.
//
//   Step(manifest, state, input, dtMs = TickMs) -> { state, events }
//
// Order within a tick:
//   1. commands, in order, at time t (may change mode, comfort, layer, trigger a saying, acknowledge the gate)
//   2. if running (not paused, not exited): gaze dwell, breath, timeline clock, Racing Thoughts, t += dt
//
// The engine owns real time and calls Step() at a fixed rate (accumulator), so render frame rate never
// affects simulation results.

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"

namespace RubiconCore
{
	FSettings DefaultSettings();
	const FTimelineStateDef& StateDef(const FManifest& M, const FSessionState& S);
	FSessionState CreateSession(const FManifest& M, uint32_t Seed, const FSettingsPatch& Settings = {});
	/** DtMs must be positive; a non-positive DtMs returns the state unchanged with no events. */
	FStepResult Step(const FManifest& M, const FSessionState& S0, const FTickInput& Input, int DtMs = TickMs);

	/** Upper bound on time from Elevation to Contemplation with zero player input (I4). Port of maxAutoRunMs. */
	double MaxAutoRunMs(const FManifest& M);
}
