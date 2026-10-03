// Copyright (c) 2026 Nelaric Contributors

#include "ObjectPool/FixedObjectPoolWorldSubsystem.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFixedObjectPoolWorld, Log, All);

UFixedObjectPoolWorldSubsystem::UFixedObjectPoolWorldSubsystem() = default;

void UFixedObjectPoolWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	check(IsInGameThread());
	Super::Initialize(Collection);
	WorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(
	    this, &UFixedObjectPoolWorldSubsystem::HandleWorldBeginTearDown);
}

void UFixedObjectPoolWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	check(IsInGameThread());
	check(&InWorld == GetWorld());
	Super::OnWorldBeginPlay(InWorld);
	if (bPrewarmAttempted || bShuttingDown || InWorld.bIsTearingDown)
	{
		return;
	}
	bPrewarmAttempted = true;
	PrewarmResult = PrewarmPools(InWorld);
	if (!PrewarmResult)
	{
		UE_LOG(LogFixedObjectPoolWorld, Error, TEXT("World %s failed to prewarm its object pools (error %u)."),
		       *InWorld.GetName(), static_cast<uint32>(PrewarmResult.Error));
	}
}

void UFixedObjectPoolWorldSubsystem::Deinitialize()
{
	check(IsInGameThread());
	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldBeginTearDownHandle);
	WorldBeginTearDownHandle.Reset();
	ShutdownOwnedPools();
	Super::Deinitialize();
}

bool UFixedObjectPoolWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFixedObjectPoolWorldSubsystem::HandleWorldBeginTearDown(UWorld* World)
{
	check(IsInGameThread());
	if (World == GetWorld())
	{
		ShutdownOwnedPools();
	}
}

void UFixedObjectPoolWorldSubsystem::ShutdownOwnedPools()
{
	if (bShuttingDown)
	{
		return;
	}
	bShuttingDown = true;
	const Nelaric::ObjectPool::FPoolResult Result = ShutdownPools();
	checkf(Result, TEXT("World pool shutdown must run outside pool callbacks (error %u)."),
	       static_cast<uint32>(Result.Error));
}
