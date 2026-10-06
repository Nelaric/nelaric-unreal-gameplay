// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSquadOrderReceiverComponent.h"
#include "AI/DemoSoldierComponent.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "AI/DemoSquadMemberComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Character/DemoCharacter.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "PawnGasBindingComponent.h"

namespace Nelaric::Squad
{
static bool ValidMemberOrderArea(const FDemoSquadArea& Area)
{
	return !Area.Center.ContainsNaN() && FMath::IsFinite(Area.Radius) && Area.Radius > 0.0f;
}

static bool Terminal(EDemoSquadOrderState State)
{
	return State == EDemoSquadOrderState::Succeeded || State == EDemoSquadOrderState::Failed ||
	       State == EDemoSquadOrderState::Cancelled;
}
} // namespace Nelaric::Squad

UDemoSquadOrderReceiverComponent::UDemoSquadOrderReceiverComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDemoSquadOrderReceiverComponent::ReceiveOrder(const FDemoSquadMemberOrder& Order)
{
	check(IsInGameThread());
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	UDemoSquadMemberComponent* Member = GetOwner()->FindComponentByClass<UDemoSquadMemberComponent>();
	ADemoSquadCommandActor* Squad = Member ? Member->GetSquad() : nullptr;
	UDemoSquadContextComponent* Context = Squad ? Squad->GetSquadContext() : nullptr;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!Character || !Character->HasAuthority() || !Character->IsPoolActive() || Character->HasCommittedDeath() ||
	    (Character->GetGasBinding()->HasCommittedState() && !Character->IsAlive()) || !Context ||
	    Order.SquadId != Context->GetSquadId() || Order.UnitId != Member->GetUnitId() ||
	    !Context->IsCurrentIdentity(Order.Identity) || !Order.OrderId.IsValid() || Order.Revision < 1 ||
	    !Nelaric::Squad::ValidMemberOrderArea(Order.Goal) ||
	    !Nelaric::Squad::ValidMemberOrderArea(Order.MovementArea) || Order.FocusLocation.ContainsNaN() ||
	    Order.Facing.ContainsNaN() || !FMath::IsFinite(Order.AcceptanceRadius) || Order.AcceptanceRadius <= 0.0f ||
	    !FMath::IsFinite(Order.IssuedAt) || !FMath::IsFinite(Order.Deadline) || !FMath::IsFinite(Order.ExpiresAt) ||
	    Order.IssuedAt > Now + 0.1 || (Order.ExpiresAt > 0.0 && Order.ExpiresAt <= Now) ||
	    static_cast<uint8>(Order.Type) > static_cast<uint8>(EDemoSquadOrderType::RegroupAt) ||
	    static_cast<uint8>(Order.Engagement) > static_cast<uint8>(EDemoSquadEngagement::FireAtWill) ||
	    FVector::DistSquared2D(Order.Goal.Center, Order.MovementArea.Center) > FMath::Square(Order.MovementArea.Radius))
	{
		return false;
	}
	if (bHasOrder && CurrentOrder.OrderId == Order.OrderId)
	{
		if (Order.Revision < CurrentOrder.Revision)
		{
			return false;
		}
		if (Order.Revision == CurrentOrder.Revision)
		{
			return true;
		}
	}
	UDemoSoldierComponent* Soldier = Character->GetSoldierComponent();
	const bool bSameConstraints = bHasOrder && bSubmitted && CurrentOrder.OrderId == Order.OrderId &&
	                              CurrentOrder.Type == Order.Type && CurrentOrder.Engagement == Order.Engagement &&
	                              CurrentOrder.bAllowStopToFight == Order.bAllowStopToFight &&
	                              CurrentOrder.bAllowLocalReposition == Order.bAllowLocalReposition &&
	                              CurrentOrder.MovementArea.Center.Equals(Order.MovementArea.Center, 1.0f) &&
	                              FMath::IsNearlyEqual(CurrentOrder.MovementArea.Radius, Order.MovementArea.Radius) &&
	                              CurrentOrder.Facing.Equals(Order.Facing, 0.001f) &&
	                              FMath::IsNearlyEqual(CurrentOrder.Goal.Radius, Order.Goal.Radius);
	const bool bMetadataUpdate = bSameConstraints && CurrentOrder.Goal.Center.Equals(Order.Goal.Center, 1.0f);
	const bool bGoalUpdate = bSameConstraints && CurrentOrder.Type == EDemoSquadOrderType::MaintainFormation &&
	                         Soldier && Soldier->UpdateOrderGoal(SoldierOrderId, Order.Goal.Center);
	if (!bGoalUpdate && !bMetadataUpdate && Soldier && SoldierOrderId.IsValid())
	{
		Soldier->CancelOrder(SoldierOrderId);
	}
	if (!bHasOrder || CurrentOrder.OrderId != Order.OrderId || !CurrentOrder.Facing.Equals(Order.Facing, 0.001f))
	{
		SearchBaseFacing = FVector::ZeroVector;
	}
	CurrentOrder = Order;
	bHasOrder = true;
	bSubmitted = bGoalUpdate || bMetadataUpdate;
	bDegradedHold = false;
	Feedback = {};
	Feedback.UnitId = Order.UnitId;
	Feedback.OrderId = Order.OrderId;
	Feedback.Revision = Order.Revision;
	SetFeedback(EDemoSquadOrderState::Accepted, EDemoSquadFailure::None, false);
	RefreshExecution();
	Member->ReportNow();
	return true;
}

