// Copyright (c) 2026 Nelaric Contributors

using UnrealBuildTool;

public class GameplayRuntime : ModuleRules
{
	public GameplayRuntime(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"OnlineSubsystemUtils",
				"EnhancedInput",
				"GameplayTags",
				"StateTreeModule",
				"GameplayStateTreeModule",
				"AIModule",
			}
		);
	}
}
