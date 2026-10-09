// Copyright (c) 2026 Nelaric Contributors

#include "Scripting/DemoScriptSubsystem.h"

void UDemoScriptSubsystem::AcquireRuntime()
{
	check(IsInGameThread());
	++ActiveInstances;
	if (!Environment)
	{
		Environment = MakeUnique<PUERTS_NAMESPACE::FJsEnv>(TEXT("JavaScript"));
		Environment->Start(TEXT("Entry"), {{TEXT("Scripts"), this}});
	}
}

void UDemoScriptSubsystem::ReleaseRuntime()
{
	check(IsInGameThread());
	if (ActiveInstances > 0 && --ActiveInstances == 0)
	{
		// Puerts restores the original UFunctions when its mixin runtime ends.
		StartBattlefrontHandler.Unbind();
		Environment.Reset();
	}
}

void UDemoScriptSubsystem::Deinitialize()
{
	StartBattlefrontHandler.Unbind();
	Environment.Reset();
	ActiveInstances = 0;
	Super::Deinitialize();
}
