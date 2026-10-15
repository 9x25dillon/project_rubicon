#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconCues.h"

#include <algorithm>

namespace RubiconCore
{
	std::optional<double> PhaseDurationMs(const FTimelineStateDef& Def, EPhase Phase)
	{
		const FTiming& T = Def.Timing;
		switch (Phase)
		{
		case EPhase::Raising: return T.RaisingMs;
		case EPhase::Settled: return T.SettledMs;
		case EPhase::Approach: return T.ApproachAutoMs;
		case EPhase::Utterance: return T.UtteranceMs;
		case EPhase::Resonance: return T.ResonanceMs;
		case EPhase::Running: return T.RunningMs;
		default: return std::nullopt;
		}
	}

	namespace CueDetail
	{
		bool InUtterance(const FTimelineStateDef& Def, const FSessionState& S)
		{
			return Def.Kind == EStateKind::Saying && S.Timeline.Phase == EPhase::Utterance;
		}

		bool TriggeredByThisUtterance(const FCue& C, const FTimelineStateDef& Def)
		{
			return !C.Trigger.bGaze && C.Trigger.State == Def.Id && C.Trigger.Phase == EPhase::Utterance;
		}
	}

	bool CueEligible(const FCue& C, const FTimelineStateDef& Def, const FSessionState& S)
	{
		if (!(C.bAllStates || C.States[static_cast<size_t>(Def.Id)])) return false;
		const FCueTrigger& Tr = C.Trigger;
		if (Tr.bGaze)
		{
			if (CueDetail::InUtterance(Def, S)) return false;
			if (Tr.Mode == ECueMode::Embodiment && S.Mode != EMode::Embodiment) return false;
			if (Tr.Mode == ECueMode::Witness && S.Mode != EMode::Witness) return false;
			if (!(S.GazeTarget == Tr.Target && S.GazeDwellMs >= Tr.MinDwellMs)) return false;
		}
		else if (!(Def.Id == Tr.State && S.Timeline.Phase == Tr.Phase && S.Timeline.PhaseElapsedMs >= Tr.AtMs))
		{
			return false;
		}
		const std::optional<double> Dur = PhaseDurationMs(Def, S.Timeline.Phase);
		return !Dur || S.Timeline.PhaseElapsedMs + C.DurationMs <= *Dur;
	}

	FCueStepResult StepCues(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S, int DtMs)
	{
		const double Now = S.T + static_cast<double>(DtMs);
		FCueStepResult R;
		R.Cues = S.Cues;
		bool bActive = S.Cues.bActive && S.Cues.EndsAt > Now;
		if (bActive && CueDetail::InUtterance(Def, S))
		{
			const int A = S.Cues.Active;
			if (A < 0 || !CueDetail::TriggeredByThisUtterance(M.Cues[static_cast<size_t>(A)], Def)) bActive = false; // the saying has the floor
		}
		auto Played = [&S](int I) { return std::find(S.Cues.Played.begin(), S.Cues.Played.end(), I) != S.Cues.Played.end(); };
		auto Start = [&](int I)
		{
			const FCue& C = M.Cues[static_cast<size_t>(I)];
			R.Cues.Played.push_back(I);
			R.Cues.bActive = true;
			R.Cues.Active = I;
			R.Cues.StartedAt = Now;
			R.Cues.EndsAt = Now + C.DurationMs;
			FEvent E;
			E.T = Now;
			E.Type = EEventType::CueStart;
			E.Cue = I;
			R.Events.push_back(E);
			return R;
		};
		const bool bActiveIsGaze = bActive && M.Cues[static_cast<size_t>(S.Cues.Active)].Trigger.bGaze;

		/* pass 1: phase cues (recorded events), which may preempt a sounding gaze cue */
		if (!bActive || bActiveIsGaze)
		{
			for (size_t I = 0; I < M.Cues.size(); ++I)
			{
				const FCue& C = M.Cues[I];
				if (!C.Trigger.bGaze && !Played(static_cast<int>(I)) && CueEligible(C, Def, S)) return Start(static_cast<int>(I));
			}
		}
		if (bActive) return R;

		/* pass 2: gaze cues (points of interest) */
		for (size_t I = 0; I < M.Cues.size(); ++I)
		{
			const FCue& C = M.Cues[I];
			if (C.Trigger.bGaze && !Played(static_cast<int>(I)) && CueEligible(C, Def, S)) return Start(static_cast<int>(I));
		}
		R.Cues.bActive = false;
		R.Cues.Active = -1;
		R.Cues.StartedAt = 0;
		R.Cues.EndsAt = 0;
		return R;
	}
}
