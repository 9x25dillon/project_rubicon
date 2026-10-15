using UnrealBuildTool;

public class RubiconEditorTarget : TargetRules
{
	public RubiconEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.AddRange(new string[] { "RubiconNarrativeCore", "Rubicon", "RubiconEditor" });
	}
}
