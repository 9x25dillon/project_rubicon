#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconConsciousness.h"
#include "NarrativeCore/RubiconMath.h"

#include <algorithm>

namespace RubiconCore
{
	namespace ConsciousnessDetail
	{
		FEvent Change(double T, bool bConscious, EConsciousnessCause Cause)
		{
			FEvent E;
			E.T = T;
			E.Type = EEventType::ConsciousnessChange;
			E.bConscious = bConscious;
			E.Cause = Cause;
			return E;
		}
	}

	double ExertionDrive(const FManifest& M, const FTickInput& Input)
	{
		const FExertionModel& X = M.Exertion;
		const double Ang = std::max(0.0, Input.HeadDegPerSec.value_or(0) - X.AngularDeadzoneDegPerSec) / X.AngularScaleDegPerSec;
		const double Lin = std::max(0.0, Input.HeadCmPerSec.value_or(0) - X.LinearDeadzoneCmPerSec) / X.LinearScaleCmPerSec;
		const double Voice = std::max(0.0, Input.Voice.value_or(0) - X.VoiceDeadzone);
		return Clamp01(Ang + Lin + Voice);
	}

	bool CanSwoonIn(const FTimelineStateDef& Def)
	{
		return Def.bAllowEmbodiment && Def.Kind != EStateKind::Gate;
	}

	void Wake(FSessionState& S, double T, EConsciousnessCause Cause, std::vector<FEvent>& Events)
	{
		if (!S.Consciousness.bSwoon) return;
		Events.push_back(ConsciousnessDetail::Change(T, true, Cause));
		S.Consciousness.bSwoon = false;
		S.Consciousness.SwoonSince = 0;
	}

	void StepConsciousness(const FManifest& M, const FTimelineStateDef& Def, FSessionState& S, const FTickInput& Input, int DtMs,
		bool bTellingSounding, std::vector<FEvent>& Events)
	{
		const FExertionModel& X = M.Exertion;
		const double Dt = static_cast<double>(DtMs);
		const double Now = S.T + Dt;
		FConsciousnessRuntime& C = S.Consciousness;

		if (C.bSwoon)
		{
			C.Exertion = Clamp01(C.Exertion - (X.SwoonRecoveryPerSec * Dt) / 1000);
			const bool bRested = C.Exertion <= X.WakeAt && Now - C.SwoonSince >= X.MinSwoonMs && !bTellingSounding;
			if (bRested)
			{
				Events.push_back(ConsciousnessDetail::Change(Now, true, EConsciousnessCause::Recovered));
				C.bSwoon = false;
				C.SwoonSince = 0;
			}
			return;
		}

		const bool bActive = CanSwoonIn(Def) && S.Mode == EMode::Embodiment;
		const double Drive = bActive ? ExertionDrive(M, Input) : 0;
		C.Exertion = Clamp01(C.Exertion + ((Drive * X.RisePerSec * Def.Fatigue - (1 - Drive) * X.RecoveryPerSec) * Dt) / 1000);
		const bool bSpeaking = Def.Kind == EStateKind::Saying && S.Timeline.Phase == EPhase::Utterance;
		if (bActive && S.Settings.bSwoon && C.Exertion >= 1 && !bSpeaking)
		{
			Events.push_back(ConsciousnessDetail::Change(Now, false, EConsciousnessCause::Exertion));
			C.bSwoon = true;
			C.SwoonSince = Now;
			S.Rt.Intensity = S.Rt.Intensity * M.RacingThoughts.WitnessEchoScale; // the mind lets go
		}
	}
}
