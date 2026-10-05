// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierComponent.h"

#include "AbilitySystemComponent.h"
#include "AI/DemoSoldierCoverPoint.h"
#include "AI/DemoSoldierTags.h"
#include "AIController.h"
#include "Character/DemoCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "GAS/DemoCombatAttributes.h"

Nelaric::Soldier::FActionFinished& UDemoSoldierComponent::OnTreeActionFinished()
{
	check(IsInGameThread());
	return TreeActionFinished;
}

void UDemoSoldierComponent::NotifyTreeActionResult()
{
	if (!bNativePlanner && TreeActionId.IsValid() && !bTreeResultNotified &&
	    TreeResult != EDemoSoldierTreeResult::Running)
	{
		bTreeResultNotified = true;
		TreeActionFinished.Broadcast(TreeActionId, TreeResult);
	}
}

bool UDemoSoldierComponent::IsAimAligned() const
{
	const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	const AActor* Target = Memory.bTargetVisible ? Memory.Target.Get() : nullptr;
	if (!Pawn || !Target)
	{
		return false;
	}
	const APawn* TargetPawn = Cast<APawn>(Target);
	const FVector Aim = TargetPawn ? TargetPawn->GetPawnViewLocation() : Target->GetActorLocation();
	const float Dot =
	    FVector::DotProduct(Pawn->GetBaseAimRotation().Vector(), (Aim - Pawn->GetPawnViewLocation()).GetSafeNormal());
	return Dot >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Settings.AimToleranceDegrees, 0.1f, 45.0f)));
}

void UDemoSoldierComponent::RefreshTreeObservers()
{
	TSet<TWeakObjectPtr<USceneComponent>> Wanted;
	auto AddRoot = [&Wanted](AActor* Actor)
	{
		if (IsValid(Actor) && Actor->GetRootComponent())
		{
			Wanted.Add(Actor->GetRootComponent());
		}
	};
	AddRoot(GetOwner());
	for (const FDemoSoldierContact& Contact : Contacts)
	{
		if (Contact.bVisible)
		{
			AddRoot(Contact.Actor.Get());
		}
	}
	if (OrderStatus == EDemoSoldierOrderStatus::Running && CurrentOrder.Type == EDemoSoldierOrderType::Follow)
	{
		AddRoot(CurrentOrder.Actor.Get());
	}
	AddRoot(Cover.Get());
	TSet<TWeakObjectPtr<AActor>> WantedActors;
	for (const auto& Root : Wanted)
	{
		WantedActors.Add(Root->GetOwner());
	}
	for (auto It = ObservedActors.CreateIterator(); It; ++It)
	{
		if (!WantedActors.Contains(*It))
		{
			if (AActor* Actor = It->Get())
			{
				Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleObservedEndPlay);
			}
			It.RemoveCurrent();
		}
	}
	for (const auto& Key : WantedActors)
	{
		if (!ObservedActors.Contains(Key))
		{
			Key->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleObservedEndPlay);
			ObservedActors.Add(Key);
		}
	}
	for (auto It = TransformHandles.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(It.Key()))
		{
			if (USceneComponent* Component = It.Key().Get())
			{
				Component->TransformUpdated.Remove(It.Value());
			}
			It.RemoveCurrent();
		}
	}
	for (const auto& Key : Wanted)
	{
		if (!TransformHandles.Contains(Key))
		{
			TransformHandles.Add(Key, Key->TransformUpdated.AddWeakLambda(
			                              this, [this](USceneComponent* Component, EUpdateTransformFlags, ETeleportType)
			                              { HandleObservedTransform(Component); }));
		}
	}
	UDemoEquipmentManagerComponent* Equipment = GetOwner()->FindComponentByClass<UDemoEquipmentManagerComponent>();
	if (Equipment != ObservedEquipment.Get())
	{
		if (auto* Previous = ObservedEquipment.Get())
		{
			Previous->OnStateChanged().Remove(EquipmentChangedHandle);
		}
		ObservedEquipment = Equipment;
		EquipmentChangedHandle = Equipment
		                             ? Equipment->OnStateChanged().AddUObject(this, &ThisClass::HandleEquipmentChanged)
		                             : FDelegateHandle();
	}
	const ADemoCharacter* Target = Memory.bTargetVisible ? Cast<ADemoCharacter>(Memory.Target.Get()) : nullptr;
	UAbilitySystemComponent* AbilitySystem = Target ? Target->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem != ObservedTargetAbilitySystem.Get())
	{
		if (auto* Previous = ObservedTargetAbilitySystem.Get())
		{
			Previous->GetGameplayAttributeValueChangeDelegate(UDemoCombatAttributes::GetHealthAttribute())
			    .Remove(TargetHealthHandle);
		}
		ObservedTargetAbilitySystem = AbilitySystem;
		TargetHealthHandle =
		    AbilitySystem
		        ? AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDemoCombatAttributes::GetHealthAttribute())
		              .AddUObject(this, &ThisClass::HandleObservedHealth)
		        : FDelegateHandle();
	}
}

