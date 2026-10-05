// Copyright (c) 2026 Nelaric Contributors

#include "Character/DemoCharacter.h"
#include "AI/DemoSoldierComponent.h"
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
#include "ObjectPool/DemoCharacterPoolSubsystem.h"
#include "Player/ControlSwitchSubsystem.h"
#include "Player/DemoOverviewPawn.h"
#include "Player/DemoPlayerController.h"
#include "PawnGasBindingComponent.h"
#include "NelaricAbilitySystemComponent.h"
#include "Pawn/PawnControlComponent.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Pawn/PawnInitializationConfig.h"
#include "Perception/AIPerceptionComponent.h"
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

UDemoSoldierComponent* ADemoCharacter::GetSoldierComponent() const
{
	check(IsInGameThread());
	UDemoSoldierComponent* Soldier = FindComponentByClass<UDemoSoldierComponent>();
	return IsValid(Soldier) && Soldier->IsRegistered() ? Soldier : nullptr;
}

bool ADemoCharacter::SetTeamId(uint8 InTeamId)
{
	check(IsInGameThread());
	if (!HasAuthority() || IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown ||
	    GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	if (TeamId == InTeamId)
	{
		return true;
	}
	TeamId = InTeamId;
	if (AAIController* Bot = Cast<AAIController>(GetController()))
	{
		Bot->SetGenericTeamId(FGenericTeamId(TeamId));
		if (UAIPerceptionComponent* Perception = Bot->GetAIPerceptionComponent())
		{
			Perception->RequestStimuliListenerUpdate();
		}
	}
	if (UDemoSoldierComponent* Soldier = GetSoldierComponent())
	{
		Soldier->NotifyTeamChanged();
	}
	FlushNetDormancy();
	ForceNetUpdate();
	TeamChanged.Broadcast();
	return true;
}

uint8 ADemoCharacter::GetTeamId() const
{
	check(IsInGameThread());
	return TeamId;
}

Nelaric::Demo::FTeamChanged& ADemoCharacter::OnTeamChanged()
{
	check(IsInGameThread());
	return TeamChanged;
}

void ADemoCharacter::SetGenericTeamId(const FGenericTeamId& InTeamId)
{
	SetTeamId(InTeamId.GetId());
}

FGenericTeamId ADemoCharacter::GetGenericTeamId() const
{
	return FGenericTeamId(GetTeamId());
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
	if (Nelaric::ObjectPool::FCharacterPoolHelper::IsPrepared(*this, PoolState))
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
		       *GetPathName(), IsPoolActive(), HasActorBegunPlay(), bIsCrouched);
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
	if (UDemoSoldierComponent* Soldier = GetSoldierComponent())
	{
		Soldier->RequestExecutionStart();
	}
	return true;
}

void ADemoCharacter::DeactivateToPool()
{
	CancelDeathReturn();
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
	return !DeathState.bDead && GetHealth() > 0.0f;
}

FVector ADemoCharacter::GetDeathImpulseDirection() const
{
	check(IsInGameThread());
	return DeathState.bDead ? FVector(DeathState.ImpulseDirection) : FVector::ZeroVector;
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
	CancelDeathReturn();
	CancelCombatActions();
	Binding->GetAbilitySystem()->SetNumericAttributeBase(UDemoCombatAttributes::GetHealthAttribute(), GetMaxHealth());
	if (UDemoEquipmentManagerComponent* Manager = FindComponentByClass<UDemoEquipmentManagerComponent>())
	{
		Manager->ResetWeaponAmmunition();
	}
	if (UDemoSoldierComponent* Soldier = GetSoldierComponent())
	{
		Soldier->ResetSoldierState();
	}
	bDeathHandled = false;
	DeathState = {};
	LastDamageDirection = FVector::ZeroVector;
	FlushNetDormancy();
	ForceNetUpdate();
	OnDeathPresentationReset();
	if (IsPoolActive() && IsAlive())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (UPawnControlComponent* Policy = FindComponentByClass<UPawnControlComponent>())
		{
			Policy->StartReadyBotLogic();
		}
	}
	NotifyCombatHealthChanged();
	return true;
}

void ADemoCharacter::NotifyCombatHealthChanged(AActor* DamageInstigator, const FVector* IncomingDamageDirection)
{
	check(IsInGameThread());
	const UPawnGasBindingComponent* Binding = GetGasBinding();
	if (IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown || !Binding ||
	    !Binding->IsReadyForActions())
	{
		return;
	}
	if (HasAuthority())
	{
		if (!DeathState.bDead && IncomingDamageDirection)
		{
			// A supplied zero direction is unknown damage, not a stale attacker.
			LastDamageDirection =
			    IncomingDamageDirection->ContainsNaN() ? FVector::ZeroVector : IncomingDamageDirection->GetSafeNormal();
		}
		const bool bNewDead = GetHealth() <= 0.0f;
		if (DeathState.bDead != bNewDead)
		{
			DeathState.bDead = bNewDead;
			DeathState.ImpulseDirection = bNewDead ? LastDamageDirection : FVector::ZeroVector;
			FlushNetDormancy();
			ForceNetUpdate();
		}
	}
	const bool bAlive = !DeathState.bDead;
	if (bAlive)
	{
		CancelDeathReturn();
		if (bDeathHandled)
		{
			LastDamageDirection = FVector::ZeroVector;
			OnDeathPresentationReset();
		}
		bDeathHandled = false;
	}
	const bool bNewDeath = !bAlive && !bDeathHandled;
	if (bNewDeath)
	{
		bDeathHandled = true;
		if (HasAuthority())
		{
			GetWorld()->GetTimerManager().SetTimer(DeathReturnTimer, this, &ThisClass::HandleDeathReturn, 4.0f, false);
			CancelCombatActions();
			if (UDemoSoldierComponent* Soldier = GetSoldierComponent())
			{
				Soldier->NotifyOwnerDeath();
			}
			GetCharacterMovement()->DisableMovement();
			StopPoolBotLogic();
		}
		else
		{
			CancelLocalWeaponActions();
		}
	}
	OnHealthChanged(GetHealth(), GetMaxHealth());
	if (bNewDeath && DeathState.bDead && bDeathHandled && !IsActorBeingDestroyed())
	{
		OnDeath(DamageInstigator);
	}
}

