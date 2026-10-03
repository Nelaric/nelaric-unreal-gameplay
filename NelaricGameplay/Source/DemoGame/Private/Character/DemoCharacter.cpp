// Copyright (c) 2026 Nelaric Contributors

#include "Character/DemoCharacter.h"
#include "Animation/DemoAnimationDataInstance.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/DemoCombatAttributes.h"
#include "GAS/DemoJumpAbility.h"
#include "PawnGasBindingComponent.h"
#include "NelaricAbilitySystemComponent.h"
#include "Pawn/PawnControlComponent.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Pawn/PawnInitializationConfig.h"
#include "GasStateProfile.h"
#include "NativeGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoCharacterPoolActivation, Log, All);

namespace Nelaric::DemoActions
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(Jump, "Action.Jump");
}

ADemoCharacter::ADemoCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultStateProfile = CreateDefaultSubobject<UGasStateProfile>(TEXT("DefaultStateProfile"));
	DefaultStateProfile->Attributes = {
	    {UDemoCombatAttributes::GetMaxHealthAttribute(), EGasStateOwnership::Pawn, 100.0f},
	    {UDemoCombatAttributes::GetHealthAttribute(), EGasStateOwnership::Pawn, 100.0f},
	    {UDemoCombatAttributes::GetAttackAttribute(), EGasStateOwnership::Pawn, 10.0f}};
	DefaultStateProfile->Abilities.Add({UDemoJumpAbility::StaticClass(), Nelaric::DemoActions::Jump, 1});
	GetGasBinding()->StateProfile = DefaultStateProfile;
}

Nelaric::UnitAnimation::IAnimationDataUpdater& ADemoCharacter::GetAnimationDataUpdater() const
{
	check(IsInGameThread());
	UAnimInstance* Instance = GetMesh()->GetAnimInstance();
	if (!IsValid(Instance) || !AnimationDataClass || Instance->GetClass() != AnimationDataClass.Get())
	{
		LowLevelFatalError(TEXT("DemoCharacter requires its main animation instance to use AnimationDataClass."));
	}
	return *static_cast<UDemoAnimationDataInstance*>(Instance);
}

void ADemoCharacter::PrepareForPool()
{
	Nelaric::ObjectPool::FCharacterPoolHelper::PrepareForPool(*this, PoolState);
}

void ADemoCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	GetMesh()->SetAnimInstanceClass(AnimationDataClass.Get());
	bPoolComponentsInitialized = true;
	if (PoolState.bPrepared)
	{
		InitializePoolControlPolicy();
		DeactivateToPool();
	}
}

bool ADemoCharacter::ActivateFromPool(const FTransform& Transform)
{
	check(IsInGameThread());
	const UPawnGasBindingComponent* Binding = GetGasBinding();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !HasAuthority() || !Binding ||
	    !Binding->IsReadyForActions() || !Binding->GetAbilitySystem() ||
	    Binding->GetAbilitySystem()->GetAvatarActor() != this)
	{
		UE_LOG(LogDemoCharacterPoolActivation, Warning,
		       TEXT("Cannot activate %s: authority=%d, begun play=%d, GAS ready=%d, GAS state=%d, GAS committed=%d."),
		       *GetPathName(), HasAuthority(), HasActorBegunPlay(), Binding && Binding->IsReadyForActions(),
		       Binding ? static_cast<int32>(Binding->GetInitState()) : -1, Binding && Binding->HasCommittedState());
		return false;
	}
	if (!Nelaric::ObjectPool::FCharacterPoolHelper::Activate(*this, PoolState, Transform))
	{
		UE_LOG(LogDemoCharacterPoolActivation, Warning,
		       TEXT("Cannot activate %s: native character transition failed (active=%d, begun play=%d, crouched=%d)."),
		       *GetPathName(), PoolState.bActive, HasActorBegunPlay(), bIsCrouched);
		return false;
	}
	if (Binding->IsReadyForActions() && IsValid(PoolControlPolicy))
	{
		// Ready was committed while idle, so it will not announce again.
		PoolControlPolicy->StartReadyBotLogic();
	}
	// Collision and brain callbacks may invalidate readiness during activation.
	if (!Binding->IsReadyForActions() || Binding->GetAbilitySystem()->GetAvatarActor() != this)
	{
		UE_LOG(LogDemoCharacterPoolActivation, Warning,
		       TEXT("Cannot activate %s: GAS readiness changed during activation."), *GetPathName());
		DeactivateToPool();
		return false;
	}
	return true;
}

void ADemoCharacter::PostNetInit()
{
	// The initial bunch is complete; bind before BeginPlay observes the actor.
	Nelaric::ObjectPool::FCharacterPoolHelper::PrepareForPool(*this, PoolState);
	Super::PostNetInit();
}

void ADemoCharacter::DeactivateToPool()
{
	Nelaric::ObjectPool::FCharacterPoolHelper::Deactivate(*this, PoolState);
}

