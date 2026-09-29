// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class Diagnostics : ModuleRules
{
	public Diagnostics(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
