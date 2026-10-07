// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSquadMemberComponent.h"
#include "AI/DemoObjectiveWorldSubsystem.h"
#include "AI/DemoSoldierComponent.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "AI/DemoSquadOrderReceiverComponent.h"
#include "Character/DemoCharacter.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PawnGasBindingComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "GAS/DemoCombatAttributes.h"
#include "Player/ControlSwitchSubsystem.h"

UDemoSquadMemberComponent::UDemoSquadMemberComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDemoSquadMemberComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = Cast<APawn>(GetOwner()); Pawn && Pawn->HasAuthority())
	{
		if (!UnitId.IsValid())
			UnitId = FGuid::NewGuid();
		if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
			Rules->RegisterSoldier(UnitId, Cast<ADemoCharacter>(Pawn));
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleControllerChanged);
	}
}

bool UDemoSquadMemberComponent::JoinSquad(ADemoSquadCommandActor* Squad, FGuid InUnitId)
{
	check(IsInGameThread());
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || !IsValid(Squad) || Squad->GetWorld() != GetWorld() ||
	    !IsRegistered() || !GetOwner()->FindComponentByClass<UDemoSquadOrderReceiverComponent>())
	{
		return false;
	}
	if (CommandActor.Get() == Squad && (!InUnitId.IsValid() || InUnitId == UnitId))
	{
		return true;
	}
	if (CommandActor.IsValid())
	{
		return false;
	}
	if (Role == EDemoSquadRole::Leader || Role == EDemoSquadRole::Deputy)
	{
		Role = EDemoSquadRole::Rifleman;
		bRequired = false;
	}
	const FGuid PreviousIdentity = UnitId;
	const FGuid Identity = InUnitId.IsValid() ? InUnitId : (UnitId.IsValid() ? UnitId : FGuid::NewGuid());
	if (const auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		if (!Rules->CanBindSoldier(Identity, Character))
			return false;
	UnitId = Identity;
	CommandActor = Squad;
	if (!Squad->GetSquadContext()->RegisterMember(this))
	{
		CommandActor.Reset();
		return false;
	}
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
	{
		if (PreviousIdentity != UnitId)
			Rules->UnregisterSoldier(PreviousIdentity, Character);
		Rules->RegisterSoldier(UnitId, Character);
	}
	BindSoldier();
	ReportNow();
	// Stagger the first heartbeat; all later checks remain low-frequency.
	const float Delay = 0.5f + float(GetTypeHash(UnitId) % 500) / 1000.0f;
	GetWorld()->GetTimerManager().SetTimer(HeartbeatTimer, this, &ThisClass::ReportNow, 1.0f, true, Delay);
	return true;
}

void UDemoSquadMemberComponent::LeaveSquad()
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (UDemoSquadOrderReceiverComponent* Receiver =
	        GetOwner()->FindComponentByClass<UDemoSquadOrderReceiverComponent>())
	{
		Receiver->ResetReceiver();
	}
	if (ADemoSquadCommandActor* Squad = CommandActor.Get())
	{
		Squad->GetSquadContext()->UnregisterMember(UnitId, BindingGeneration);
	}
	if (auto* Character = Cast<ADemoCharacter>(GetOwner());
	    Character &&
	    (!Character->IsPoolActive() || Character->HasCommittedDeath() || Character->IsActorBeingDestroyed()))
		if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
			Rules->UnregisterSoldier(UnitId, Character);
	CommandActor.Reset();
	ClearSoldier();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimer);
	}
	++BindingGeneration;
}

ADemoSquadCommandActor* UDemoSquadMemberComponent::GetSquad() const
{
	return CommandActor.Get();
}

FGuid UDemoSquadMemberComponent::GetUnitId() const
{
	return UnitId;
}

