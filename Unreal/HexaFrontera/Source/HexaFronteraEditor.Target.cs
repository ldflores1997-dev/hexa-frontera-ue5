using UnrealBuildTool;

public class HexaFronteraEditorTarget : TargetRules
{
	public HexaFronteraEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HexaFrontera");
	}
}
