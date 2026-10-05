// Copyright (c) 2026 Nelaric Contributors

#include "Spawning/DemoInitialCharacterSpawnPoint.h"

#include "Engine/World.h"
#include "Pawn/PawnInitializationComponent.h"
#include "PawnGasBindingComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoInitialCharacterSpawn, Log, All);

ADemoInitialCharacterSpawnPoint::ADemoInitialCharacterSpawnPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

ADemoCharacter* ADemoInitialCharacterSpawnPoint::GetSpawnedCharacter() const
{
	check(IsInGameThread());
	const UWorld* World = GetWorld();
	const UDemoCharacterPoolSubsystem* Pool = World ? World->GetSubsystem<UDemoCharacterPoolSubsystem>() : nullptr;
	return Pool ? Pool->Get(SpawnedHandle) : nullptr;
}

void ADemoInitialCharacterSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || World->GetNetMode() == NM_Client || !HasAuthority() ||
	    IsActorBeingDestroyed() || bEndingPlay || bSpawnAttempted)
	{
		return;
	}
	// The first timer pass starts readiness waiting; it does not order GAS.
	SpawnTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::SpawnInitialCharacter);
}

void ADemoInitialCharacterSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopWaitingForInitialCharacters();
	Super::EndPlay(EndPlayReason);
}

void ADemoInitialCharacterSpawnPoint::SpawnInitialCharacter()
{
	check(IsInGameThread());
	SpawnTimer.Invalidate();
	UWorld* World = GetWorld();
	if (bSpawnAttempted || bEndingPlay || IsActorBeingDestroyed() || !World || World->bIsTearingDown ||
	    World->GetNetMode() == NM_Client || !HasAuthority())
	{
		return;
	}
	UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool)
	{
		bSpawnAttempted = true;
		StopWaitingForInitialCharacters();
		UE_LOG(LogDemoInitialCharacterSpawn, Warning,
		       TEXT("Initial character spawn point %s has no world character pool."), *GetPathName());
		return;
	}
	UPawnInitializationComponent* Initialization = nullptr;
	if (Pool->IsReady() && !Pool->AreInitialCharactersReady(Initialization))
	{
		WaitForInitialCharacters(Initialization);
		return;
	}
	bSpawnAttempted = true;
	StopWaitingForInitialCharacters();
	const UDemoCharacterPoolSubsystem::FLease Lease = Pool->TryAcquire(GetActorTransform(), TeamId);
	if (!Lease)
	{
		if (Lease.Result.Error == Nelaric::ObjectPool::EPoolError::Full)
		{
			UE_LOG(LogDemoInitialCharacterSpawn, Warning,
			       TEXT("Initial character spawn point %s exceeds the available pool capacity (%u); it remains "
			            "empty."),
			       *GetPathName(), UDemoCharacterPoolSubsystem::Capacity);
		}
		else
		{
			UE_LOG(LogDemoInitialCharacterSpawn, Warning,
			       TEXT("Initial character spawn point %s could not acquire a character (pool error %u)."),
			       *GetPathName(), static_cast<uint32>(Lease.Result.Error));
		}
		return;
	}
	SpawnedHandle = Lease.Handle;
	UE_LOG(LogDemoInitialCharacterSpawn, Log, TEXT("Initial character spawned: point=%s character=%s team=%u."),
	       *GetName(), *GetNameSafe(Lease.Object), Lease.Object->GetTeamId());
}

void ADemoInitialCharacterSpawnPoint::WaitForInitialCharacters(UPawnInitializationComponent* Initialization)
{
	UWorld* World = GetWorld();
	if (!bReadinessWaitStarted)
	{
		bReadinessWaitStarted = true;
		World->GetTimerManager().SetTimer(ReadinessTimeoutTimer, this, &ThisClass::HandleReadinessTimeout, 10.f, false);
	}
	// A short fallback also covers GAS progress outside the pawn Ready group.
	World->GetTimerManager().SetTimer(SpawnTimer, this, &ThisClass::SpawnInitialCharacter, 0.05f, false);
	if (PendingInitialization.Get() != Initialization)
	{
		ClearInitializationSubscription();
		PendingInitialization = Initialization;
		if (Initialization)
		{
			FPawnInitializationCallback Callback;
			Callback.BindDynamic(this, &ThisClass::HandleCharacterInitialized);
			Initialization->RegisterAndCallPawnInitialized(Callback);
		}
	}
}

void ADemoInitialCharacterSpawnPoint::HandleCharacterInitialized(UPawnInitializationComponent* Initialization)
{
	UWorld* World = GetWorld();
	if (bSpawnAttempted || bEndingPlay || IsActorBeingDestroyed() || !World || World->bIsTearingDown ||
	    World->GetNetMode() == NM_Client || !HasAuthority() || PendingInitialization.Get() != Initialization)
	{
		return;
	}
	// Leave initialization and its Ready broadcasts before lease activation.
	World->GetTimerManager().ClearTimer(SpawnTimer);
	SpawnTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::SpawnInitialCharacter);
}

void ADemoInitialCharacterSpawnPoint::ClearInitializationSubscription()
{
	if (UPawnInitializationComponent* Initialization = PendingInitialization.Get())
	{
		FPawnInitializationCallback Callback;
		Callback.BindDynamic(this, &ThisClass::HandleCharacterInitialized);
		Initialization->UnregisterPawnInitializationCallback(Callback);
	}
	PendingInitialization.Reset();
}

void ADemoInitialCharacterSpawnPoint::StopWaitingForInitialCharacters()
{
	ClearInitializationSubscription();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);
		World->GetTimerManager().ClearTimer(ReadinessTimeoutTimer);
	}
}

void ADemoInitialCharacterSpawnPoint::HandleReadinessTimeout()
{
	UWorld* World = GetWorld();
	if (bSpawnAttempted || bEndingPlay || IsActorBeingDestroyed() || !World || World->bIsTearingDown ||
	    World->GetNetMode() == NM_Client || !HasAuthority())
	{
		return;
	}
	const UPawnInitializationComponent* Initialization = PendingInitialization.Get();
	const ADemoCharacter* Character = Initialization ? Cast<ADemoCharacter>(Initialization->GetOwner()) : nullptr;
	const UPawnGasBindingComponent* Binding = Character ? Character->GetGasBinding() : nullptr;
	UE_LOG(LogDemoInitialCharacterSpawn, Warning,
	       TEXT("Initial character spawn point %s timed out after 10 seconds waiting for %s "
	            "(pawn ready: %d, GAS ready: %d, GAS state: %d, GAS committed: %d); it remains empty."),
	       *GetPathName(), *GetPathNameSafe(Character), Initialization && Initialization->IsPawnInitialized(),
	       Binding && Binding->IsReadyForActions(), Binding ? static_cast<int32>(Binding->GetInitState()) : -1,
	       Binding && Binding->HasCommittedState());
	bSpawnAttempted = true;
	StopWaitingForInitialCharacters();
}
