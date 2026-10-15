using UnrealBuildTool;

public class RubiconEditor : ModuleRules
{
	public RubiconEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DataValidation" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "AssetRegistry", "AssetTools", "LevelEditor", "Slate", "SlateCore",
			"RubiconNarrativeCore", "Rubicon"
		});
	}
}
