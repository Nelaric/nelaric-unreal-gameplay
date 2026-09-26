// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricGameplayEditorTarget : TargetRules
{
	public NelaricGameplayEditorTarget(TargetInfo Target)
		: base(Target)
	{
		Type = TargetType.Editor;
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
