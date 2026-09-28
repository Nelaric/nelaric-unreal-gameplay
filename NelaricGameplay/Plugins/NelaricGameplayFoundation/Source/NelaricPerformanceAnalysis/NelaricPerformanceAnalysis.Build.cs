// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricPerformanceAnalysis : ModuleRules
{
	public NelaricPerformanceAnalysis(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
