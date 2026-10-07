// Copyright (c) 2026 Nelaric Contributors

#include "Spawning/DemoRuntimeCharacterSpawnPoint.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/StaticArray.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoRuntimeCharacterSpawn, Log, All);

ADemoRuntimeCharacterSpawnPoint::ADemoRuntimeCharacterSpawnPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(GetRootComponent());
	SpawnArea->InitBoxExtent(FVector(500.0, 500.0, 250.0));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetGenerateOverlapEvents(false);
	SpawnArea->SetCanEverAffectNavigation(false);
	SpawnArea->SetHiddenInGame(true);
	SpawnArea->ShapeColor = FColor(64, 200, 255);
}

void ADemoRuntimeCharacterSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || World->GetNetMode() == NM_Client || !HasAuthority() ||
	    IsActorBeingDestroyed() || bEndingPlay)
	{
		return;
	}
	LocationInitializationDeadline = World->GetTimeSeconds() + 10.0;
	LocationInitializationTimer =
	    World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeSpawnLocations);
}

void ADemoRuntimeCharacterSpawnPoint::InitializeSpawnLocations()
{
	check(IsInGameThread());
	UWorld* World = GetWorld();
	if (bLocationInitializationFinished || bEndingPlay || !World || World->bIsTearingDown ||
	    World->GetNetMode() == NM_Client || !HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	UPawnInitializationComponent* PendingInitialization = nullptr;
	EDemoRuntimeCharacterSpawnResult WaitingResult = EDemoRuntimeCharacterSpawnResult::Spawned;
	if (!Pool || !Pool->IsReady() || !Pool->AreInitialCharactersReady(PendingInitialization))
	{
		WaitingResult = EDemoRuntimeCharacterSpawnResult::NotReady;
	}
	else if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
	{
		WaitingResult = EDemoRuntimeCharacterSpawnResult::NavigationUnavailable;
	}
	if (WaitingResult != EDemoRuntimeCharacterSpawnResult::Spawned)
	{
		if (World->GetTimeSeconds() < LocationInitializationDeadline)
		{
			World->GetTimerManager().SetTimer(LocationInitializationTimer, this, &ThisClass::InitializeSpawnLocations,
			                                  0.05f, false);
		}
		else
		{
			FinishSpawnLocationInitialization(WaitingResult);
		}
		return;
	}
	const UClass* CharacterClass = Pool->CharacterClass.Get();
	const ADemoCharacter* CharacterTemplate =
	    CharacterClass ? CharacterClass->GetDefaultObject<ADemoCharacter>() : nullptr;
	FinishSpawnLocationInitialization(CharacterTemplate ? BuildSpawnLocations(*CharacterTemplate)
	                                                    : EDemoRuntimeCharacterSpawnResult::PoolUnavailable);
}

void ADemoRuntimeCharacterSpawnPoint::FinishSpawnLocationInitialization(EDemoRuntimeCharacterSpawnResult Result)
{
	check(Result != EDemoRuntimeCharacterSpawnResult::Spawned ||
	      (!CachedSpawnLocations.IsEmpty() && CachedSpawnLocations.Num() <= MaxSpawnLocationCount));
	bLocationInitializationFinished = true;
	LocationInitializationResult = Result;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocationInitializationTimer);
	}
	if (Result == EDemoRuntimeCharacterSpawnResult::Spawned)
	{
		UE_LOG(LogDemoRuntimeCharacterSpawn, Log, TEXT("Runtime spawn area %s cached %d valid locations."),
		       *GetPathName(), CachedSpawnLocations.Num());
	}
	else
	{
		UE_LOG(LogDemoRuntimeCharacterSpawn, Warning,
		       TEXT("Runtime spawn area %s initialization failed (result %u, valid locations %d of %d)."),
		       *GetPathName(), static_cast<uint32>(Result), CachedSpawnLocations.Num(), MaxSpawnLocationCount);
		CachedSpawnLocations.Reset();
	}
}