bool UDemoSquadMemberComponent::RestoreUnitIdentity(FGuid Identity)
{
	check(IsInGameThread());
	auto* Squad = CommandActor.Get();
	auto* Character = Cast<ADemoCharacter>(GetOwner());
	if (!Squad || !Character || !Character->HasAuthority() || !Identity.IsValid())
		return false;
	if (Identity == UnitId)
		return true;
	if (const auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		if (!Rules->CanBindSoldier(Identity, Character))
			return false;
	for (const auto& Member : Squad->GetSquadContext()->GetMembers())
		if (Member.UnitId == Identity)
			return false;
	const FGuid Previous = UnitId;
	Squad->GetSquadContext()->UnregisterMember(UnitId, BindingGeneration);
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->UnregisterSoldier(UnitId, Character);
	if (auto* Receiver = Character->FindComponentByClass<UDemoSquadOrderReceiverComponent>())
		Receiver->ResetReceiver();
	UnitId = Identity;
	++BindingGeneration;
	if (!Squad->GetSquadContext()->RegisterMember(this))
	{
		UnitId = Previous;
		Squad->GetSquadContext()->RegisterMember(this);
		return false;
	}
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->RegisterSoldier(UnitId, Character);
	ReportNow();
	return true;
}

int32 UDemoSquadMemberComponent::GetBindingGeneration() const
{
	return BindingGeneration;
}

FDemoSquadMemberStatus UDemoSquadMemberComponent::CaptureStatus() const
{
	check(IsInGameThread());
	FDemoSquadMemberStatus Status;
	Status.UnitId = UnitId;
	Status.BindingGeneration = BindingGeneration;
	Status.Role = Role;
	Status.SuccessionPriority = SuccessionPriority;
	Status.bCanCommand = bCanCommand;
	Status.bRequired = bRequired;
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	Status.Character = Character;
	if (!Character)
	{
		return Status;
	}
	const bool bCommitted = Character->GetGasBinding()->HasCommittedState();
	Status.bAlive = Character->IsPoolActive() && !Character->HasCommittedDeath() &&
	                (bCommitted ? Character->IsAlive() : LastStatus.bAlive);
	Status.bPlayerControlled = Character->IsPlayerControlled();
	Status.TeamId = Character->GetTeamId();
	Status.bCanCommand &= Character->GetCharacterMovement()->MovementMode != MOVE_None;
	if (const auto* Coordinator = GetWorld()->GetSubsystem<UControlSwitchSubsystem>())
	{
		Status.bCanCommand &= !Coordinator->IsControlTransitionRecoveryRequired(Character);
	}
	Status.bMobile = Status.bAlive && GetInitState() == Nelaric::EInitState::Ready &&
	                 Character->GetGasBinding()->IsReadyForActions() && Character->GetController() &&
	                 Character->GetCharacterMovement()->MovementMode != MOVE_None;
	Status.Location = Character->GetActorLocation();
	Status.ReportedAt = GetWorld()->GetTimeSeconds();
	if (const UDemoSoldierComponent* Soldier = Character->GetSoldierComponent())
	{
		Status.Suppression = Soldier->GetMemory().Suppression;
	}
	const UDemoEquipmentManagerComponent* Equipment = Character->FindComponentByClass<UDemoEquipmentManagerComponent>();
	const UDemoWeaponInstance* Weapon = Equipment ? Equipment->GetActiveWeapon() : nullptr;
	if (Weapon)
	{
		const FDemoWeaponState State = Weapon->GetWeaponState();
		Status.MagazineAmmo = State.MagazineAmmo;
		Status.ReserveAmmo = State.ReserveAmmo;
		Status.bReloading = State.bReloading;
		const UDemoWeaponDefinition* Definition = Weapon->GetWeaponDefinition();
		Status.WeaponRange = Definition ? Definition->Range : 0.0f;
	}
	if (const UDemoSquadOrderReceiverComponent* Receiver =
	        Character->FindComponentByClass<UDemoSquadOrderReceiverComponent>())
	{
		Status.Feedback = Receiver->GetFeedback();
	}
	return Status;
}

void UDemoSquadMemberComponent::BindSoldier()
{
	UDemoEquipmentManagerComponent* Equipment = GetOwner()->FindComponentByClass<UDemoEquipmentManagerComponent>();
	if (Equipment != ObservedEquipment.Get())
	{
		if (auto* Previous = ObservedEquipment.Get())
		{
			Previous->OnStateChanged().Remove(EquipmentHandle);
		}
		ObservedEquipment = Equipment;
		EquipmentHandle = Equipment ? Equipment->OnStateChanged().AddUObject(this, &ThisClass::HandleEquipmentChanged)
		                            : FDelegateHandle();
	}
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem != ObservedAbilitySystem.Get())
	{
		if (auto* Previous = ObservedAbilitySystem.Get())
		{
			Previous->GetGameplayAttributeValueChangeDelegate(UDemoCombatAttributes::GetHealthAttribute())
			    .Remove(HealthHandle);
		}
		ObservedAbilitySystem = AbilitySystem;
		HealthHandle =
		    AbilitySystem
		        ? AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDemoCombatAttributes::GetHealthAttribute())
		              .AddUObject(this, &ThisClass::HandleHealthChanged)
		        : FDelegateHandle();
	}
	USceneComponent* Root = GetOwner()->GetRootComponent();
	if (Root != ObservedRoot.Get())
	{
		if (auto* Previous = ObservedRoot.Get())
		{
			Previous->TransformUpdated.Remove(TransformHandle);
		}
		ObservedRoot = Root;
		TransformHandle =
		    Root ? Root->TransformUpdated.AddWeakLambda(this, [this](USceneComponent* Component, EUpdateTransformFlags,
		                                                             ETeleportType) { HandleBodyTransform(Component); })
		         : FDelegateHandle();
	}
	UDemoSoldierComponent* Soldier = GetOwner()->FindComponentByClass<UDemoSoldierComponent>();
	if (ObservedSoldier.Get() == Soldier)
	{
		return;
	}
	if (auto* Previous = ObservedSoldier.Get())
	{
		Previous->OnEvent().Remove(SoldierHandle);
	}
	SoldierHandle.Reset();
	ObservedSoldier = Soldier;
	if (Soldier)
	{
		SoldierHandle = Soldier->OnEvent().AddUObject(this, &ThisClass::HandleSoldierEvent);
	}
}

