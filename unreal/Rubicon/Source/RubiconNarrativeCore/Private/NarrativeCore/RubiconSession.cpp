#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconSession.h"
#include "NarrativeCore/RubiconJson.h"
#include "NarrativeCore/RubiconMath.h"
#include "NarrativeCore/RubiconRacingThoughts.h"

#include <algorithm>

namespace RubiconCore
{
	namespace SessionDetail
	{
		constexpr double BreathTauMs = 800;

		/** `e >= t.x!` in TypeScript: false when the timing field is absent (undefined). */
		bool Reached(double Elapsed, const std::optional<double>& Limit) { return Limit && Elapsed >= *Limit; }

		FSettings ApplyPatch(FSettings S, const FSettingsPatch& P)
		{
			if (P.IntensityCeiling) S.IntensityCeiling = Clamp01(*P.IntensityCeiling);
			if (P.ReducedEffects) S.bReducedEffects = *P.ReducedEffects;
			if (P.Seated) S.bSeated = *P.Seated;
			if (P.HapticsEnabled) S.bHapticsEnabled = *P.HapticsEnabled;
			return S;
		}

		FEvent MakeEvent(double T, EEventType Type)
		{
			FEvent E;
			E.T = T;
			E.Type = Type;
			return E;
		}

		void SetPhase(FSessionState& S, EPhase Phase, std::vector<FEvent>& Events, const FTimelineStateDef& Def)
		{
			FEvent E = MakeEvent(S.T, EEventType::TimelinePhase);
			E.State = Def.Id;
			E.Phase = Phase;
			Events.push_back(E);
			S.Timeline.Phase = Phase;
			S.Timeline.PhaseElapsedMs = 0;
		}

		void ApplyWitnessEntry(const FManifest& M, FSessionState& S)
		{
			S.Rt.Intensity = S.Rt.Intensity * M.RacingThoughts.WitnessEchoScale;
		}

		void EnterState(const FManifest& M, FSessionState& S, int Index, std::vector<FEvent>& Events)
		{
			if (Index < 0 || Index >= static_cast<int>(M.Timeline.size())) return; // past terminal: unreachable
			const FTimelineStateDef& Def = M.Timeline[static_cast<size_t>(Index)];
			const EPhase Phase = FirstPhase(Def.Kind);
			S.Timeline = FTimelineRuntime();
			S.Timeline.Index = Index;
			S.Timeline.Phase = Phase;
			S.Rt.FloorBoost = 0;

			FEvent Enter = MakeEvent(S.T, EEventType::TimelineEnter);
			Enter.State = Def.Id;
			Events.push_back(Enter);
			FEvent Ph = MakeEvent(S.T, EEventType::TimelinePhase);
			Ph.State = Def.Id;
			Ph.Phase = Phase;
			Events.push_back(Ph);

			if (Def.ForcedMode && S.Mode != *Def.ForcedMode)
			{
				const EVantage Vantage = Def.ForcedVantage.value_or(S.Vantage);
				S.Mode = *Def.ForcedMode;
				S.Vantage = Vantage;
				if (*Def.ForcedMode == EMode::Witness) ApplyWitnessEntry(M, S);
				FEvent E = MakeEvent(S.T, EEventType::ModeChange);
				E.Mode = S.Mode;
				E.Vantage = Vantage;
				E.bForced = true;
				Events.push_back(E);
			}
			else if (Def.ForcedVantage && S.Mode == EMode::Witness && S.Vantage != *Def.ForcedVantage)
			{
				S.Vantage = *Def.ForcedVantage;
				FEvent E = MakeEvent(S.T, EEventType::ModeChange);
				E.Mode = S.Mode;
				E.Vantage = S.Vantage;
				E.bForced = true;
				Events.push_back(E);
			}

			if (S.Mode == EMode::Embodiment && Def.Rt.EntryImpulse > 0)
			{
				S.Rt.Intensity = std::max(S.Rt.Intensity, Def.Rt.EntryImpulse);
			}
		}

		void AdvanceToNextState(const FManifest& M, FSessionState& S, std::vector<FEvent>& Events)
		{
			const int NextIndex = S.Timeline.Index + 1;
			if (NextIndex >= TimelineLength) return;
			EnterState(M, S, NextIndex, Events);
		}

		void TriggerSaying(const FManifest& M, FSessionState& S, ETriggerVia Via, std::vector<FEvent>& Events)
		{
			const FTimelineStateDef& Def = StateDef(M, S);
			FEvent E = MakeEvent(S.T, EEventType::SayingTrigger);
			E.Saying = Def.Saying ? Def.Saying->Id : Def.Id;
			E.Via = Via;
			Events.push_back(E);
			S.Timeline.TriggeredVia = Via;
			SetPhase(S, EPhase::Utterance, Events, Def);
		}