EDemoRuntimeCharacterSpawnResult ADemoRuntimeCharacterSpawnPoint::SpawnCharacter(ADemoCharacter*& OutCharacter)
{
	check(IsInGameThread());
	OutCharacter = nullptr;
	UWorld* World = GetWorld();
	if (!HasAuthority() || (World && World->GetNetMode() == NM_Client))
	{
		return EDemoRuntimeCharacterSpawnResult::NotAuthority;
	}
	if (!World || !World->HasBegunPlay() || World->bIsTearingDown || !HasActorBegunPlay() || bEndingPlay ||
	    IsActorBeingDestroyed())
	{
		return EDemoRuntimeCharacterSpawnResult::NotReady;
	}
	if (bSpawning)
	{
		return EDemoRuntimeCharacterSpawnResult::Busy;
	}
	TGuardValue<bool> SpawnGuard(bSpawning, true);
	UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->IsReady())
	{
		return EDemoRuntimeCharacterSpawnResult::NotReady;
	}
	if (ADemoCharacter* Character = Pool->Get(SpawnedHandle))
	{
		OutCharacter = Character;
		return EDemoRuntimeCharacterSpawnResult::AlreadySpawned;
	}
	// Drop an expired token before acquiring; it must not resolve a reused slot.
	SpawnedHandle = {};
	if (!bLocationInitializationFinished)
	{
		return EDemoRuntimeCharacterSpawnResult::NotReady;
	}
	if (LocationInitializationResult != EDemoRuntimeCharacterSpawnResult::Spawned)
	{
		return LocationInitializationResult;
	}
	FTransform SpawnTransform;
	const EDemoRuntimeCharacterSpawnResult PlacementResult = FindSpawnTransform(SpawnTransform);
	if (PlacementResult != EDemoRuntimeCharacterSpawnResult::Spawned)
	{
		return PlacementResult;
	}
	const UDemoCharacterPoolSubsystem::FLease Lease = Pool->TryAcquire(SpawnTransform, TeamId);
	if (Lease)
	{
		// Activation callbacks may end the marker; the world still owns the lease.
		if (!bEndingPlay)
		{
			SpawnedHandle = Lease.Handle;
		}
		OutCharacter = Lease.Object;
		UE_LOG(LogDemoRuntimeCharacterSpawn, Log,
		       TEXT("Runtime character spawned: area=%s character=%s team=%u location=%s spawnYaw=%.2f pawnYaw=%.2f."),
		       *GetName(), *GetNameSafe(Lease.Object), Lease.Object->GetTeamId(),
		       *SpawnTransform.GetLocation().ToString(), SpawnTransform.Rotator().Yaw,
		       Lease.Object->GetActorRotation().Yaw);
		return EDemoRuntimeCharacterSpawnResult::Spawned;
	}
	switch (Lease.Result.Error)
	{
	case Nelaric::ObjectPool::EPoolError::NotReady:
		return EDemoRuntimeCharacterSpawnResult::NotReady;
	case Nelaric::ObjectPool::EPoolError::Full:
		return EDemoRuntimeCharacterSpawnResult::PoolFull;
	case Nelaric::ObjectPool::EPoolError::InTransition:
		return EDemoRuntimeCharacterSpawnResult::Busy;
	default:
		UE_LOG(LogDemoRuntimeCharacterSpawn, Warning,
		       TEXT("Runtime character spawn point %s could not acquire a character (pool error %u)."), *GetPathName(),
		       static_cast<uint32>(Lease.Result.Error));
		return Lease.Result.Error == Nelaric::ObjectPool::EPoolError::ActivationFailed
		           ? EDemoRuntimeCharacterSpawnResult::ActivationFailed
		           : EDemoRuntimeCharacterSpawnResult::PoolUnavailable;
	}
}

