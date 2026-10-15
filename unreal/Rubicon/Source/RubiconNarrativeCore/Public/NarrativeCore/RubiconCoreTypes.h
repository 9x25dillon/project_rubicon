// Project Rubicon, Prototype 01: The Passion
// Narrative core type contract. Port of src/types.ts.
//
// This header and everything under NarrativeCore/ is standard C++17 only: no UObject, no engine headers,
// no clock, no IO, no unseeded randomness. It compiles inside the UE module and in the standalone parity
// harness (native/), so the code that is tested against the TypeScript goldens is the code that ships.
//
// Ids are interned at load time: gaze targets, fragments and sources are indices into the manifest arrays
// (-1 = null). Timeline states, phases and every string union become enums with exact string tables.

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace RubiconCore
{
	/* ------------------------------------------------------------------ */
	/* Enums (string tables in RubiconCoreTypes.cpp match src/types.ts)    */
	/* ------------------------------------------------------------------ */

	enum class EEvidence : uint8_t { GospelText, TextualVariant, HistoricalInference, ArtisticInterpretation };
	enum class EStress : uint8_t { Relief, Low, Medium, High, Extreme };
	enum class EMode : uint8_t { Embodiment, Witness };
	enum class ELayer : uint8_t { Historical, Theological };
	enum class EVantage : uint8_t { Centurion, BelovedDisciple, Mary, WomenAfar };
	/** Where the camera rig should be: the body, a chosen Witness vantage, or (unconscious) a telling's vantage. */
	enum class EViewMode : uint8_t { Embodiment, Witness, Telling };
	enum class EGospel : uint8_t { Mark, Matthew, Luke, John };
	enum class EConsciousnessCause : uint8_t { Exertion, Recovered, Saying, Death, Setting };
	enum class EStateKind : uint8_t { Gate, Elevation, Saying, Timed, Terminal };
	enum class EPhase : uint8_t { Advisory, Raising, Settled, Approach, Utterance, Resonance, Running, Open };
	enum class ETriggerVia : uint8_t { Select, Speech, Auto };
	enum class EFragmentKind : uint8_t { Verbal, Sonic };
	enum class EFragmentCategory : uint8_t
	{
		ScripturalEcho, CrowdEcho, Accusation, MemoryFlash, PrayerCollapse, Dread, SensoryIntrusion
	};
	enum class ESourceType : uint8_t { Gospel, NewTestament, HebrewBible, AncientText, Archaeology, Scholarship, Tradition };
	enum class ERendering : uint8_t { PublicDomainTranslation, OwnTranslation, Paraphrase, Description };
	enum class ECueKind : uint8_t { Spoken, Narration, Sound, Telling };
	enum class ECueTriggerType : uint8_t { Gaze, Phase, Telling };
	enum class ECueMode : uint8_t { Embodiment, Witness, Any };

	/** Canonical, immutable order (I1). Index == position in the timeline. */
	enum class EStateId : uint8_t
	{
		Threshold, Elevation, Forgiveness, Paradise, Mother, Abandonment, Thirst, Completion, Surrender,
		Release, Spear, Contemplation
	};
	constexpr int TimelineLength = 12;
	constexpr int StressLevelCount = 5;
	constexpr int TickMs = 10;

	enum class ECommandType : uint8_t
	{
		AcknowledgeAdvisory, TriggerSaying, SetMode, SetLayer, SetComfort, Pause, Resume, Exit, OpenEvidence,
		CloseEvidence, SetSettings
	};

	enum class EEventType : uint8_t
	{
		TimelineEnter, TimelinePhase, SayingTrigger, ModeChange, LayerChange, ComfortChange, PauseChange, Exit,
		EvidenceChange, SettingsChange, RtFragment, CommandRejected, CueStart, ConsciousnessChange
	};

	const char* ToString(EEvidence V);
	const char* ToString(EStress V);
	const char* ToString(EMode V);
	const char* ToString(ELayer V);
	const char* ToString(EVantage V);
	const char* ToString(EStateKind V);
	const char* ToString(EPhase V);
	const char* ToString(ETriggerVia V);
	const char* ToString(EFragmentKind V);
	const char* ToString(EFragmentCategory V);
	const char* ToString(EStateId V);
	const char* ToString(ECommandType V);
	const char* ToString(EEventType V);
	const char* ToString(ESourceType V);
	const char* ToString(ERendering V);
	const char* ToString(ECueKind V);
	const char* ToString(ECueMode V);
	const char* ToString(EViewMode V);
	const char* ToString(EGospel V);
	const char* ToString(EConsciousnessCause V);

	bool Parse(const std::string& S, EEvidence& Out);
	bool Parse(const std::string& S, EStress& Out);
	bool Parse(const std::string& S, EMode& Out);
	bool Parse(const std::string& S, ELayer& Out);
	bool Parse(const std::string& S, EVantage& Out);
	bool Parse(const std::string& S, EStateKind& Out);
	bool Parse(const std::string& S, EPhase& Out);
	bool Parse(const std::string& S, ETriggerVia& Out);
	bool Parse(const std::string& S, EFragmentKind& Out);
	bool Parse(const std::string& S, EFragmentCategory& Out);
	bool Parse(const std::string& S, EStateId& Out);
	bool Parse(const std::string& S, ECommandType& Out);
	bool Parse(const std::string& S, ESourceType& Out);
	bool Parse(const std::string& S, ERendering& Out);
	bool Parse(const std::string& S, ECueKind& Out);
	bool Parse(const std::string& S, ECueMode& Out);
	bool Parse(const std::string& S, EGospel& Out);
	/** Ancient texts, archaeology and scholarship describe the world around the scene (historical-inference). */
	bool IsNonScriptural(ESourceType V);

	/** First phase of each state kind (PHASES_BY_KIND[kind][0]). */
	EPhase FirstPhase(EStateKind Kind);

	/* ------------------------------------------------------------------ */
	/* Manifest                                                            */
	/* ------------------------------------------------------------------ */

	struct FExcerpt
	{
		std::string Text;
		ERendering Rendering = ERendering::Description;
		std::optional<std::string> Translator;
	};

	struct FSourceRef
	{
		std::string Id;
		std::string Citation;
		EEvidence Evidence = EEvidence::GospelText;
		ESourceType SourceType = ESourceType::Gospel;
		std::optional<std::string> KjvText;
		std::optional<FExcerpt> Excerpt;
		std::optional<std::string> Debate;
		std::optional<std::string> Greek;
		std::optional<std::string> Note;
		std::vector<std::string> ManuscriptNotes; // empty == absent
		bool bHasManuscriptNotes = false;
	};

	struct FCastMember
	{
		std::string Id;
		std::string Name;
		std::vector<std::string> Sources;
		EEvidence Evidence = EEvidence::GospelText;
		std::optional<std::string> Note;
	};

	struct FGazeTargetDef
	{
		std::string Id;
		std::string Label;
		EStress Stress = EStress::Low;
		std::vector<std::string> Actors;
		std::vector<std::string> Sources;
		EEvidence Evidence = EEvidence::GospelText;
		std::optional<std::string> Note;
	};

	struct FWitnessVantageDef
	{
		EVantage Id = EVantage::Centurion;
		std::string CastId;
		std::vector<std::string> SourceIds;
		EEvidence Evidence = EEvidence::GospelText;
	};

	struct FRtStateParams
	{
		double Floor = 0;
		double Ceiling = 0;
		std::optional<double> CeilingEnd;
		double TauUpMs = 1;
		double TauDownMs = 1;
		double UtteranceDuck = 1;
		double PostUtteranceFloorBoost = 0;
		double EntryImpulse = 0;
	};

	struct FTiming
	{
		std::optional<double> RaisingMs;
		std::optional<double> SettledMs;
		std::optional<double> ApproachMinMs;
		std::optional<double> ApproachAutoMs;
		std::optional<double> UtteranceMs;
		std::optional<double> ResonanceMs;
		std::optional<double> RunningMs;
	};

	struct FPronunciation
	{
		std::optional<std::string> Greek;
		std::optional<std::string> GreekTranslit;
		std::optional<std::string> AramaicReconstruction;
		std::optional<std::string> AramaicTranslit;
		EEvidence Evidence = EEvidence::HistoricalInference;
		std::optional<std::string> Note;
	};

	struct FSaying
	{
		EStateId Id = EStateId::Forgiveness;
		std::string Theme;
		std::vector<std::string> Sources;   // human citations
		std::vector<std::string> SourceIds; // keys into Manifest.Sources
		std::vector<int> SourceIndices;     // resolved
		EEvidence Evidence = EEvidence::GospelText;
		std::string DialogueKey;
		std::string GazeTarget;
		int GazeTargetIndex = -1;
		EStress StressLevel = EStress::Low;
		std::string HistoricalCue;
		std::string TheologicalCue;
		std::optional<std::string> SourceNote;
		std::string PsychologicalNote;
		FPronunciation Pronunciation;
		std::string Attestation;
	};

	struct FAtmosphereKey
	{
		double SunIlluminanceLux = 0;
		double SkyCctK = 0;
		double AerosolOpticalDepth = 0;
		EEvidence Evidence = EEvidence::HistoricalInference;
		std::optional<std::string> Note;
	};

	struct FTimelineStateDef
	{
		EStateId Id = EStateId::Threshold;
		EStateKind Kind = EStateKind::Gate;
		std::string Label;
		std::vector<std::string> SourceIds;
		std::vector<int> SourceIndices;
		EEvidence Evidence = EEvidence::GospelText;
		FTiming Timing;
		FRtStateParams Rt;
		/** Indexed by gaze target index; -1 = no override, else (int)EStress. */
		std::vector<int8_t> StressOverride;
		bool bAllowEmbodiment = true;
		std::optional<EMode> ForcedMode;
		std::optional<EVantage> ForcedVantage;
		std::optional<FAtmosphereKey> Atmosphere;
		std::optional<FSaying> Saying;
		std::vector<std::string> Notes;
		double Fatigue = 1; // multiplier on exertion build-up
	};

	struct FFragment
	{
		std::string Id;
		EFragmentKind Kind = EFragmentKind::Verbal;
		EFragmentCategory Category = EFragmentCategory::Dread;
		std::optional<std::string> TextKey;
		std::optional<std::string> CueId;
		EEvidence Evidence = EEvidence::ArtisticInterpretation;
		std::vector<std::string> Anchors;
		std::vector<int> AnchorIndices;
		double MinIntensity = 0;
		double DurationMs = 1;
		double Weight = 1;
		bool bAllStates = false;
		std::array<bool, TimelineLength> States{}; // membership by EStateId
		std::vector<int> TargetAffinity;          // gaze target indices
		/** Dwell-tiered line: eligible only embodied, gazing at GazeTarget, in exactly tier GazeTier. */
		bool bHasGaze = false;
		int GazeTarget = -1;
		int GazeTier = 0; // index into FRacingThoughtsModel::DwellTiers
	};

	struct FDwellTier
	{
		std::string Id;
		double MinDwellMs = 0;
	};

	struct FCueTrigger
	{
		ECueTriggerType Type = ECueTriggerType::Gaze;
		int Target = -1;                     // gaze
		double MinDwellMs = 0;               // gaze
		ECueMode Mode = ECueMode::Any;       // gaze
		EStateId State = EStateId::Threshold; // phase, telling
		EPhase Phase = EPhase::Advisory;     // phase
		double AtMs = 0;                     // phase
	};

	struct FCue
	{
		std::string Id;
		ECueKind Kind = ECueKind::Spoken;
		std::string Speaker; // cast id or "narrator"
		std::string TextKey;
		std::optional<std::string> CueId;
		int Origin = -1; // gaze target index
		std::vector<std::string> SourceIds;
		std::vector<int> SourceIndices;
		EEvidence Evidence = EEvidence::GospelText;
		FCueTrigger Trigger;
		bool bAllStates = false;
		std::array<bool, TimelineLength> States{};
		double DurationMs = 1;
		bool bBeforeSaying = false;
		std::optional<std::string> Note;
		std::optional<EGospel> Telling;   // telling cues
		std::optional<EVantage> Vantage;  // telling cues
	};

	/** Exertion model (see src/types.ts ExertionModel). */
	struct FExertionModel
	{
		double AngularDeadzoneDegPerSec = 30;
		double AngularScaleDegPerSec = 150;
		double LinearDeadzoneCmPerSec = 10;
		double LinearScaleCmPerSec = 60;
		double VoiceDeadzone = 0.15;
		double RisePerSec = 0.1;
		double RecoveryPerSec = 0.04;
		double SwoonRecoveryPerSec = 0.05;
		double WakeAt = 0.35;
		double MinSwoonMs = 15000;
		double SayingCost = 0.1;
		EEvidence Evidence = EEvidence::ArtisticInterpretation;
		std::string Disclosure;
	};

	struct FRacingThoughtsModel
	{
		std::array<double, StressLevelCount> StressWeights{};
		EStress NoTargetStress = EStress::Low;
		double DwellTauMs = 3000;
		double WitnessEchoScale = 0.35;
		double WitnessEchoTauMs = 2500;
		double ComfortGain = 0.04;
		double ComfortReleaseTauMs = 4000;
		double AffinityMultiplier = 1;
		std::vector<FDwellTier> DwellTiers;
		double RateMinPerSec = 0;
		double RateMaxPerSec = 0;
		double RateGamma = 1;
		int MaxVoices = 1;
		int RecentWindow = 1;
		double DuckStartAt = 1;
		double DuckMaxDb = 0;
		EEvidence Evidence = EEvidence::ArtisticInterpretation;
		std::string Disclosure;
	};

	struct FManifest
	{
		std::string SchemaVersion;
		std::string Id;
		std::string Title;
		std::string Edition;
		std::string HarmonizationNote;
		std::vector<FCastMember> Cast;
		std::vector<FSourceRef> Sources;
		std::vector<FGazeTargetDef> GazeTargets;
		std::vector<FWitnessVantageDef> WitnessVantages;
		FRacingThoughtsModel RacingThoughts;
		FExertionModel Exertion;
		std::vector<FFragment> Fragments;
		std::vector<FCue> Cues;
		std::vector<FTimelineStateDef> Timeline; // exactly TimelineLength, canonical order
		std::string AdvisoryTitleKey;
		std::string AdvisoryBodyKey;

		int FindGazeTarget(const std::string& Id) const;
		int FindSource(const std::string& Id) const;
		int FindFragment(const std::string& Id) const;
		int FindCue(const std::string& Id) const;
		int FindCast(const std::string& Id) const;
	};

	using FLocale = std::map<std::string, std::string>;

	/* ------------------------------------------------------------------ */
	/* Runtime state                                                       */
	/* ------------------------------------------------------------------ */

	struct FSettings
	{
		double IntensityCeiling = 1;
		bool bReducedEffects = false;
		bool bSeated = true;
		bool bHapticsEnabled = false; // this experience never uses pain haptics in any case
		bool bSwoon = true;           // false: exertion never takes consciousness
		// subtitles: always true (sayings are always captioned)
	};

	struct FSettingsPatch
	{
		std::optional<double> IntensityCeiling;
		std::optional<bool> ReducedEffects;
		std::optional<bool> Seated;
		std::optional<bool> HapticsEnabled;
		std::optional<bool> Swoon;
	};

	struct FVoice
	{
		int Fragment = -1;
		double StartedAt = 0;
		double EndsAt = 0;
	};

	struct FTimelineRuntime
	{
		int Index = 0;
		EPhase Phase = EPhase::Advisory;
		double PhaseElapsedMs = 0;
		double StateElapsedMs = 0;
		std::optional<ETriggerVia> TriggeredVia;
		double AttendedTargetMs = 0;
	};

	struct FRacingThoughtsRuntime
	{
		double Intensity = 0;
		double Gain = 1;
		double FloorBoost = 0;
		double SpawnAccumulator = 0;
		std::vector<FVoice> Voices;
		std::vector<int> Recent; // fragment indices, oldest first
	};

	struct FCueRuntime
	{
		std::vector<int> Played; // cue indices in the order they started
		bool bActive = false;
		int Active = -1;
		double StartedAt = 0;
		double EndsAt = 0;
	};

	struct FConsciousnessRuntime
	{
		double Exertion = 0;
		bool bSwoon = false;
		double SwoonSince = 0;
	};

	struct FSessionState
	{
		double T = 0;
		uint32_t Seed = 0;
		uint32_t Rng = 0;
		FTimelineRuntime Timeline;
		EMode Mode = EMode::Embodiment;
		EVantage Vantage = EVantage::BelovedDisciple;
		ELayer Layer = ELayer::Historical;
		bool bComfort = false;
		bool bPaused = false;
		bool bExited = false;
		int EvidenceOpen = -1; // source index
		FSettings Settings;
		int GazeTarget = -1; // gaze target index
		double GazeDwellMs = 0;
		double Breath = 0;
		FRacingThoughtsRuntime Rt;
		FCueRuntime Cues;
		FConsciousnessRuntime Consciousness;
	};

	/* ------------------------------------------------------------------ */
	/* Inputs and events                                                   */
	/* ------------------------------------------------------------------ */

	struct FCommand
	{
		ECommandType Type = ECommandType::Pause;
		ETriggerVia Via = ETriggerVia::Select;     // TriggerSaying (Select or Speech)
		EMode Mode = EMode::Embodiment;            // SetMode
		std::optional<EVantage> Vantage;           // SetMode
		ELayer Layer = ELayer::Historical;         // SetLayer
		bool bOn = false;                          // SetComfort
		std::string SourceId;                      // OpenEvidence (may be unknown: rejected)
		FSettingsPatch Settings;                   // SetSettings

		static FCommand Make(ECommandType T) { FCommand C; C.Type = T; return C; }
	};

	struct FTickInput
	{
		int Gaze = -1; // gaze target index, -1 = null
		std::optional<double> Breath;
		std::optional<double> HeadDegPerSec;
		std::optional<double> HeadCmPerSec;
		std::optional<double> Voice;
		std::vector<FCommand> Commands;
	};

	struct FEvent
	{
		double T = 0;
		EEventType Type = EEventType::Exit;
		EStateId State = EStateId::Threshold;   // TimelineEnter, TimelinePhase
		EPhase Phase = EPhase::Advisory;         // TimelinePhase
		EStateId Saying = EStateId::Forgiveness; // SayingTrigger
		ETriggerVia Via = ETriggerVia::Auto;     // SayingTrigger
		EMode Mode = EMode::Embodiment;          // ModeChange
		EVantage Vantage = EVantage::BelovedDisciple;
		bool bForced = false;
		ELayer Layer = ELayer::Historical;       // LayerChange
		bool bOn = false;                        // ComfortChange
		bool bPaused = false;                    // PauseChange
		int SourceIndex = -1;                    // EvidenceChange (-1 = null)
		FSettings Settings;                      // SettingsChange
		int Fragment = -1;                       // RtFragment
		EFragmentKind FragmentKind = EFragmentKind::Verbal;
		int Voice = 0;
		ECommandType Command = ECommandType::Pause; // CommandRejected
		std::string Reason;
		int Cue = -1;                            // CueStart
		bool bConscious = true;                  // ConsciousnessChange
		EConsciousnessCause Cause = EConsciousnessCause::Exertion;
	};

	struct FStepResult
	{
		FSessionState State;
		std::vector<FEvent> Events;
	};
}
