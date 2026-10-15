// Plain-text rendering of FRubiconFrameOutputs for the debug HUD (desktop canvas and the in-headset panel).

#pragma once

#include "CoreMinimal.h"
#include "RubiconNarrativeTypes.h"

namespace RubiconDebugFormat
{
	/** ASCII bar, e.g. Bar(0.42, 20) == "[########------------]". */
	RUBICON_API FString Bar(double Value01, int32 Width);
	RUBICON_API TArray<FString> Lines(const FRubiconFrameOutputs& F);
	/** Greedy word wrap for the canvas HUD. */
	RUBICON_API TArray<FString> Wrap(const FString& Text, int32 Columns);
}
