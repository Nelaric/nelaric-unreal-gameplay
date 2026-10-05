// Copyright (c) 2026 Nelaric Contributors

#include "GameplayRuntimeModule.h"
#include "ObjectPool/PoolNetworkRuntime.h"

void FGameplayRuntimeModule::StartupModule()
{
	Nelaric::ObjectPool::Private::FPoolNetworkRuntime::Start();
}

void FGameplayRuntimeModule::ShutdownModule()
{
	Nelaric::ObjectPool::Private::FPoolNetworkRuntime::Stop();
}

IMPLEMENT_MODULE(FGameplayRuntimeModule, GameplayRuntime)
