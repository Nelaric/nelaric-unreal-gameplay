// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierComponent.h"

#include "AI/DemoSoldierCoverPoint.h"
#include "AI/DemoSoldierTags.h"
#include "AIController.h"
#include "AITypes.h"
#include "BrainComponent.h"
#include "Character/DemoCharacter.h"
#include "Components/StateTreeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "PawnGasBindingComponent.h"
#include "Pawn/PawnControlComponent.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Player/ControlSwitchSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoSoldier, Log, All);

namespace Nelaric::Soldier
{
static bool ValidSettings(const FDemoSoldierSettings& Settings)
{
	const float Values[] = {Settings.DecisionInterval,  Settings.AimToleranceDegrees, Settings.MemorySeconds,
	                        Settings.SearchSeconds,     Settings.AlertSeconds,        Settings.TargetLockSeconds,
	                        Settings.TargetSwitchRatio, Settings.UnderFireSeconds,    Settings.SuppressionDecay,
	                        Settings.CoverSearchRadius, Settings.CoverRetrySeconds,   Settings.MoveTimeoutSeconds,
	                        Settings.SprintSpeed};
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			return false;
		}
	}
	return Settings.DecisionInterval >= 0.05f && Settings.MemorySeconds > 0.0f && Settings.UnderFireSeconds > 0.0f &&
	       Settings.MoveTimeoutSeconds > 0.0f && FMath::IsFinite(Settings.ReactionSeconds.X) &&
	       FMath::IsFinite(Settings.ReactionSeconds.Y) && FMath::IsFinite(Settings.ObserveSeconds.X) &&
	       FMath::IsFinite(Settings.ObserveSeconds.Y);
}

static float SampleSeconds(FVector2D Range)
{
	const float Minimum = FMath::IsFinite(Range.X) ? FMath::Clamp(Range.X, 0.01, 10.0) : 0.2f;
	const float Maximum = FMath::IsFinite(Range.Y) ? FMath::Clamp(Range.Y, double(Minimum), 10.0) : Minimum;
	return FMath::FRandRange(Minimum, Maximum);
}

static FVector ObservedAimLocation(const AActor& Target)
{
	const APawn* Pawn = Cast<APawn>(&Target);
	return Pawn ? Pawn->GetPawnViewLocation() : Target.GetActorLocation();
}
} // namespace Nelaric::Soldier

UDemoSoldierComponent::UDemoSoldierComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDemoSoldierComponent::BeginPlay()
{
	Super::BeginPlay();
	ADemoCharacter* Character = GetPawn<ADemoCharacter>();
	UPawnInitializationComponent* Initialization =
	    Character && Character->HasAuthority() ? Character->GetPawnInitializationComponent() : nullptr;
	if (!Initialization)
	{
		return;
	}
	PawnInitialization = Initialization;
	FPawnInitializationCallback Revoked;
	Revoked.BindDynamic(this, &ThisClass::HandlePawnInitializationRevoked);
	Initialization->RegisterPawnInitializationRevoked(Revoked);
	FPawnInitializationCallback Ready;
	Ready.BindDynamic(this, &ThisClass::HandlePawnInitialized);
	Initialization->RegisterAndCallPawnInitialized(Ready);
}

void UDemoSoldierComponent::HandlePawnInitialized(UPawnInitializationComponent* Initialization)
{
	if (Initialization != PawnInitialization.Get() || !IsRegistered() || GetInitState() != Nelaric::EInitState::Ready ||
	    !Initialization->IsPawnInitialized())
	{
		return;
	}
	RequestExecutionStart();
}

void UDemoSoldierComponent::RequestExecutionStart()
{
	check(IsInGameThread());
	const ADemoCharacter* Character = GetPawn<ADemoCharacter>();
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || !IsRegistered() || !Character || Character->IsActorBeingDestroyed() ||
	    !Character->HasAuthority() || !Character->IsPoolActive() || !Character->IsAlive())
	{
		return;
	}
	if (!World->GetTimerManager().IsTimerActive(BrainStartTimer))
	{
		// Leave Ready broadcasts and pool activation before changing possession.
		World->GetTimerManager().SetTimer(BrainStartTimer, this, &ThisClass::StartReadyBrain, 0.001f, false);
	}
}

void UDemoSoldierComponent::StartReadyBrain()
{
	BrainStartTimer.Invalidate();
	ADemoCharacter* Character = GetPawn<ADemoCharacter>();
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || !IsRegistered() || !Character || Character->IsActorBeingDestroyed() ||
	    !Character->HasAuthority() || !Character->IsPoolActive() || !Character->IsAlive() ||
	    GetInitState() != Nelaric::EInitState::Ready || !PawnInitialization.IsValid() ||
	    !PawnInitialization->IsPawnInitialized() || !Character->GetGasBinding()->IsReadyForActions())
	{
		return;
	}
	UPawnControlComponent* Policy = Character->FindComponentByClass<UPawnControlComponent>();
	if (!Policy || !Policy->IsRegistered() || Policy->GetInitState() != Nelaric::EInitState::Ready ||
	    !Policy->bStartBotLogicOnReady || Policy->IsControlTransitionInProgress())
	{
		return;
	}
	if (!Character->GetController())
	{
		if (!Character->AIControllerClass || !Character->AIControllerClass->IsChildOf(AAIController::StaticClass()))
		{
			UE_LOG(LogDemoSoldier, Warning,
			       TEXT("Cannot create soldier controller for %s: configure AIControllerClass."),
			       *GetNameSafe(Character));
			return;
		}
		UControlSwitchSubsystem* Coordinator = World->GetSubsystem<UControlSwitchSubsystem>();
		if (!Coordinator)
		{
			UE_LOG(LogDemoSoldier, Warning, TEXT("Cannot create soldier controller for %s: no control coordinator."),
			       *GetNameSafe(Character));
			return;
		}
		FActorSpawnParameters Parameters;
		Parameters.Instigator = Character->GetInstigator();
		Parameters.OverrideLevel = Character->GetLevel();
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAIController* Bot = World->SpawnActor<AAIController>(
		    Character->AIControllerClass, Character->GetActorLocation(), Character->GetActorRotation(), Parameters);
		if (!Bot)
		{
			UE_LOG(LogDemoSoldier, Warning, TEXT("Cannot create soldier controller for %s: spawning failed."),
			       *GetNameSafe(Character));
			return;
		}
		// Pool prewarm has installed GAS state in custody. Only the coordinator
		// may transfer that state to the bot's participant before possession.
		const EControlSwitchResult Result =
		    Coordinator->ExecuteControlSwitch(Bot, EControlSwitchAction::TakeControl, Character);
		if (Result != EControlSwitchResult::Succeeded)
		{
			UE_LOG(LogDemoSoldier, Warning, TEXT("Cannot start soldier controller for %s: control result=%s."),
			       *GetNameSafe(Character), *UEnum::GetValueAsString(Result));
			if (IsValid(Bot) && !Bot->GetPawn() && !Coordinator->IsControlTransitionInProgress(Bot))
			{
				Bot->Destroy();
			}
			return;
		}
		// Possession invalidates the old Ready context. Its callback resumes us;
		// one deferred attempt also covers synchronous Ready during possession.
		RequestExecutionStart();
		return;
	}
	if (Character->GetController<AAIController>())
	{
		Policy->StartReadyBotLogic();
	}
}

