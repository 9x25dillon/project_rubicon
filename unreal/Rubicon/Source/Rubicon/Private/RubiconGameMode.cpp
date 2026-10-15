#include "RubiconGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RubiconDebugPanelComponent.h"
#include "RubiconEmbodiedPawn.h"
#include "RubiconEpisodeAsset.h"
#include "RubiconGolgothaGreybox.h"
#include "RubiconNarrativeSubsystem.h"
#include "RubiconSettings.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRubiconGame, Log, All);

ARubiconGameMode::ARubiconGameMode()
{
	DefaultPawnClass = ARubiconEmbodiedPawn::StaticClass();
	HUDClass = ARubiconDebugHUD::StaticClass();
}

void ARubiconGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	EnsureGreybox();
}

ARubiconGolgothaGreybox* ARubiconGameMode::EnsureGreybox()
{
	if (Greybox) return Greybox;
	for (TActorIterator<ARubiconGolgothaGreybox> It(GetWorld()); It; ++It)
	{
		Greybox = *It;
		return Greybox;
	}
	// Any empty map works: the site is built in code.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Greybox = GetWorld()->SpawnActor<ARubiconGolgothaGreybox>(ARubiconGolgothaGreybox::StaticClass(), FTransform::Identity, Params);
	UE_LOG(LogRubiconGame, Log, TEXT("no greybox in the level; spawned one"));
	return Greybox;
}

AActor* ARubiconGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return EnsureGreybox();
}

APawn* ARubiconGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// The embodied pawn always starts at the eye point on the cross, facing the way the crucified faces.
	const FTransform Eye = EnsureGreybox() ? Greybox->GetEmbodimentEyeTransform() : SpawnTransform;
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Eye);
}

URubiconEpisodeAsset* ARubiconGameMode::LoadEpisode()
{
	const URubiconSettings* S = URubiconSettings::Get();
	if (URubiconEpisodeAsset* Asset = Cast<URubiconEpisodeAsset>(S->Episode.TryLoad())) return Asset;

#if !UE_BUILD_SHIPPING
	// Development fallback: read the repository content directly (same validation and compile path).
	const FString Content = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("content")));
	URubiconLocaleAsset* LocaleAsset = NewObject<URubiconLocaleAsset>(this, TEXT("TransientLocale"), RF_Transient);
	URubiconEpisodeAsset* EpisodeAsset = NewObject<URubiconEpisodeAsset>(this, TEXT("TransientEpisode"), RF_Transient);
	TArray<FString> Errors;
	if (FFileHelper::LoadFileToString(LocaleAsset->LocaleJson, *FPaths::Combine(Content, TEXT("locale.en.json"))) &&
		FFileHelper::LoadFileToString(EpisodeAsset->ManifestJson, *FPaths::Combine(Content, TEXT("passion.episode.json"))) &&
		LocaleAsset->RebuildFromJson(Errors))
	{
		EpisodeAsset->Locale = LocaleAsset;
		if (EpisodeAsset->RebuildViews(Errors))
		{
			UE_LOG(LogRubiconGame, Warning, TEXT("episode asset not imported; using %s directly (run Rubicon.ImportEpisode)"), *Content);
			return EpisodeAsset;
		}
	}
	for (const FString& E : Errors) UE_LOG(LogRubiconGame, Error, TEXT("%s"), *E);
#endif
	return nullptr;
}

void ARubiconGameMode::StartPlay()
{
	Super::StartPlay();
	EnsureGreybox();
	URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>();
	Episode = LoadEpisode();
	if (!N || !Episode)
	{
		UE_LOG(LogRubiconGame, Error, TEXT("cannot start the session: %s"), N ? TEXT("no episode (run Rubicon.ImportEpisode)") : TEXT("no narrative subsystem"));
		return;
	}
	if (Greybox)
	{
		Greybox->EnsureGazeTargets();
		const TArray<FName> Missing = Greybox->FindMissingGazeTargets(Episode);
		for (const FName& Id : Missing) UE_LOG(LogRubiconGame, Error, TEXT("gaze target '%s' has no tagged actor (Gaze.Target.%s)"), *Id.ToString(), *Id.ToString());
	}
	const URubiconSettings* S = URubiconSettings::Get();
	N->OnExit.AddDynamic(this, &ARubiconGameMode::HandleExit);
	N->StartSession(Episode, S->DefaultSeed, S->DefaultPlayerSettings);
}

void ARubiconGameMode::HandleExit(double TimeMs)
{
	APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0);
	if (Pc && Pc->PlayerCameraManager) Pc->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.5f, FLinearColor::Black, true, true);
	if (!URubiconSettings::Get()->bQuitOnExit) return;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	}), 0.6f, false);
}
