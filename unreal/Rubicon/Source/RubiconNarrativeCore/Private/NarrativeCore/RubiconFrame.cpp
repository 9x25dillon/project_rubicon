#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconFrame.h"
#include "NarrativeCore/RubiconRacingThoughts.h"
#include "NarrativeCore/RubiconSession.h"

#include <algorithm>
#include <set>

namespace RubiconCore
{
	namespace FrameDetail
	{
		std::string Label(const FLocale* Locale, EEvidence E)
		{
			if (Locale)
			{
				const auto It = Locale->find(std::string("ui.evidence.") + ToString(E));
				if (It != Locale->end()) return It->second;
			}
			return ToString(E);
		}

		const std::optional<double>* PhaseDuration(const FTiming& T, EPhase P)
		{
			switch (P)
			{
			case EPhase::Raising: return &T.RaisingMs;
			case EPhase::Settled: return &T.SettledMs;
			case EPhase::Approach: return &T.ApproachAutoMs;
			case EPhase::Utterance: return &T.UtteranceMs;
			case EPhase::Resonance: return &T.ResonanceMs;
			case EPhase::Running: return &T.RunningMs;
			default: return nullptr;
			}
		}
	}

	std::vector<FEvidenceItem> EvidenceForState(const FManifest& M, const FSessionState& S, const FLocale* Locale)
	{
		const FTimelineStateDef& Def = StateDef(M, S);
		std::set<int> Seen;
		std::vector<FEvidenceItem> Items;
		auto PushSource = [&](int Idx)
		{
			if (Idx < 0 || Seen.count(Idx)) return;
			Seen.insert(Idx);
			const FSourceRef& Src = M.Sources[static_cast<size_t>(Idx)];
			FEvidenceItem It;
			It.Category = Src.Evidence;
			It.Citation = Src.Citation;
			It.Label = FrameDetail::Label(Locale, Src.Evidence);
			if (Src.Note && !Src.Note->empty()) It.Note = Src.Note;
			It.bHasManuscriptNotes = Src.bHasManuscriptNotes;
			It.ManuscriptNotes = Src.ManuscriptNotes;
			Items.push_back(std::move(It));
		};
		if (Def.Saying) for (int Idx : Def.Saying->SourceIndices) PushSource(Idx);
		for (int Idx : Def.SourceIndices) PushSource(Idx);

		/* active internal voices: always disclosed as artistic interpretation, with the passage they echo */
		for (const FVoice& V : S.Rt.Voices)
		{
			if (V.Fragment < 0) continue;
			const FFragment& F = M.Fragments[static_cast<size_t>(V.Fragment)];
			std::string Anchors;
			for (int A : F.AnchorIndices)
			{
				if (A < 0) continue;
				if (!Anchors.empty()) Anchors += "; ";
				Anchors += M.Sources[static_cast<size_t>(A)].Citation;
			}
			FEvidenceItem It;
			It.Category = EEvidence::ArtisticInterpretation;
			It.Citation = Anchors.empty() ? "internal voice" : "internal voice, echoing " + Anchors;
			It.Label = FrameDetail::Label(Locale, EEvidence::ArtisticInterpretation);
			It.Note = M.RacingThoughts.Disclosure;
			Items.push_back(std::move(It));
		}
		if (Def.Atmosphere)
		{
			FEvidenceItem It;
			It.Category = Def.Atmosphere->Evidence;
			It.Citation = "lighting and atmosphere";
			It.Label = FrameDetail::Label(Locale, Def.Atmosphere->Evidence);
			if (Def.Atmosphere->Note && !Def.Atmosphere->Note->empty()) It.Note = Def.Atmosphere->Note;
			Items.push_back(std::move(It));
		}
		return Items;
	}

	FFrameOutputs SelectFrame(const FManifest& M, const FSessionState& S, const FLocale* Locale)
	{
		const FTimelineStateDef& Def = StateDef(M, S);
		FFrameOutputs F;
		const std::optional<double>* Dur = FrameDetail::PhaseDuration(Def.Timing, S.Timeline.Phase);
		const double Output = OutputIntensity(M, Def, S);
		const bool bLive = !S.bExited;

		F.T = S.T;
		F.State = Def.Id;
		F.Phase = S.Timeline.Phase;
		F.PhaseProgress = (Dur && Dur->has_value() && **Dur != 0) ? std::min(1.0, S.Timeline.PhaseElapsedMs / **Dur) : 0;
		F.Mode = S.Mode;
		F.Vantage = S.Vantage;
		F.Layer = S.Layer;
		F.bComfort = S.bComfort;
		F.bPaused = S.bPaused;
		F.bExited = S.bExited;
		F.bEmbodimentAvailable = Def.bAllowEmbodiment;
		F.bControlPause = F.bControlExit = F.bControlWitness = F.bControlComfort = bLive;

		F.ElevationTransition = (S.bComfort || S.Settings.bReducedEffects) ? EElevationTransition::FadeSettle : EElevationTransition::SlowTilt;
		F.Atmosphere = Def.Atmosphere;
		F.bTheological = S.Layer == ELayer::Theological;

		F.RtIntensity = S.Rt.Intensity;
		F.RtOutput = Output;
		F.RtMaxVoices = MaxActiveVoices(M, Output);
		for (const FVoice& V : S.Rt.Voices) F.ActiveVoices.push_back({ V.Fragment, S.T - V.StartedAt });
		F.ExternalDuckDb = ExternalDuckDb(M, Output);
		F.bDialogueActive = S.Timeline.Phase == EPhase::Utterance;
		F.BreathFocus = S.Breath;

		F.GazeTarget = S.GazeTarget;
		F.GazeDwellMs = S.GazeDwellMs;
		F.GazeStress = EffectiveStress(M, Def, S.GazeTarget);

		if (Def.Saying)
		{
			const FSaying& Sy = *Def.Saying;
			FFrameSaying Fs;
			Fs.Id = Sy.Id;
			if (S.Timeline.Phase == EPhase::Utterance || S.Timeline.Phase == EPhase::Resonance)
			{
				const auto It = Locale ? Locale->find(Sy.DialogueKey) : FLocale::const_iterator();
				Fs.Subtitle = (Locale && It != Locale->end()) ? It->second : Sy.DialogueKey;
			}
			Fs.Citations = Sy.Sources;
			Fs.Evidence = Sy.Evidence;
			Fs.SourceNote = Sy.SourceNote;
			Fs.GazeTarget = Sy.GazeTargetIndex;
			Fs.bAttended = S.Timeline.AttendedTargetMs > 0;
			F.Saying = std::move(Fs);
		}
		F.EvidenceOverlay = EvidenceForState(M, S, Locale);
		F.EvidenceOpen = S.EvidenceOpen;
		return F;
	}
}