EDemoRuntimeCharacterSpawnResult
ADemoRuntimeCharacterSpawnPoint::BuildSpawnLocations(const ADemoCharacter& CharacterTemplate)
{
	const UCapsuleComponent* Capsule = CharacterTemplate.GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = CharacterTemplate.GetCharacterMovement();
	if (!IsValid(SpawnArea) || !Capsule || !Movement || !FMath::IsFinite(GroundClearance) || GroundClearance < 0.0f ||
	    GroundClearance > 50.0f)
	{
		return EDemoRuntimeCharacterSpawnResult::InvalidConfiguration;
	}
	const FTransform AreaTransform = SpawnArea->GetComponentTransform();
	const FVector AreaExtent = SpawnArea->GetScaledBoxExtent().GetAbs();
	const float Radius = Capsule->GetUnscaledCapsuleRadius();
	const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	if (AreaTransform.ContainsNaN() || AreaExtent.ContainsNaN() || AreaExtent.GetMin() <= 0.0 ||
	    !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius <= 0.0f || HalfHeight < Radius)
	{
		return EDemoRuntimeCharacterSpawnResult::InvalidConfiguration;
	}
	const FQuat AreaRotation = AreaTransform.GetRotation();
	const FVector AxisX = AreaRotation.GetAxisX();
	const FVector AxisY = AreaRotation.GetAxisY();
	const FVector AxisZ = AreaRotation.GetAxisZ();
	// The upright capsule's support distance along each rotated box axis.
	const float SegmentHalfHeight = HalfHeight - Radius;
	const FVector CapsuleExtent(Radius + SegmentHalfHeight * FMath::Abs(AxisX.Z),
	                            Radius + SegmentHalfHeight * FMath::Abs(AxisY.Z),
	                            Radius + SegmentHalfHeight * FMath::Abs(AxisZ.Z));
	const FVector CenterExtent = AreaExtent - CapsuleExtent;
	if (CenterExtent.GetMin() < 0.0)
	{
		return EDemoRuntimeCharacterSpawnResult::NoValidLocation;
	}
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		return EDemoRuntimeCharacterSpawnResult::NavigationUnavailable;
	}
	FNavAgentProperties Agent = CharacterTemplate.GetNavAgentPropertiesRef();
	Agent.AgentRadius = Radius;
	Agent.AgentHeight = HalfHeight * 2.0f;
	const ANavigationData* NavData = Navigation->GetNavDataForProps(Agent, AreaTransform.GetLocation());
	if (!NavData)
	{
		return EDemoRuntimeCharacterSpawnResult::NavigationUnavailable;
	}
	const double VerticalExtent =
	    FMath::Abs(AxisX.Z) * AreaExtent.X + FMath::Abs(AxisY.Z) * AreaExtent.Y + FMath::Abs(AxisZ.Z) * AreaExtent.Z;
	const FVector ProjectionExtent(FMath::Max(50.0f, Radius), FMath::Max(50.0f, Radius), VerticalExtent + HalfHeight);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoRuntimeCharacterSpawn), false, this);
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	const FQuat SpawnRotation = FRotator(0.0, AreaRotation.Rotator().Yaw, 0.0).Quaternion();
	CachedSpawnLocations.Reserve(MaxSpawnLocationCount);
	constexpr int32 MaxCandidateAttempts = MaxSpawnLocationCount * 32;
	for (int32 Attempt = 0; Attempt < MaxCandidateAttempts; ++Attempt)
	{
		const FVector LocalSample(FMath::FRandRange(-CenterExtent.X, CenterExtent.X),
		                          FMath::FRandRange(-CenterExtent.Y, CenterExtent.Y),
		                          FMath::FRandRange(-CenterExtent.Z, CenterExtent.Z));
		const FVector Sample = AreaTransform.GetLocation() + AreaRotation.RotateVector(LocalSample);
		FNavLocation Projected;
		if (!Navigation->ProjectPointToNavigation(Sample, Projected, ProjectionExtent, NavData))
		{
			continue;
		}
		FHitResult Ground;
		const FVector GroundProbe(0.0, 0.0, 50.0);
		if (!World->LineTraceSingleByChannel(Ground, Projected.Location + GroundProbe, Projected.Location - GroundProbe,
		                                     Channel, Query, Responses) ||
		    !Movement->IsWalkable(Ground) || Ground.ImpactNormal.Z <= UE_SMALL_NUMBER)
		{
			continue;
		}
		// Reject a support surface that is detached from the projected nav floor.
		FNavLocation GroundNavigation;
		if (!Navigation->ProjectPointToNavigation(Ground.ImpactPoint, GroundNavigation, FVector(2.0, 2.0, 20.0),
		                                          NavData) ||
		    FVector::DistSquared2D(Ground.ImpactPoint, GroundNavigation.Location) > 4.0 ||
		    FMath::Abs(Ground.ImpactPoint.Z - GroundNavigation.Location.Z) > 20.0)
		{
			continue;
		}
		// Lift the lower capsule sphere off the actual walkable support plane.
		const double GroundLift = SegmentHalfHeight + (Radius + GroundClearance) / Ground.ImpactNormal.Z;
		const FVector Center = Ground.ImpactPoint + FVector(0.0, 0.0, GroundLift);
		const FVector LocalCenter = AreaRotation.UnrotateVector(Center - AreaTransform.GetLocation());
		if (FMath::Abs(LocalCenter.X) > CenterExtent.X || FMath::Abs(LocalCenter.Y) > CenterExtent.Y ||
		    FMath::Abs(LocalCenter.Z) > CenterExtent.Z ||
		    World->OverlapBlockingTestByChannel(Center, SpawnRotation, Channel, Shape, Query, Responses))
		{
			continue;
		}
		if (CachedSpawnLocations.ContainsByPredicate([&Center](const FVector& Existing)
		                                             { return FVector::DistSquared(Existing, Center) < 1.0; }))
		{
			continue;
		}
		CachedSpawnLocations.Add(Center);
		if (CachedSpawnLocations.Num() == MaxSpawnLocationCount)
		{
			break;
		}
	}
	if (CachedSpawnLocations.IsEmpty())
	{
		return EDemoRuntimeCharacterSpawnResult::NoValidLocation;
	}
	CachedSpawnRotation = SpawnRotation;
	CachedCapsuleRadius = Radius;
	CachedCapsuleHalfHeight = HalfHeight;
	CachedCollisionChannel = Channel;
	CachedCollisionResponses = Capsule->GetCollisionResponseToChannels();
	return EDemoRuntimeCharacterSpawnResult::Spawned;
}

