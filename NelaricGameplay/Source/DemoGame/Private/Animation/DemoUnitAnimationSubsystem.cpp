// Copyright (c) 2026 Nelaric Contributors

#include "Animation/DemoUnitAnimationSubsystem.h"

#include "Animation/AnimationDataUpdater.h"
#include "Animation/DemoAnimationDataInstance.h"
#include "Animation/AnimInstance.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "ConvexVolume.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IAnimationBudgetAllocator.h"
#include "SceneView.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace Nelaric::UnitAnimation
{
APlayerController* FindReferenceController(const UWorld& World)
{
	for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (IsValid(Controller) && Controller->IsLocalController())
		{
			return Controller;
		}
	}
	return nullptr;
}
} // namespace Nelaric::UnitAnimation

void UDemoUnitAnimationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDemoCharacterPoolSubsystem>();
	bShuttingDown = false;
	bBudgetPoolRegistered = false;
	if (IAnimationBudgetAllocator* Budget = IAnimationBudgetAllocator::Get(GetWorld()))
	{
		Budget->SetEnabled(GetWorld()->GetNetMode() != NM_DedicatedServer);
	}
	WorldTickStartHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &ThisClass::HandleWorldTickStart);
	WorldPreActorTickHandle =
	    FWorldDelegates::OnWorldPreActorTick.AddUObject(this, &ThisClass::HandleWorldPreActorTick);
}

void UDemoUnitAnimationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	check(&InWorld == GetWorld());

	const float IntervalSeconds = static_cast<float>(Nelaric::UnitAnimation::DistanceUpdateIntervalMs) / 1000.0f;
	InWorld.GetTimerManager().SetTimer(DistanceUpdateTimer, this, &ThisClass::UpdateDistanceLevels, IntervalSeconds,
	                                   true);
}

void UDemoUnitAnimationSubsystem::Deinitialize()
{
	bShuttingDown = true;
	FWorldDelegates::OnWorldTickStart.Remove(WorldTickStartHandle);
	WorldTickStartHandle.Reset();
	FWorldDelegates::OnWorldPreActorTick.Remove(WorldPreActorTickHandle);
	WorldPreActorTickHandle.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DistanceUpdateTimer);
	}
	// Budgeted mesh EndPlay unregisters itself; the pool owns actor teardown.
	bBudgetPoolRegistered = false;
	ResetLevelCounts();
	ResetUpdateState();
	bDistanceLevelsReady = false;
	Super::Deinitialize();
}

bool UDemoUnitAnimationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDemoUnitAnimationSubsystem::HandleWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	check(IsInGameThread());
	if (bShuttingDown || World != GetWorld() || World->bIsTearingDown || !World->HasBegunPlay() ||
	    World->GetNetMode() == NM_DedicatedServer || TickType != LEVELTICK_All)
	{
		return;
	}
	const UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->IsReady())
	{
		return;
	}
	IAnimationBudgetAllocator* Budget = IAnimationBudgetAllocator::Get(World);
	if (!Budget)
	{
		return;
	}

	APlayerController* Controller = Nelaric::UnitAnimation::FindReferenceController(*World);
	const ACharacter* ReferenceCharacter = Controller ? Controller->GetCharacter() : nullptr;
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData Projection;
	FConvexVolume ViewFrustum;
	const bool bHasView = IsValid(ReferenceCharacter) && LocalPlayer && LocalPlayer->ViewportClient &&
	                      LocalPlayer->ViewportClient->Viewport &&
	                      LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection);
	if (bHasView)
	{
		GetViewFrustumBounds(ViewFrustum, Projection.ComputeViewProjectionMatrix(), true);
	}

	// Gate demand before the allocator's pre-actor callback. Its render-time
	// grace period alone would keep recently visible off-view meshes in budget.
	for (int32 UnitIndex = 0; UnitIndex < MaxUnits; ++UnitIndex)
	{
		const ADemoCharacter* Character = Pool->GetByIndexUnchecked(static_cast<uint32>(UnitIndex));
		USkeletalMeshComponentBudgeted* Mesh = CastChecked<USkeletalMeshComponentBudgeted>(Character->GetMesh());
		if (!bBudgetPoolRegistered)
		{
			Budget->RegisterComponent(Mesh);
		}
		float Significance = 0.0f;
		if (bHasView && Character->IsPoolActive() && !Character->IsHidden() && Mesh->IsVisible() &&
		    ViewFrustum.IntersectBox(Mesh->Bounds.Origin, Mesh->Bounds.BoxExtent))
		{
			const double DistanceSquared =
			    FVector::DistSquared(ReferenceCharacter->GetActorLocation(), Character->GetActorLocation());
			const int32 Level = Nelaric::UnitAnimation::GetDistanceLevel(DistanceSquared);
			if (Nelaric::UnitAnimation::UpdateIntervalFrames[Level] > 0)
			{
				Significance = static_cast<float>(NumLevels - Level) / static_cast<float>(NumLevels);
			}
		}
		Budget->SetComponentSignificance(Mesh, Significance, false, false);
		Budget->SetComponentTickEnabled(Mesh, Significance > 0.0f);
	}
	bBudgetPoolRegistered = true;
}

