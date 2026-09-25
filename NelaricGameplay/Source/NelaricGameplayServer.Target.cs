// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricGameplayServerTarget : TargetRules
{
	public NelaricGameplayServerTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("NelaricGameplayCore");
	}
}
