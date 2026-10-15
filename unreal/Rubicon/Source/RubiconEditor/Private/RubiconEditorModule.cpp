#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "RubiconEpisodeImporter.h"

DEFINE_LOG_CATEGORY_STATIC(LogRubiconEditor, Log, All);

class FRubiconEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rubicon.ImportEpisode"),
			TEXT("Validate content/passion.episode.json + content/locale.en.json and import them into /Game/Rubicon/Data. Args: [ManifestPath] [LocalePath]"),
			FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				FString Report;
				const bool bOk = URubiconEpisodeImporter::ImportEpisode(Args.IsValidIndex(0) ? Args[0] : FString(), Args.IsValidIndex(1) ? Args[1] : FString(), Report);
				TArray<FString> Lines;
				Report.ParseIntoArrayLines(Lines);
				for (const FString& L : Lines) UE_LOG(LogRubiconEditor, Display, TEXT("%s"), *L);
				UE_LOG(LogRubiconEditor, Display, TEXT("Rubicon.ImportEpisode: %s"), bOk ? TEXT("OK") : TEXT("FAILED"));
			}),
			ECVF_Default));

		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rubicon.CreateGolgothaMap"),
			TEXT("Create /Game/Rubicon/Maps/Golgotha containing the greybox actor."),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				FString Report;
				const bool bOk = URubiconEpisodeImporter::CreateGolgothaMap(Report);
				UE_LOG(LogRubiconEditor, Display, TEXT("Rubicon.CreateGolgothaMap: %s %s"), bOk ? TEXT("OK") : TEXT("FAILED"), *Report);
			}),
			ECVF_Default));
	}

	virtual void ShutdownModule() override
	{
		for (IConsoleObject* C : Commands) IConsoleManager::Get().UnregisterConsoleObject(C);
		Commands.Reset();
	}

private:
	TArray<IConsoleObject*> Commands;
};

IMPLEMENT_MODULE(FRubiconEditorModule, RubiconEditor)
