// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierComponent.h"

#include "AI/DemoSoldierCoverPoint.h"
#include "AI/DemoSoldierTags.h"
#include "AIController.h"
#include "Character/DemoCharacter.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace Nelaric::Soldier
{
static float SampleTreeDelay(FVector2D Range)
{
	const double Minimum = FMath::Clamp(Range.X, 0.01, 10.0);
	return FMath::FRandRange(float(Minimum), float(FMath::Clamp(Range.Y, Minimum, 10.0)));
}
} // namespace Nelaric::Soldier

bool UDemoSoldierComponent::TestTreeCondition(EDemoSoldierTreeTest Test) const
{
	check(IsInGameThread());
	const ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwner());
	if (!Character || !GetWorld())
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bHasTarget = Memory.Target.IsValid();
	const bool bHasOrder =
	    OrderStatus == EDemoSoldierOrderStatus::Running && CurrentOrder.Type != EDemoSoldierOrderType::None;
	const bool bReturn = !IsInsideOrderArea(Character->GetActorLocation());
	const bool bCover =
	    !MustKeepMoving() && (!bHasOrder || CurrentOrder.bAllowLocalReposition) && !Memory.bInCover &&
	    Now >= NextCoverTime &&
	    (Memory.bUnderFire || bSuppressed || Character->GetHealth() < Character->GetMaxHealth() * 0.3f) &&
	    (bHasTarget || !Memory.DamageDirection.IsNearlyZero());
	const double CueTime = FMath::Max(Memory.LastHeardTime, Memory.LastDamageTime);
	const bool bAlert = !MustKeepMoving() && !bReturn && !bHasTarget && CueTime > ConsumedAlertTime &&
	                    Now - CueTime < Settings.AlertSeconds;
	const UDemoWeaponInstance* Weapon = GetWeapon();
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	const bool bEmpty =
	    !Definition || (Weapon->GetWeaponState().MagazineAmmo == 0 && Weapon->GetWeaponState().ReserveAmmo == 0);
	const bool bApproach = bHasTarget && Memory.bTargetVisible && Definition && CanPursue() &&
	                       IsInsideOrderArea(Memory.TargetLocation) &&
	                       FVector::DistSquared(Character->GetPawnViewLocation(), Memory.TargetLocation) >
	                           FMath::Square(Definition->Range * 0.9f);
	const bool bReload = WantsReload();
	const bool bCombat = !MustKeepMoving() && !bReturn && (bHasTarget || bReload || bCover);
	const bool bGrenade = IsDangerous(Character->GetActorLocation()) ||
	                      (TreeAction == EDemoSoldierTreeAction::AvoidGrenade && !Grenades.IsEmpty());
	switch (Test)
	{
	case EDemoSoldierTreeTest::Alive:
		return Character->IsAlive();
	case EDemoSoldierTreeTest::Grenade:
		return bGrenade;
	case EDemoSoldierTreeTest::Combat:
		return bCombat;
	case EDemoSoldierTreeTest::NeedsReload:
		return bReload;
	case EDemoSoldierTreeTest::NeedsCover:
		return bCover;
	case EDemoSoldierTreeTest::OutOfAmmo:
		return bHasTarget && Memory.bTargetVisible && bEmpty;
	case EDemoSoldierTreeTest::TargetVisible:
		return bHasTarget && Memory.bTargetVisible;
	case EDemoSoldierTreeTest::CanApproach:
		return bApproach;
	case EDemoSoldierTreeTest::CanSearchMove:
		return bHasTarget && !Memory.bTargetVisible && CanPursue() && IsInsideOrderArea(Memory.TargetLocation);
	case EDemoSoldierTreeTest::FiringBlocked:
		return bHasTarget && Memory.bTargetVisible && !Memory.bFiringLineClear;
	case EDemoSoldierTreeTest::CanReposition:
		return bHasTarget && !bEmpty && !Memory.bFiringLineClear && !MustKeepMoving() &&
		       (!bHasOrder || CurrentOrder.bAllowLocalReposition) && Now >= NextRepositionTime;
	case EDemoSoldierTreeTest::FacingUnspecified:
		return bHasOrder && CurrentOrder.FacingDirection.GetSafeNormal2D().IsNearlyZero();
	case EDemoSoldierTreeTest::Alert:
		return bAlert;
	case EDemoSoldierTreeTest::DamageCue:
		return Memory.LastDamageTime > Memory.LastHeardTime;
	case EDemoSoldierTreeTest::HasOrder:
		return bHasOrder;
	case EDemoSoldierTreeTest::MoveOrder:
		return bHasOrder && CurrentOrder.Type == EDemoSoldierOrderType::Move;
	case EDemoSoldierTreeTest::HoldOrder:
		return bHasOrder && CurrentOrder.Type == EDemoSoldierOrderType::Hold;
	case EDemoSoldierTreeTest::AttackOrder:
		return bHasOrder && CurrentOrder.Type == EDemoSoldierOrderType::Attack;
	case EDemoSoldierTreeTest::FollowOrder:
		return bHasOrder && CurrentOrder.Type == EDemoSoldierOrderType::Follow;
	case EDemoSoldierTreeTest::DefendOrder:
		return bHasOrder && CurrentOrder.Type == EDemoSoldierOrderType::Defend;
	case EDemoSoldierTreeTest::ReturnToArea:
		return bReturn;
	case EDemoSoldierTreeTest::ShouldReconsider:
		break;
	default:
		return false;
	}

	if (!Character->IsAlive())
	{
		return TreeAction != EDemoSoldierTreeAction::Dead;
	}
	if (!TreeActionId.IsValid() || TreeOrderRevision != OrderRevision)
	{
		return true;
	}
	if (bGrenade && TreeAction != EDemoSoldierTreeAction::AvoidGrenade)
	{
		return true;
	}
	// Completed actions take their authored completion transitions first.
	if (TreeResult != EDemoSoldierTreeResult::Running)
	{
		return false;
	}
	if (bGrenade)
	{
		return false;
	}
	if (TreeAction == EDemoSoldierTreeAction::AvoidGrenade)
	{
		return false;
	}
	if (bReturn && TreeAction != EDemoSoldierTreeAction::ExecuteOrder)
	{
		return true;
	}
	// Ordinary target changes do not repeatedly cancel cover or owned reloads.
	if (TreeAction == EDemoSoldierTreeAction::Reload || TreeAction == EDemoSoldierTreeAction::TakeCover)
	{
		return false;
	}
	if (!bReturn && (bReload || bCover))
	{
		return true;
	}
	if (bCombat)
	{
		switch (TreeAction)
		{
		case EDemoSoldierTreeAction::Aim:
		case EDemoSoldierTreeAction::FireBurst:
			return ActionTarget != Memory.Target || !Memory.bTargetVisible || !Memory.bFiringLineClear || bApproach ||
			       bEmpty;
		case EDemoSoldierTreeAction::Reposition:
			return ActionTarget != Memory.Target || bEmpty;
		case EDemoSoldierTreeAction::ApproachTarget:
			return ActionTarget != Memory.Target || !bApproach || bEmpty;
		case EDemoSoldierTreeAction::MoveToMemory:
		case EDemoSoldierTreeAction::Search:
			return ActionTarget != Memory.Target || Memory.bTargetVisible;
		case EDemoSoldierTreeAction::Observe:
			return ActionTarget != Memory.Target;
		case EDemoSoldierTreeAction::OutOfAmmo:
			return !bEmpty || !Memory.bTargetVisible || ActionTarget != Memory.Target;
		default:
			return true;
		}
	}
	if (TreeAction == EDemoSoldierTreeAction::ExecuteOrder)
	{
		return !bHasOrder || bAlert;
	}
	if (TreeAction == EDemoSoldierTreeAction::InvestigateDamage ||
	    TreeAction == EDemoSoldierTreeAction::InvestigateSound)
	{
		// A continuing gunshot cue must not restart its movement every shot.
		return !bAlert;
	}
	return TreeAction != EDemoSoldierTreeAction::Idle || bHasOrder || bAlert;
}