bool UDemoSquadOrderReceiverComponent::CancelOrder(FGuid OrderId)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || !bHasOrder || CurrentOrder.OrderId != OrderId)
	{
		return false;
	}
	if (bDegradedHold && Nelaric::Squad::Terminal(Feedback.State))
	{
		return true;
	}
	if (UDemoSoldierComponent* Soldier = GetOwner()->FindComponentByClass<UDemoSoldierComponent>())
	{
		Soldier->CancelOrder(SoldierOrderId);
	}
	bSubmitted = false;
	SetFeedback(EDemoSquadOrderState::Cancelled, EDemoSquadFailure::Cancelled, false);
	EnterDegradedHold();
	return true;
}

FDemoSquadMemberOrder UDemoSquadOrderReceiverComponent::GetOrder() const
{
	check(IsInGameThread());
	return CurrentOrder;
}

FDemoSquadMemberFeedback UDemoSquadOrderReceiverComponent::GetFeedback() const
{
	check(IsInGameThread());
	return Feedback;
}

bool UDemoSquadOrderReceiverComponent::HasActiveOrder() const
{
	return bHasOrder && !Nelaric::Squad::Terminal(Feedback.State);
}

void UDemoSquadOrderReceiverComponent::SetFeedback(EDemoSquadOrderState State, EDemoSquadFailure Failure, bool bReady)
{
	Feedback.State = State;
	Feedback.Failure = Failure;
	Feedback.bReady = bReady;
	Feedback.ReportedAt = GetWorld()->GetTimeSeconds();
	const float Distance = FVector::Dist2D(GetOwner()->GetActorLocation(), CurrentOrder.Goal.Center);
	Feedback.Progress = FMath::Clamp(1.0f - Distance / FMath::Max(CurrentOrder.Goal.Radius, 1.0f), 0.0f, 1.0f);
}

bool UDemoSquadOrderReceiverComponent::SubmitSoldierIntent()
{
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	UDemoSoldierComponent* Soldier = Character ? Character->GetSoldierComponent() : nullptr;
	if (!Soldier || !Character->GetController() || Character->IsPlayerControlled() ||
	    Soldier->GetInitState() != Nelaric::EInitState::Ready || !Character->GetGasBinding()->IsReadyForActions())
	{
		return false;
	}
	FDemoSoldierOrder Order;
	Order.Id = CurrentOrder.OrderId;
	Order.Type = (CurrentOrder.Type == EDemoSquadOrderType::HoldSector ||
	              CurrentOrder.Type == EDemoSquadOrderType::SupportSector ||
	              CurrentOrder.Type == EDemoSquadOrderType::SearchArea)
	                 ? EDemoSoldierOrderType::Defend
	                 : EDemoSoldierOrderType::Move;
	Order.Location = CurrentOrder.Goal.Center;
	Order.AcceptanceRadius = CurrentOrder.AcceptanceRadius;
	// The preferred arrival slot does not impose a second movement boundary.
	Order.HoldRadius =
	    CurrentOrder.MovementArea.Radius + FVector::Dist2D(CurrentOrder.Goal.Center, CurrentOrder.MovementArea.Center);
	Order.bAllowPursuit = CurrentOrder.bAllowLocalReposition && CurrentOrder.Type != EDemoSquadOrderType::SupportSector;
	Order.bAllowStopToFight = CurrentOrder.bAllowStopToFight;
	Order.bAllowLocalReposition = CurrentOrder.bAllowLocalReposition;
	Order.bLimitMovement = true;
	Order.MovementAreaCenter = CurrentOrder.MovementArea.Center;
	Order.MovementAreaRadius = CurrentOrder.MovementArea.Radius;
	Order.FirePolicy = static_cast<EDemoSoldierFirePolicy>(CurrentOrder.Engagement);
	Order.FacingDirection = CurrentOrder.Facing;
	if (!Soldier->IssueOrder(Order))
	{
		return false;
	}
	SoldierOrderId = Order.Id;
	bSubmitted = true;
	return true;
}