void UDemoUnitAnimationSubsystem::UpdateDistanceLevels()
{
	check(IsInGameThread());
	ResetLevelCounts();
	bDistanceLevelsReady = false;

	UWorld* World = GetWorld();
	if (bShuttingDown || !World || World->bIsTearingDown)
	{
		return;
	}

	const UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->IsReady())
	{
		return;
	}

	const APlayerController* Controller = Nelaric::UnitAnimation::FindReferenceController(*World);
	const ACharacter* ReferenceCharacter = Controller ? Controller->GetCharacter() : nullptr;
	if (!IsValid(ReferenceCharacter))
	{
		return;
	}

	bDistanceLevelsReady = true;
	const FVector ReferenceLocation = ReferenceCharacter->GetActorLocation();
	for (int32 UnitIndex = 0; UnitIndex < MaxUnits; ++UnitIndex)
	{
		const ADemoCharacter* Character = Pool->GetByIndexUnchecked(static_cast<uint32>(UnitIndex));
		if (!IsValid(Character) || !Character->IsPoolActive())
		{
			SlotAnimationInstances[UnitIndex].Reset();
			ElapsedUpdateSeconds[UnitIndex] = 0.0f;
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(ReferenceLocation, Character->GetActorLocation());
		const int32 AssignedLevel = Nelaric::UnitAnimation::GetDistanceLevel(DistanceSquared);
		LevelIndices[AssignedLevel][LevelCounts[AssignedLevel]++] = UnitIndex;
	}
}

namespace Nelaric::UnitAnimation
{
struct FAnimationUpdateJob
{
	TStrongObjectPtr<UAnimInstance> Instance;
	Nelaric::UnitAnimation::IAnimationDataUpdater* Updater = nullptr;
	int32 UnitIndex = INDEX_NONE;
	float DeltaSeconds = 0.0f;
};
} // namespace Nelaric::UnitAnimation

void UDemoUnitAnimationSubsystem::ResetUpdateState()
{
	FrameCounter = 0;
	for (int32 UnitIndex = 0; UnitIndex < MaxUnits; ++UnitIndex)
	{
		SlotAnimationInstances[UnitIndex].Reset();
		ElapsedUpdateSeconds[UnitIndex] = 0.0f;
	}
}

