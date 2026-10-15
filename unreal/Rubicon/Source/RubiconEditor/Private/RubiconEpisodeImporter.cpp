#include "RubiconEpisodeImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "LevelEditorSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RubiconConvert.h"
#include "RubiconEpisodeAsset.h"
#include "RubiconGolgothaGreybox.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "NarrativeCore/RubiconContent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRubiconImport, Log, All);

using namespace RubiconEditorPaths;

namespace RubiconImportDetail
{
	template <typename T>
	T* FindOrCreateAsset(const FString& Folder, const FString& Name, UPackage*& OutPackage, bool& bOutCreated)
	{
		const FString PackageName = Folder / Name;
		OutPackage = nullptr;
		bOutCreated = false;
		if (FPackageName::DoesPackageExist(PackageName))
		{
			OutPackage = LoadPackage(nullptr, *PackageName, LOAD_None);
		}
		if (!OutPackage) OutPackage = CreatePackage(*PackageName);
		OutPackage->FullyLoad();
		T* Asset = FindObject<T>(OutPackage, *Name);
		if (!Asset)
		{
			Asset = NewObject<T>(OutPackage, T::StaticClass(), *Name, RF_Public | RF_Standalone | RF_Transactional);
			bOutCreated = true;
		}
		return Asset;
	}

	bool Save(UPackage* Package, UObject* Asset, FString& Report)
	{
		Package->MarkPackageDirty();
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.Error = GError;
		if (!UPackage::SavePackage(Package, Asset, *File, Args))
		{
			Report += FString::Printf(TEXT("failed to save %s\n"), *File);
			return false;
		}
		Report += FString::Printf(TEXT("saved %s\n"), *Package->GetName());
		return true;
	}
}

FString URubiconEpisodeImporter::DefaultManifestPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("content"), TEXT("passion.episode.json")));
}

FString URubiconEpisodeImporter::DefaultLocalePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("content"), TEXT("locale.en.json")));
}

bool URubiconEpisodeImporter::ImportEpisode(const FString& ManifestPathIn, const FString& LocalePathIn, FString& OutReport)
{
	using namespace RubiconImportDetail;
	const FString ManifestPath = ManifestPathIn.IsEmpty() ? DefaultManifestPath() : ManifestPathIn;
	const FString LocalePath = LocalePathIn.IsEmpty() ? DefaultLocalePath() : LocalePathIn;
	OutReport.Reset();

	FString ManifestJson, LocaleJson;
	if (!FFileHelper::LoadFileToString(ManifestJson, *ManifestPath) || !FFileHelper::LoadFileToString(LocaleJson, *LocalePath))
	{
		OutReport = FString::Printf(TEXT("cannot read %s or %s\n"), *ManifestPath, *LocalePath);
		return false;
	}

	// Validate before touching any asset: an invalid manifest never reaches the content browser.
	RubiconCore::FManifest M;
	RubiconCore::FLocale L;
	std::vector<std::string> Errors, Warnings;
	if (!RubiconCore::LoadEpisode(RubiconConvert::ToUtf8(ManifestJson), RubiconConvert::ToUtf8(LocaleJson), M, L, Errors, &Warnings))
	{
		for (const std::string& E : Errors) OutReport += TEXT("error ") + RubiconConvert::ToFString(E) + TEXT("\n");
		OutReport += TEXT("import aborted: manifest failed validation (fix content/passion.episode.json, run npm run validate)\n");
		return false;
	}
	for (const std::string& W : Warnings) OutReport += TEXT("warn  ") + RubiconConvert::ToFString(W) + TEXT("\n");

	UPackage* LocalePackage = nullptr;
	bool bLocaleCreated = false;
	URubiconLocaleAsset* LocaleAsset = FindOrCreateAsset<URubiconLocaleAsset>(DataFolder, LocaleAssetName, LocalePackage, bLocaleCreated);
	LocaleAsset->Modify();
	LocaleAsset->SourcePath = LocalePath;
	LocaleAsset->LocaleJson = LocaleJson;
	TArray<FString> AssetErrors;
	if (!LocaleAsset->RebuildFromJson(AssetErrors))
	{
		for (const FString& E : AssetErrors) OutReport += TEXT("error ") + E + TEXT("\n");
		return false;
	}

	UPackage* EpisodePackage = nullptr;
	bool bEpisodeCreated = false;
	URubiconEpisodeAsset* Episode = FindOrCreateAsset<URubiconEpisodeAsset>(DataFolder, EpisodeAssetName, EpisodePackage, bEpisodeCreated);
	Episode->Modify();
	Episode->SourcePath = ManifestPath;
	Episode->ManifestJson = ManifestJson;
	Episode->Locale = LocaleAsset;
	if (!Episode->RebuildViews(AssetErrors))
	{
		for (const FString& E : AssetErrors) OutReport += TEXT("error ") + E + TEXT("\n");
		return false;
	}

	if (bLocaleCreated) FAssetRegistryModule::AssetCreated(LocaleAsset);
	if (bEpisodeCreated) FAssetRegistryModule::AssetCreated(Episode);
	const bool bSaved = Save(LocalePackage, LocaleAsset, OutReport) && Save(EpisodePackage, Episode, OutReport);
	OutReport += FString::Printf(TEXT("%s %s: %d states, %d gaze targets, %d fragments, %d sources\n"), *Episode->Title, *Episode->Edition,
		Episode->Timeline.Num(), Episode->GazeTargets.Num(), Episode->Fragments.Num(), Episode->Sources.Num());
	return bSaved;
}

bool URubiconEpisodeImporter::CreateGolgothaMap(FString& OutReport)
{
	ULevelEditorSubsystem* LevelEditor = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
	UEditorActorSubsystem* Actors = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!LevelEditor || !Actors)
	{
		OutReport = TEXT("editor subsystems unavailable");
		return false;
	}
	if (!LevelEditor->NewLevel(MapPackage))
	{
		OutReport = FString::Printf(TEXT("could not create %s (does it already exist?)"), MapPackage);
		return false;
	}
	ARubiconGolgothaGreybox* Greybox = Cast<ARubiconGolgothaGreybox>(Actors->SpawnActorFromClass(ARubiconGolgothaGreybox::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Greybox)
	{
		OutReport = TEXT("could not spawn the greybox actor");
		return false;
	}
	// Persist the tagged gaze actors and the embodied body in the level so designers can move them.
	const int32 Spawned = Greybox->EnsureGazeTargets();
	const bool bSaved = LevelEditor->SaveCurrentLevel();
	OutReport = FString::Printf(TEXT("%s %s with ARubiconGolgothaGreybox and %d gaze/body actors. Set GameDefaultMap/EditorStartupMap in Config/DefaultEngine.ini to use it."),
		bSaved ? TEXT("created") : TEXT("created (unsaved)"), MapPackage, Spawned);
	return bSaved;
}
