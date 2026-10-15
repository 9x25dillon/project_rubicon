using UnrealBuildTool;

// Engine adapter: greybox, embodied pawn, gaze resolution, debug HUD. Reads FRubiconFrameOutputs, writes only
// inputs (gaze, breath, commands) to the narrative subsystem.
public class Rubicon : ModuleRules
{
	public Rubicon(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "HeadMountedDisplay", "UMG",
			"DeveloperSettings", "RubiconNarrativeCore"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
