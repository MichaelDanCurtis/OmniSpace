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
			"ProceduralMeshComponent", "Niagara", "ChaosVehicles"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });
	}
}