void UDemoSoldierComponent::HandlePawnInitializationRevoked(UPawnInitializationComponent* Initialization)
{
	if (Initialization == PawnInitialization.Get())
	{
		CancelInitGenerationWork();
	}
}

bool UDemoSoldierComponent::AcceptsObservation() const
{
	const ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	return Character && Character->HasAuthority() && Character->IsPoolActive() &&
	       Behavior != EDemoSoldierBehavior::Dead;
}

void UDemoSoldierComponent::ClearGrenade(AActor* Source)
{
	check(IsInGameThread());
	if (GetOwner()->HasAuthority())
	{
		Grenades.RemoveAll([Source](const auto& Entry) { return Entry.Source == Source; });
		Wake();
	}
}

bool UDemoSoldierComponent::IssueOrder(const FDemoSoldierOrder& Order)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || Behavior == EDemoSoldierBehavior::Dead || Order.Location.ContainsNaN() ||
	    !FMath::IsFinite(Order.AcceptanceRadius) || Order.AcceptanceRadius <= 0.0f ||
	    !FMath::IsFinite(Order.HoldRadius) || Order.HoldRadius <= 0.0f ||
	    static_cast<uint8>(Order.Type) > static_cast<uint8>(EDemoSoldierOrderType::Defend) ||
	    (Order.Type == EDemoSoldierOrderType::Follow && !Order.Actor.IsValid()) ||
	    (Order.Actor.IsValid() && Order.Actor->GetWorld() != GetWorld()))
	{
		return false;
	}
	CancelAction();
	CurrentOrder = Order;
	++OrderRevision;
	if (!CurrentOrder.Id.IsValid())
	{
		CurrentOrder.Id = FGuid::NewGuid();
	}
	if (CurrentOrder.Type == EDemoSoldierOrderType::Attack && CurrentOrder.Actor.IsValid())
	{
		// The issuer supplies this initial position; only sight refreshes it.
		CurrentOrder.Location = CurrentOrder.Actor->GetActorLocation();
	}
	OrderStatus = CurrentOrder.Type == EDemoSoldierOrderType::None ? EDemoSoldierOrderStatus::Canceled
	                                                               : EDemoSoldierOrderStatus::Running;
	Behavior = EDemoSoldierBehavior::Idle;
	Emit(Nelaric::Soldier::OrderChanged);
	Wake();
	return true;
}

FDemoSoldierOrder UDemoSoldierComponent::GetOrder() const
{
	check(IsInGameThread());
	return CurrentOrder;
}

EDemoSoldierOrderStatus UDemoSoldierComponent::GetOrderStatus() const
{
	check(IsInGameThread());
	return OrderStatus;
}

FDemoSoldierMemory UDemoSoldierComponent::GetMemory() const
{
	check(IsInGameThread());
	return Memory;
}

EDemoSoldierBehavior UDemoSoldierComponent::GetBehavior() const
{
	check(IsInGameThread());
	return Behavior;
}

Nelaric::Soldier::FEvent& UDemoSoldierComponent::OnEvent()
{
	check(IsInGameThread());
	return Events;
}

uint8 UDemoSoldierComponent::GetTeamId() const
{
	check(IsInGameThread());
	const ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	return Character ? Character->GetTeamId() : FGenericTeamId::NoTeam.GetId();
}

void UDemoSoldierComponent::NotifyTeamChanged()
{
	check(IsInGameThread());
	const ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || Character->IsActorBeingDestroyed() || !GetWorld() ||
	    GetWorld()->bIsTearingDown)
	{
		return;
	}
	UpdateObservations(GetWorld()->GetTimeSeconds());
	if (AAIController* Bot = Cast<AAIController>(Character->GetController()))
	{
		if (UAIPerceptionComponent* Perception = Bot->GetAIPerceptionComponent())
		{
			TArray<AActor*> Visible;
			Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
			for (AActor* Actor : Visible)
			{
				if (IsValid(Actor))
				{
					ReportSight(Actor, Actor->GetActorLocation(), true);
				}
			}
		}
	}
	Wake();
}

bool UDemoSoldierComponent::IsHostile_Implementation(AActor* Actor) const
{
	const uint8 TeamId = GetTeamId();
	if (!IsValid(Actor) || Actor == GetOwner() || Actor->GetWorld() != GetWorld() || TeamId == 255)
	{
		return false;
	}
	if (const ADemoCharacter* Other = Cast<ADemoCharacter>(Actor))
	{
		return Other->GetTeamId() != 255 && Other->GetTeamId() != TeamId;
	}
	const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(Actor);
	if (!Agent)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		Agent = Pawn ? Cast<IGenericTeamAgentInterface>(Pawn->GetController()) : nullptr;
	}
	return Agent && Agent->GetGenericTeamId() != FGenericTeamId::NoTeam && Agent->GetGenericTeamId().GetId() != TeamId;
}

void UDemoSoldierComponent::ReportSight(AActor* Actor, FVector Location, bool bVisible)
{
	check(IsInGameThread());
	if (!AcceptsObservation() || !IsHostile(Actor) || Location.ContainsNaN())
	{
		return;
	}
	FDemoSoldierContact* Contact =
	    Contacts.FindByPredicate([Actor](const auto& Entry) { return Entry.Actor == Actor; });
	if (!Contact)
	{
		if (!bVisible || Contacts.Num() >= 64)
		{
			return;
		}
		Contact = &Contacts.AddDefaulted_GetRef();
		Contact->Actor = Actor;
	}
	const bool bChanged = Contact->bVisible != bVisible;
	Contact->bVisible = bVisible;
	if (bVisible)
	{
		Contact->Location = Location;
		Contact->LastSeenTime = GetWorld()->GetTimeSeconds();
	}
	else if (bChanged)
	{
		// Start memory expiry at the loss report without reading unseen position.
		Contact->LastSeenTime = GetWorld()->GetTimeSeconds();
	}
	if (bChanged)
	{
		Emit(bVisible ? Nelaric::Soldier::EnemySeen : Nelaric::Soldier::EnemyLost);
	}
	Wake();
}

void UDemoSoldierComponent::ReportSound(FVector Location)
{
	check(IsInGameThread());
	if (!AcceptsObservation() || Location.ContainsNaN())
	{
		return;
	}
	Memory.HeardLocation = Location;
	Memory.LastHeardTime = GetWorld()->GetTimeSeconds();
	Emit(Nelaric::Soldier::SoundHeard);
	Wake();
}

void UDemoSoldierComponent::ReportDamage(AActor* Source, float Amount, FVector TowardSource)
{
	check(IsInGameThread());
	if (!AcceptsObservation() || !FMath::IsFinite(Amount) || Amount <= 0.0f || TowardSource.ContainsNaN())
	{
		return;
	}
	RefreshPressure(GetWorld()->GetTimeSeconds());
	Memory.LastDamageTime = GetWorld()->GetTimeSeconds();
	Memory.DamageDirection = TowardSource.GetSafeNormal();
	Memory.bUnderFire = true;
	Memory.Suppression = FMath::Clamp(Memory.Suppression + Amount * 0.015f, 0.0f, 1.0f);
	if (FDemoSoldierContact* Contact =
	        Contacts.FindByPredicate([Source](const auto& Entry) { return Entry.Actor == Source; }))
	{
		Contact->LastDamageTime = Memory.LastDamageTime;
	}
	Emit(Nelaric::Soldier::Damaged);
	Wake();
}