void ADemoCharacter::OnRep_DeathState()
{
	if (IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	if (!DeathState.bDead)
	{
		bDeathHandled = false;
		OnDeathPresentationReset();
		return;
	}
	if (bDeathHandled)
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
	DOREPLIFETIME(ADemoCharacter, DeathState);
	DOREPLIFETIME(ADemoCharacter, TeamId);
}

void ADemoCharacter::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);
	if (bPoolComponentsInitialized && Nelaric::ObjectPool::FCharacterPoolHelper::IsPrepared(*this, PoolState))
	{
		if (bNewHidden && !IsPoolActive())
		{
			OnDeathPresentationReset();
		}
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
		// Wait for the DA-created policy before applying pool restrictions.
		if (!bPoolInitializationSubscribed)
		{
			bPoolInitializationSubscribed = true;
			FPawnInitializationCallback Callback;
			Callback.BindDynamic(this, &ThisClass::HandlePoolPawnInitialized);
			GetPawnInitializationComponent()->RegisterAndCallPawnInitialized(Callback);
		}
		return;
	}
	if (!Policy)
	{
		return;
	}
	PoolControlPolicy = Policy;
	bPoolControlPolicyActive = false;
	// Preserve the authored control settings once.
	bPoolAllowPlayerControl = Policy->bAllowPlayerControl;
	bPoolAllowReturnControl = Policy->bAllowReturnControl;
	bPoolReturnToBot = Policy->bReturnToBot;
	bPoolStartBotLogicOnReady = Policy->bStartBotLogicOnReady;
	Policy->bAllowPlayerControl = false;
	Policy->bAllowReturnControl = false;
	Policy->bReturnToBot = false;
	Policy->bStartBotLogicOnReady = false;
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
	if (Initialization == GetPawnInitializationComponent() &&
	    Nelaric::ObjectPool::FCharacterPoolHelper::IsPrepared(*this, PoolState) && bPoolComponentsInitialized)
	{
		InitializePoolControlPolicy();
		ApplyPoolControlPolicy();
	}
}

void ADemoCharacter::ApplyPoolControlPolicy()
{
	const bool bActive = IsPoolActive();
	if (!IsValid(PoolControlPolicy) || bPoolControlPolicyActive == bActive)
	{
		return;
	}
	bPoolControlPolicyActive = bActive;
	PoolControlPolicy->bAllowPlayerControl = bActive && bPoolAllowPlayerControl;
	PoolControlPolicy->bAllowReturnControl = bActive && bPoolAllowReturnControl;
	PoolControlPolicy->bReturnToBot = bActive && bPoolReturnToBot;
	PoolControlPolicy->bStartBotLogicOnReady = bActive && bPoolStartBotLogicOnReady;
	if (!bActive && HasAuthority())
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
	CancelDeathReturn();
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

void ADemoCharacter::CancelDeathReturn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathReturnTimer);
	}
}

void ADemoCharacter::HandleDeathReturn()
{
	check(IsInGameThread());
	DeathReturnTimer.Invalidate();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || World->bIsTearingDown || IsActorBeingDestroyed() || !DeathState.bDead ||
	    !IsPoolActive())
	{
		return;
	}
	if (ADemoPlayerController* Player = Cast<ADemoPlayerController>(GetController()))
	{
		// Keep the coordinated GAS transfer when returning to the overview camera.
		UControlSwitchSubsystem* Coordinator = World->GetSubsystem<UControlSwitchSubsystem>();
		ADemoOverviewPawn* Overview = Player->GetOverviewPawn();
		if (!Coordinator || !IsValid(Overview) ||
		    Coordinator->ExecuteControlSwitch(Player, EControlSwitchAction::TakeControl, Overview) !=
		        EControlSwitchResult::Succeeded)
		{
			UE_LOG(LogDemoCharacterPoolActivation, Warning, TEXT("Cannot return dead %s: player control is retained."),
			       *GetName());
			return;
		}
	}
	UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->ReleaseDeadCharacter(this))
	{
		UE_LOG(LogDemoCharacterPoolActivation, Warning, TEXT("Cannot return dead %s: no releasable character lease."),
		       *GetName());
	}
	else
	{
		UE_LOG(LogDemoCharacterPoolActivation, Log, TEXT("Dead character returned: %s (free slots: %u)."), *GetName(),
		       Pool->NumFree());
	}
}
