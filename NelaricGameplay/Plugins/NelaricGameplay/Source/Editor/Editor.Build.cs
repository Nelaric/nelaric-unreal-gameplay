// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class Editor : ModuleRules
{
	public Editor(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
