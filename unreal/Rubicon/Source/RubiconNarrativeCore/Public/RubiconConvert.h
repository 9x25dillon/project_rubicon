// Core <-> engine conversions. The only place std types meet UE types.

#pragma once

#include "CoreMinimal.h"
#include "RubiconNarrativeTypes.h"
#include "NarrativeCore/RubiconCoreTypes.h"
#include "NarrativeCore/RubiconFrame.h"

#include <string>

namespace RubiconConvert
{
	RUBICONNARRATIVECORE_API FString ToFString(const std::string& Utf8);
	RUBICONNARRATIVECORE_API std::string ToUtf8(const FString& S);

	inline ERubiconTimelineState ToUE(RubiconCore::EStateId V) { return static_cast<ERubiconTimelineState>(V); }
	inline ERubiconPhase ToUE(RubiconCore::EPhase V) { return static_cast<ERubiconPhase>(V); }
	inline ERubiconExperienceMode ToUE(RubiconCore::EMode V) { return static_cast<ERubiconExperienceMode>(V); }
	inline ERubiconVisualLayer ToUE(RubiconCore::ELayer V) { return static_cast<ERubiconVisualLayer>(V); }
	inline ERubiconWitnessVantage ToUE(RubiconCore::EVantage V) { return static_cast<ERubiconWitnessVantage>(V); }
	inline ERubiconEvidence ToUE(RubiconCore::EEvidence V) { return static_cast<ERubiconEvidence>(V); }
	inline ERubiconStress ToUE(RubiconCore::EStress V) { return static_cast<ERubiconStress>(V); }
	inline ERubiconTriggerVia ToUE(RubiconCore::ETriggerVia V) { return static_cast<ERubiconTriggerVia>(V); }
	inline ERubiconFragmentKind ToUE(RubiconCore::EFragmentKind V) { return static_cast<ERubiconFragmentKind>(V); }
	inline ERubiconElevationTransition ToUE(RubiconCore::EElevationTransition V) { return static_cast<ERubiconElevationTransition>(V); }
	inline ERubiconSourceType ToUE(RubiconCore::ESourceType V) { return static_cast<ERubiconSourceType>(V); }
	inline ERubiconCueKind ToUE(RubiconCore::ECueKind V) { return static_cast<ERubiconCueKind>(V); }

	inline RubiconCore::EMode ToCore(ERubiconExperienceMode V) { return static_cast<RubiconCore::EMode>(V); }
	inline RubiconCore::ELayer ToCore(ERubiconVisualLayer V) { return static_cast<RubiconCore::ELayer>(V); }
	inline RubiconCore::EVantage ToCore(ERubiconWitnessVantage V) { return static_cast<RubiconCore::EVantage>(V); }
	inline RubiconCore::ETriggerVia ToCore(ERubiconTriggerVia V) { return static_cast<RubiconCore::ETriggerVia>(V); }

	RUBICONNARRATIVECORE_API FRubiconSettings ToUE(const RubiconCore::FSettings& S);
	RUBICONNARRATIVECORE_API RubiconCore::FSettingsPatch ToPatch(const FRubiconSettings& S);
	RUBICONNARRATIVECORE_API FRubiconFragmentInfo FragmentInfo(const RubiconCore::FManifest& M, const RubiconCore::FLocale& L, int FragmentIndex);
	RUBICONNARRATIVECORE_API FName GazeTargetName(const RubiconCore::FManifest& M, int Index);
	RUBICONNARRATIVECORE_API FRubiconCueInfo CueInfo(const RubiconCore::FManifest& M, const RubiconCore::FLocale& L, int CueIndex);
	/** Gaze.Target.<id> */
	RUBICONNARRATIVECORE_API FName GazeTagFor(const FString& TargetId);
	RUBICONNARRATIVECORE_API void ToUE(const RubiconCore::FManifest& M, const RubiconCore::FFrameOutputs& In, int64 Tick, FRubiconFrameOutputs& Out);
}
