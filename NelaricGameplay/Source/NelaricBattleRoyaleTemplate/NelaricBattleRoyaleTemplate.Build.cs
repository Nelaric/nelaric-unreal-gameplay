// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricBattleRoyaleTemplate : ModuleRules
{
	public NelaricBattleRoyaleTemplate(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NelaricFoundation" });
	}
}
