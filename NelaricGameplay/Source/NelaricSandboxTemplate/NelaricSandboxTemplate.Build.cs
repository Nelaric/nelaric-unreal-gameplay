// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricSandboxTemplate : ModuleRules
{
	public NelaricSandboxTemplate(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NelaricFoundation" });
	}
}
