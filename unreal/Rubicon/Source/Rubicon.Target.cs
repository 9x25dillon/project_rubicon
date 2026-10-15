using UnrealBuildTool;

public class RubiconTarget : TargetRules
{
	public RubiconTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.AddRange(new string[] { "RubiconNarrativeCore", "Rubicon" });
	}
}