void UDemoSoldierComponent::ReportNearMiss(float Intensity)
{
	check(IsInGameThread());
	if (!AcceptsObservation() || !FMath::IsFinite(Intensity) || Intensity <= 0.0f)
	{
		return;
	}
	RefreshPressure(GetWorld()->GetTimeSeconds());
	Memory.LastDamageTime = GetWorld()->GetTimeSeconds();
	Memory.bUnderFire = true;
	Memory.Suppression = FMath::Clamp(Memory.Suppression + Intensity, 0.0f, 1.0f);
	Emit(Nelaric::Soldier::NearMiss);
	Wake();
}

void UDemoSoldierComponent::ReportGrenade(AActor* Source, FVector Location, float Radius, float RemainingSeconds)
{
	check(IsInGameThread());
	if (!AcceptsObservation() || Location.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.0f ||
	    !FMath::IsFinite(RemainingSeconds) || RemainingSeconds <= 0.0f)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	Grenades.RemoveAll([Now](const auto& Entry) { return Entry.ExpiresAt <= Now; });
	Nelaric::Soldier::FGrenadeDanger* Danger =
	    Grenades.FindByPredicate([Source](const auto& Entry) { return Entry.Source == Source; });
	if (!Danger)
	{
		if (Grenades.Num() >= 8)
		{
			return;
		}
		Danger = &Grenades.AddDefaulted_GetRef();
	}
	Danger->Source = Source;
	Danger->Location = Location;
	Danger->Radius = FMath::Min(Radius, 10000.0f);
	Danger->ExpiresAt = Now + FMath::Min(RemainingSeconds, 30.0f);
	if (Behavior == EDemoSoldierBehavior::AvoidGrenade && IsDangerous(MoveGoal))
	{
		CancelAction();
		Behavior = EDemoSoldierBehavior::Idle;
		if (!bNativePlanner)
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
	}
	EscapeAttempts = 0;
	Emit(Nelaric::Soldier::GrenadeDanger);
	Wake();
}

bool UDemoSoldierComponent::StartExecution(AAIController* InController, UObject* Driver, bool bInNativePlanner)
{
	check(IsInGameThread());
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	if (!Nelaric::Soldier::ValidSettings(Settings))
	{
		UE_LOG(LogDemoSoldier, Warning, TEXT("Cannot start soldier %s: invalid decision settings."),
		       *GetNameSafe(GetOwner()));
		return false;
	}
	if (GetInitState() != Nelaric::EInitState::Ready || !PawnInitialization.IsValid() ||
	    !PawnInitialization->IsPawnInitialized() || !Character || !Character->HasAuthority() || !IsValid(Driver) ||
	    !IsValid(InController) || InController->GetPawn() != Character || !Character->IsPoolActive() ||
	    !Character->IsAlive() || !Character->GetGasBinding()->IsReadyForActions() ||
	    (ExecutionDriver.IsValid() && ExecutionDriver != Driver))
	{
		UE_LOG(LogDemoSoldier, Verbose,
		       TEXT("Soldier start deferred: pawn=%s state=%d pawnReady=%d controller=%s active=%d alive=%d "
		            "GASReady=%d driver=%s currentDriver=%s."),
		       *GetNameSafe(Character), static_cast<int32>(GetInitState()),
		       PawnInitialization.IsValid() && PawnInitialization->IsPawnInitialized(), *GetNameSafe(InController),
		       Character && Character->IsPoolActive(), Character && Character->IsAlive(),
		       Character && Character->GetGasBinding()->IsReadyForActions(), *GetNameSafe(Driver),
		       *GetNameSafe(ExecutionDriver.Get()));
		return false;
	}
	if (ExecutionDriver == Driver)
	{
		return true;
	}
	Controller = InController;
	ExecutionDriver = Driver;
	bNativePlanner = bInNativePlanner;
	InController->SetGenericTeamId(FGenericTeamId(GetTeamId()));
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	OriginalWalkSpeed = Movement->MaxWalkSpeed;
	bSavedOrientMovement = Movement->bOrientRotationToMovement;
	bSavedRotationYaw = Character->bUseControllerRotationYaw;
	bSavedCrouched = Character->bIsCrouched;
	Character->bUseControllerRotationYaw = true;
	Movement->bOrientRotationToMovement = false;
	MoveHandle =
	    InController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &ThisClass::HandleMoveFinished);
	RefreshPressure(GetWorld()->GetTimeSeconds());
	Behavior = EDemoSoldierBehavior::Idle;
	if (!bNativePlanner)
	{
		LastSelfLocation = Character->GetActorLocation();
		RefreshTreeObservers();
	}
	if (UAIPerceptionComponent* Perception = InController->GetAIPerceptionComponent())
	{
		Perception->RequestStimuliListenerUpdate();
		TArray<AActor*> Visible;
		Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
		for (AActor* Actor : Visible)
		{
			if (IsValid(Actor))
			{
				ReportSight(Actor, Actor->GetActorLocation(), true);
			}
		}
	}
	Wake();
	UE_LOG(LogDemoSoldier, Log,
	       TEXT("Soldier execution started: pawn=%s controller=%s driver=%s nativePlanner=%d team=%u pawnYaw=%.2f "
	            "controlYaw=%.2f."),
	       *GetNameSafe(Character), *GetNameSafe(InController), *GetNameSafe(Driver), bNativePlanner, GetTeamId(),
	       Character->GetActorRotation().Yaw, InController->GetControlRotation().Yaw);
	return true;
}

bool UDemoSoldierComponent::IsExecutingFor(const UObject* Driver) const
{
	return Driver && ExecutionDriver.Get() == Driver;
}

void UDemoSoldierComponent::StopExecution(UObject* Driver)
{
	check(IsInGameThread());
	if (Driver && ExecutionDriver.Get() != Driver)
	{
		return;
	}
	if (!ExecutionDriver.IsValid() && !Controller.IsValid())
	{
		return;
	}
	ClearTreeObservers();
	CancelAction();
	TreeResult = EDemoSoldierTreeResult::Failed;
	NotifyTreeActionResult();
	TreeActionId.Invalidate();
	if (AAIController* Bot = Controller.Get())
	{
		Bot->GetPathFollowingComponent()->OnRequestFinished.Remove(MoveHandle);
		Bot->ClearFocus(EAIFocusPriority::Gameplay);
	}
	MoveHandle.Reset();
	RestoreMovement();
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Controller.Reset();
	ExecutionDriver.Reset();
	for (FDemoSoldierContact& Contact : Contacts)
	{
		Contact.bVisible = false;
	}
	Memory.bTargetVisible = false;
	if (Behavior != EDemoSoldierBehavior::Dead)
	{
		Behavior = EDemoSoldierBehavior::Idle;
	}
}

void UDemoSoldierComponent::NotifyOwnerDeath()
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	StopExecution(nullptr);
	Behavior = EDemoSoldierBehavior::Dead;
	Contacts.Reset();
	Memory = {};
	Grenades.Reset();
	if (OrderStatus == EDemoSoldierOrderStatus::Running)
	{
		CompleteOrder(EDemoSoldierOrderStatus::Failed);
	}
}

