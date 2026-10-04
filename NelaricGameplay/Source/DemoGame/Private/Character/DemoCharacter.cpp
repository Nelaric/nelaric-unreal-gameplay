// Copyright (c) 2026 Nelaric Contributors

#include "Character/DemoCharacter.h"
#include "Animation/DemoAnimationDataInstance.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/DemoCombatAttributes.h"
#include "GAS/DemoJumpAbility.h"
#include "GAS/DemoWeaponAbilities.h"
#include "GAS/DemoWeaponTags.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PawnGasBindingComponent.h"
#include "NelaricAbilitySystemComponent.h"
#include "Pawn/PawnControlComponent.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Pawn/PawnInitializationConfig.h"
#include "GasStateProfile.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "SkeletalMeshComponentBudgeted.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoCharacterPoolActivation, Log, All);

namespace Nelaric::DemoActions
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(Jump, "Action.Jump");
}

ADemoCharacter::ADemoCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USkeletalMeshComponentBudgeted>(ACharacter::MeshComponentName))
{
	USkeletalMeshComponentBudgeted* BudgetedMesh = CastChecked<USkeletalMeshComponentBudgeted>(GetMesh());
	BudgetedMesh->SetAutoRegisterWithBudgetAllocator(false);
	BudgetedMesh->SetAutoCalculateSignificance(false);
	BudgetedMesh->SetShouldUseActorRenderedFlag(false);
	BudgetedMesh->bUseScreenRenderStateForUpdate = true;
	DefaultStateProfile = CreateDefaultSubobject<UGasStateProfile>(TEXT("DefaultStateProfile"));
	DefaultStateProfile->Attributes = {
	    {UDemoCombatAttributes::GetMaxHealthAttribute(), EGasStateOwnership::Pawn, 100.0f},
	    {UDemoCombatAttributes::GetHealthAttribute(), EGasStateOwnership::Pawn, 100.0f},
	    {UDemoCombatAttributes::GetAttackAttribute(), EGasStateOwnership::Pawn, 10.0f}};
	DefaultStateProfile->Abilities.Add({UDemoJumpAbility::StaticClass(), Nelaric::DemoActions::Jump, 1});
	DefaultStateProfile->Abilities.Add({UDemoFireAbility::StaticClass(), Nelaric::DemoWeaponTags::Fire, 1});
	DefaultStateProfile->Abilities.Add({UDemoReloadAbility::StaticClass(), Nelaric::DemoWeaponTags::Reload, 1});
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
	if (GetNetMode() != NM_DedicatedServer)
	{
		// Apply after Blueprint defaults, before mesh BeginPlay can auto-register.
		USkeletalMeshComponentBudgeted* BudgetedMesh = CastChecked<USkeletalMeshComponentBudgeted>(GetMesh());
		BudgetedMesh->SetAutoRegisterWithBudgetAllocator(false);
		BudgetedMesh->SetAutoCalculateSignificance(false);
		BudgetedMesh->SetShouldUseActorRenderedFlag(false);
		BudgetedMesh->bUseScreenRenderStateForUpdate = true;
		BudgetedMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}
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
	CancelCombatActions();
	Nelaric::ObjectPool::FCharacterPoolHelper::Deactivate(*this, PoolState);
}

float ADemoCharacter::GetHealth() const
{
	check(IsInGameThread());
	const UPawnGasBindingComponent* Binding = GetGasBinding();
	const UNelaricAbilitySystemComponent* ASC =
	    Binding && Binding->HasCommittedState() ? Binding->GetAbilitySystem() : nullptr;
	return ASC ? ASC->GetNumericAttribute(UDemoCombatAttributes::GetHealthAttribute()) : 0.0f;
}

float ADemoCharacter::GetMaxHealth() const
{
	check(IsInGameThread());
	const UPawnGasBindingComponent* Binding = GetGasBinding();
	const UNelaricAbilitySystemComponent* ASC =
	    Binding && Binding->HasCommittedState() ? Binding->GetAbilitySystem() : nullptr;
	return ASC ? ASC->GetNumericAttribute(UDemoCombatAttributes::GetMaxHealthAttribute()) : 0.0f;
}

