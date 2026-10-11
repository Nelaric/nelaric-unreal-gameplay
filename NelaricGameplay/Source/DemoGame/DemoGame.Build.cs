// Copyright (c) 2026 Nelaric Contributors

using UnrealBuildTool;

public class DemoGame : ModuleRules
{
	public DemoGame(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.Add("Core");
	}
}
