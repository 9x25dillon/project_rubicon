using UnrealBuildTool;

// Deterministic narrative core (Public/NarrativeCore, Private/NarrativeCore: standard C++17, no UObject) plus a
// thin UE layer: Blueprint types, data assets and the fixed-step world subsystem.
public class RubiconNarrativeCore : ModuleRules
{
	public RubiconNarrativeCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// NarrativeCore translation units set floating-point pragmas (no FMA contraction); keep each TU
		// separate so those pragmas never leak into, or get skipped by, unity-merged engine-facing code.
		bUseUnity = false;
		bEnableExceptions = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
