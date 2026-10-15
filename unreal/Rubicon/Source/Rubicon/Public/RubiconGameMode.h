// Starts the vertical slice: ensures the greybox exists, spawns the embodied pawn at the eye point on the
// cross, compiles the episode and starts the narrative session, and handles Exit.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RubiconGameMode.generated.h"

class ARubiconGolgothaGreybox;
class URubiconEpisodeAsset;

UCLASS()
class RUBICON_API ARubiconGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARubiconGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
	UPROPERTY(Transient) TObjectPtr<ARubiconGolgothaGreybox> Greybox;
	UPROPERTY(Transient) TObjectPtr<URubiconEpisodeAsset> Episode;

	ARubiconGolgothaGreybox* EnsureGreybox();
	URubiconEpisodeAsset* LoadEpisode();

	UFUNCTION() void HandleExit(double TimeMs);
};
