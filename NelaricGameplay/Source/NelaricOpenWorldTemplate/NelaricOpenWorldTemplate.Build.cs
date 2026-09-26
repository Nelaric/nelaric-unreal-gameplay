// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricOpenWorldTemplate : ModuleRules
{
	public NelaricOpenWorldTemplate(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NelaricFoundation" });
	}
}