void UDemoSoldierComponent::ResetSoldierState()
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	CancelAction();
	CurrentOrder = {};
	++OrderRevision;
	TreeResult = EDemoSoldierTreeResult::Failed;
	NotifyTreeActionResult();
	TreeActionId.Invalidate();
	OrderStatus = EDemoSoldierOrderStatus::None;
	Memory = {};
	Contacts.Reset();
	Grenades.Reset();
	Cover.Reset();
	ConsumedAlertTime = -1.0;
	NextCoverTime = 0.0;
	NextReloadTime = 0.0;
	bInitialTreeOrderApplied = false;
	bSuppressed = false;
	Behavior = EDemoSoldierBehavior::Idle;
	Emit(Nelaric::Soldier::OrderChanged);
	Wake();
}

void UDemoSoldierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPawnInitializationComponent* Initialization = PawnInitialization.Get())
	{
		FPawnInitializationCallback Ready;
		Ready.BindDynamic(this, &ThisClass::HandlePawnInitialized);
		Initialization->UnregisterPawnInitializationCallback(Ready);
		FPawnInitializationCallback Revoked;
		Revoked.BindDynamic(this, &ThisClass::HandlePawnInitializationRevoked);
		Initialization->UnregisterPawnInitializationCallback(Revoked);
	}
	PawnInitialization.Reset();
	StopExecution(nullptr);
	GetWorld()->GetTimerManager().ClearTimer(EventTimer);
	GetWorld()->GetTimerManager().ClearTimer(BrainStartTimer);
	PendingEvents.Reset();
	Events.Clear();
	TreeActionFinished.Clear();
	Super::EndPlay(EndPlayReason);
}

void UDemoSoldierComponent::CancelInitGenerationWork()
{
	TWeakObjectPtr<UObject> PreviousDriver = ExecutionDriver;
	StopExecution(nullptr);
	if (UBrainComponent* Brain = Cast<UBrainComponent>(PreviousDriver.Get()); Brain && Brain->IsRunning())
	{
		// A tree must exit its tasks before the next pawn Ready can restart it.
		Brain->StopLogic(TEXT("Soldier pawn initialization revoked"));
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EventTimer);
		World->GetTimerManager().ClearTimer(BrainStartTimer);
	}
	PendingEvents.Reset();
	Super::CancelInitGenerationWork();
}

void UDemoSoldierComponent::Emit(FGameplayTag Tag)
{
	// Deliver outside mutations so observers may safely replace orders.
	if (PendingEvents.Num() < 32)
	{
		PendingEvents.AddUnique(Tag);
	}
	if (!GetWorld()->GetTimerManager().IsTimerActive(EventTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(EventTimer, this, &ThisClass::FlushEvents, 0.001f, false);
	}
}

void UDemoSoldierComponent::FlushEvents()
{
	EventTimer.Invalidate();
	const TArray<FGameplayTag> Dispatch = MoveTemp(PendingEvents);
	for (FGameplayTag Tag : Dispatch)
	{
		if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed())
		{
			return;
		}
		if (UStateTreeComponent* Tree = Cast<UStateTreeComponent>(ExecutionDriver.Get()))
		{
			Tree->SendStateTreeEvent(Tag);
		}
		Events.Broadcast(Tag);
	}
}

void UDemoSoldierComponent::Wake()
{
	if (ExecutionDriver.IsValid() && Controller.IsValid())
	{
		const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(UpdateTimer);
		if (Remaining <= 0.0f || Remaining > 0.01f)
		{
			Schedule(0.01f);
		}
	}
}

void UDemoSoldierComponent::Schedule(float Delay)
{
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::Update, FMath::Max(0.01f, Delay), false);
}

void UDemoSoldierComponent::RestoreMovement()
{
	if (ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner()); Character && Controller.IsValid())
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = OriginalWalkSpeed;
		Character->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrientMovement;
		Character->bUseControllerRotationYaw = bSavedRotationYaw;
		if (!bSavedCrouched)
		{
			Character->UnCrouch();
		}
	}
}

void UDemoSoldierComponent::CancelAction()
{
	++ActionRevision;
	MoveId = FAIRequestID::InvalidRequest;
	bMoveFinished = false;
	if (AAIController* Bot = Controller.Get())
	{
		Bot->StopMovement();
		Bot->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (UDemoWeaponInstance* Weapon = ActionWeapon.Get())
	{
		Weapon->OnReloadFinished().Remove(ReloadHandle);
		Weapon->OnActionsCanceled().Remove(WeaponCanceledHandle);
		if (ReloadId.IsValid())
		{
			Weapon->CancelReload(ReloadId);
		}
	}
	ReloadId.Invalidate();
	ReloadHandle.Reset();
	WeaponCanceledHandle.Reset();
	ActionWeapon.Reset();
	ActionTarget.Reset();
	RoundsRemaining = 0;
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	if (Character && ExecutionDriver.IsValid())
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = OriginalWalkSpeed;
	}
}

void UDemoSoldierComponent::SetBehavior(EDemoSoldierBehavior Next, double Now)
{
	if (Behavior != Next)
	{
		CancelAction();
		Behavior = Next;
		ActionDeadline = Now;
	}
}

UDemoWeaponInstance* UDemoSoldierComponent::GetWeapon() const
{
	const UDemoEquipmentManagerComponent* Equipment =
	    GetOwner()->FindComponentByClass<UDemoEquipmentManagerComponent>();
	return Equipment ? Equipment->GetActiveWeapon() : nullptr;
}

bool UDemoSoldierComponent::IsInsideOrderArea(FVector Location) const
{
	return OrderStatus != EDemoSoldierOrderStatus::Running ||
	       (CurrentOrder.Type != EDemoSoldierOrderType::Hold && CurrentOrder.Type != EDemoSoldierOrderType::Defend) ||
	       FVector::DistSquared2D(Location, CurrentOrder.Location) <= FMath::Square(CurrentOrder.HoldRadius);
}

bool UDemoSoldierComponent::CanPursue() const
{
	return OrderStatus != EDemoSoldierOrderStatus::Running || CurrentOrder.bAllowPursuit;
}

void UDemoSoldierComponent::SetObservationFocus(FVector Location)
{
	AAIController* Bot = Controller.Get();
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	const FVector Direction = Location - Pawn->GetPawnViewLocation();
	if (!FAISystem::IsValidLocation(Location) || Direction.ContainsNaN() || Direction.SizeSquared2D() <= 1.0)
	{
		// A coincident or vertical-only cue has no yaw; UE would use zero.
		Bot->ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}
	Bot->SetFocalPoint(Location);
}

