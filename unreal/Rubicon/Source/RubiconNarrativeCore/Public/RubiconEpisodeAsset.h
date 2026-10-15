// Data assets imported from content/passion.episode.json and content/locale.en.json (editor: Rubicon.ImportEpisode).
//
// The canonical JSON text is stored verbatim and is what the runtime compiles, through the same code path the
// parity tests exercise, so the shipped manifest is byte-for-byte the validated one. The typed arrays are a
// derived, read-only view for the editor details panel and for Blueprint queries.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RubiconNarrativeTypes.h"
#include "NarrativeCore/RubiconCoreTypes.h"
#include "RubiconEpisodeAsset.generated.h"

UCLASS(BlueprintType)
class RUBICONNARRATIVECORE_API URubiconLocaleAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourcePath;

	UPROPERTY(VisibleAnywhere, Category = "Source", meta = (MultiLine = true))
	FString LocaleJson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Strings")
	TMap<FString, FString> Strings;

	/** Parse LocaleJson and rebuild Strings. */
	bool RebuildFromJson(TArray<FString>& OutErrors);

	UFUNCTION(BlueprintPure, Category = "Rubicon")
	FString Lookup(const FString& Key) const;
};

UCLASS(BlueprintType)
class RUBICONNARRATIVECORE_API URubiconEpisodeAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Source")
	FString SourcePath;

	/** Canonical manifest text (single source of truth: edit content/passion.episode.json and re-import). */
	UPROPERTY(VisibleAnywhere, Category = "Source", meta = (MultiLine = true))
	FString ManifestJson;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TObjectPtr<URubiconLocaleAsset> Locale;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") FString EpisodeId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") FString Title;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") FString Edition;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") FString SchemaVersion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") TArray<FRubiconStateInfo> Timeline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") TArray<FRubiconGazeTargetInfo> GazeTargets;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") TArray<FRubiconSourceInfo> Sources;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") TArray<FRubiconFragmentInfo> Fragments;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") TArray<FRubiconCueInfo> Cues;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Episode") TArray<FString> ValidationWarnings;

	/** Validate and compile the stored JSON into the runtime manifest (the same path the parity tests use). */
	bool CompileCore(RubiconCore::FManifest& OutManifest, RubiconCore::FLocale& OutLocale, TArray<FString>& OutErrors,
		TArray<FString>* OutWarnings = nullptr) const;

	/** Recompile and refresh the derived, read-only view (editor import). */
	bool RebuildViews(TArray<FString>& OutErrors);

	UFUNCTION(BlueprintPure, Category = "Rubicon")
	bool FindGazeTarget(FName Id, FRubiconGazeTargetInfo& OutInfo) const;
};
