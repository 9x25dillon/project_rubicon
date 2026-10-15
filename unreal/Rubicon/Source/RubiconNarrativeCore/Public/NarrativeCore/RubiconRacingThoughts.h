// Racing Thoughts: continuous, deterministic intensity system. Port of src/racingThoughts.ts.
//
// It READS timeline state (I5) and never writes it. Per tick (dt fixed):
//
//   w      = stressWeight(effectiveStress(state, gazeTarget))          relief < 0 < low < ... < extreme
//   d      = 1 - exp(-dwellMs / dwellTau)                               prolonged gaze costs more
//   floor' = floor + floorBoost
//   target = clamp( w >= 0 ? floor' + w*d*(1-floor') : floor' * (1 + w*d),  0, ceiling(t) )
//   target = 0                                                           when in Witness
//   I     <- I + (target - I)(1 - exp(-dt/tau)),   tau = tauUp if target > I else tauDown
//   gain   : Comfort snaps to comfortGain immediately; release ramps back with comfortReleaseTau
//   output = I * gain * settings.intensityCeiling * (utterance ? utteranceDuck : 1) * (paused ? 0 : 1)
//
// Fragments: rate = min + (max-min) * output^gamma; accumulator += rate*dt; each unit spawns one fragment
// if a voice slot is free (cap = 1 + floor(output * (maxVoices-1))). Weighted, seeded, recent-excluded.

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"

namespace RubiconCore
{
	double NominalStateDurationMs(const FTimelineStateDef& Def);
	EStress EffectiveStress(const FManifest& M, const FTimelineStateDef& Def, int Target);
	double CurrentCeiling(const FTimelineStateDef& Def, double StateElapsedMs);
	double IntensityTarget(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S);
	double OutputIntensity(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S);
	int MaxActiveVoices(const FManifest& M, double Output);
	double SpawnRatePerSec(const FManifest& M, double Output);
	double ExternalDuckDb(const FManifest& M, double Output);

	struct FRtStepResult
	{
		FRacingThoughtsRuntime Rt;
		uint32_t Rng = 0;
		std::vector<FEvent> Events;
	};

	/** Advance the Racing Thoughts subsystem by DtMs. Pure. */
	FRtStepResult StepRacingThoughts(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S, int DtMs);
}
