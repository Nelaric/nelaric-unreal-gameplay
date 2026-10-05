// Copyright (c) 2026 Nelaric Contributors

#include "ObjectPool/DemoCharacterPoolSubsystem.h"

#include "EngineUtils.h"
#include "Pawn/PawnInitializationComponent.h"
#include "PawnGasBindingComponent.h"
#include "Spawning/DemoInitialCharacterSpawnPoint.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoCharacterPool, Log, All);

UDemoCharacterPoolSubsystem::UDemoCharacterPoolSubsystem()
{
	static ConstructorHelpers::FClassFinder<ADemoCharacter> BlueprintClass(
	    TEXT("/Game/Demo/Demo1_GrandWarfront/Characters/BP_DemoCharacter"));
	CharacterClass = BlueprintClass.Class;
}

bool UDemoCharacterPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World;
}

Nelaric::ObjectPool::FPoolResult UDemoCharacterPoolSubsystem::ReleaseDeadCharacter(ADemoCharacter* Character)
{
	check(IsInGameThread());
	if (!CanUsePool())
	{
		return {Nelaric::ObjectPool::EPoolError::NotReady};
	}
	if (!IsValid(Character) || Character->GetWorld() != GetWorld() || Character->IsAlive() ||
	    Character->IsPlayerControlled())
	{
		return {Nelaric::ObjectPool::EPoolError::InvalidHandle};
	}
	for (FHandle Handle : ActiveHandles)
	{
		if (Handle && Pool.Get(Handle) == Character)
		{
			return Release(Handle);
		}
	}
	return {Nelaric::ObjectPool::EPoolError::InvalidHandle};
}

void UDemoCharacterPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	uint32 NumInitialPoints = 0;
	for (TActorIterator<ADemoInitialCharacterSpawnPoint> It(&InWorld); It; ++It)
	{
		++NumInitialPoints;
	}
	if (NumInitialPoints > Capacity)
	{
		UE_LOG(LogDemoCharacterPool, Warning,
		       TEXT("World %s has %u initial character spawn points, exceeding the fixed pool capacity of %u. "
		            "Points without an available slot will remain empty."),
		       *InWorld.GetName(), NumInitialPoints, Capacity);
	}
}

Nelaric::ObjectPool::FPoolResult UDemoCharacterPoolSubsystem::PrewarmPools(UWorld& World)
{
	if (World.GetNetMode() == NM_Client)
	{
		// The client owns a service facade; its actors come from replication.
		return {};
	}
	if (!IsValid(CharacterClass.Get()))
	{
		return {Nelaric::ObjectPool::EPoolError::CreationFailed};
	}
	FPool::FCreateArgs Args;
	Args.World = &World;
	Args.Class = CharacterClass.Get();
	Args.ParkTransform = FTransform::Identity;
	return Pool.Prewarm(Args);
}

bool UDemoCharacterPoolSubsystem::AreInitialCharactersReady(UPawnInitializationComponent*& PendingInitialization)
{
	check(IsInGameThread());
	PendingInitialization = nullptr;
	if (bInitialCharactersReady)
	{
		return true;
	}
	// This startup gate is separate from storage readiness and lease policy.
	check(IsReady());
	for (uint32 Index = 0; Index < Capacity; ++Index)
	{
		ADemoCharacter* Character = Pool.GetByIndexUnchecked(Index);
		UPawnInitializationComponent* Initialization = Character->GetPawnInitializationComponent();
		const UPawnGasBindingComponent* Binding = Character->GetGasBinding();
		if (!Character->HasActorBegunPlay() || !Initialization->IsPawnInitialized() || !Binding->IsReadyForActions())
		{
			PendingInitialization = Initialization;
			return false;
		}
	}
	// Later streamed points use the normal per-lease readiness checks.
	bInitialCharactersReady = true;
	return true;
}
