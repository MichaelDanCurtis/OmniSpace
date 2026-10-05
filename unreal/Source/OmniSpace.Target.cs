using UnrealBuildTool;
using System.Collections.Generic;

public class OmniSpaceTarget : TargetRules
{
	public OmniSpaceTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "OmniSpace" });
	}
}