		void Reject(const FSessionState& S, const FCommand& C, const std::string& Reason, std::vector<FEvent>& Events)
		{
			FEvent E = MakeEvent(S.T, EEventType::CommandRejected);
			E.Command = C.Type;
			E.Reason = Reason;
			Events.push_back(E);
		}

		void ApplyCommand(const FManifest& M, FSessionState& S, const FCommand& C, std::vector<FEvent>& Events)
		{
			if (S.bExited) { Reject(S, C, "session exited", Events); return; }
			const FTimelineStateDef& Def = StateDef(M, S);

			switch (C.Type)
			{
			case ECommandType::Exit:
				Events.push_back(MakeEvent(S.T, EEventType::Exit));
				S.bExited = true;
				S.Rt.Voices.clear();
				return;

			case ECommandType::Pause:
			{
				if (S.bPaused) return;
				FEvent E = MakeEvent(S.T, EEventType::PauseChange);
				E.bPaused = true;
				Events.push_back(E);
				S.bPaused = true;
				return;
			}

			case ECommandType::Resume:
			{
				if (!S.bPaused) return;
				FEvent E = MakeEvent(S.T, EEventType::PauseChange);
				E.bPaused = false;
				Events.push_back(E);
				S.bPaused = false;
				return;
			}

			case ECommandType::AcknowledgeAdvisory:
				if (Def.Kind != EStateKind::Gate) { Reject(S, C, "no advisory pending", Events); return; }
				if (S.bPaused) { Reject(S, C, "paused", Events); return; }
				AdvanceToNextState(M, S, Events);
				return;

			case ECommandType::TriggerSaying:
			{
				if (S.bPaused) { Reject(S, C, "paused", Events); return; }
				if (Def.Kind != EStateKind::Saying || S.Timeline.Phase != EPhase::Approach) { Reject(S, C, "no saying available", Events); return; }
				const double Min = Def.Timing.ApproachMinMs.value_or(0);
				if (S.Timeline.PhaseElapsedMs < Min)
				{
					Reject(S, C, "too early (" + JsNumberToString(S.Timeline.PhaseElapsedMs) + "ms < " + JsNumberToString(Min) + "ms)", Events);
					return;
				}
				TriggerSaying(M, S, C.Via, Events);
				return;
			}

			case ECommandType::SetMode:
			{
				const EVantage Vantage = C.Vantage.value_or(S.Vantage);
				if (C.Mode == EMode::Embodiment && !Def.bAllowEmbodiment)
				{
					Reject(S, C, std::string("embodiment unavailable in ") + ToString(Def.Id), Events);
					return;
				}
				if (C.Mode == S.Mode && Vantage == S.Vantage) return;
				const EMode Prev = S.Mode;
				S.Mode = C.Mode;
				S.Vantage = Vantage;
				if (C.Mode == EMode::Witness && Prev != EMode::Witness) ApplyWitnessEntry(M, S);
				FEvent E = MakeEvent(S.T, EEventType::ModeChange);
				E.Mode = S.Mode;
				E.Vantage = Vantage;
				E.bForced = false;
				Events.push_back(E);
				return;
			}

			case ECommandType::SetLayer:
			{
				if (C.Layer == S.Layer) return;
				FEvent E = MakeEvent(S.T, EEventType::LayerChange);
				E.Layer = C.Layer;
				Events.push_back(E);
				S.Layer = C.Layer;
				return;
			}

			case ECommandType::SetComfort:
			{
				if (C.bOn == S.bComfort) return;
				FEvent E = MakeEvent(S.T, EEventType::ComfortChange);
				E.bOn = C.bOn;
				Events.push_back(E);
				// Enabling snaps gain immediately: reliable, same-tick reduction.
				S.bComfort = C.bOn;
				if (C.bOn) S.Rt.Gain = M.RacingThoughts.ComfortGain;
				return;
			}

			case ECommandType::OpenEvidence:
			{
				const int Idx = M.FindSource(C.SourceId);
				if (Idx < 0) { Reject(S, C, "unknown source " + C.SourceId, Events); return; }
				FEvent E = MakeEvent(S.T, EEventType::EvidenceChange);
				E.SourceIndex = Idx;
				Events.push_back(E);
				S.EvidenceOpen = Idx;
				return;
			}

			case ECommandType::CloseEvidence:
			{
				if (S.EvidenceOpen < 0) return;
				FEvent E = MakeEvent(S.T, EEventType::EvidenceChange);
				E.SourceIndex = -1;
				Events.push_back(E);
				S.EvidenceOpen = -1;
				return;
			}

			case ECommandType::SetSettings:
			{
				S.Settings = ApplyPatch(S.Settings, C.Settings);
				FEvent E = MakeEvent(S.T, EEventType::SettingsChange);
				E.Settings = S.Settings;
				Events.push_back(E);
				return;
			}
			}
		}

