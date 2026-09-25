// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricGameplayCore : ModuleRules
{
	public NelaricGameplayCore(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PublicDependencyModuleNames.Add("NelaricFoundation");
	}
}
