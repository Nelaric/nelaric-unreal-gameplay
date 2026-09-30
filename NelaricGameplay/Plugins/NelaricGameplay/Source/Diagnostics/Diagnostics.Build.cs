// Copyright (c) 2026 Nelaric Contributors

using UnrealBuildTool;

public class Diagnostics : ModuleRules
{
	public Diagnostics(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		if (Target.Type != TargetType.Server)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "RenderCore", "RHI" });
		}
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "LevelEditor", "UnrealEd" });
		}
	}
}