void UDemoSoldierComponent::EnsureInitialTreeOrder(EDemoSoldierOrderType Type, float Radius)
{
	check(IsInGameThread());
	if (bInitialTreeOrderApplied || bNativePlanner || !ExecutionDriver.IsValid())
	{
		return;
	}
	bInitialTreeOrderApplied = true;
	if (OrderStatus != EDemoSoldierOrderStatus::None ||
	    (Type != EDemoSoldierOrderType::Hold && Type != EDemoSoldierOrderType::Defend))
	{
		return;
	}
	FDemoSoldierOrder Order;
	Order.Type = Type;
	Order.Location = GetOwner()->GetActorLocation();
	Order.HoldRadius = FMath::IsFinite(Radius) ? FMath::Max(1.0f, Radius) : 500.0f;
	IssueOrder(Order);
}

bool UDemoSoldierComponent::BeginTreeAction(EDemoSoldierTreeAction Action, UObject* Driver, FGuid& Id)
{
	check(IsInGameThread());
	Id.Invalidate();
	if (bNativePlanner || !IsExecutingFor(Driver) || !Controller.IsValid())
	{
		return false;
	}
	CancelAction();
	TreeActionId = FGuid::NewGuid();
	Id = TreeActionId;
	TreeOrderRevision = OrderRevision;
	TreeAction = Action;
	TreeResult = EDemoSoldierTreeResult::Running;
	bTreeResultNotified = false;
	bAimWakeRequested = false;
	bTreeReturningToArea =
	    Action == EDemoSoldierTreeAction::ExecuteOrder && TestTreeCondition(EDemoSoldierTreeTest::ReturnToArea);
	Behavior = EDemoSoldierBehavior::Idle;
	const double Now = GetWorld()->GetTimeSeconds();
	ActionDeadline = Now;
	ADemoCharacter* Character = CastChecked<ADemoCharacter>(GetOwner());
	UDemoWeaponInstance* Weapon = GetWeapon();
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	switch (Action)
	{
	case EDemoSoldierTreeAction::Idle:
		if (!Memory.bTargetVisible && !CurrentOrder.FacingDirection.GetSafeNormal2D().IsNearlyZero())
		{
			SetObservationFocus(Character->GetActorLocation() +
			                    CurrentOrder.FacingDirection.GetSafeNormal2D() * 1000.0f);
		}
		break;
	case EDemoSoldierTreeAction::Dead:
		Behavior = EDemoSoldierBehavior::Dead;
		break;
	case EDemoSoldierTreeAction::AvoidGrenade:
		SetBehavior(EDemoSoldierBehavior::AvoidGrenade, Now);
		EscapeAttempts = 0;
		bMoveFinished = true;
		bMoveSucceeded = false;
		break;
	case EDemoSoldierTreeAction::TakeCover:
		if (!TryCover(Now))
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Reload:
		if (!StartReload(Now))
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Aim:
		SetBehavior(EDemoSoldierBehavior::Aim, Now);
		ActionTarget = Memory.Target;
		Controller->SetFocus(ActionTarget.Get());
		ActionDeadline = Now + Nelaric::Soldier::SampleTreeDelay(Settings.ReactionSeconds);
		MoveDeadline = ActionDeadline + 2.0;
		break;
	case EDemoSoldierTreeAction::FireBurst:
		SetBehavior(EDemoSoldierBehavior::FireBurst, Now);
		ActionTarget = Memory.Target;
		if (!Definition || !Memory.bTargetVisible || !ActionTarget.IsValid())
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
			break;
		}
		ActionWeapon = Weapon;
		Controller->SetFocus(ActionTarget.Get());
		WeaponCanceledHandle = Weapon->OnActionsCanceled().AddUObject(this, &ThisClass::HandleWeaponCanceled);
		{
			const int32 Minimum = FMath::Clamp(Settings.BurstRounds.X, 1, 30);
			RoundsRemaining = FMath::RandRange(Minimum, FMath::Clamp(Settings.BurstRounds.Y, Minimum, 30));
		}
		NextShotTime = Now;
		ActionDeadline = Now + RoundsRemaining * Definition->FireInterval + 2.0;
		break;
	case EDemoSoldierTreeAction::Observe:
		SetBehavior(EDemoSoldierBehavior::Observe, Now);
		ActionTarget = Memory.Target;
		ActionDeadline = Now + Nelaric::Soldier::SampleTreeDelay(Settings.ObserveSeconds);
		break;
	case EDemoSoldierTreeAction::ApproachTarget:
		SetBehavior(EDemoSoldierBehavior::ApproachTarget, Now);
		ActionTarget = Memory.Target;
		if (!TestTreeCondition(EDemoSoldierTreeTest::CanApproach) ||
		    !StartMove(Memory.TargetLocation, FMath::Max(100.0f, Definition->Range * 0.7f)))
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Reposition:
		SetBehavior(EDemoSoldierBehavior::Reposition, Now);
		ActionTarget = Memory.Target;
		{
			FVector Position;
			const bool bPermitted = TestTreeCondition(EDemoSoldierTreeTest::CanReposition);
			NextRepositionTime = Now + FMath::Max(0.1f, Settings.RepositionRetrySeconds);
			if (!bPermitted || !FindObservationPosition(Memory.TargetLocation, Position) || !StartMove(Position, 40.0f))
			{
				TreeResult = EDemoSoldierTreeResult::Failed;
			}
		}
		break;
	case EDemoSoldierTreeAction::MoveToMemory:
		SetBehavior(EDemoSoldierBehavior::MoveToMemory, Now);
		ActionTarget = Memory.Target;
		if (!TestTreeCondition(EDemoSoldierTreeTest::CanSearchMove) || !StartMove(Memory.TargetLocation, 120.0f))
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Search:
		SetBehavior(EDemoSoldierBehavior::Search, Now);
		ActionTarget = Memory.Target;
		SetObservationFocus(Memory.TargetLocation);
		SearchDeadline = Now + FMath::Max(0.1f, Settings.SearchSeconds);
		break;
	case EDemoSoldierTreeAction::ForgetTarget:
		ForgetTarget();
		TreeResult = EDemoSoldierTreeResult::Succeeded;
		break;
	case EDemoSoldierTreeAction::InvestigateDamage:
	case EDemoSoldierTreeAction::InvestigateSound:
		SetBehavior(EDemoSoldierBehavior::Investigate, Now);
		InvestigatedAlertTime = FMath::Max(Memory.LastDamageTime, Memory.LastHeardTime);
		ActionDeadline = InvestigatedAlertTime + FMath::Max(0.1f, Settings.AlertSeconds);
		{
			const FVector Point = Action == EDemoSoldierTreeAction::InvestigateDamage
			                          ? Character->GetActorLocation() + Memory.DamageDirection * 600.0f
			                          : Memory.HeardLocation;
			if (CanPursue() && IsInsideOrderArea(Point))
			{
				StartMove(Point, 150.0f);
			}
			else
			{
				FVector Observation;
				const bool bCanMove =
				    !MustKeepMoving() && CurrentOrder.bAllowLocalReposition && Now >= NextRepositionTime;
				NextRepositionTime = Now + FMath::Max(0.1f, Settings.RepositionRetrySeconds);
				if (!bCanMove || !FindObservationPosition(Point, Observation) || !StartMove(Observation, 40.0f))
				{
					bMoveFinished = true;
					MoveGoal = Point;
				}
				SetObservationFocus(Point);
			}
		}
		break;
	case EDemoSoldierTreeAction::ExecuteOrder:
		ExecuteOrder(Now);
		break;
	case EDemoSoldierTreeAction::OutOfAmmo:
		SetBehavior(EDemoSoldierBehavior::OutOfAmmo, Now);
		ActionTarget = Memory.Target;
		Controller->SetFocus(ActionTarget.Get());
		break;
	default:
		TreeResult = EDemoSoldierTreeResult::Failed;
		break;
	}
	Wake();
	return true;
}

