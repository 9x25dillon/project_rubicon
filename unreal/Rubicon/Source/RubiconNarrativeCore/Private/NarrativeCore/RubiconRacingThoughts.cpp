#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconRacingThoughts.h"
#include "NarrativeCore/RubiconMath.h"

#include <algorithm>
#include <cmath>

namespace RubiconCore
{
	double NominalStateDurationMs(const FTimelineStateDef& Def)
	{
		const FTiming& T = Def.Timing;
		switch (Def.Kind)
		{
		case EStateKind::Elevation: return T.RaisingMs.value_or(0) + T.SettledMs.value_or(0);
		case EStateKind::Saying: return T.ApproachAutoMs.value_or(0) + T.UtteranceMs.value_or(0) + T.ResonanceMs.value_or(0);
		case EStateKind::Timed: return T.RunningMs.value_or(1);
		default: return 1;
		}
	}

	EStress EffectiveStress(const FManifest& M, const FTimelineStateDef& Def, int Target)
	{
		if (Target < 0) return M.RacingThoughts.NoTargetStress;
		const size_t Idx = static_cast<size_t>(Target);
		if (Idx < Def.StressOverride.size() && Def.StressOverride[Idx] >= 0) return static_cast<EStress>(Def.StressOverride[Idx]);
		return Idx < M.GazeTargets.size() ? M.GazeTargets[Idx].Stress : M.RacingThoughts.NoTargetStress;
	}

	double CurrentCeiling(const FTimelineStateDef& Def, double StateElapsedMs)
	{
		if (!Def.Rt.CeilingEnd) return Def.Rt.Ceiling;
		const double P = Clamp01(StateElapsedMs / NominalStateDurationMs(Def));
		return Lerp(Def.Rt.Ceiling, *Def.Rt.CeilingEnd, P);
	}

	namespace RtMind
	{
		/** Outside the body (Witness) or out of it (unconscious): the storm has nothing to hold on to. */
		bool Away(const FSessionState& S) { return S.Mode == EMode::Witness || S.Consciousness.bSwoon; }
	}

	double IntensityTarget(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S)
	{
		if (RtMind::Away(S)) return 0;
		const FRacingThoughtsModel& Model = M.RacingThoughts;
		const EStress Lvl = EffectiveStress(M, Def, S.GazeTarget);
		const double W = Model.StressWeights[static_cast<size_t>(Lvl)];
		const double D = S.GazeTarget < 0 ? 1 : 1 - Exp(-S.GazeDwellMs / Model.DwellTauMs);
		const double Floor = Clamp01(Def.Rt.Floor + S.Rt.FloorBoost);
		const double Raw = W >= 0 ? Floor + W * D * (1 - Floor) : Floor * (1 + W * D);
		return Clamp(Raw, 0, CurrentCeiling(Def, S.Timeline.StateElapsedMs));
	}

	double OutputIntensity(const FManifest&, const FTimelineStateDef& Def, const FSessionState& S)
	{
		if (S.bPaused || S.bExited) return 0;
		const double Duck = (Def.Kind == EStateKind::Saying && S.Timeline.Phase == EPhase::Utterance) ? Def.Rt.UtteranceDuck : 1;
		return Clamp01(S.Rt.Intensity * S.Rt.Gain * S.Settings.IntensityCeiling * Duck);
	}

	int MaxActiveVoices(const FManifest& M, double Output)
	{
		return 1 + static_cast<int>(std::floor(Output * (M.RacingThoughts.MaxVoices - 1)));
	}

	double SpawnRatePerSec(const FManifest& M, double Output)
	{
		const FRacingThoughtsModel& R = M.RacingThoughts;
		return Output <= 0 ? 0 : R.RateMinPerSec + (R.RateMaxPerSec - R.RateMinPerSec) * Pow(Output, R.RateGamma);
	}

	double ExternalDuckDb(const FManifest& M, double Output)
	{
		const FRacingThoughtsModel& R = M.RacingThoughts;
		if (Output <= R.DuckStartAt) return 0;
		const double T = Clamp01((Output - R.DuckStartAt) / (1 - R.DuckStartAt));
		return R.DuckMaxDb * T * T * (3 - 2 * T);
	}

	int DwellTierIndex(const FManifest& M, double DwellMs)
	{
		int Idx = 0;
		const auto& Tiers = M.RacingThoughts.DwellTiers;
		for (size_t I = 0; I < Tiers.size(); ++I) if (Tiers[I].MinDwellMs <= DwellMs) Idx = static_cast<int>(I);
		return Idx;
	}

	namespace RtDetail
	{
		/** Dwell-tiered lines answer the embodied gaze only: the current target, in exactly the current tier. */
		bool GazeEligible(const FManifest& M, const FFragment& F, const FSessionState& S)
		{
			if (!F.bHasGaze) return true;
			return S.Mode == EMode::Embodiment && !S.Consciousness.bSwoon && S.GazeTarget == F.GazeTarget && DwellTierIndex(M, S.GazeDwellMs) == F.GazeTier;
		}
	}

