// Editor utility: import content/*.json into data assets, and build the Golgotha greybox map.
//
//   Console:   Rubicon.ImportEpisode [ManifestPath] [LocalePath]
//              Rubicon.CreateGolgothaMap
//   Blueprint: URubiconEpisodeImporter::ImportEpisode (callable from an Editor Utility Widget)
//
// The import refuses to write anything if the manifest fails validation (same rules as src/validate.ts).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RubiconEpisodeImporter.generated.h"

namespace RubiconEditorPaths
{
	inline const TCHAR* DataFolder = TEXT("/Game/Rubicon/Data");
	inline const TCHAR* EpisodeAssetName = TEXT("DA_PassionEpisode");
	inline const TCHAR* LocaleAssetName = TEXT("DA_Locale_en");
	inline const TCHAR* MapPackage = TEXT("/Game/Rubicon/Maps/Golgotha");
}

UCLASS()
class RUBICONEDITOR_API URubiconEpisodeImporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Repository content paths (<ProjectDir>/../../content/...). */
	static FString DefaultManifestPath();
	static FString DefaultLocalePath();

	/** Validate, then create or update DA_Locale_en and DA_PassionEpisode under /Game/Rubicon/Data and save them. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Editor", meta = (DevelopmentOnly))
	static bool ImportEpisode(const FString& ManifestPath, const FString& LocalePath, FString& OutReport);

	/** New level with the Golgotha greybox actor, saved to /Game/Rubicon/Maps/Golgotha. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Editor", meta = (DevelopmentOnly))
	static bool CreateGolgothaMap(FString& OutReport);
};
