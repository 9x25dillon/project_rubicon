// Blueprint-facing mirror of the narrative core types (src/types.ts, src/frame.ts).
// Enum order equals the core enum order; RubiconConvert.cpp static_asserts every count.

#pragma once

#include "CoreMinimal.h"
#include "RubiconNarrativeTypes.generated.h"

UENUM(BlueprintType)
enum class ERubiconTimelineState : uint8
{
	Threshold, Elevation, Forgiveness, Paradise, Mother, Abandonment, Thirst, Completion, Surrender, Release, Spear, Contemplation
};

UENUM(BlueprintType)
enum class ERubiconPhase : uint8 { Advisory, Raising, Settled, Approach, Utterance, Resonance, Running, Open };

UENUM(BlueprintType)
enum class ERubiconExperienceMode : uint8 { Embodiment, Witness };

UENUM(BlueprintType)
enum class ERubiconVisualLayer : uint8 { Historical, Theological };

UENUM(BlueprintType)
enum class ERubiconWitnessVantage : uint8 { Centurion, BelovedDisciple, Mary };

UENUM(BlueprintType)
enum class ERubiconEvidence : uint8 { GospelText, TextualVariant, HistoricalInference, ArtisticInterpretation };

UENUM(BlueprintType)
enum class ERubiconStress : uint8 { Relief, Low, Medium, High, Extreme };

UENUM(BlueprintType)
enum class ERubiconTriggerVia : uint8 { Select, Speech, Auto };

UENUM(BlueprintType)
enum class ERubiconFragmentKind : uint8 { Verbal, Sonic };

UENUM(BlueprintType)
enum class ERubiconElevationTransition : uint8 { SlowTilt, FadeSettle };

/** What kind of witness a source is: shown beside the evidence category. */
UENUM(BlueprintType)
enum class ERubiconSourceType : uint8 { Gospel, NewTestament, HebrewBible, AncientText, Archaeology, Scholarship };

/** Spoken: a character's recorded words (KJV). Narration: a historical account read by a narrator. Sound: a recorded action. */
UENUM(BlueprintType)
enum class ERubiconCueKind : uint8 { Spoken, Narration, Sound };

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconSettings
{
	GENERATED_BODY()

	/** Player-chosen hard cap on Racing Thoughts output [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubicon", meta = (ClampMin = "0", ClampMax = "1"))
	double IntensityCeiling = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubicon")
	bool bReducedEffects = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubicon")
	bool bSeated = true;

	/** Never used for pain; this experience has no pain haptics in any mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubicon")
	bool bHapticsEnabled = false;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconControls
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bPause = true;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bExit = true;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bWitness = true;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bComfort = true;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconAtmosphere
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double SunIlluminanceLux = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double SkyCctK = 6500;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double AerosolOpticalDepth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Evidence = ERubiconEvidence::HistoricalInference;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Note;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconActiveVoice
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FName FragmentId;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double ElapsedMs = 0;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconRacingThoughtsFrame
{
	GENERATED_BODY()
	/** Internal state [0,1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double Intensity = 0;
	/** What the mixer applies [0,1] (comfort gain, ceiling, utterance duck and pause already applied). */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double Output = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") int32 MaxVoices = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") TArray<FRubiconActiveVoice> ActiveVoices;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconEvidenceItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Category = ERubiconEvidence::GospelText;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Citation;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Label;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Note;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") TArray<FString> ManuscriptNotes;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bHasSourceType = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconSourceType SourceType = ERubiconSourceType::Gospel;
	/** Authenticity, dating or identification questions; always shown with the source. */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Debate;
};

/** The dialogue cue sounding now (the world speaking; never the internal monologue). */
USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconCueFrame
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FName Id;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconCueKind Kind = ERubiconCueKind::Spoken;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Speaker;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString SpeakerLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Subtitle;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") TArray<FString> Citations;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Evidence = ERubiconEvidence::GospelText;
	/** kjv, public-domain-translation, own-translation, paraphrase (never display in quotation marks), description, mixed. */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Rendering;
	/** Gaze target to spatialize the voice from; None for the narrator (head-locked, like the internal voices). */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FName Origin;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double ElapsedMs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double DurationMs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FName AudioCueId;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconSayingFrame
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconTimelineState Id = ERubiconTimelineState::Forgiveness;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bHasSubtitle = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString Subtitle;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") TArray<FString> Citations;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Evidence = ERubiconEvidence::GospelText;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FString SourceNote;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FName GazeTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bAttended = false;
};