void UDemoSquadMemberComponent::ClearSoldier()
{
	if (auto* Equipment = ObservedEquipment.Get())
	{
		Equipment->OnStateChanged().Remove(EquipmentHandle);
	}
	if (auto* AbilitySystem = ObservedAbilitySystem.Get())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDemoCombatAttributes::GetHealthAttribute())
		    .Remove(HealthHandle);
	}
	if (auto* Root = ObservedRoot.Get())
	{
		Root->TransformUpdated.Remove(TransformHandle);
	}
	ObservedEquipment.Reset();
	ObservedAbilitySystem.Reset();
	ObservedRoot.Reset();
	EquipmentHandle.Reset();
	HealthHandle.Reset();
	TransformHandle.Reset();
	if (UDemoSoldierComponent* Soldier = ObservedSoldier.Get())
	{
		Soldier->OnEvent().Remove(SoldierHandle);
	}
	ObservedSoldier.Reset();
	SoldierHandle.Reset();
}

void UDemoSquadMemberComponent::ReportNow()
{
	if (auto* Character = Cast<ADemoCharacter>(GetOwner()); Character && Character->HasAuthority() &&
	                                                        Character->HasCommittedDeath() && GetWorld() &&
	                                                        !GetWorld()->bIsTearingDown)
		if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
			Rules->RecordSoldierDeath(UnitId);
	if (bReporting || !GetOwner()->HasAuthority() || !CommandActor.IsValid() || !GetWorld() ||
	    GetWorld()->bIsTearingDown)
	{
		return;
	}
	TGuardValue<bool> Guard(bReporting, true);
	BindSoldier();
	if (UDemoSquadOrderReceiverComponent* Receiver =
	        GetOwner()->FindComponentByClass<UDemoSquadOrderReceiverComponent>())
	{
		Receiver->RefreshExecution();
	}
	UDemoSquadContextComponent* Context = CommandActor->GetSquadContext();
	LastStatus = CaptureStatus();
	LastReportedAt = LastStatus.ReportedAt;
	Context->ReportMember(LastStatus);
	if (UDemoSoldierComponent* Soldier = ObservedSoldier.Get())
	{
		for (const FDemoSoldierContact& Observed : Soldier->GetObservedContacts())
		{
			if (Observed.ObservedAt < 0.0)
			{
				continue;
			}
			FDemoSquadContact Report;
			Report.ObserverUnitId = UnitId;
			Report.Actor = Observed.Actor;
			Report.Location = Observed.Location;
			Report.ObservedAt = Observed.ObservedAt;
			Context->ReportContact(Report);
		}
	}
}

