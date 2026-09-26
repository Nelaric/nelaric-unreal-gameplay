// Copyright (c) 2026 Nelaric

using UnrealBuildTool;

public class NelaricFoundation : ModuleRules
{
	public NelaricFoundation(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(
			new string[] { "Core", "CoreUObject", "Engine", "NelaricCore", "OnlineSubsystemUtils" }
		);
	}
}
