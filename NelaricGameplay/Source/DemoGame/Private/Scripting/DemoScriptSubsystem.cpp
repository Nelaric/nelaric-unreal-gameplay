// Copyright (c) 2026 Nelaric Contributors

#include "Scripting/DemoScriptSubsystem.h"

void UDemoScriptSubsystem::AcquireRuntime()
{
	check(IsInGameThread());
	++ActiveInstances;
	if (!Environment)
	{
		Environment = MakeUnique<PUERTS_NAMESPACE::FJsEnv>(TEXT("JavaScript"));
		Environment->Start(TEXT("Entry"));
	}
}

void UDemoScriptSubsystem::ReleaseRuntime()
{
	check(IsInGameThread());
	if (ActiveInstances > 0 && --ActiveInstances == 0)
	{
		// Puerts restores the original UFunctions when its mixin runtime ends.
		Environment.Reset();
	}
}

void UDemoScriptSubsystem::Deinitialize()
{
	Environment.Reset();
	ActiveInstances = 0;
	Super::Deinitialize();
}
