// Copyright (c) 2026 Nelaric Contributors

using UnrealBuildTool;

public class GameplayAbilitiesIntegration : ModuleRules
{
	public GameplayAbilitiesIntegration(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayRuntime",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
			}
		);
		PrivateDependencyModuleNames.Add("NetCore");
	}
}
