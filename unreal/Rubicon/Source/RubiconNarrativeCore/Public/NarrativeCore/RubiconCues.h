// Dialogue cues: the world speaking. Port of src/cues.ts. Pure, deterministic, no randomness.
//
// Reads timeline, gaze and mode; never writes the timeline or Racing Thoughts state (I7). One cue at a time,
// each at most once per session, manifest order on ties. Rules: fit (a cue starts only if it can finish before
// the current phase's nominal end), floor (during a saying's utterance only cues triggered by that utterance may
// sound; others are cut), preempt (an eligible phase cue cuts a sounding gaze cue).

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"

namespace RubiconCore
{
	/** Nominal phase length; empty for open-ended phases (advisory, open). */
	std::optional<double> PhaseDurationMs(const FTimelineStateDef& Def, EPhase Phase);
	/** The telling being read now (holds waking until it finishes). */
	bool TellingSounding(const FManifest& M, const FSessionState& S, double Now);
	bool CueEligible(const FCue& C, const FTimelineStateDef& Def, const FSessionState& S);

	struct FCueStepResult
	{
		FCueRuntime Cues;
		std::vector<FEvent> Events;
	};

	/** Against the state after gaze and timeline have been updated for this tick. */
	FCueStepResult StepCues(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S, int DtMs);
}
