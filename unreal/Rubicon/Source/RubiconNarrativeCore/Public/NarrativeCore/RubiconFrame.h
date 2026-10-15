// Frame selector: the single read-only view the engine adapter consumes each tick. Port of src/frame.ts.

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"

namespace RubiconCore
{
	enum class EElevationTransition : uint8_t { SlowTilt, FadeSettle };

	struct FEvidenceItem
	{
		EEvidence Category = EEvidence::GospelText;
		std::string Citation;
		std::string Label;
		std::optional<ESourceType> SourceType; // set for manifest sources
		std::optional<std::string> Debate;
		std::optional<std::string> Note;
		std::vector<std::string> ManuscriptNotes;
		bool bHasManuscriptNotes = false;
	};

	struct FActiveVoice
	{
		int Fragment = -1;
		double ElapsedMs = 0;
	};

	struct FFrameSaying
	{
		EStateId Id = EStateId::Forgiveness;
		std::optional<std::string> Subtitle; // shown during utterance and resonance
		std::vector<std::string> Citations;
		EEvidence Evidence = EEvidence::GospelText;
		std::optional<std::string> SourceNote;
		int GazeTarget = -1;
		bool bAttended = false;
	};

	/** How a cue's words were rendered, when all its cited sources agree. */
	enum class ECueRendering : uint8_t { Kjv, PublicDomainTranslation, OwnTranslation, Paraphrase, Description, Mixed };
	const char* ToString(ECueRendering V);

	struct FCueFrame
	{
		int Cue = -1;
		ECueKind Kind = ECueKind::Spoken;
		std::string Speaker;
		std::string SpeakerLabel;
		std::string Subtitle;
		std::vector<std::string> Citations;
		EEvidence Evidence = EEvidence::GospelText;
		ECueRendering Rendering = ECueRendering::Kjv;
		int Origin = -1; // gaze target to spatialize from; -1 for the narrator
		double ElapsedMs = 0;
		double DurationMs = 0;
		std::optional<std::string> CueId;
	};

	struct FFrameOutputs
	{
		double T = 0;
		EStateId State = EStateId::Threshold;
		EPhase Phase = EPhase::Advisory;
		double PhaseProgress = 0; // [0,1] for timed phases, 0 for open phases
		EMode Mode = EMode::Embodiment;
		EVantage Vantage = EVantage::BelovedDisciple;
		ELayer Layer = ELayer::Historical;
		bool bComfort = false;
		bool bPaused = false;
		bool bExited = false;
		bool bEmbodimentAvailable = true;
		bool bControlPause = true, bControlExit = true, bControlWitness = true, bControlComfort = true; // always true while not exited

		EElevationTransition ElevationTransition = EElevationTransition::SlowTilt;
		std::optional<FAtmosphereKey> Atmosphere;
		bool bTheological = false;

		double RtIntensity = 0;
		double RtOutput = 0;
		int RtMaxVoices = 1;
		std::vector<FActiveVoice> ActiveVoices;
		double ExternalDuckDb = 0;
		bool bDialogueActive = false;
		double BreathFocus = 0;

		int GazeTarget = -1;
		double GazeDwellMs = 0;
		EStress GazeStress = EStress::Low;
		int GazeTier = 0;
		std::optional<FCueFrame> Cue;

		std::optional<FFrameSaying> Saying;
		std::vector<FEvidenceItem> EvidenceOverlay;
		int EvidenceOpen = -1;
	};

	std::vector<FEvidenceItem> EvidenceForState(const FManifest& M, const FSessionState& S, const FLocale* Locale);
	std::optional<FCueFrame> CueFrame(const FManifest& M, const FSessionState& S, const FLocale* Locale);
	FFrameOutputs SelectFrame(const FManifest& M, const FSessionState& S, const FLocale* Locale);
}