bool UDemoSquadOrderReceiverComponent::HasSupportSolution() const
{
	const ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	const UDemoEquipmentManagerComponent* Equipment =
	    GetOwner()->FindComponentByClass<UDemoEquipmentManagerComponent>();
	const UDemoWeaponInstance* Weapon = Equipment ? Equipment->GetActiveWeapon() : nullptr;
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	if (!Character || !Definition || !CurrentOrder.bHasFocus ||
	    CurrentOrder.Engagement == EDemoSquadEngagement::HoldFire || Weapon->GetWeaponState().bReloading ||
	    Weapon->GetWeaponState().MagazineAmmo <= 0)
	{
		return false;
	}
	const FVector Start = Character->GetPawnViewLocation();
	const FVector End = CurrentOrder.FocusLocation;
	if (FVector::DistSquared(Start, End) > FMath::Square(Definition->Range))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SquadSupportReadiness), false, Character);
	// This sector check grants no target knowledge or permission to shoot.
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Definition->TraceChannel, Params))
	{
		return true;
	}
	const UDemoSoldierComponent* Soldier = Character->GetSoldierComponent();
	return Soldier && Soldier->GetMemory().bTargetVisible && Hit.GetActor() == Soldier->GetMemory().Target.Get();
}

void UDemoSquadOrderReceiverComponent::RefreshExecution()
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (!HasActiveOrder())
	{
		if (bHasOrder && Feedback.Failure == EDemoSquadFailure::OrderExpired && !bDegradedHold)
		{
			EnterDegradedHold();
		}
		return;
	}
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	const double Now = GetWorld()->GetTimeSeconds();
	if (CurrentOrder.ExpiresAt > 0.0 && Now >= CurrentOrder.ExpiresAt)
	{
		CancelOrder(CurrentOrder.OrderId);
		SetFeedback(EDemoSquadOrderState::Failed, EDemoSquadFailure::OrderExpired, false);
		EnterDegradedHold();
		return;
	}
	if (!Character || Character->HasCommittedDeath() || !Character->IsPoolActive() ||
	    (Character->GetGasBinding()->HasCommittedState() && !Character->IsAlive()))
	{
		CancelOrder(CurrentOrder.OrderId);
		SetFeedback(EDemoSquadOrderState::Failed, EDemoSquadFailure::Incapacitated, false);
		return;
	}
	if (Character->IsPlayerControlled())
	{
		bSubmitted = false;
		SetFeedback(EDemoSquadOrderState::Suspended, EDemoSquadFailure::PlayerControlled, false);
		return;
	}
	if (GetInitState() != Nelaric::EInitState::Ready || !Character->GetGasBinding()->HasCommittedState() ||
	    !Character->GetGasBinding()->IsReadyForActions())
	{
		bSubmitted = false;
		SetFeedback(EDemoSquadOrderState::Suspended, EDemoSquadFailure::Incapacitated, false);
		return;
	}
	if (!bSubmitted && !SubmitSoldierIntent())
	{
		SetFeedback(EDemoSquadOrderState::Accepted, EDemoSquadFailure::None, false);
		return;
	}
	UDemoSoldierComponent* Soldier = Character->GetSoldierComponent();
	if (!Soldier || Soldier->GetOrder().Id != SoldierOrderId)
	{
		SetFeedback(EDemoSquadOrderState::Failed, EDemoSquadFailure::Cancelled, false);
		return;
	}
	const AAIController* Bot = Character->GetController<AAIController>();
	if (!Soldier->IsExecutingFor(Bot ? Bot->GetBrainComponent() : nullptr))
	{
		SetFeedback(EDemoSquadOrderState::Suspended, EDemoSquadFailure::Incapacitated, false);
		return;
	}
	const bool bAtGoal = FVector::DistSquared2D(Character->GetActorLocation(), CurrentOrder.Goal.Center) <=
	                     FMath::Square(CurrentOrder.Goal.Radius);
	const bool bContinuous = CurrentOrder.Type == EDemoSquadOrderType::HoldSector ||
	                         CurrentOrder.Type == EDemoSquadOrderType::SupportSector ||
	                         CurrentOrder.Type == EDemoSquadOrderType::MaintainFormation ||
	                         CurrentOrder.Type == EDemoSquadOrderType::SearchArea;
	if (CurrentOrder.Deadline > 0.0 && Now >= CurrentOrder.Deadline && !bAtGoal)
	{
		CancelOrder(CurrentOrder.OrderId);
		SetFeedback(EDemoSquadOrderState::Failed, EDemoSquadFailure::PhaseTimeout, false);
		return;
	}
	if (Soldier->GetOrderStatus() == EDemoSoldierOrderStatus::Failed)
	{
		SetFeedback(EDemoSquadOrderState::Failed,
		            Soldier->GetOrderFailure() == EDemoSoldierOrderFailure::MoveTimeout
		                ? EDemoSquadFailure::Stuck
		                : EDemoSquadFailure::Unreachable,
		            false);
		return;
	}
	if (Soldier->GetBehavior() == EDemoSoldierBehavior::AvoidGrenade)
	{
		SetFeedback(EDemoSquadOrderState::Suspended, EDemoSquadFailure::None, false);
		return;
	}
	if (CurrentOrder.Type == EDemoSquadOrderType::SearchArea && bAtGoal)
	{
		const float Angle = FMath::Fmod(float(Now - CurrentOrder.IssuedAt) * 45.0f, 360.0f);
		if (SearchBaseFacing.IsNearlyZero())
		{
			SearchBaseFacing =
			    CurrentOrder.Facing.IsNearlyZero() ? Soldier->GetOrder().FacingDirection : CurrentOrder.Facing;
		}
		if (!SearchBaseFacing.IsNearlyZero())
		{
			Soldier->UpdateOrderFacing(SoldierOrderId, SearchBaseFacing.RotateAngleAxis(Angle, FVector::UpVector));
		}
	}
	bool bReady = bAtGoal;
	EDemoSquadFailure Reason = EDemoSquadFailure::None;
	if (CurrentOrder.Type == EDemoSquadOrderType::SupportSector)
	{
		const FDemoSquadMemberStatus Status =
		    GetOwner()->FindComponentByClass<UDemoSquadMemberComponent>()->CaptureStatus();
		if (Status.Suppression >= 0.65f)
		{
			bReady = false;
			Reason = EDemoSquadFailure::Suppressed;
		}
		else if (Status.MagazineAmmo <= 0)
		{
			bReady = false;
			Reason = Status.ReserveAmmo <= 0 ? EDemoSquadFailure::NoAmmo : EDemoSquadFailure::None;
		}
		else
		{
			bReady &= HasSupportSolution();
			if (!bReady && bAtGoal)
			{
				Reason = EDemoSquadFailure::NoValidFiringPosition;
			}
		}
	}
	SetFeedback(!bContinuous && Soldier->GetOrderStatus() == EDemoSoldierOrderStatus::Completed
	                ? EDemoSquadOrderState::Succeeded
	                : EDemoSquadOrderState::Executing,
	            Reason, bReady);
}