	FRtStepResult StepRacingThoughts(const FManifest& M, const FTimelineStateDef& Def, const FSessionState& S, int DtMs)
	{
		const FRacingThoughtsModel& Model = M.RacingThoughts;
		const double Dt = static_cast<double>(DtMs);
		FRtStepResult Out;
		Out.Rng = S.Rng;

		/* intensity */
		const double Target = IntensityTarget(M, Def, S);
		const double Tau = RtMind::Away(S) ? Model.WitnessEchoTauMs : (Target > S.Rt.Intensity ? Def.Rt.TauUpMs : Def.Rt.TauDownMs);
		const double Intensity = Clamp01(SmoothToward(S.Rt.Intensity, Target, Dt, Tau));

		/* comfort gain: snap down on enable (handled in command), ramp up on release */
		const double Gain = S.bComfort ? Model.ComfortGain : Clamp01(SmoothToward(S.Rt.Gain, 1, Dt, Model.ComfortReleaseTauMs));

		FSessionState Next = S;
		Next.Rt.Intensity = Intensity;
		Next.Rt.Gain = Gain;
		const double Output = OutputIntensity(M, Def, Next);

		/* voices */
		std::vector<FVoice> Voices;
		for (const FVoice& V : S.Rt.Voices) if (V.EndsAt > S.T + Dt) Voices.push_back(V);
		std::vector<int> Recent = S.Rt.Recent;
		double Acc = S.Rt.SpawnAccumulator + (SpawnRatePerSec(M, Output) * Dt) / 1000;
		const int Cap = MaxActiveVoices(M, Output);

		std::vector<int> Pool;
		std::vector<double> Weights;
		while (Acc >= 1)
		{
			if (static_cast<int>(Voices.size()) >= Cap)
			{
				Acc = 1; // hold one pending spawn until a voice frees; prevents bursts after saturation
				break;
			}
			Pool.clear();
			for (size_t I = 0; I < M.Fragments.size(); ++I)
			{
				const FFragment& F = M.Fragments[I];
				const int Fi = static_cast<int>(I);
				if (!(F.bAllStates || F.States[static_cast<size_t>(Def.Id)])) continue;
				if (!(F.MinIntensity <= Output)) continue;
				if (std::any_of(Voices.begin(), Voices.end(), [Fi](const FVoice& V) { return V.Fragment == Fi; })) continue;
				if (std::find(Recent.begin(), Recent.end(), Fi) != Recent.end()) continue;
				if (!RtDetail::GazeEligible(M, F, S)) continue;
				Pool.push_back(Fi);
			}
			if (Pool.empty())
			{
				Acc = 0;
				break;
			}
			Weights.clear();
			double Total = 0;
			for (int Fi : Pool)
			{
				const FFragment& F = M.Fragments[static_cast<size_t>(Fi)];
				const bool bAffine = S.GazeTarget >= 0 && std::find(F.TargetAffinity.begin(), F.TargetAffinity.end(), S.GazeTarget) != F.TargetAffinity.end();
				const double W = F.Weight * (bAffine ? Model.AffinityMultiplier : 1);
				Weights.push_back(W);
				Total = Total + W;
			}
			const FRngDraw Draw = RngNext(Out.Rng);
			Out.Rng = Draw.State;
			double Pick = Draw.Value * Total;
			int Chosen = Pool.back();
			for (size_t I = 0; I < Pool.size(); ++I)
			{
				Pick -= Weights[I];
				if (Pick < 0)
				{
					Chosen = Pool[I];
					break;
				}
			}
			const FFragment& Cf = M.Fragments[static_cast<size_t>(Chosen)];
			const double StartedAt = S.T + Dt;
			Voices.push_back({ Chosen, StartedAt, StartedAt + Cf.DurationMs });
			Recent.push_back(Chosen);
			const size_t Window = static_cast<size_t>(std::max(0, Model.RecentWindow));
			if (Recent.size() > Window) Recent.erase(Recent.begin(), Recent.begin() + static_cast<std::ptrdiff_t>(Recent.size() - Window));
			FEvent E;
			E.T = StartedAt;
			E.Type = EEventType::RtFragment;
			E.Fragment = Chosen;
			E.FragmentKind = Cf.Kind;
			E.Voice = static_cast<int>(Voices.size()) - 1;
			Out.Events.push_back(E);
			Acc -= 1;
		}

		Out.Rt = Next.Rt;
		Out.Rt.Voices = std::move(Voices);
		Out.Rt.Recent = std::move(Recent);
		Out.Rt.SpawnAccumulator = Acc;
		return Out;
	}
}