/** Everything the engine needs per tick. Port of FrameOutputs (src/frame.ts). */
USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconFrameOutputs
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double TimeMs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") int64 Tick = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconTimelineState State = ERubiconTimelineState::Threshold;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconPhase Phase = ERubiconPhase::Advisory;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") double PhaseProgress = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconExperienceMode Mode = ERubiconExperienceMode::Embodiment;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconWitnessVantage Vantage = ERubiconWitnessVantage::BelovedDisciple;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") ERubiconVisualLayer Layer = ERubiconVisualLayer::Historical;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bComfort = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bPaused = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bExited = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") bool bEmbodimentAvailable = true;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon") FRubiconControls Controls;

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Visual") ERubiconElevationTransition ElevationTransition = ERubiconElevationTransition::SlowTilt;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Visual") bool bHasAtmosphere = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Visual") FRubiconAtmosphere Atmosphere;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Visual") bool bTheological = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Audio") FRubiconRacingThoughtsFrame RacingThoughts;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Audio") double ExternalDuckDb = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Audio") bool bDialogueActive = false;
	/** [0,1], audio focus only. */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Audio") double BreathFocus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Gaze") FName GazeTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Gaze") double GazeDwellMs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Gaze") ERubiconStress GazeStress = ERubiconStress::Low;
	/** Dwell tier of the current gaze (glance, sustained, long, extreme). */
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Gaze") FName GazeTier;

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Dialogue") bool bHasCue = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Dialogue") FRubiconCueFrame Cue;

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Saying") bool bHasSaying = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Saying") FRubiconSayingFrame Saying;

	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Evidence") TArray<FRubiconEvidenceItem> EvidenceOverlay;
	UPROPERTY(BlueprintReadOnly, Category = "Rubicon|Evidence") FString EvidenceOpen;
};

/** Static description of one internal-voice fragment, for audio and the overlay. */
USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconFragmentInfo
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName Id;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconFragmentKind Kind = ERubiconFragmentKind::Verbal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Category;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString TextKey;
	/** Localized text for verbal fragments (temp VO script, subtitles in debug). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Text;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName CueId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") double DurationMs = 0;
	/** Scripture the fragment echoes, for the live disclosure ("internal voice, echoing ..."). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") TArray<FString> AnchorCitations;
};

/** Static description of a dialogue cue (delegate payload and data asset view). */
USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconCueInfo
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName Id;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconCueKind Kind = ERubiconCueKind::Spoken;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Speaker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString SpeakerLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Text;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") TArray<FString> Citations;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Evidence = ERubiconEvidence::GospelText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName Origin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName AudioCueId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") double DurationMs = 0;
	/** Human-readable trigger, e.g. "gaze crowd >= 2.5 s (embodiment)" or "paradise/approach at 0.5 s". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Trigger;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconGazeTargetInfo
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName Id;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconStress Stress = ERubiconStress::Low;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Evidence = ERubiconEvidence::GospelText;
	/** Actor or component tag the gaze resolver looks for: Gaze.Target.<id>. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FName Tag;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconSourceInfo
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Id;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Citation;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconEvidence Evidence = ERubiconEvidence::GospelText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString KjvText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") TArray<FString> ManuscriptNotes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconSourceType SourceType = ERubiconSourceType::Gospel;
	/** Non-KJV text (ancient authors, inscriptions) and how its English was produced. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Excerpt;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Rendering;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Translator;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Debate;
};

USTRUCT(BlueprintType)
struct RUBICONNARRATIVECORE_API FRubiconStateInfo
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") ERubiconTimelineState State = ERubiconTimelineState::Threshold;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Kind;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") bool bAllowEmbodiment = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") TArray<FString> SayingCitations;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") FString DialogueKey;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubicon") double NominalDurationMs = 0;
};

/* One delegate per core event type (SessionEvent in src/types.ts). Times are simulation milliseconds. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconTimelineEnterSignature, double, TimeMs, ERubiconTimelineState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRubiconTimelinePhaseSignature, double, TimeMs, ERubiconTimelineState, State, ERubiconPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRubiconSayingTriggerSignature, double, TimeMs, ERubiconTimelineState, Saying, ERubiconTriggerVia, Via);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRubiconModeChangeSignature, double, TimeMs, ERubiconExperienceMode, Mode, ERubiconWitnessVantage, Vantage, bool, bForced);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconLayerChangeSignature, double, TimeMs, ERubiconVisualLayer, Layer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconComfortChangeSignature, double, TimeMs, bool, bOn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconPauseChangeSignature, double, TimeMs, bool, bPaused);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRubiconExitSignature, double, TimeMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconEvidenceChangeSignature, double, TimeMs, const FString&, SourceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconSettingsChangeSignature, double, TimeMs, const FRubiconSettings&, Settings);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRubiconFragmentSignature, double, TimeMs, const FRubiconFragmentInfo&, Fragment, int32, Voice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRubiconCueSignature, double, TimeMs, const FRubiconCueInfo&, Cue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRubiconCommandRejectedSignature, double, TimeMs, const FString&, Command, const FString&, Reason);
