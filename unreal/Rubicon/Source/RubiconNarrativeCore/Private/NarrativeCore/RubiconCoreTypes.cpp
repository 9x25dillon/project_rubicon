#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconCoreTypes.h"

#include <cstring>

namespace RubiconCore
{
	namespace TypesDetail
	{
		static const char* const EvidenceNames[] = { "gospel-text", "textual-variant", "historical-inference", "artistic-interpretation" };
		static const char* const StressNames[] = { "relief", "low", "medium", "high", "extreme" };
		static const char* const ModeNames[] = { "embodiment", "witness" };
		static const char* const LayerNames[] = { "historical", "theological" };
		static const char* const VantageNames[] = { "centurion", "beloved_disciple", "mary" };
		static const char* const KindNames[] = { "gate", "elevation", "saying", "timed", "terminal" };
		static const char* const PhaseNames[] = { "advisory", "raising", "settled", "approach", "utterance", "resonance", "running", "open" };
		static const char* const ViaNames[] = { "select", "speech", "auto" };
		static const char* const FragmentKindNames[] = { "verbal", "sonic" };
		static const char* const CategoryNames[] = { "scriptural-echo", "crowd-echo", "accusation", "memory-flash", "prayer-collapse", "dread", "sensory-intrusion" };
		static const char* const StateNames[] = { "threshold", "elevation", "forgiveness", "paradise", "mother", "abandonment", "thirst", "completion", "surrender", "release", "spear", "contemplation" };
		static const char* const CommandNames[] = { "acknowledgeAdvisory", "triggerSaying", "setMode", "setLayer", "setComfort", "pause", "resume", "exit", "openEvidence", "closeEvidence", "setSettings" };
		static const char* const EventNames[] = { "timeline.enter", "timeline.phase", "saying.trigger", "mode.change", "layer.change", "comfort.change", "pause.change", "exit", "evidence.change", "settings.change", "rt.fragment", "command.rejected", "cue.start" };
		static const char* const SourceTypeNames[] = { "gospel", "new-testament", "hebrew-bible", "ancient-text", "archaeology", "scholarship" };
		static const char* const RenderingNames[] = { "public-domain-translation", "own-translation", "paraphrase", "description" };
		static const char* const CueKindNames[] = { "spoken", "narration", "sound" };
		static const char* const CueModeNames[] = { "embodiment", "witness", "any" };

		template <typename E, size_t N>
		bool ParseTable(const std::string& S, const char* const (&Table)[N], E& Out)
		{
			for (size_t I = 0; I < N; ++I)
			{
				if (S == Table[I]) { Out = static_cast<E>(I); return true; }
			}
			return false;
		}
	}

	const char* ToString(EEvidence V) { return TypesDetail::EvidenceNames[static_cast<int>(V)]; }
	const char* ToString(EStress V) { return TypesDetail::StressNames[static_cast<int>(V)]; }
	const char* ToString(EMode V) { return TypesDetail::ModeNames[static_cast<int>(V)]; }
	const char* ToString(ELayer V) { return TypesDetail::LayerNames[static_cast<int>(V)]; }
	const char* ToString(EVantage V) { return TypesDetail::VantageNames[static_cast<int>(V)]; }
	const char* ToString(EStateKind V) { return TypesDetail::KindNames[static_cast<int>(V)]; }
	const char* ToString(EPhase V) { return TypesDetail::PhaseNames[static_cast<int>(V)]; }
	const char* ToString(ETriggerVia V) { return TypesDetail::ViaNames[static_cast<int>(V)]; }
	const char* ToString(EFragmentKind V) { return TypesDetail::FragmentKindNames[static_cast<int>(V)]; }
	const char* ToString(EFragmentCategory V) { return TypesDetail::CategoryNames[static_cast<int>(V)]; }
	const char* ToString(EStateId V) { return TypesDetail::StateNames[static_cast<int>(V)]; }
	const char* ToString(ECommandType V) { return TypesDetail::CommandNames[static_cast<int>(V)]; }
	const char* ToString(EEventType V) { return TypesDetail::EventNames[static_cast<int>(V)]; }
	const char* ToString(ESourceType V) { return TypesDetail::SourceTypeNames[static_cast<int>(V)]; }
	const char* ToString(ERendering V) { return TypesDetail::RenderingNames[static_cast<int>(V)]; }
	const char* ToString(ECueKind V) { return TypesDetail::CueKindNames[static_cast<int>(V)]; }
	const char* ToString(ECueMode V) { return TypesDetail::CueModeNames[static_cast<int>(V)]; }

