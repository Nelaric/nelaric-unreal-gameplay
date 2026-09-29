// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class Benchmark : ModuleRules
{
	public Benchmark(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
