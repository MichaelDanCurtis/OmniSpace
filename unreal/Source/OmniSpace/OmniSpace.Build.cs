using UnrealBuildTool;

public class OmniSpace : ModuleRules
{
	public OmniSpace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"HTTP", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore",
			"ProceduralMeshComponent", "Niagara"
			// "ChaosVehicles" (ChaosVehiclesPlugin, Experimental) is enabled in the .uproject but
			// nothing links against it yet; add it back here when vehicle code appears.
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });
	}
}
