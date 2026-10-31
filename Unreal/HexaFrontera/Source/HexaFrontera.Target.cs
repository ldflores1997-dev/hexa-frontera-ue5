using UnrealBuildTool;

public class HexaFronteraTarget : TargetRules
{
	public HexaFronteraTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HexaFrontera");
	}
}
