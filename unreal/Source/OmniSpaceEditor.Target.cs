using UnrealBuildTool;
using System.Collections.Generic;

public class OmniSpaceEditorTarget : TargetRules
{
	public OmniSpaceEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "OmniSpace" });
	}
}