void ADemoCharacter::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);
	if (bPoolComponentsInitialized && PoolState.bPrepared)
	{
		// Both native authority transitions and client replication use this.
		// Read logical pool state; ordinary visibility edits do not toggle it.
		InitializePoolControlPolicy();
		ApplyPoolControlPolicy();
	}
}

void ADemoCharacter::InitializePoolControlPolicy()
{
	if (IsValid(PoolControlPolicy))
	{
		return;
	}
	UPawnControlComponent* Policy = FindComponentByClass<UPawnControlComponent>();
	if (!Policy && HasConfiguredPoolControlPolicy())
	{
		// The configuration creates its component during BeginPlay. Do not
		// add a competing fallback before that phase has run.
		if (!bPoolInitializationSubscribed)
		{
			bPoolInitializationSubscribed = true;
			FPawnInitializationCallback Callback;
			Callback.BindDynamic(this, &ThisClass::HandlePoolPawnInitialized);
			GetPawnInitializationComponent()->RegisterAndCallPawnInitialized(Callback);
		}
		return;
	}
	const bool bCreated = !Policy;
	if (bCreated)
	{
		Policy = NewObject<UPawnControlComponent>(this, TEXT("PoolControlPolicy"), RF_Transient);
		AddInstanceComponent(Policy);
	}
	PoolControlPolicy = Policy;
	bPoolControlPolicyActive = false;
	// Preserve authored settings or the fallback's native defaults once.
	bPoolAllowPlayerControl = Policy->bAllowPlayerControl;
	bPoolAllowReturnControl = Policy->bAllowReturnControl;
	bPoolReturnToBot = Policy->bReturnToBot;
	bPoolStartBotLogicOnReady = Policy->bStartBotLogicOnReady;
	Policy->bAllowPlayerControl = false;
	Policy->bAllowReturnControl = false;
	Policy->bReturnToBot = false;
	Policy->bStartBotLogicOnReady = false;
	if (bCreated)
	{
		Policy->RegisterComponent();
	}
}

bool ADemoCharacter::HasConfiguredPoolControlPolicy() const
{
	const UPawnInitializationConfig* Config = GetPawnInitializationComponent()->InitializationConfig;
	if (!Config)
	{
		return false;
	}
	for (const FPawnInitializationEntry& Entry : Config->Components)
	{
		if (Entry.ComponentClass && Entry.ComponentClass->IsChildOf(UPawnControlComponent::StaticClass()) &&
		    (Entry.bReplicateComponent || (HasAuthority() && Entry.bCreateOnAuthority) ||
		     (GetNetMode() != NM_DedicatedServer && Entry.bCreateOnClient)))
		{
			return true;
		}
	}
	return false;
}

void ADemoCharacter::HandlePoolPawnInitialized(UPawnInitializationComponent* Initialization)
{
	if (Initialization == GetPawnInitializationComponent() && PoolState.bPrepared && bPoolComponentsInitialized)
	{
		InitializePoolControlPolicy();
		ApplyPoolControlPolicy();
	}
}

void ADemoCharacter::ApplyPoolControlPolicy()
{
	if (!IsValid(PoolControlPolicy) || bPoolControlPolicyActive == PoolState.bActive)
	{
		return;
	}
	bPoolControlPolicyActive = PoolState.bActive;
	PoolControlPolicy->bAllowPlayerControl = PoolState.bActive && bPoolAllowPlayerControl;
	PoolControlPolicy->bAllowReturnControl = PoolState.bActive && bPoolAllowReturnControl;
	PoolControlPolicy->bReturnToBot = PoolState.bActive && bPoolReturnToBot;
	PoolControlPolicy->bStartBotLogicOnReady = PoolState.bActive && bPoolStartBotLogicOnReady;
	if (!PoolState.bActive && HasAuthority())
	{
		StopPoolBotLogic();
	}
}

void ADemoCharacter::StopPoolBotLogic()
{
	AAIController* Bot = GetController<AAIController>();
	if (Bot)
	{
		Bot->StopMovement();
	}
	for (AActor* BrainOwner : {static_cast<AActor*>(Bot), static_cast<AActor*>(this)})
	{
		if (!IsValid(BrainOwner) || BrainOwner->IsActorBeingDestroyed())
		{
			continue;
		}
		TInlineComponentArray<UBrainComponent*> Brains(BrainOwner);
		for (UBrainComponent* Brain : Brains)
		{
			if (IsValid(Brain) && Brain->IsRegistered() && Brain->IsRunning())
			{
				Brain->StopLogic(TEXT("Character returned to pool"));
			}
		}
	}
}

void ADemoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PoolState.bActive = false;
	if (bPoolInitializationSubscribed)
	{
		FPawnInitializationCallback Callback;
		Callback.BindDynamic(this, &ThisClass::HandlePoolPawnInitialized);
		GetPawnInitializationComponent()->UnregisterPawnInitializationCallback(Callback);
		bPoolInitializationSubscribed = false;
	}
	Super::EndPlay(EndPlayReason);
}
