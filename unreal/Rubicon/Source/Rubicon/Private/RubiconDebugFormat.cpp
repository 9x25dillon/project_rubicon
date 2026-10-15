#include "RubiconDebugFormat.h"

namespace RubiconDebugFormat
{
	template <typename E>
	FString EnumName(E Value)
	{
		return StaticEnum<E>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	FString Bar(double Value01, int32 Width)
	{
		const int32 N = FMath::Clamp(FMath::RoundToInt32(FMath::Clamp(Value01, 0.0, 1.0) * Width), 0, Width);
		return TEXT("[") + FString::ChrN(N, TEXT('#')) + FString::ChrN(Width - N, TEXT('-')) + TEXT("]");
	}

	TArray<FString> Lines(const FRubiconFrameOutputs& F)
	{
		TArray<FString> L;
		if (!F.bValid)
		{
			L.Add(TEXT("RUBICON  no session (import the episode: Rubicon.ImportEpisode)"));
			return L;
		}
		const FRubiconRacingThoughtsFrame& Rt = F.RacingThoughts;
		L.Add(FString::Printf(TEXT("RUBICON  t %.2f s  tick %lld  %s / %s %3.0f%%%s%s"), F.TimeMs / 1000.0, F.Tick, *EnumName(F.State), *EnumName(F.Phase),
			F.PhaseProgress * 100.0, F.bPaused ? TEXT("  PAUSED") : TEXT(""), F.bExited ? TEXT("  EXITED") : TEXT("")));
		L.Add(FString::Printf(TEXT("mode %s (%s)  layer %s  comfort %s  elevation %s"), *EnumName(F.Mode), *EnumName(F.Vantage), *EnumName(F.Layer),
			F.bComfort ? TEXT("ON") : TEXT("off"), *EnumName(F.ElevationTransition)));
		L.Add(FString::Printf(TEXT("RT output    %.4f %s"), Rt.Output, *Bar(Rt.Output, 30)));
		L.Add(FString::Printf(TEXT("RT intensity %.4f %s"), Rt.Intensity, *Bar(Rt.Intensity, 30)));
		L.Add(FString::Printf(TEXT("voices %d/%d  world duck %.1f dB  breath %.2f%s"), Rt.ActiveVoices.Num(), Rt.MaxVoices, F.ExternalDuckDb, F.BreathFocus,
			F.bDialogueActive ? TEXT("  DIALOGUE") : TEXT("")));
		L.Add(FString::Printf(TEXT("gaze %s (%s)  dwell %.1f s"), F.GazeTarget.IsNone() ? TEXT("-") : *F.GazeTarget.ToString(), *EnumName(F.GazeStress), F.GazeDwellMs / 1000.0));
		for (const FRubiconActiveVoice& V : Rt.ActiveVoices) L.Add(FString::Printf(TEXT("  voice %s  %.1f s"), *V.FragmentId.ToString(), V.ElapsedMs / 1000.0));
		if (F.bHasSaying)
		{
			L.Add(FString::Printf(TEXT("saying %s [%s] %s  target %s%s"), *EnumName(F.Saying.Id), *EnumName(F.Saying.Evidence), *FString::Join(F.Saying.Citations, TEXT("; ")),
				*F.Saying.GazeTarget.ToString(), F.Saying.bAttended ? TEXT(" (attended)") : TEXT("")));
			if (!F.Saying.SourceNote.IsEmpty()) L.Add(TEXT("  note: ") + F.Saying.SourceNote);
			if (F.Saying.bHasSubtitle) L.Add(TEXT("  \"") + F.Saying.Subtitle + TEXT("\""));
		}
		if (F.bHasAtmosphere)
		{
			L.Add(FString::Printf(TEXT("atmosphere %.0f lux  %.0f K  AOD %.2f [%s]"), F.Atmosphere.SunIlluminanceLux, F.Atmosphere.SkyCctK, F.Atmosphere.AerosolOpticalDepth,
				*EnumName(F.Atmosphere.Evidence)));
		}
		for (const FRubiconEvidenceItem& E : F.EvidenceOverlay) L.Add(FString::Printf(TEXT("  [%s] %s"), *E.Label, *E.Citation));
		L.Add(TEXT("trigger say/consent  A witness  B comfort  X pause  menu exit  Y layer  L-stick recenter"));
		return L;
	}

	TArray<FString> Wrap(const FString& Text, int32 Columns)
	{
		TArray<FString> Words, Out;
		Text.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& W : Words)
		{
			if (!Line.IsEmpty() && Line.Len() + 1 + W.Len() > Columns)
			{
				Out.Add(Line);
				Line.Reset();
			}
			Line += Line.IsEmpty() ? W : TEXT(" ") + W;
		}
		if (!Line.IsEmpty()) Out.Add(Line);
		return Out;
	}
}
