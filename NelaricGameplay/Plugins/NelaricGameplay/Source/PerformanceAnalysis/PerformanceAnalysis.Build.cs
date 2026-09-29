// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class PerformanceAnalysis : ModuleRules
{
	public PerformanceAnalysis(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
