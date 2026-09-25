// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricCore : ModuleRules
{
	public NelaricCore(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.Add("Core");
	}
}