void UDemoSquadMemberComponent::HandleSoldierEvent(FGameplayTag Tag)
{
	if (Tag == FGameplayTag::RequestGameplayTag(TEXT("AI.Event.DecisionChanged")) &&
	    GetWorld()->GetTimeSeconds() - LastReportedAt < 0.25)
	{
		return;
	}
	ReportNow();
}

void UDemoSquadMemberComponent::HandleEquipmentChanged()
{
	const auto Status = CaptureStatus();
	if (Status.bReloading != LastStatus.bReloading || (Status.MagazineAmmo == 0) != (LastStatus.MagazineAmmo == 0) ||
	    (Status.MagazineAmmo + Status.ReserveAmmo == 0) != (LastStatus.MagazineAmmo + LastStatus.ReserveAmmo == 0) ||
	    Status.ReportedAt - LastReportedAt >= 0.25)
	{
		ReportNow();
	}
}

void UDemoSquadMemberComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	ReportNow();
}

void UDemoSquadMemberComponent::HandleBodyTransform(USceneComponent* Component)
{
	if (!CommandActor.IsValid() || !IsValid(Component))
	{
		return;
	}
	const FVector Location = Component->GetComponentLocation();
	bool bMeaningful = FVector::DistSquared2D(Location, LastStatus.Location) > FMath::Square(400.0f);
	if (const auto* Receiver = GetOwner()->FindComponentByClass<UDemoSquadOrderReceiverComponent>())
	{
		const auto Order = Receiver->GetOrder();
		bMeaningful |=
		    (FVector::DistSquared2D(Location, Order.Goal.Center) <= FMath::Square(Order.Goal.Radius)) !=
		    (FVector::DistSquared2D(LastStatus.Location, Order.Goal.Center) <= FMath::Square(Order.Goal.Radius));
	}
	if (bMeaningful)
	{
		ReportNow();
	}
}

void UDemoSquadMemberComponent::HandleControllerChanged(APawn* Pawn, AController* OldController,
                                                        AController* NewController)
{
	ReportNow();
}

void UDemoSquadMemberComponent::CancelInitGenerationWork()
{
	Super::CancelInitGenerationWork();
	ReportNow();
}

void UDemoSquadMemberComponent::OnInitReady()
{
	Super::OnInitReady();
	ReportNow();
}

void UDemoSquadMemberComponent::NotifyNewLife()
{
	ADemoSquadCommandActor* Previous = CommandActor.Get();
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->UnregisterSoldier(UnitId, Cast<ADemoCharacter>(GetOwner()));
	if (Previous)
		LeaveSquad();
	UnitId = FGuid::NewGuid();
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->RegisterSoldier(UnitId, Cast<ADemoCharacter>(GetOwner()));
	if (Previous)
		JoinSquad(Previous, UnitId);
}

void UDemoSquadMemberComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
	}
	LeaveSquad();
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->UnregisterSoldier(UnitId, Cast<ADemoCharacter>(GetOwner()));
	Super::EndPlay(EndPlayReason);
}