		void AdvanceTimeline(const FManifest& M, FSessionState& S, double Dt, std::vector<FEvent>& Events)
		{
			const FTimelineStateDef& Def = StateDef(M, S);
			const EPhase Phase = S.Timeline.Phase;
			const bool bGazeOnSayingTarget = Def.Saying && S.GazeTarget == Def.Saying->GazeTargetIndex && S.Mode == EMode::Embodiment;
			S.Timeline.PhaseElapsedMs = S.Timeline.PhaseElapsedMs + Dt;
			S.Timeline.StateElapsedMs = S.Timeline.StateElapsedMs + Dt;
			S.Timeline.AttendedTargetMs = S.Timeline.AttendedTargetMs + (bGazeOnSayingTarget ? Dt : 0);
			const double E = S.Timeline.PhaseElapsedMs;
			const FTiming& T = Def.Timing;

			switch (Def.Kind)
			{
			case EStateKind::Gate:
			case EStateKind::Terminal:
				return;
			case EStateKind::Elevation:
				if (Phase == EPhase::Raising && Reached(E, T.RaisingMs)) { SetPhase(S, EPhase::Settled, Events, Def); return; }
				if (Phase == EPhase::Settled && Reached(E, T.SettledMs)) { AdvanceToNextState(M, S, Events); return; }
				return;
			case EStateKind::Saying:
				if (Phase == EPhase::Approach && Reached(E, T.ApproachAutoMs)) { TriggerSaying(M, S, ETriggerVia::Auto, Events); return; }
				if (Phase == EPhase::Utterance && Reached(E, T.UtteranceMs))
				{
					S.Rt.FloorBoost = S.Rt.FloorBoost + Def.Rt.PostUtteranceFloorBoost;
					SetPhase(S, EPhase::Resonance, Events, Def);
					return;
				}
				if (Phase == EPhase::Resonance && Reached(E, T.ResonanceMs)) { AdvanceToNextState(M, S, Events); return; }
				return;
			case EStateKind::Timed:
				if (Reached(E, T.RunningMs)) AdvanceToNextState(M, S, Events);
				return;
			}
		}
	}

	FSettings DefaultSettings() { return FSettings(); }

	const FTimelineStateDef& StateDef(const FManifest& M, const FSessionState& S)
	{
		// Index is only ever written by EnterState, which bounds it; clamp defensively (no exceptions in the core).
		const int I = std::min(std::max(S.Timeline.Index, 0), static_cast<int>(M.Timeline.size()) - 1);
		return M.Timeline[static_cast<size_t>(I)];
	}

	FSessionState CreateSession(const FManifest& M, uint32_t Seed, const FSettingsPatch& Settings)
	{
		FSessionState S;
		S.T = 0;
		S.Seed = Seed;
		S.Rng = Seed;
		S.Timeline.Index = 0;
		S.Timeline.Phase = FirstPhase(M.Timeline.front().Kind);
		S.Settings = SessionDetail::ApplyPatch(DefaultSettings(), Settings);
		return S;
	}

	FStepResult Step(const FManifest& M, const FSessionState& S0, const FTickInput& Input, int DtMs)
	{
		FStepResult R;
		R.State = S0;
		if (DtMs <= 0) return R;
		FSessionState& S = R.State;
		std::vector<FEvent>& Events = R.Events;
		const double Dt = static_cast<double>(DtMs);

		for (const FCommand& C : Input.Commands) SessionDetail::ApplyCommand(M, S, C, Events);
		if (S.bPaused || S.bExited) return R;

		/* gaze and breath */
		const bool bSameTarget = Input.Gaze == S.GazeTarget;
		S.GazeTarget = Input.Gaze;
		S.GazeDwellMs = (bSameTarget ? S.GazeDwellMs : 0) + Dt;
		S.Breath = Clamp01(SmoothToward(S.Breath, Input.Breath.value_or(S.Breath), Dt, SessionDetail::BreathTauMs));

		/* timeline */
		SessionDetail::AdvanceTimeline(M, S, Dt, Events);

		/* racing thoughts, against the (possibly new) current state */
		FRtStepResult Rt = StepRacingThoughts(M, StateDef(M, S), S, DtMs);
		S.Rt = std::move(Rt.Rt);
		S.Rng = Rt.Rng;
		S.T = S.T + Dt;
		Events.insert(Events.end(), Rt.Events.begin(), Rt.Events.end());
		return R;
	}

	double MaxAutoRunMs(const FManifest& M)
	{
		double Acc = 0;
		for (const FTimelineStateDef& D : M.Timeline)
		{
			const FTiming& T = D.Timing;
			switch (D.Kind)
			{
			case EStateKind::Elevation: Acc = Acc + (T.RaisingMs.value_or(0) + T.SettledMs.value_or(0)); break;
			case EStateKind::Saying: Acc = Acc + (T.ApproachAutoMs.value_or(0) + T.UtteranceMs.value_or(0) + T.ResonanceMs.value_or(0)); break;
			case EStateKind::Timed: Acc = Acc + T.RunningMs.value_or(0); break;
			default: break;
			}
		}
		return Acc;
	}
}
