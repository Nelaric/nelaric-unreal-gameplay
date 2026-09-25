// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricGameplay : ModuleRules
{
	public NelaricGameplay(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.Add("Core");
	}
}