void UDemoSoldierComponent::ClearTreeObservers()
{
	for (const auto& Key : ObservedActors)
	{
		if (AActor* Actor = Key.Get())
		{
			Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleObservedEndPlay);
		}
	}
	ObservedActors.Reset();
	for (const auto& Entry : TransformHandles)
	{
		if (USceneComponent* Component = Entry.Key.Get())
		{
			Component->TransformUpdated.Remove(Entry.Value);
		}
	}
	TransformHandles.Reset();
	if (auto* Equipment = ObservedEquipment.Get())
	{
		Equipment->OnStateChanged().Remove(EquipmentChangedHandle);
	}
	if (auto* AbilitySystem = ObservedTargetAbilitySystem.Get())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDemoCombatAttributes::GetHealthAttribute())
		    .Remove(TargetHealthHandle);
	}
	EquipmentChangedHandle.Reset();
	TargetHealthHandle.Reset();
	ObservedEquipment.Reset();
	ObservedTargetAbilitySystem.Reset();
}

void UDemoSoldierComponent::HandleEquipmentChanged()
{
	if (ActionWeapon.IsValid() && ActionWeapon.Get() != GetWeapon())
	{
		TreeResult = EDemoSoldierTreeResult::Failed;
		NotifyTreeActionResult();
	}
	Wake();
}

void UDemoSoldierComponent::HandleObservedHealth(const FOnAttributeChangeData& Change)
{
	const auto* Visible = Contacts.FindByPredicate([Target = Memory.Target](const auto& Contact)
	                                               { return Contact.Actor == Target && Contact.bVisible; });
	if (Change.NewValue <= 0.0f && Memory.bTargetVisible && Visible)
	{
		// Commit observed death before an immediate actor destruction invalidates it.
		Emit(Nelaric::Soldier::TargetDead);
		if (OrderStatus == EDemoSoldierOrderStatus::Running && CurrentOrder.Type == EDemoSoldierOrderType::Attack &&
		    CurrentOrder.Actor == Memory.Target)
		{
			CompleteOrder(EDemoSoldierOrderStatus::Completed);
		}
		ForgetTarget();
	}
	Wake();
}

void UDemoSoldierComponent::HandleObservedEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	Wake();
}

void UDemoSoldierComponent::HandleObservedTransform(USceneComponent* Component)
{
	if (!ExecutionDriver.IsValid() || !IsValid(Component))
	{
		return;
	}
	AActor* Actor = Component->GetOwner();
	if (Actor == GetOwner())
	{
		const FVector Location = Actor->GetActorLocation();
		bool bChanged = IsInsideOrderArea(Location) != IsInsideOrderArea(LastSelfLocation) ||
		                IsDangerous(Location) != IsDangerous(LastSelfLocation);
		if (TreeAction == EDemoSoldierTreeAction::ApproachTarget)
		{
			bChanged |= !TestTreeCondition(EDemoSoldierTreeTest::CanApproach);
		}
		if (Memory.bInCover && Cover.IsValid())
		{
			bChanged |= FVector::DistSquared2D(Location, Cover->GetActorLocation()) > FMath::Square(150.0f);
		}
		if (TreeAction == EDemoSoldierTreeAction::Aim && !bAimWakeRequested &&
		    GetWorld()->GetTimeSeconds() >= ActionDeadline && IsAimAligned())
		{
			bAimWakeRequested = true;
			bChanged = true;
		}
		LastSelfLocation = Location;
		if (bChanged)
		{
			Wake();
		}
		return;
	}
	if (FDemoSoldierContact* Contact =
	        Contacts.FindByPredicate([Actor](const auto& Entry) { return Entry.Actor == Actor; });
	    Contact && Contact->bVisible)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		const FVector Position = Pawn ? Pawn->GetPawnViewLocation() : Actor->GetActorLocation();
		if (!Contact->Location.Equals(Position, 1.0f))
		{
			Contact->Location = Position;
			Contact->LastSeenTime = GetWorld()->GetTimeSeconds();
			Wake();
		}
	}
	if (CurrentOrder.Type == EDemoSoldierOrderType::Follow && OrderStatus == EDemoSoldierOrderStatus::Running &&
	    CurrentOrder.Actor == Actor)
	{
		const float Radius = FMath::Max(150.0f, CurrentOrder.AcceptanceRadius);
		if ((Behavior == EDemoSoldierBehavior::ExecuteOrder &&
		     FVector::DistSquared2D(Actor->GetActorLocation(), MoveGoal) > FMath::Square(Radius)) ||
		    (Behavior != EDemoSoldierBehavior::ExecuteOrder &&
		     FVector::DistSquared2D(GetOwner()->GetActorLocation(), Actor->GetActorLocation()) >
		         FMath::Square(CurrentOrder.AcceptanceRadius)))
		{
			Wake();
		}
	}
	if (Actor == Cover.Get() && Memory.bInCover)
	{
		Wake();
	}
}