bool UDemoSoldierComponent::StartMove(FVector Destination, float Radius, bool bEmergency)
{
	AAIController* Bot = Controller.Get();
	if (!Bot || Destination.ContainsNaN() || (!bEmergency && !IsInsideOrderArea(Destination)))
	{
		bMoveFinished = true;
		bMoveSucceeded = false;
		Emit(Nelaric::Soldier::MoveFailed);
		return false;
	}
	MoveId = FAIRequestID::InvalidRequest;
	Bot->StopMovement();
	Memory.bInCover = false;
	if (ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner()))
	{
		Character->UnCrouch();
	}
	FAIMoveRequest Request;
	Request.SetGoalLocation(Destination);
	Request.SetAcceptanceRadius(Radius);
	Request.SetAllowPartialPath(false);
	Request.SetReachTestIncludesAgentRadius(false);
	const FPathFollowingRequestResult Result = Bot->MoveTo(Request);
	MoveId = Result.MoveId;
	MoveGoal = Destination;
	MoveDeadline = GetWorld()->GetTimeSeconds() + FMath::Clamp(Settings.MoveTimeoutSeconds, 1.0f, 120.0f);
	bMoveFinished = Result.Code != EPathFollowingRequestResult::RequestSuccessful;
	bMoveSucceeded = Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
	if (bMoveFinished)
	{
		Emit(bMoveSucceeded ? Nelaric::Soldier::MoveCompleted : Nelaric::Soldier::MoveFailed);
	}
	return Result.Code != EPathFollowingRequestResult::Failed;
}

void UDemoSoldierComponent::HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result)
{
	if (!ExecutionDriver.IsValid() || !MoveId.IsValid() || RequestId != MoveId)
	{
		return;
	}
	MoveId = FAIRequestID::InvalidRequest;
	bMoveFinished = true;
	bMoveSucceeded = Result.IsSuccess();
	Emit(bMoveSucceeded ? Nelaric::Soldier::MoveCompleted : Nelaric::Soldier::MoveFailed);
	Wake();
}

bool UDemoSoldierComponent::FinishMovement(double Now)
{
	if (!bMoveFinished && Now >= MoveDeadline)
	{
		MoveId = FAIRequestID::InvalidRequest;
		Controller->StopMovement();
		bMoveFinished = true;
		bMoveSucceeded = false;
		Emit(Nelaric::Soldier::MoveFailed);
	}
	return bMoveFinished;
}

void UDemoSoldierComponent::RefreshPressure(double Now)
{
	const float Elapsed = FMath::Max(0.0f, float(Now - LastUpdateTime));
	LastUpdateTime = Now;
	Memory.Suppression = FMath::Max(0.0f, Memory.Suppression - Elapsed * FMath::Max(0.01f, Settings.SuppressionDecay));
	Memory.bUnderFire = Memory.LastDamageTime >= 0.0 && Now - Memory.LastDamageTime < Settings.UnderFireSeconds;
	bSuppressed = Memory.Suppression >= (bSuppressed ? 0.35f : 0.65f);
}

void UDemoSoldierComponent::UpdateObservations(double Now)
{
	RefreshPressure(Now);
	Grenades.RemoveAll([Now](const auto& Danger) { return Now >= Danger.ExpiresAt; });
	for (int32 Index = Contacts.Num() - 1; Index >= 0; --Index)
	{
		FDemoSoldierContact& Contact = Contacts[Index];
		AActor* Actor = Contact.Actor.Get();
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !IsHostile(Actor) ||
		    (!Contact.bVisible && Now - Contact.LastSeenTime >= Settings.MemorySeconds))
		{
			Contacts.RemoveAtSwap(Index);
			continue;
		}
		if (Contact.bVisible)
		{
			const ADemoCharacter* Character = Cast<ADemoCharacter>(Actor);
			if (Character && (!Character->IsAlive() || !Character->IsPoolActive()))
			{
				if (Memory.Target == Actor)
				{
					Emit(Nelaric::Soldier::TargetDead);
				}
				if (OrderStatus == EDemoSoldierOrderStatus::Running &&
				    CurrentOrder.Type == EDemoSoldierOrderType::Attack && CurrentOrder.Actor == Actor)
				{
					CompleteOrder(EDemoSoldierOrderStatus::Completed);
				}
				Contacts.RemoveAtSwap(Index);
				continue;
			}
			Contact.Location = Nelaric::Soldier::ObservedAimLocation(*Actor);
			Contact.LastSeenTime = Now;
		}
	}
	if (Memory.bInCover &&
	    (!Cover.IsValid() || !Cover->bEnabled ||
	     FVector::DistSquared2D(GetOwner()->GetActorLocation(), Cover->GetActorLocation()) > FMath::Square(150.0f)))
	{
		Memory.bInCover = false;
	}
	SelectTarget(Now);
}

float UDemoSoldierComponent::ScoreContact(const FDemoSoldierContact& Contact, double Now) const
{
	const float Distance = FVector::Distance(GetOwner()->GetActorLocation(), Contact.Location);
	float Score = (Contact.bVisible ? 100.0f : 0.0f) + 30.0f / (1.0f + Distance / 1000.0f);
	if (Contact.LastDamageTime >= 0.0 && Now - Contact.LastDamageTime < Settings.MemorySeconds)
	{
		Score += 80.0f;
	}
	if (OrderStatus == EDemoSoldierOrderStatus::Running && CurrentOrder.Type == EDemoSoldierOrderType::Attack &&
	    CurrentOrder.Actor == Contact.Actor)
	{
		Score += 30.0f;
	}
	return Score;
}

void UDemoSoldierComponent::SelectTarget(double Now)
{
	FDemoSoldierContact* Current = nullptr;
	FDemoSoldierContact* Best = nullptr;
	float BestScore = -1.0f;
	for (FDemoSoldierContact& Contact : Contacts)
	{
		if (Contact.Actor == Memory.Target)
		{
			Current = &Contact;
		}
		const float Score = ScoreContact(Contact, Now);
		if (Score > BestScore)
		{
			Best = &Contact;
			BestScore = Score;
		}
	}
	if (Current && Best != Current && !(Best && Best->bVisible && !Current->bVisible) &&
	    (Now - TargetSelectedTime < Settings.TargetLockSeconds ||
	     BestScore <= ScoreContact(*Current, Now) * FMath::Max(1.0f, Settings.TargetSwitchRatio)))
	{
		Best = Current;
	}
	const TWeakObjectPtr<AActor> Next = Best ? Best->Actor : nullptr;
	if (Next != Memory.Target)
	{
		Memory.Target = Next;
		TargetSelectedTime = Now;
		Emit(Nelaric::Soldier::TargetChanged);
	}
	Memory.bTargetVisible = Best && Best->bVisible;
	if (Best)
	{
		Memory.TargetLocation = Best->Location;
	}
}

void UDemoSoldierComponent::ForgetTarget()
{
	const TWeakObjectPtr<AActor> Previous = Memory.Target;
	Contacts.RemoveAll([Previous](const auto& Contact) { return Contact.Actor == Previous; });
	Memory.Target.Reset();
	Memory.bTargetVisible = false;
	Emit(Nelaric::Soldier::TargetChanged);
}

bool UDemoSoldierComponent::IsDangerous(FVector Location) const
{
	return Grenades.ContainsByPredicate(
	    [Location](const auto& Danger)
	    { return FVector::DistSquared(Location, Danger.Location) < FMath::Square(Danger.Radius + 150.0f); });
}

