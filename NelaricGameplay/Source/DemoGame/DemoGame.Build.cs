// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class DemoGame : ModuleRules
{
	public DemoGame(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(
			new string[] { "Core", "CoreUObject", "Engine", "GameplayRuntime", "EnhancedInput", "GameplayTags" }
		);
	}
}
