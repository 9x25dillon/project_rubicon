// Exertion and consciousness. Port of src/consciousness.ts. Pure, deterministic.
//
// Looking is free. Moving the head fast or far, and talking, cost the body; rest recovers it. At exertion 1 the
// player loses consciousness: the storm lets go (as on Witness entry) and the current moment is told, Gospel by
// Gospel, from the disciples' vantages. The timeline never waits. Consciousness returns after rest (not before
// MinSwoonMs, not mid-telling) or when a saying comes due. Settings.bSwoon = false turns it off.

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"

namespace RubiconCore
{
	double ExertionDrive(const FManifest& M, const FTickInput& Input);
	bool CanSwoonIn(const FTimelineStateDef& Def);
	/** Wake now (saying due, setting off, death). No-op when conscious. */
	void Wake(FSessionState& S, double T, EConsciousnessCause Cause, std::vector<FEvent>& Events);
	/** Advance exertion by DtMs (after commands and gaze, before the timeline clock). */
	void StepConsciousness(const FManifest& M, const FTimelineStateDef& Def, FSessionState& S, const FTickInput& Input, int DtMs,
		bool bTellingSounding, std::vector<FEvent>& Events);
}