bool UDemoSoldierComponent::FindEscape(FVector& Location) const
{
	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Navigation || Grenades.IsEmpty())
	{
		return false;
	}
	const FVector Start = GetOwner()->GetActorLocation();
	float BestDistance = TNumericLimits<float>::Max();
	bool bFound = false;
	for (const Nelaric::Soldier::FGrenadeDanger& Danger : Grenades)
	{
		FVector Away = (Start - Danger.Location).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
		}
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector Direction = Away.RotateAngleAxis(Index * 45.0f, FVector::UpVector);
			const FVector Candidate = Danger.Location + Direction * (Danger.Radius + 350.0f);
			FNavLocation Projected;
			if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(150.0f, 150.0f, 400.0f)) ||
			    IsDangerous(Projected.Location))
			{
				continue;
			}
			const float Distance = FVector::DistSquared(Start, Projected.Location);
			if (Distance < BestDistance)
			{
				const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
				    GetWorld(), Start, Projected.Location, Controller.Get());
				if (!Path || !Path->IsValid() || Path->IsPartial())
				{
					continue;
				}
				Location = Projected.Location;
				BestDistance = Distance;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool UDemoSoldierComponent::TryCover(double Now)
{
	const ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	const bool bLowHealth = Character && Character->GetHealth() < Character->GetMaxHealth() * 0.3f;
	if (Memory.bInCover || Now < NextCoverTime || !(Memory.bUnderFire || bLowHealth || bSuppressed))
	{
		return false;
	}
	NextCoverTime = Now + FMath::Max(0.1f, Settings.CoverRetrySeconds);
	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Navigation)
	{
		return false;
	}
	const FVector Origin = GetOwner()->GetActorLocation();
	const FVector Threat = Memory.Target.IsValid() ? Memory.TargetLocation : Origin + Memory.DamageDirection * 2000.0f;
	if (!Memory.Target.IsValid() && Memory.DamageDirection.IsNearlyZero())
	{
		return false;
	}
	ADemoSoldierCoverPoint* Best = nullptr;
	FVector Goal = FVector::ZeroVector;
	float BestDistance = FMath::Square(Settings.CoverSearchRadius);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SoldierCover), false, GetOwner());
	Query.AddIgnoredActor(Memory.Target.Get());
	for (TActorIterator<ADemoSoldierCoverPoint> It(GetWorld()); It; ++It)
	{
		const FVector Point = It->GetActorLocation();
		const float Distance = FVector::DistSquared2D(Origin, Point);
		if (!It->bEnabled || Distance >= BestDistance || !IsInsideOrderArea(Point) || IsDangerous(Point))
		{
			continue;
		}
		FNavLocation Projected;
		FHitResult Hit;
		if (Navigation->ProjectPointToNavigation(Point, Projected, FVector(100.0f, 100.0f, 200.0f)) &&
		    IsInsideOrderArea(Projected.Location) &&
		    GetWorld()->LineTraceSingleByChannel(Hit, Projected.Location + FVector(0.0f, 0.0f, 48.0f), Threat,
		                                         ECC_Visibility, Query))
		{
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			    GetWorld(), Origin, Projected.Location, Controller.Get());
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				continue;
			}
			Best = *It;
			Goal = Projected.Location;
			BestDistance = Distance;
		}
	}
	if (!Best)
	{
		return false;
	}
	SetBehavior(EDemoSoldierBehavior::TakeCover, Now);
	Cover = Best;
	StartMove(Goal, 75.0f);
	return true;
}

bool UDemoSoldierComponent::CanShoot()
{
	UDemoWeaponInstance* Weapon = GetWeapon();
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	AActor* Target = Memory.Target.Get();
	if (!Definition || !Pawn || !IsValid(Target) || !Memory.bTargetVisible || !IsHostile(Target) ||
	    Weapon->GetWeaponState().bReloading || Weapon->GetWeaponState().MagazineAmmo <= 0)
	{
		return false;
	}
	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector Aim = Nelaric::Soldier::ObservedAimLocation(*Target);
	if (FVector::DistSquared(Start, Aim) > FMath::Square(Definition->Range))
	{
		return false;
	}
	Controller->SetFocus(Target);
	const float Dot = FVector::DotProduct(Pawn->GetBaseAimRotation().Vector(), (Aim - Start).GetSafeNormal());
	if (Dot < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Settings.AimToleranceDegrees, 0.1f, 45.0f))))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SoldierFireSafety), false, Pawn);
	// A fresh shot-time trace prevents stale perception from firing through cover.
	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Aim, Definition->TraceChannel, Query) ||
	       Hit.GetActor() == Target;
}

bool UDemoSoldierComponent::WantsReload() const
{
	const UDemoWeaponInstance* Weapon = GetWeapon();
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	if (!Definition)
	{
		return false;
	}
	const FDemoWeaponState State = Weapon->GetWeaponState();
	return GetWorld()->GetTimeSeconds() >= NextReloadTime && !State.bReloading && State.ReserveAmmo > 0 &&
	       State.MagazineAmmo < Definition->MagazineCapacity &&
	       (State.MagazineAmmo == 0 ||
	        (State.MagazineAmmo <= Settings.ReloadThreshold && (!Memory.bTargetVisible || Memory.bInCover)));
}

bool UDemoSoldierComponent::StartReload(double Now)
{
	SetBehavior(EDemoSoldierBehavior::Reload, Now);
	UDemoWeaponInstance* Weapon = GetWeapon();
	if (!Weapon)
	{
		return false;
	}
	ActionWeapon = Weapon;
	ReloadHandle = Weapon->OnReloadFinished().AddUObject(this, &ThisClass::HandleReloadFinished);
	WeaponCanceledHandle = Weapon->OnActionsCanceled().AddUObject(this, &ThisClass::HandleWeaponCanceled);
	if (Weapon->BeginReload(ReloadId) != EDemoWeaponResult::Success)
	{
		NextReloadTime = Now + 0.5;
		CancelAction();
		Behavior = EDemoSoldierBehavior::Observe;
		ActionDeadline = Now + 0.4;
		return false;
	}
	ActionDeadline = Now + Weapon->GetReloadRemainingTime() + 1.0;
	return true;
}

void UDemoSoldierComponent::HandleReloadFinished(FGuid Id, EDemoWeaponResult Result)
{
	if (Id != ReloadId || !ExecutionDriver.IsValid())
	{
		return;
	}
	ReloadId.Invalidate();
	if (!bNativePlanner)
	{
		TreeResult =
		    Result == EDemoWeaponResult::Success ? EDemoSoldierTreeResult::Succeeded : EDemoSoldierTreeResult::Failed;
		if (Result == EDemoWeaponResult::Success)
		{
			Emit(Nelaric::Soldier::ReloadCompleted);
		}
		else
		{
			NextReloadTime = GetWorld()->GetTimeSeconds() + 0.5;
		}
		Wake();
		NotifyTreeActionResult();
		return;
	}
	SetBehavior(EDemoSoldierBehavior::Observe, GetWorld()->GetTimeSeconds());
	ActionDeadline += 0.2;
	if (Result == EDemoWeaponResult::Success)
	{
		Emit(Nelaric::Soldier::ReloadCompleted);
	}
	Wake();
}

void UDemoSoldierComponent::HandleWeaponCanceled()
{
	if (!bNativePlanner)
	{
		TreeResult = EDemoSoldierTreeResult::Failed;
		NextReloadTime = GetWorld()->GetTimeSeconds() + 0.5;
		Wake();
		NotifyTreeActionResult();
		return;
	}
	SetBehavior(EDemoSoldierBehavior::Observe, GetWorld()->GetTimeSeconds());
	ActionDeadline += 0.2;
	Wake();
}