bool ADemoCharacter::IsAlive() const
{
	return !bCombatDead && GetHealth() > 0.0f;
}

void ADemoCharacter::CancelCombatActions()
{
	check(IsInGameThread());
	if (!HasAuthority())
	{
		return;
	}
	if (UDemoEquipmentManagerComponent* Manager = FindComponentByClass<UDemoEquipmentManagerComponent>())
	{
		Manager->CancelWeaponActions();
	}
	UPawnGasBindingComponent* Binding = GetGasBinding();
	UNelaricAbilitySystemComponent* ASC = Binding ? Binding->GetAbilitySystem() : nullptr;
	if (ASC && ASC->GetAvatarActor() == this)
	{
		ASC->SubmitAction(Nelaric::DemoActions::Jump, false);
		ASC->SubmitAction(Nelaric::DemoWeaponTags::Fire, false);
		ASC->SubmitAction(Nelaric::DemoWeaponTags::Reload, false);
		ASC->CancelAbilities();
	}
}

bool ADemoCharacter::ResetCombatState()
{
	check(IsInGameThread());
	UPawnGasBindingComponent* Binding = GetGasBinding();
	if (!HasAuthority() || IsActorBeingDestroyed() || !Binding || !Binding->IsReadyForActions())
	{
		return false;
	}
	CancelCombatActions();
	Binding->GetAbilitySystem()->SetNumericAttributeBase(UDemoCombatAttributes::GetHealthAttribute(), GetMaxHealth());
	if (UDemoEquipmentManagerComponent* Manager = FindComponentByClass<UDemoEquipmentManagerComponent>())
	{
		Manager->ResetWeaponAmmunition();
	}
	bDeathHandled = false;
	bCombatDead = false;
	FlushNetDormancy();
	ForceNetUpdate();
	if (IsPoolActive() && IsAlive())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (IsValid(PoolControlPolicy))
		{
			PoolControlPolicy->StartReadyBotLogic();
		}
	}
	NotifyCombatHealthChanged();
	return true;
}

void ADemoCharacter::NotifyCombatHealthChanged(AActor* DamageInstigator)
{
	const UPawnGasBindingComponent* Binding = GetGasBinding();
	if (IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown || !Binding ||
	    !Binding->IsReadyForActions())
	{
		return;
	}
	if (HasAuthority())
	{
		const bool bNewDead = GetHealth() <= 0.0f;
		if (bCombatDead != bNewDead)
		{
			bCombatDead = bNewDead;
			FlushNetDormancy();
			ForceNetUpdate();
		}
	}
	const bool bAlive = !bCombatDead;
	if (bAlive)
	{
		bDeathHandled = false;
	}
	const bool bNewDeath = !bAlive && !bDeathHandled;
	if (bNewDeath)
	{
		bDeathHandled = true;
		if (HasAuthority())
		{
			CancelCombatActions();
			GetCharacterMovement()->DisableMovement();
			StopPoolBotLogic();
		}
		else
		{
			CancelLocalWeaponActions();
		}
	}
	OnHealthChanged(GetHealth(), GetMaxHealth());
	if (bNewDeath && bCombatDead && bDeathHandled && !IsActorBeingDestroyed())
	{
		OnDeath(DamageInstigator);
	}
}

void ADemoCharacter::OnRep_CombatDead()
{
	if (!bCombatDead)
	{
		bDeathHandled = false;
		return;
	}
	if (bDeathHandled || IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	bDeathHandled = true;
	CancelLocalWeaponActions();
	OnDeath(nullptr);
}

void ADemoCharacter::CancelLocalWeaponActions()
{
	if (UDemoEquipmentManagerComponent* Manager = FindComponentByClass<UDemoEquipmentManagerComponent>())
	{
		for (UDemoEquipmentInstance* Item : Manager->GetEquipment())
		{
			if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Item))
			{
				Weapon->CancelWeaponActions();
			}
		}
	}
}

void ADemoCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADemoCharacter, bCombatDead);
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
	if (UDemoEquipmentManagerComponent* Manager = FindComponentByClass<UDemoEquipmentManagerComponent>())
	{
		Manager->RefreshEquipmentPresentation();
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
	CancelCombatActions();
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
