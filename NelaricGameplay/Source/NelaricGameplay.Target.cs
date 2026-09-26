// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricGameplayTarget : TargetRules
{
	public NelaricGameplayTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.AddRange(
			new string[]
			{
				"NelaricOpenWorldTemplate",
				"NelaricBattleRoyaleTemplate",
				"NelaricMobaTemplate",
				"NelaricSandboxTemplate",
			}
		);
	}
}