void UDemoSoldierComponent::CompleteOrder(EDemoSoldierOrderStatus Status)
{
	OrderStatus = Status;
	Emit(Status == EDemoSoldierOrderStatus::Completed ? Nelaric::Soldier::OrderCompleted
	                                                  : Nelaric::Soldier::OrderFailed);
}

void UDemoSoldierComponent::ExecuteCombat(double Now)
{
	if (!Memory.bTargetVisible)
	{
		if (ActionTarget != Memory.Target ||
		    (Behavior != EDemoSoldierBehavior::MoveToMemory && Behavior != EDemoSoldierBehavior::Search))
		{
			const bool bMayMove = CanPursue() && IsInsideOrderArea(Memory.TargetLocation);
			SetBehavior(bMayMove ? EDemoSoldierBehavior::MoveToMemory : EDemoSoldierBehavior::Search, Now);
			ActionTarget = Memory.Target;
			SearchDeadline = Now + FMath::Max(0.1f, Settings.SearchSeconds);
			if (bMayMove)
			{
				StartMove(Memory.TargetLocation, 120.0f);
			}
			else
			{
				SetObservationFocus(Memory.TargetLocation);
			}
		}
		if (Behavior == EDemoSoldierBehavior::MoveToMemory && FinishMovement(Now))
		{
			SetBehavior(EDemoSoldierBehavior::Search, Now);
			ActionTarget = Memory.Target;
			SearchDeadline = Now + FMath::Max(0.1f, Settings.SearchSeconds);
			SetObservationFocus(Memory.TargetLocation);
		}
		if (Behavior == EDemoSoldierBehavior::Search && Now >= SearchDeadline)
		{
			ForgetTarget();
			SetBehavior(EDemoSoldierBehavior::Idle, Now);
		}
		return;
	}
	UDemoWeaponInstance* Weapon = GetWeapon();
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	if (!Definition || (Weapon->GetWeaponState().MagazineAmmo == 0 && Weapon->GetWeaponState().ReserveAmmo == 0))
	{
		SetBehavior(EDemoSoldierBehavior::OutOfAmmo, Now);
		Controller->SetFocus(Memory.Target.Get());
		return;
	}
	if (Weapon->GetWeaponState().bReloading)
	{
		SetBehavior(EDemoSoldierBehavior::Observe, Now);
		ActionDeadline = Now + 0.2;
		return;
	}
	const FVector View = Controller->GetPawn()->GetPawnViewLocation();
	if (FVector::DistSquared(View, Memory.TargetLocation) > FMath::Square(Definition->Range * 0.9f))
	{
		if (CanPursue() && IsInsideOrderArea(Memory.TargetLocation))
		{
			if (Behavior != EDemoSoldierBehavior::ApproachTarget || ActionTarget != Memory.Target)
			{
				SetBehavior(EDemoSoldierBehavior::ApproachTarget, Now);
				ActionTarget = Memory.Target;
				StartMove(Memory.TargetLocation, FMath::Max(100.0f, Definition->Range * 0.7f));
			}
			if (FinishMovement(Now))
			{
				SetBehavior(EDemoSoldierBehavior::Observe, Now);
				ActionDeadline = Now + 0.5;
			}
		}
		else
		{
			SetBehavior(EDemoSoldierBehavior::Observe, Now);
			SetObservationFocus(Memory.TargetLocation);
			ActionDeadline = Now + 0.5;
		}
		return;
	}
	if (ActionTarget != Memory.Target ||
	    (Behavior != EDemoSoldierBehavior::Aim && Behavior != EDemoSoldierBehavior::FireBurst))
	{
		SetBehavior(EDemoSoldierBehavior::Aim, Now);
		ActionTarget = Memory.Target;
		ActionDeadline = Now + Nelaric::Soldier::SampleSeconds(Settings.ReactionSeconds);
		MoveDeadline = ActionDeadline + 2.0;
		Controller->SetFocus(Memory.Target.Get());
	}
	if (Behavior == EDemoSoldierBehavior::Aim)
	{
		Controller->SetFocus(Memory.Target.Get());
		if (Now >= ActionDeadline && CanShoot())
		{
			SetBehavior(EDemoSoldierBehavior::FireBurst, Now);
			ActionTarget = Memory.Target;
			ActionWeapon = Weapon;
			WeaponCanceledHandle = Weapon->OnActionsCanceled().AddUObject(this, &ThisClass::HandleWeaponCanceled);
			const int32 Minimum = FMath::Clamp(Settings.BurstRounds.X, 1, 30);
			RoundsRemaining = FMath::RandRange(Minimum, FMath::Clamp(Settings.BurstRounds.Y, Minimum, 30));
			NextShotTime = Now;
			ActionDeadline = Now + RoundsRemaining * Definition->FireInterval + 2.0;
		}
		else if (Now >= MoveDeadline)
		{
			SetBehavior(EDemoSoldierBehavior::Observe, Now);
			ActionDeadline = Now + Nelaric::Soldier::SampleSeconds(Settings.ObserveSeconds);
		}
	}
	if (Behavior != EDemoSoldierBehavior::FireBurst)
	{
		return;
	}
	if (ActionWeapon != Weapon || Now >= ActionDeadline || !CanShoot())
	{
		SetBehavior(EDemoSoldierBehavior::Observe, Now);
		ActionDeadline = Now + Nelaric::Soldier::SampleSeconds(Settings.ObserveSeconds);
		return;
	}
	if (Now < NextShotTime)
	{
		Schedule(FMath::Min(float(NextShotTime - Now), Settings.DecisionInterval));
		return;
	}
	const uint64 Revision = ActionRevision;
	const EDemoWeaponResult Result = Weapon->TryFire();
	if (Revision != ActionRevision || !ExecutionDriver.IsValid())
	{
		return;
	}
	if (Result == EDemoWeaponResult::Success)
	{
		--RoundsRemaining;
	}
	else if (Result != EDemoWeaponResult::RateLimited)
	{
		RoundsRemaining = 0;
	}
	NextShotTime = Now + Definition->FireInterval;
	if (RoundsRemaining <= 0)
	{
		SetBehavior(EDemoSoldierBehavior::Observe, Now);
		ActionDeadline = Now + Nelaric::Soldier::SampleSeconds(Settings.ObserveSeconds);
		if (Result == EDemoWeaponResult::Success)
		{
			Emit(Nelaric::Soldier::BurstCompleted);
		}
	}
	else
	{
		Schedule(FMath::Min(Definition->FireInterval, Settings.DecisionInterval));
	}
}