	bool Parse(const std::string& S, EEvidence& Out) { return TypesDetail::ParseTable(S, TypesDetail::EvidenceNames, Out); }
	bool Parse(const std::string& S, EStress& Out) { return TypesDetail::ParseTable(S, TypesDetail::StressNames, Out); }
	bool Parse(const std::string& S, EMode& Out) { return TypesDetail::ParseTable(S, TypesDetail::ModeNames, Out); }
	bool Parse(const std::string& S, ELayer& Out) { return TypesDetail::ParseTable(S, TypesDetail::LayerNames, Out); }
	bool Parse(const std::string& S, EVantage& Out) { return TypesDetail::ParseTable(S, TypesDetail::VantageNames, Out); }
	bool Parse(const std::string& S, EStateKind& Out) { return TypesDetail::ParseTable(S, TypesDetail::KindNames, Out); }
	bool Parse(const std::string& S, EPhase& Out) { return TypesDetail::ParseTable(S, TypesDetail::PhaseNames, Out); }
	bool Parse(const std::string& S, ETriggerVia& Out) { return TypesDetail::ParseTable(S, TypesDetail::ViaNames, Out); }
	bool Parse(const std::string& S, EFragmentKind& Out) { return TypesDetail::ParseTable(S, TypesDetail::FragmentKindNames, Out); }
	bool Parse(const std::string& S, EFragmentCategory& Out) { return TypesDetail::ParseTable(S, TypesDetail::CategoryNames, Out); }
	bool Parse(const std::string& S, EStateId& Out) { return TypesDetail::ParseTable(S, TypesDetail::StateNames, Out); }
	bool Parse(const std::string& S, ECommandType& Out) { return TypesDetail::ParseTable(S, TypesDetail::CommandNames, Out); }
	bool Parse(const std::string& S, ESourceType& Out) { return TypesDetail::ParseTable(S, TypesDetail::SourceTypeNames, Out); }
	bool Parse(const std::string& S, ERendering& Out) { return TypesDetail::ParseTable(S, TypesDetail::RenderingNames, Out); }
	bool Parse(const std::string& S, ECueKind& Out) { return TypesDetail::ParseTable(S, TypesDetail::CueKindNames, Out); }
	bool Parse(const std::string& S, ECueMode& Out) { return TypesDetail::ParseTable(S, TypesDetail::CueModeNames, Out); }

	bool IsNonScriptural(ESourceType V)
	{
		return V == ESourceType::AncientText || V == ESourceType::Archaeology || V == ESourceType::Scholarship;
	}

	EPhase FirstPhase(EStateKind Kind)
	{
		switch (Kind)
		{
		case EStateKind::Gate: return EPhase::Advisory;
		case EStateKind::Elevation: return EPhase::Raising;
		case EStateKind::Saying: return EPhase::Approach;
		case EStateKind::Timed: return EPhase::Running;
		case EStateKind::Terminal: return EPhase::Open;
		}
		return EPhase::Open;
	}

	int FManifest::FindGazeTarget(const std::string& InId) const
	{
		for (size_t I = 0; I < GazeTargets.size(); ++I) if (GazeTargets[I].Id == InId) return static_cast<int>(I);
		return -1;
	}

	int FManifest::FindSource(const std::string& InId) const
	{
		for (size_t I = 0; I < Sources.size(); ++I) if (Sources[I].Id == InId) return static_cast<int>(I);
		return -1;
	}

	int FManifest::FindFragment(const std::string& InId) const
	{
		for (size_t I = 0; I < Fragments.size(); ++I) if (Fragments[I].Id == InId) return static_cast<int>(I);
		return -1;
	}

	int FManifest::FindCue(const std::string& InId) const
	{
		for (size_t I = 0; I < Cues.size(); ++I) if (Cues[I].Id == InId) return static_cast<int>(I);
		return -1;
	}

	int FManifest::FindCast(const std::string& InId) const
	{
		for (size_t I = 0; I < Cast.size(); ++I) if (Cast[I].Id == InId) return static_cast<int>(I);
		return -1;
	}
}