void UDemoUnitAnimationSubsystem::HandleWorldPreActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	check(IsInGameThread());
	if (bShuttingDown || World != GetWorld() || World->bIsTearingDown || !World->HasBegunPlay() ||
	    TickType != LEVELTICK_All || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
	{
		return;
	}

	const UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->IsReady())
	{
		ResetUpdateState();
		return;
	}
	if (!bDistanceLevelsReady)
	{
		UpdateDistanceLevels();
	}
	++FrameCounter;
	IAnimationBudgetAllocator* Budget =
	    World->GetNetMode() != NM_DedicatedServer ? IAnimationBudgetAllocator::Get(World) : nullptr;

	using FAnimationUpdateJob = Nelaric::UnitAnimation::FAnimationUpdateJob;
	TArray<FAnimationUpdateJob, TInlineAllocator<MaxUnits>> Jobs;
	for (int32 Level = 0; Level < NumLevels; ++Level)
	{
		const int32 IntervalFrames = Nelaric::UnitAnimation::UpdateIntervalFrames[Level];
		const bool bUpdateThisFrame = IntervalFrames > 0 && FrameCounter % IntervalFrames == 0;
		for (int32 Entry = 0; Entry < LevelCounts[Level]; ++Entry)
		{
			const int32 UnitIndex = LevelIndices[Level][Entry];
			const ADemoCharacter* Character = Pool->GetByIndexUnchecked(static_cast<uint32>(UnitIndex));
			if (!IsValid(Character) || !Character->IsPoolActive() || IntervalFrames == 0)
			{
				SlotAnimationInstances[UnitIndex].Reset();
				ElapsedUpdateSeconds[UnitIndex] = 0.0f;
				continue;
			}

			USkeletalMeshComponent* Mesh = Character->GetMesh();
			if (Budget && !Budget->IsComponentTickEnabled(CastChecked<USkeletalMeshComponentBudgeted>(Mesh)))
			{
				SlotAnimationInstances[UnitIndex].Reset();
				ElapsedUpdateSeconds[UnitIndex] = 0.0f;
				continue;
			}
			UAnimInstance* Instance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
			if (SlotAnimationInstances[UnitIndex].Get() != Instance)
			{
				SlotAnimationInstances[UnitIndex] = Instance;
				ElapsedUpdateSeconds[UnitIndex] = 0.0f;
			}
			if (!IsValid(Instance))
			{
				ElapsedUpdateSeconds[UnitIndex] = 0.0f;
				continue;
			}
			ElapsedUpdateSeconds[UnitIndex] += DeltaSeconds;
			if (!bUpdateThisFrame)
			{
				continue;
			}

			// Finish previous mesh work before granting exclusive instance access.
			Mesh->HandleExistingParallelEvaluationTask(true, true);
			if (bShuttingDown || World->bIsTearingDown || !Pool->IsReady())
			{
				return;
			}
			if (!IsValid(Character) || !Character->IsPoolActive() || !IsValid(Mesh) || Character->GetMesh() != Mesh)
			{
				continue;
			}
			Instance = Mesh->GetAnimInstance();
			if (SlotAnimationInstances[UnitIndex].Get() != Instance)
			{
				SlotAnimationInstances[UnitIndex] = Instance;
				ElapsedUpdateSeconds[UnitIndex] = DeltaSeconds;
			}
			if (!IsValid(Instance))
			{
				ElapsedUpdateSeconds[UnitIndex] = 0.0f;
				continue;
			}
			if (Jobs.ContainsByPredicate([Instance](const FAnimationUpdateJob& Job)
			                             { return Job.Instance.Get() == Instance; }))
			{
				continue;
			}

			FAnimationUpdateJob& Job = Jobs.Emplace_GetRef();
			Job.Instance.Reset(Instance);
			Job.Updater = &Character->GetAnimationDataUpdater();
			Job.UnitIndex = UnitIndex;
			Job.DeltaSeconds = ElapsedUpdateSeconds[UnitIndex];
		}
	}

	// Completing mesh tasks can invoke callbacks that change earlier slots.
	Jobs.RemoveAllSwap(
	    [Pool, Budget](const FAnimationUpdateJob& Job)
	    {
		    const ADemoCharacter* Character = Pool->GetByIndexUnchecked(static_cast<uint32>(Job.UnitIndex));
		    USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
		    return !IsValid(Character) || !Character->IsPoolActive() || !IsValid(Mesh) ||
		           Mesh->GetAnimInstance() != Job.Instance.Get() || Mesh->IsRunningParallelEvaluation() ||
		           (Budget && !Budget->IsComponentTickEnabled(CastChecked<USkeletalMeshComponentBudgeted>(Mesh)));
	    });
	if (Jobs.IsEmpty())
	{
		return;
	}

	// All mesh callbacks have completed. Capture values before any worker runs.
	for (const FAnimationUpdateJob& Job : Jobs)
	{
		const ADemoCharacter* Character = Pool->GetByIndexUnchecked(static_cast<uint32>(Job.UnitIndex));
		static_cast<UDemoAnimationDataInstance*>(Job.Instance.Get())->PrepareAnimationData(*Character);
	}

	// The future wait does not pump game-thread work. Actor ticks resume only
	// after all calculations finish, so implementations need no synchronization.
	TFuture<void> UpdateTask = Async(EAsyncExecution::ThreadPool,
	                                 [&Jobs]()
	                                 {
		                                 check(!IsInGameThread());
		                                 ParallelFor(Jobs.Num(),
		                                             [&Jobs](int32 JobIndex)
		                                             {
			                                             check(!IsInGameThread());
			                                             FAnimationUpdateJob& Job = Jobs[JobIndex];
			                                             Job.Updater->UpdateAnimationData(Job.DeltaSeconds);
		                                             });
	                                 });
	UpdateTask.Wait();
	for (const FAnimationUpdateJob& Job : Jobs)
	{
		ElapsedUpdateSeconds[Job.UnitIndex] = 0.0f;
	}
}
