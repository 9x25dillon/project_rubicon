#include "RubiconConvert.h"

#include "Containers/StringConv.h"

static_assert(static_cast<int>(ERubiconTimelineState::Contemplation) + 1 == RubiconCore::TimelineLength, "timeline enum drift");
static_assert(static_cast<int>(ERubiconTimelineState::Spear) == static_cast<int>(RubiconCore::EStateId::Spear), "timeline enum drift");
static_assert(static_cast<int>(ERubiconPhase::Open) == static_cast<int>(RubiconCore::EPhase::Open), "phase enum drift");
static_assert(static_cast<int>(ERubiconWitnessVantage::Mary) == static_cast<int>(RubiconCore::EVantage::Mary), "vantage enum drift");
static_assert(static_cast<int>(ERubiconEvidence::ArtisticInterpretation) == static_cast<int>(RubiconCore::EEvidence::ArtisticInterpretation), "evidence enum drift");
static_assert(static_cast<int>(ERubiconStress::Extreme) + 1 == RubiconCore::StressLevelCount, "stress enum drift");
static_assert(static_cast<int>(ERubiconTriggerVia::Auto) == static_cast<int>(RubiconCore::ETriggerVia::Auto), "trigger enum drift");
static_assert(static_cast<int>(ERubiconFragmentKind::Sonic) == static_cast<int>(RubiconCore::EFragmentKind::Sonic), "fragment enum drift");
static_assert(static_cast<int>(ERubiconSourceType::Scholarship) == static_cast<int>(RubiconCore::ESourceType::Scholarship), "source type enum drift");
static_assert(static_cast<int>(ERubiconCueKind::Sound) == static_cast<int>(RubiconCore::ECueKind::Sound), "cue kind enum drift");
static_assert(static_cast<int>(ERubiconElevationTransition::FadeSettle) == static_cast<int>(RubiconCore::EElevationTransition::FadeSettle), "elevation enum drift");