void UDemoSquadOrderReceiverComponent::LimitRetainedOrder(double ExpiresAt)
{
	if (HasActiveOrder() && (CurrentOrder.ExpiresAt <= 0.0 || CurrentOrder.ExpiresAt > ExpiresAt))
	{
		CurrentOrder.ExpiresAt = ExpiresAt;
	}
}

void UDemoSquadOrderReceiverComponent::EnterDegradedHold()
{
	if (bDegradedHold)
	{
		return;
	}
	ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	UDemoSoldierComponent* Soldier = Character ? Character->GetSoldierComponent() : nullptr;
	if (!Soldier || !Character->IsAlive() || !Character->IsPoolActive() || Character->IsPlayerControlled())
	{
		return;
	}
	FDemoSoldierOrder Hold;
	Hold.Id = FGuid::NewGuid();
	Hold.Type = EDemoSoldierOrderType::Hold;
	Hold.Location = Character->GetActorLocation();
	Hold.HoldRadius = 150.0f;
	Hold.bAllowPursuit = false;
	Hold.bAllowLocalReposition = false;
	Hold.FirePolicy = EDemoSoldierFirePolicy::SelfDefense;
	if (Soldier->IssueOrder(Hold))
	{
		SoldierOrderId = Hold.Id;
		bDegradedHold = true;
	}
}

void UDemoSquadOrderReceiverComponent::ResetReceiver()
{
	CancelOrder(CurrentOrder.OrderId);
	if (bDegradedHold)
	{
		if (auto* Soldier = GetOwner()->FindComponentByClass<UDemoSoldierComponent>())
		{
			Soldier->CancelOrder(SoldierOrderId);
		}
	}
	bHasOrder = false;
	bSubmitted = false;
	bDegradedHold = false;
	SoldierOrderId.Invalidate();
	CurrentOrder = {};
	SearchBaseFacing = FVector::ZeroVector;
	Feedback = {};
}

void UDemoSquadOrderReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetReceiver();
	Super::EndPlay(EndPlayReason);
}