void UDemoSoldierComponent::ExecuteOrder(double Now)
{
	if (OrderStatus != EDemoSoldierOrderStatus::Running || CurrentOrder.Type == EDemoSoldierOrderType::None)
	{
		SetBehavior(EDemoSoldierBehavior::Idle, Now);
		return;
	}
	FVector Goal = CurrentOrder.Location;
	if (CurrentOrder.Type == EDemoSoldierOrderType::Attack && CurrentOrder.Actor.IsStale())
	{
		CompleteOrder(EDemoSoldierOrderStatus::Failed);
		SetBehavior(EDemoSoldierBehavior::Idle, Now);
		return;
	}
	if (CurrentOrder.Type == EDemoSoldierOrderType::Follow)
	{
		if (!CurrentOrder.Actor.IsValid() || CurrentOrder.Actor->IsActorBeingDestroyed())
		{
			CompleteOrder(EDemoSoldierOrderStatus::Failed);
			SetBehavior(EDemoSoldierBehavior::Idle, Now);
			return;
		}
		Goal = CurrentOrder.Actor->GetActorLocation();
	}
	if (IsDangerous(Goal))
	{
		SetBehavior(EDemoSoldierBehavior::Hold, Now);
		return;
	}
	const bool bGuard =
	    CurrentOrder.Type == EDemoSoldierOrderType::Hold || CurrentOrder.Type == EDemoSoldierOrderType::Defend;
	const float Radius = CurrentOrder.AcceptanceRadius;
	if ((bGuard && IsInsideOrderArea(GetOwner()->GetActorLocation())) ||
	    FVector::DistSquared2D(GetOwner()->GetActorLocation(), Goal) <= FMath::Square(Radius))
	{
		SetBehavior(bGuard ? EDemoSoldierBehavior::Hold : EDemoSoldierBehavior::Idle, Now);
		if (CurrentOrder.Type == EDemoSoldierOrderType::Move ||
		    (CurrentOrder.Type == EDemoSoldierOrderType::Attack && !CurrentOrder.Actor.IsValid()))
		{
			CompleteOrder(EDemoSoldierOrderStatus::Completed);
		}
		return;
	}
	if (Behavior != EDemoSoldierBehavior::ExecuteOrder)
	{
		SetBehavior(EDemoSoldierBehavior::ExecuteOrder, Now);
		StartMove(Goal, Radius);
	}
	else if (CurrentOrder.Type == EDemoSoldierOrderType::Follow &&
	         FVector::DistSquared2D(Goal, MoveGoal) > FMath::Square(FMath::Max(150.0f, Radius)))
	{
		StartMove(Goal, Radius);
	}
	if (FinishMovement(Now))
	{
		const bool bSucceeded = bMoveSucceeded;
		SetBehavior(bGuard ? EDemoSoldierBehavior::Hold : EDemoSoldierBehavior::Idle, Now);
		if (!bSucceeded)
		{
			CompleteOrder(EDemoSoldierOrderStatus::Failed);
		}
		else if (CurrentOrder.Type == EDemoSoldierOrderType::Move ||
		         (CurrentOrder.Type == EDemoSoldierOrderType::Attack && !CurrentOrder.Actor.IsValid()))
		{
			CompleteOrder(EDemoSoldierOrderStatus::Completed);
		}
	}
}

void UDemoSoldierComponent::Update()
{
	if (bUpdating)
	{
		return;
	}
	TGuardValue<bool> Guard(bUpdating, true);
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	if (GetInitState() != Nelaric::EInitState::Ready || !PawnInitialization.IsValid() ||
	    !PawnInitialization->IsPawnInitialized() || !ExecutionDriver.IsValid() || !Controller.IsValid() ||
	    Controller->GetPawn() != Character || !Character || !Character->IsAlive() || !Character->IsPoolActive() ||
	    !Character->GetGasBinding()->IsReadyForActions() || GetWorld()->bIsTearingDown)
	{
		StopExecution(nullptr);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	UpdateObservations(Now);
	if (!bNativePlanner)
	{
		RefreshTreeObservers();
		UpdateTreeAction(Now);
		NotifyTreeActionResult();
		Emit(Nelaric::Soldier::DecisionChanged);
		ScheduleTreeWake(Now);
		return;
	}
	Schedule(FMath::Clamp(Settings.DecisionInterval, 0.05f, 1.0f));
	if (IsDangerous(Character->GetActorLocation()) ||
	    (Behavior == EDemoSoldierBehavior::AvoidGrenade && !Grenades.IsEmpty()))
	{
		if (Behavior != EDemoSoldierBehavior::AvoidGrenade)
		{
			SetBehavior(EDemoSoldierBehavior::AvoidGrenade, Now);
			EscapeAttempts = 0;
			bMoveFinished = true;
			bMoveSucceeded = false;
		}
		if (FinishMovement(Now) && (!bMoveSucceeded || IsDangerous(Character->GetActorLocation())) &&
		    EscapeAttempts < 3)
		{
			FVector Escape;
			++EscapeAttempts;
			if (FindEscape(Escape))
			{
				Character->GetCharacterMovement()->MaxWalkSpeed = FMath::Max(OriginalWalkSpeed, Settings.SprintSpeed);
				StartMove(Escape, 50.0f, true);
			}
			else
			{
				// No reachable escape: bounded fallback while the report expires.
				EscapeAttempts = 3;
				Character->Crouch();
			}
		}
		return;
	}
	if (Behavior == EDemoSoldierBehavior::AvoidGrenade)
	{
		SetBehavior(EDemoSoldierBehavior::Idle, Now);
	}
	if (Behavior == EDemoSoldierBehavior::TakeCover)
	{
		if (FinishMovement(Now))
		{
			const bool bReached = bMoveSucceeded && Cover.IsValid() && Cover->bEnabled;
			SetBehavior(EDemoSoldierBehavior::Observe, Now);
			Memory.bInCover = bReached;
			if (bReached)
			{
				Character->Crouch();
			}
			ActionDeadline = Now + 0.4;
		}
		return;
	}
	if (Behavior == EDemoSoldierBehavior::Reload)
	{
		if (GetWeapon() != ActionWeapon.Get() || Now >= ActionDeadline || !ReloadId.IsValid())
		{
			SetBehavior(EDemoSoldierBehavior::Observe, Now);
			ActionDeadline = Now + 0.4;
		}
		return;
	}
	if (!IsInsideOrderArea(Character->GetActorLocation()))
	{
		ExecuteOrder(Now);
		return;
	}
	if (TryCover(Now))
	{
		return;
	}
	if (WantsReload())
	{
		StartReload(Now);
		return;
	}
	if (Behavior == EDemoSoldierBehavior::Observe && Now < ActionDeadline)
	{
		return;
	}
	if (Memory.Target.IsValid())
	{
		ExecuteCombat(Now);
		return;
	}
	const double CueTime = FMath::Max(Memory.LastHeardTime, Memory.LastDamageTime);
	if (CueTime > ConsumedAlertTime && Now - CueTime < Settings.AlertSeconds)
	{
		if (Behavior != EDemoSoldierBehavior::Investigate || CueTime > InvestigatedAlertTime)
		{
			if (Behavior == EDemoSoldierBehavior::Investigate)
			{
				CancelAction();
			}
			SetBehavior(EDemoSoldierBehavior::Investigate, Now);
			InvestigatedAlertTime = CueTime;
			ActionDeadline = CueTime + FMath::Max(0.1f, Settings.AlertSeconds);
			const FVector Point = Memory.LastDamageTime > Memory.LastHeardTime
			                          ? Character->GetActorLocation() + Memory.DamageDirection * 600.0f
			                          : Memory.HeardLocation;
			if (CanPursue() && IsInsideOrderArea(Point))
			{
				StartMove(Point, 150.0f);
			}
			else
			{
				bMoveFinished = true;
				MoveGoal = Point;
				SetObservationFocus(Point);
			}
		}
		if (FinishMovement(Now))
		{
			SetObservationFocus(MoveGoal);
		}
		return;
	}
	ConsumedAlertTime = CueTime;
	ExecuteOrder(Now);
}
