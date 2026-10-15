// Project settings (Project Settings > Game > Rubicon). Stored in Config/DefaultGame.ini.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RubiconNarrativeTypes.h"
#include "RubiconSettings.generated.h"

class URubiconEpisodeAsset;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Rubicon"))
class RUBICON_API URubiconSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Imported by Rubicon.ImportEpisode. In editor builds, a missing asset falls back to reading content/*.json. */
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (AllowedClasses = "/Script/RubiconNarrativeCore.RubiconEpisodeAsset"))
	FSoftObjectPath Episode;

	UPROPERTY(Config, EditAnywhere, Category = "Session")
	int32 DefaultSeed = 1;

	UPROPERTY(Config, EditAnywhere, Category = "Session")
	FRubiconSettings DefaultPlayerSettings;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bStartInDebugHUD = true;

	/** Exit ends the application after a short fade. Off: exit fades and holds (useful in PIE). */
	UPROPERTY(Config, EditAnywhere, Category = "Safeguards")
	bool bQuitOnExit = true;

	/**
	 * Off (default): Witness moves the player by position only; the camera is never rotated by code, so the player
	 * turns their own head toward the cross. On: during the fully black part of the Witness fade, yaw is aligned to
	 * face the cross (a snap, never a rotation the player sees). See CLAUDE.md open decisions.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Safeguards")
	bool bAlignYawOnWitness = false;

	static const URubiconSettings* Get() { return GetDefault<URubiconSettings>(); }
};
