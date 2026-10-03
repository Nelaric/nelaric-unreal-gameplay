// Copyright (c) 2026 Nelaric Contributors

#include "ObjectPool/CharacterPoolHelper.h"

#include "ObjectPool/CharacterPoolReplicationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/EngineVersionComparison.h"

void Nelaric::ObjectPool::FCharacterPoolHelper::PrepareForPool(ACharacter& Character, FCharacterPoolState& State)
{
	check(IsInGameThread());
	UWorld* World = Character.GetWorld();
	check(World);
	if (World->GetNetMode() == NM_Client)
	{
		if (auto* Replication = Character.FindComponentByClass<UCharacterPoolReplicationComponent>();
		    Replication && !Replication->bApplyingReplication)
		{
			State.bPrepared = true;
			Replication->State = &State;
			Replication->AppliedTransition = 0;
			Replication->ApplyTransition();
		}
		return;
	}
	check(Character.HasAuthority());
	check(!Character.HasActorBegunPlay());
	State.bPrepared = true;

	Character.bAlwaysRelevant = true;
	Character.bOnlyRelevantToOwner = false;
	Character.bNetUseOwnerRelevancy = false;
	Character.SetNetDormancy(DORM_Awake);
	if (!Character.FindComponentByClass<UCharacterPoolReplicationComponent>())
	{
		auto* Replication =
		    NewObject<UCharacterPoolReplicationComponent>(&Character, TEXT("PoolReplication"), RF_Transient);
		Character.AddInstanceComponent(Replication);
		Replication->RegisterComponent();
	}
	Character.AutoPossessAI = EAutoPossessAI::Disabled;
	Character.AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Character.InitialLifeSpan = 0.f;
	Character.PrimaryActorTick.bStartWithTickEnabled = false;
	Character.GetCharacterMovement()->PrimaryComponentTick.bStartWithTickEnabled = false;
	Character.GetCharacterMovement()->bRunPhysicsWithNoController = true;
	Character.GetMesh()->PrimaryComponentTick.bStartWithTickEnabled = false;
	Deactivate(Character, State);
}

bool Nelaric::ObjectPool::FCharacterPoolHelper::Activate(ACharacter& Character, FCharacterPoolState& State,
                                                         const FTransform& Transform)
{
	return ActivateInternal(Character, State, Transform, false);
}

bool Nelaric::ObjectPool::FCharacterPoolHelper::ActivateInternal(ACharacter& Character, FCharacterPoolState& State,
                                                                 const FTransform& Transform, bool bFromReplication)
{
	check(IsInGameThread());
	UWorld* World = Character.GetWorld();
	auto* Replication = Character.FindComponentByClass<UCharacterPoolReplicationComponent>();
	const bool bClient = World && World->GetNetMode() == NM_Client;
	if (!World || World->bIsTearingDown || Character.IsActorBeingDestroyed() || State.bActive ||
	    !Character.HasActorBegunPlay() ||
	    (bClient ? !bFromReplication || !Replication || !Replication->bApplyingReplication
	             : !Character.HasAuthority()) ||
	    (!bClient && Replication && Replication->Transition >= MAX_uint64 - 4))
	{
		return false;
	}

	UCharacterMovementComponent* Move = Character.GetCharacterMovement();
	USkeletalMeshComponent* CharacterMesh = Character.GetMesh();
	if (!bClient)
	{
		// Wake before changing replicated actor or component state.
		Character.SetNetDormancy(DORM_Awake);
		if (!Character.SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics))
		{
			return false;
		}

		Character.UnCrouch(false);
		if (Character.bIsCrouched)
		{
			Move->UnCrouch(false);
			if (Character.bIsCrouched)
			{
				return false;
			}
		}
		Move->bForceNextFloorCheck = true;
		Move->SetMovementMode(MOVE_Walking);
	}
	else
	{
		// Keep UE's received placement and movement mode on remote proxies.
		Move->ApplyNetworkMovementMode(Character.GetReplicatedMovementMode());
		CharacterMesh->SetRelativeLocationAndRotation(Character.GetBaseTranslationOffset(),
		                                              Character.GetBaseRotationOffset());
		Character.OnRep_ReplicatedMovement();
		Character.OnRep_ReplicatedBasedMovement();
	}

	State.bActive = true;
	Character.SetCanBeDamaged(!bClient);
	CharacterMesh->bPauseAnims = false;
	CharacterMesh->SetComponentTickEnabled(true);
	Move->SetComponentTickEnabled(true);
	Character.SetActorTickEnabled(true);
	Character.SetActorHiddenInGame(false);
	Character.SetActorEnableCollision(true);
	if (!bClient && Replication)
	{
		Replication->Publish(true);
	}
	return true;
}

void Nelaric::ObjectPool::FCharacterPoolHelper::Deactivate(ACharacter& Character, FCharacterPoolState& State)
{
	DeactivateInternal(Character, State, false);
}

void Nelaric::ObjectPool::FCharacterPoolHelper::DeactivateInternal(ACharacter& Character, FCharacterPoolState& State,
                                                                   bool bFromReplication)
{
	check(IsInGameThread());
	UWorld* World = Character.GetWorld();
	auto* Replication = Character.FindComponentByClass<UCharacterPoolReplicationComponent>();
	const bool bClient = World && World->GetNetMode() == NM_Client;
	if (bClient ? !bFromReplication || !Replication || !Replication->bApplyingReplication : !Character.HasAuthority())
	{
		return;
	}
	if (!bClient)
	{
		Character.SetNetDormancy(DORM_Awake);
	}

	// Callbacks observe the logical inactive state before collision changes.
	State.bActive = false;
	Character.SetCanBeDamaged(false);
	Character.SetActorEnableCollision(false);
	Character.SetActorHiddenInGame(true);
	Character.SetActorTickEnabled(false);

	UCharacterMovementComponent* Move = Character.GetCharacterMovement();
	USkeletalMeshComponent* CharacterMesh = Character.GetMesh();
	Move->StopMovementImmediately();
	Move->ClearAccumulatedForces();
	Move->ResetPredictionData_Client();
	Move->ResetPredictionData_Server();
	Move->DisableMovement();
	// Reset after leaving MOVE_Falling so jump counts are cleared too.
	Character.ResetJumpState();
	Character.ConsumeMovementInputVector();
	Move->SetComponentTickEnabled(false);
#if UE_VERSION_OLDER_THAN(5, 8, 0)
	Character.SetBase(static_cast<UPrimitiveComponent*>(nullptr), NAME_None, false);
#else
	Character.SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr), NAME_None, false);
#endif
	CharacterMesh->bPauseAnims = true;
	CharacterMesh->SetComponentTickEnabled(false);
	if (!bClient && Replication)
	{
		Replication->Publish(false);
	}
}