EDemoRuntimeCharacterSpawnResult ADemoRuntimeCharacterSpawnPoint::FindSpawnTransform(FTransform& OutTransform) const
{
	const int32 LocationCount = CachedSpawnLocations.Num();
	check(LocationCount > 0 && LocationCount <= MaxSpawnLocationCount);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(CachedCapsuleRadius, CachedCapsuleHalfHeight);
	const FCollisionResponseParams Responses(CachedCollisionResponses);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoRuntimeCharacterSpawn), false, this);
	TStaticArray<int32, MaxSpawnLocationCount> Indices;
	for (int32 Index = 0; Index < LocationCount; ++Index)
	{
		Indices[Index] = Index;
	}
	for (int32 Remaining = LocationCount; Remaining > 0; --Remaining)
	{
		const int32 Draw = FMath::RandRange(0, Remaining - 1);
		const int32 Index = Indices[Draw];
		Indices[Draw] = Indices[Remaining - 1];
		const FVector& Center = CachedSpawnLocations[Index];
		if (!GetWorld()->OverlapBlockingTestByChannel(Center, CachedSpawnRotation, CachedCollisionChannel, Shape, Query,
		                                              Responses))
		{
			OutTransform = FTransform(CachedSpawnRotation, Center, FVector::OneVector);
			return EDemoRuntimeCharacterSpawnResult::Spawned;
		}
	}
	return EDemoRuntimeCharacterSpawnResult::NoValidLocation;
}

TArray<FVector> ADemoRuntimeCharacterSpawnPoint::GetSpawnLocations() const
{
	check(IsInGameThread());
	return !bEndingPlay && bLocationInitializationFinished &&
	               LocationInitializationResult == EDemoRuntimeCharacterSpawnResult::Spawned
	           ? CachedSpawnLocations
	           : TArray<FVector>{};
}

ADemoCharacter* ADemoRuntimeCharacterSpawnPoint::GetSpawnedCharacter() const
{
	check(IsInGameThread());
	if (bEndingPlay)
	{
		return nullptr;
	}
	const UWorld* World = GetWorld();
	const UDemoCharacterPoolSubsystem* Pool = World ? World->GetSubsystem<UDemoCharacterPoolSubsystem>() : nullptr;
	return Pool ? Pool->Get(SpawnedHandle) : nullptr;
}

void ADemoRuntimeCharacterSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocationInitializationTimer);
	}
	CachedSpawnLocations.Reset();
	SpawnedHandle = {};
	Super::EndPlay(EndPlayReason);
}