EDemoSoldierTreeResult UDemoSoldierComponent::GetTreeActionResult(FGuid Id) const
{
	check(IsInGameThread());
	return Id.IsValid() && Id == TreeActionId && ExecutionDriver.IsValid() ? TreeResult
	                                                                       : EDemoSoldierTreeResult::Failed;
}

void UDemoSoldierComponent::EndTreeAction(FGuid Id)
{
	check(IsInGameThread());
	if (Id.IsValid() && Id == TreeActionId)
	{
		TreeActionId.Invalidate();
		TreeResult = EDemoSoldierTreeResult::Failed;
		CancelAction();
		if (Behavior != EDemoSoldierBehavior::Dead)
		{
			Behavior = EDemoSoldierBehavior::Idle;
		}
	}
}

void UDemoSoldierComponent::UpdateTreeAction(double Now)
{
	if (!TreeActionId.IsValid() || TreeResult != EDemoSoldierTreeResult::Running || TreeOrderRevision != OrderRevision)
	{
		return;
	}
	ADemoCharacter* Character = CastChecked<ADemoCharacter>(GetOwner());
	UDemoWeaponInstance* Weapon = GetWeapon();
	const UDemoWeaponDefinition* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	switch (TreeAction)
	{
	case EDemoSoldierTreeAction::AvoidGrenade:
		if (Grenades.IsEmpty())
		{
			TreeResult = EDemoSoldierTreeResult::Succeeded;
			break;
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
				EscapeAttempts = 3;
				Character->Crouch();
			}
		}
		break;
	case EDemoSoldierTreeAction::TakeCover:
		if (FinishMovement(Now))
		{
			Memory.bInCover = bMoveSucceeded && Cover.IsValid() && Cover->bEnabled;
			TreeResult = Memory.bInCover ? EDemoSoldierTreeResult::Succeeded : EDemoSoldierTreeResult::Failed;
			if (Memory.bInCover)
			{
				Character->Crouch();
			}
		}
		break;
	case EDemoSoldierTreeAction::Reload:
		if (Weapon != ActionWeapon.Get() || Now >= ActionDeadline || !ReloadId.IsValid())
		{
			NextReloadTime = Now + 0.5;
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Aim:
		if (ActionTarget != Memory.Target || !Memory.bTargetVisible || !Memory.bFiringLineClear)
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		else if (Now >= ActionDeadline && (bAimWakeRequested = IsAimAligned()) && CanShoot())
		{
			TreeResult = EDemoSoldierTreeResult::Succeeded;
		}
		else if (Now >= MoveDeadline)
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::FireBurst:
		if (Now < NextShotTime)
		{
			break;
		}
		if (!Definition || ActionWeapon != Weapon || ActionTarget != Memory.Target || Now >= ActionDeadline ||
		    !CanShoot())
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
			break;
		}
		{
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
				TreeResult = EDemoSoldierTreeResult::Failed;
			}
			NextShotTime = Now + Definition->FireInterval;
			if (RoundsRemaining <= 0 && TreeResult == EDemoSoldierTreeResult::Running)
			{
				TreeResult = EDemoSoldierTreeResult::Succeeded;
				Emit(Nelaric::Soldier::BurstCompleted);
			}
		}
		break;
	case EDemoSoldierTreeAction::Observe:
		if (Now >= ActionDeadline)
		{
			TreeResult = EDemoSoldierTreeResult::Succeeded;
		}
		break;
	case EDemoSoldierTreeAction::ApproachTarget:
		if (ActionTarget != Memory.Target || !Memory.bTargetVisible || !Definition)
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		else if (!TestTreeCondition(EDemoSoldierTreeTest::CanApproach) || FinishMovement(Now))
		{
			TreeResult = !TestTreeCondition(EDemoSoldierTreeTest::CanApproach) || bMoveSucceeded
			                 ? EDemoSoldierTreeResult::Succeeded
			                 : EDemoSoldierTreeResult::Failed;
		}
		else if (FVector::DistSquared2D(Memory.TargetLocation, MoveGoal) > FMath::Square(150.0f))
		{
			StartMove(Memory.TargetLocation, FMath::Max(100.0f, Definition->Range * 0.7f));
		}
		break;
	case EDemoSoldierTreeAction::MoveToMemory:
		if (FinishMovement(Now))
		{
			TreeResult = bMoveSucceeded ? EDemoSoldierTreeResult::Succeeded : EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Reposition:
		if (ActionTarget != Memory.Target)
		{
			TreeResult = EDemoSoldierTreeResult::Failed;
		}
		else if (Memory.bFiringLineClear)
		{
			TreeResult = EDemoSoldierTreeResult::Succeeded;
		}
		else if (FinishMovement(Now))
		{
			TreeResult = bMoveSucceeded && !Memory.bTargetVisible ? EDemoSoldierTreeResult::Succeeded
			                                                      : EDemoSoldierTreeResult::Failed;
		}
		break;
	case EDemoSoldierTreeAction::Search:
		if (Now >= SearchDeadline)
		{
			TreeResult = EDemoSoldierTreeResult::Succeeded;
		}
		break;
	case EDemoSoldierTreeAction::InvestigateDamage:
	case EDemoSoldierTreeAction::InvestigateSound:
		if (FinishMovement(Now))
		{
			SetObservationFocus(MoveGoal);
		}
		if (Now >= ActionDeadline)
		{
			ConsumedAlertTime = FMath::Max(ConsumedAlertTime, InvestigatedAlertTime);
			TreeResult = EDemoSoldierTreeResult::Succeeded;
		}
		break;
	case EDemoSoldierTreeAction::ExecuteOrder:
		ExecuteOrder(Now);
		if (OrderStatus != EDemoSoldierOrderStatus::Running)
		{
			TreeResult = OrderStatus == EDemoSoldierOrderStatus::Completed ? EDemoSoldierTreeResult::Succeeded
			                                                               : EDemoSoldierTreeResult::Failed;
		}
		else if (bTreeReturningToArea && IsInsideOrderArea(Character->GetActorLocation()))
		{
			TreeResult = EDemoSoldierTreeResult::Succeeded;
		}
		break;
	default:
		break;
	}
}