namespace RubiconConvert
{
	FString ToFString(const std::string& Utf8)
	{
		const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Utf8.data()), static_cast<int32>(Utf8.size()));
		return FString(Conv.Length(), Conv.Get());
	}

	std::string ToUtf8(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S, S.Len());
		return std::string(reinterpret_cast<const char*>(Conv.Get()), static_cast<size_t>(Conv.Length()));
	}

	FRubiconSettings ToUE(const RubiconCore::FSettings& S)
	{
		FRubiconSettings Out;
		Out.IntensityCeiling = S.IntensityCeiling;
		Out.bReducedEffects = S.bReducedEffects;
		Out.bSeated = S.bSeated;
		Out.bHapticsEnabled = S.bHapticsEnabled;
		return Out;
	}

	RubiconCore::FSettingsPatch ToPatch(const FRubiconSettings& S)
	{
		RubiconCore::FSettingsPatch P;
		P.IntensityCeiling = S.IntensityCeiling;
		P.ReducedEffects = S.bReducedEffects;
		P.Seated = S.bSeated;
		P.HapticsEnabled = S.bHapticsEnabled;
		return P;
	}

	FRubiconFragmentInfo FragmentInfo(const RubiconCore::FManifest& M, const RubiconCore::FLocale& L, int FragmentIndex)
	{
		FRubiconFragmentInfo Info;
		if (FragmentIndex < 0 || FragmentIndex >= static_cast<int>(M.Fragments.size())) return Info;
		const RubiconCore::FFragment& F = M.Fragments[static_cast<size_t>(FragmentIndex)];
		Info.Id = FName(*ToFString(F.Id));
		Info.Kind = ToUE(F.Kind);
		Info.Category = ToFString(RubiconCore::ToString(F.Category));
		if (F.TextKey)
		{
			Info.TextKey = ToFString(*F.TextKey);
			const auto It = L.find(*F.TextKey);
			Info.Text = It != L.end() ? ToFString(It->second) : Info.TextKey;
		}
		if (F.CueId) Info.CueId = FName(*ToFString(*F.CueId));
		Info.DurationMs = F.DurationMs;
		for (int A : F.AnchorIndices)
		{
			if (A >= 0) Info.AnchorCitations.Add(ToFString(M.Sources[static_cast<size_t>(A)].Citation));
		}
		return Info;
	}

	FName GazeTargetName(const RubiconCore::FManifest& M, int Index)
	{
		if (Index < 0 || Index >= static_cast<int>(M.GazeTargets.size())) return NAME_None;
		return FName(*ToFString(M.GazeTargets[static_cast<size_t>(Index)].Id));
	}

	FRubiconCueInfo CueInfo(const RubiconCore::FManifest& M, const RubiconCore::FLocale& L, int CueIndex)
	{
		FRubiconCueInfo Info;
		if (CueIndex < 0 || CueIndex >= static_cast<int>(M.Cues.size())) return Info;
		const RubiconCore::FCue& C = M.Cues[static_cast<size_t>(CueIndex)];
		Info.Id = FName(*ToFString(C.Id));
		Info.Kind = ToUE(C.Kind);
		Info.Speaker = ToFString(C.Speaker);
		if (C.Speaker == "narrator")
		{
			const auto It = L.find("ui.narrator");
			Info.SpeakerLabel = It != L.end() ? ToFString(It->second) : FString(TEXT("Narrator"));
		}
		else
		{
			const int Ci = M.FindCast(C.Speaker);
			Info.SpeakerLabel = Ci >= 0 ? ToFString(M.Cast[static_cast<size_t>(Ci)].Name) : Info.Speaker;
		}
		const auto Tx = L.find(C.TextKey);
		Info.Text = Tx != L.end() ? ToFString(Tx->second) : ToFString(C.TextKey);
		for (int Idx : C.SourceIndices)
		{
			if (Idx >= 0) Info.Citations.Add(ToFString(M.Sources[static_cast<size_t>(Idx)].Citation));
		}
		Info.Evidence = ToUE(C.Evidence);
		const int Origin = C.Origin >= 0 ? C.Origin : (C.Trigger.bGaze && C.Kind != RubiconCore::ECueKind::Narration ? C.Trigger.Target : -1);
		Info.Origin = GazeTargetName(M, Origin);
		if (C.CueId) Info.AudioCueId = FName(*ToFString(*C.CueId));
		Info.DurationMs = C.DurationMs;
		Info.Trigger = C.Trigger.bGaze
			? FString::Printf(TEXT("gaze %s >= %.1f s (%s)"), *GazeTargetName(M, C.Trigger.Target).ToString(), C.Trigger.MinDwellMs / 1000.0, *ToFString(RubiconCore::ToString(C.Trigger.Mode)))
			: FString::Printf(TEXT("%s/%s at %.1f s"), *ToFString(RubiconCore::ToString(C.Trigger.State)), *ToFString(RubiconCore::ToString(C.Trigger.Phase)), C.Trigger.AtMs / 1000.0);
		return Info;
	}

	FName GazeTagFor(const FString& TargetId)
	{
		return FName(*(TEXT("Gaze.Target.") + TargetId));
	}

	void ToUE(const RubiconCore::FManifest& M, const RubiconCore::FFrameOutputs& In, int64 Tick, FRubiconFrameOutputs& Out)
	{
		Out.bValid = true;
		Out.TimeMs = In.T;
		Out.Tick = Tick;
		Out.State = ToUE(In.State);
		Out.Phase = ToUE(In.Phase);
		Out.PhaseProgress = In.PhaseProgress;
		Out.Mode = ToUE(In.Mode);
		Out.Vantage = ToUE(In.Vantage);
		Out.Layer = ToUE(In.Layer);
		Out.bComfort = In.bComfort;
		Out.bPaused = In.bPaused;
		Out.bExited = In.bExited;
		Out.bEmbodimentAvailable = In.bEmbodimentAvailable;
		Out.Controls.bPause = In.bControlPause;
		Out.Controls.bExit = In.bControlExit;
		Out.Controls.bWitness = In.bControlWitness;
		Out.Controls.bComfort = In.bControlComfort;

		Out.ElevationTransition = ToUE(In.ElevationTransition);
		Out.bHasAtmosphere = In.Atmosphere.has_value();
		if (In.Atmosphere)
		{
			Out.Atmosphere.SunIlluminanceLux = In.Atmosphere->SunIlluminanceLux;
			Out.Atmosphere.SkyCctK = In.Atmosphere->SkyCctK;
			Out.Atmosphere.AerosolOpticalDepth = In.Atmosphere->AerosolOpticalDepth;
			Out.Atmosphere.Evidence = ToUE(In.Atmosphere->Evidence);
			Out.Atmosphere.Note = In.Atmosphere->Note ? ToFString(*In.Atmosphere->Note) : FString();
		}
		Out.bTheological = In.bTheological;

		Out.RacingThoughts.Intensity = In.RtIntensity;
		Out.RacingThoughts.Output = In.RtOutput;
		Out.RacingThoughts.MaxVoices = In.RtMaxVoices;
		Out.RacingThoughts.ActiveVoices.Reset(static_cast<int32>(In.ActiveVoices.size()));
		for (const RubiconCore::FActiveVoice& V : In.ActiveVoices)
		{
			FRubiconActiveVoice& Av = Out.RacingThoughts.ActiveVoices.AddDefaulted_GetRef();
			Av.FragmentId = V.Fragment >= 0 ? FName(*ToFString(M.Fragments[static_cast<size_t>(V.Fragment)].Id)) : NAME_None;
			Av.ElapsedMs = V.ElapsedMs;
		}
		Out.ExternalDuckDb = In.ExternalDuckDb;
		Out.bDialogueActive = In.bDialogueActive;
		Out.BreathFocus = In.BreathFocus;

		Out.GazeTarget = GazeTargetName(M, In.GazeTarget);
		Out.GazeDwellMs = In.GazeDwellMs;
		Out.GazeStress = ToUE(In.GazeStress);
		Out.GazeTier = In.GazeTier >= 0 && In.GazeTier < static_cast<int>(M.RacingThoughts.DwellTiers.size())
			? FName(*ToFString(M.RacingThoughts.DwellTiers[static_cast<size_t>(In.GazeTier)].Id)) : NAME_None;

		Out.bHasCue = In.Cue.has_value();
		if (In.Cue)
		{
			const RubiconCore::FCueFrame& C = *In.Cue;
			Out.Cue.Id = FName(*ToFString(M.Cues[static_cast<size_t>(C.Cue)].Id));
			Out.Cue.Kind = ToUE(C.Kind);
			Out.Cue.Speaker = ToFString(C.Speaker);
			Out.Cue.SpeakerLabel = ToFString(C.SpeakerLabel);
			Out.Cue.Subtitle = ToFString(C.Subtitle);
			Out.Cue.Citations.Reset();
			for (const std::string& Ct : C.Citations) Out.Cue.Citations.Add(ToFString(Ct));
			Out.Cue.Evidence = ToUE(C.Evidence);
			Out.Cue.Rendering = ToFString(RubiconCore::ToString(C.Rendering));
			Out.Cue.Origin = GazeTargetName(M, C.Origin);
			Out.Cue.ElapsedMs = C.ElapsedMs;
			Out.Cue.DurationMs = C.DurationMs;
			Out.Cue.AudioCueId = C.CueId ? FName(*ToFString(*C.CueId)) : NAME_None;
		}

		Out.bHasSaying = In.Saying.has_value();
		if (In.Saying)
		{
			const RubiconCore::FFrameSaying& S = *In.Saying;
			Out.Saying.Id = ToUE(S.Id);
			Out.Saying.bHasSubtitle = S.Subtitle.has_value();
			Out.Saying.Subtitle = S.Subtitle ? ToFString(*S.Subtitle) : FString();
			Out.Saying.Citations.Reset();
			for (const std::string& C : S.Citations) Out.Saying.Citations.Add(ToFString(C));
			Out.Saying.Evidence = ToUE(S.Evidence);
			Out.Saying.SourceNote = S.SourceNote ? ToFString(*S.SourceNote) : FString();
			Out.Saying.GazeTarget = GazeTargetName(M, S.GazeTarget);
			Out.Saying.bAttended = S.bAttended;
		}

		Out.EvidenceOverlay.Reset(static_cast<int32>(In.EvidenceOverlay.size()));
		for (const RubiconCore::FEvidenceItem& E : In.EvidenceOverlay)
		{
			FRubiconEvidenceItem& Item = Out.EvidenceOverlay.AddDefaulted_GetRef();
			Item.Category = ToUE(E.Category);
			Item.Citation = ToFString(E.Citation);
			Item.Label = ToFString(E.Label);
			Item.Note = E.Note ? ToFString(*E.Note) : FString();
			for (const std::string& N : E.ManuscriptNotes) Item.ManuscriptNotes.Add(ToFString(N));
			Item.bHasSourceType = E.SourceType.has_value();
			if (E.SourceType) Item.SourceType = ToUE(*E.SourceType);
			Item.Debate = E.Debate ? ToFString(*E.Debate) : FString();
		}
		Out.EvidenceOpen = In.EvidenceOpen >= 0 ? ToFString(M.Sources[static_cast<size_t>(In.EvidenceOpen)].Id) : FString();
	}
}