void UDemoSoldierComponent::ScheduleTreeWake(double Now)
{
	if (!ExecutionDriver.IsValid())
	{
		return;
	}
	double Next = TNumericLimits<double>::Max();
	auto Deadline = [Now, &Next](double Time)
	{
		if (Time > Now)
		{
			Next = FMath::Min(Next, Time);
		}
	};
	for (const FDemoSoldierContact& Contact : Contacts)
	{
		if (!Contact.bVisible)
		{
			Deadline(Contact.LastSeenTime + Settings.MemorySeconds);
		}
	}
	for (const auto& Grenade : Grenades)
	{
		Deadline(Grenade.ExpiresAt);
	}
	if (Contacts.Num() > 1)
	{
		Deadline(TargetSelectedTime + Settings.TargetLockSeconds);
	}
	if (Memory.bUnderFire)
	{
		Deadline(Memory.LastDamageTime + Settings.UnderFireSeconds);
	}
	if (Memory.Suppression > 0.0f)
	{
		const float Threshold = bSuppressed ? 0.35f : 0.0f;
		Deadline(Now + FMath::Max(0.0f, Memory.Suppression - Threshold) / FMath::Max(0.01f, Settings.SuppressionDecay) +
		         0.01);
	}
	Deadline(NextCoverTime);
	Deadline(NextReloadTime);
	const double Cue = FMath::Max(Memory.LastHeardTime, Memory.LastDamageTime);
	if (Cue > ConsumedAlertTime)
	{
		Deadline(Cue + FMath::Max(0.1f, Settings.AlertSeconds));
	}
	if (TreeActionId.IsValid() && TreeResult == EDemoSoldierTreeResult::Running && TreeOrderRevision == OrderRevision)
	{
		if (MoveId.IsValid())
		{
			Deadline(MoveDeadline);
		}
		switch (TreeAction)
		{
		case EDemoSoldierTreeAction::Aim:
			Deadline(ActionDeadline);
			Deadline(MoveDeadline);
			break;
		case EDemoSoldierTreeAction::FireBurst:
			Deadline(NextShotTime);
			Deadline(ActionDeadline);
			break;
		case EDemoSoldierTreeAction::Reload:
		case EDemoSoldierTreeAction::Observe:
		case EDemoSoldierTreeAction::InvestigateDamage:
		case EDemoSoldierTreeAction::InvestigateSound:
			Deadline(ActionDeadline);
			break;
		case EDemoSoldierTreeAction::Search:
			Deadline(SearchDeadline);
			break;
		case EDemoSoldierTreeAction::AvoidGrenade:
			if (bMoveFinished && !bMoveSucceeded && EscapeAttempts < 3)
			{
				Deadline(Now + 0.01);
			}
			break;
		default:
			break;
		}
	}
	if (Next != TNumericLimits<double>::Max())
	{
		const float Delay = float(FMath::Max(0.01, Next - Now));
		const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(UpdateTimer);
		if (Remaining <= 0.0f || Remaining > Delay)
		{
			Schedule(Delay);
		}
	}
}
