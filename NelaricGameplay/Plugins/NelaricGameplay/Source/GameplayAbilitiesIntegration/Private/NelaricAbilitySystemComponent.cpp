// Copyright (c) 2026 Nelaric Contributors

#include "NelaricAbilitySystemComponent.h"
#include "GasStateSnapshot.h"
#include "PawnGasBindingComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"
#include "Player/ControlSwitchSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Templates/UnrealTemplate.h"
#include "NelaricGasPlayerState.h"
#include "NativeGameplayTags.h"

namespace Nelaric::GAS
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(PlayerInput, "Input.Source.Player");
UE_DEFINE_GAMEPLAY_TAG_STATIC(BotInput, "Input.Source.AI");
} // namespace Nelaric::GAS

UNelaricAbilitySystemComponent::UNelaricAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleEffectRemoved);
}

void UNelaricAbilitySystemComponent::HandleEffectRemoved(const FActiveGameplayEffect& Effect)
{
	OwnedEffects.Remove(Effect.Handle);
}

void UNelaricAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (BoundInputSourceTag.IsValid())
	{
		RemoveLooseGameplayTag(BoundInputSourceTag);
	}
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	BoundInputSourceTag = GetInputSourceTag();
	if (BoundInputSourceTag.IsValid())
	{
		AddLooseGameplayTag(BoundInputSourceTag);
	}
}

FGameplayTag UNelaricAbilitySystemComponent::GetInputSourceTag() const
{
	const auto* Participant = Cast<ANelaricGasPlayerState>(GetOwnerActor());
	return GetAvatarActor() && Participant && !Participant->IsStateCustodian()
	           ? FGameplayTag(Participant->IsABot() ? Nelaric::GAS::BotInput : Nelaric::GAS::PlayerInput)
	           : FGameplayTag();
}

bool UNelaricAbilitySystemComponent::IsTransferringState() const
{
	const UControlSwitchSubsystem* Switch = GetWorld() ? GetWorld()->GetSubsystem<UControlSwitchSubsystem>() : nullptr;
	const auto* AvatarPawn = Cast<APawn>(GetAvatarActor());
	const auto* Binding = AvatarPawn ? AvatarPawn->FindComponentByClass<UPawnGasBindingComponent>() : nullptr;
	return bStateTransferBlocked || (Binding && !Binding->HasCommittedState()) ||
	       (Switch && (Switch->IsControlTransitionInProgress(GetOwnerActor()) ||
	                   Switch->IsControlTransitionInProgress(GetAvatarActor())));
}

void UNelaricAbilitySystemComponent::SetStateTransferBlocked(bool bBlocked)
{
	bStateTransferBlocked = bBlocked;
	if (bBlocked)
	{
		ClearActionInput();
	}
}

void UNelaricAbilitySystemComponent::ClearActionInput()
{
	ABILITYLIST_SCOPE_LOCK();
	for (auto& Spec : GetActivatableAbilities())
	{
		if (Spec.InputPressed)
		{
			Spec.InputPressed = false;
			AbilitySpecInputReleased(Spec);
			if (Spec.IsActive())
			{
				InvokeReplicatedEvent(
				    EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
				    Spec.GetPrimaryInstance()
				        ? Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey()
				        : FPredictionKey());
			}
		}
	}
}

bool UNelaricAbilitySystemComponent::SubmitAction(FGameplayTag ActionTag, bool bPressed)
{
	check(IsInGameThread());
	const APawn* Pawn = Cast<APawn>(GetAvatarActor());
	if (!ActionTag.IsValid() || !IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !Pawn->GetController() ||
	    (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled()) || IsTransferringState())
	{
		return false;
	}
	TArray<FGameplayAbilitySpecHandle> Activate;
	bool bMatched = false;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (auto& Spec : GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(ActionTag))
			{
				bMatched = true;
				Spec.InputPressed = bPressed;
				if (bPressed)
				{
					AbilitySpecInputPressed(Spec);
				}
				else
				{
					AbilitySpecInputReleased(Spec);
				}
				if (Spec.IsActive())
				{
					InvokeReplicatedEvent(
					    bPressed ? EAbilityGenericReplicatedEvent::InputPressed
					             : EAbilityGenericReplicatedEvent::InputReleased,
					    Spec.Handle,
					    Spec.GetPrimaryInstance()
					        ? Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey()
					        : FPredictionKey());
				}
				else if (bPressed)
				{
					Activate.Add(Spec.Handle);
				}
			}
		}
	}
	for (const auto& Handle : Activate)
	{
		TryActivateAbility(Handle);
	}
	return bMatched;
}

bool UNelaricAbilitySystemComponent::TrackEffect(FActiveGameplayEffectHandle Handle, APawn* Pawn,
                                                 EGasStateOwnership Ownership)
{
	check(IsInGameThread());
	if (!IsOwnerActorAuthoritative() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld() ||
	    !GetActiveGameplayEffect(Handle) || Ownership == EGasStateOwnership::Participant)
	{
		return false;
	}
	if (const auto* Existing = OwnedEffects.Find(Handle);
	    Existing && (Existing->Pawn.Get() != Pawn || Existing->Ownership != Ownership))
	{
		return false;
	}
	OwnedEffects.Add(Handle, {Pawn, Ownership});
	return true;
}

bool UNelaricAbilitySystemComponent::ExportPawnEffects(APawn* Pawn, TArray<Nelaric::GAS::FEffectState>& OutEffects,
                                                       const TArray<FActiveGameplayEffectHandle>& CustomEffects) const
{
	for (const auto& Pair : OwnedEffects)
	{
		if (CustomEffects.Contains(Pair.Key) || Pair.Value.Pawn.Get() != Pawn ||
		    Pair.Value.Ownership != EGasStateOwnership::Pawn)
		{
			continue;
		}
		const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Pair.Key);
		if (!Active)
		{
			continue;
		}
		if (Active->Spec.Period > 0.0f &&
		    Active->Spec.Def->PeriodicInhibitionPolicy != EGameplayEffectPeriodInhibitionRemovedPolicy::NeverReset)
		{
			// Reset-on-uninhibit semantics require a domain restoration adapter.
			return false;
		}
		auto& State = OutEffects.AddDefaulted_GetRef();
		State.Spec = Active->Spec;
		State.StartWorldTime = Active->StartWorldTime;
		State.StartServerWorldTime = Active->StartServerWorldTime;
		State.EndTime = Active->Spec.GetDuration() < 0.0f ? -1.0 : Active->GetEndTime();
		State.bInhibited = Active->bIsInhibited;
		const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(Active->PeriodHandle);
		State.NextPeriodTime = Remaining < 0.0f ? -1.0 : GetWorld()->GetTimeSeconds() + Remaining;
	}
	return true;
}

bool UNelaricAbilitySystemComponent::CanReceiveEffects(const TArray<Nelaric::GAS::FEffectState>& Effects) const
{
	const auto* Coordinator = GetWorld()->GetSubsystem<UControlSwitchSubsystem>();
	for (const auto& Saved : Effects)
	{
		if (!Saved.Spec.Def)
		{
			return false;
		}
		if (Saved.Spec.Def->StackingType == EGameplayEffectStackingType::None)
		{
			continue;
		}
		for (const auto& Existing : &GetActiveGameplayEffects())
		{
			if (Existing.Spec.Def != Saved.Spec.Def)
			{
				continue;
			}
			const auto* Owned = OwnedEffects.Find(Existing.Handle);
			// A reserved pawn's effects are exported and removed as a batch.
			if (Owned && Coordinator && Coordinator->IsControlTransitionInProgress(Owned->Pawn.Get()))
			{
				continue;
			}
			if (Saved.Spec.Def->StackingType == EGameplayEffectStackingType::AggregateByTarget ||
			    Existing.Spec.GetContext().GetInstigatorAbilitySystemComponent() ==
			        Saved.Spec.GetContext().GetInstigatorAbilitySystemComponent())
			{
				return false;
			}
		}
	}
	return true;
}

bool UNelaricAbilitySystemComponent::RemovePawnEffects(APawn* Pawn)
{
	TArray<FActiveGameplayEffectHandle> Remove;
	for (const auto& Pair : OwnedEffects)
	{
		if (Pair.Value.Pawn.Get() == Pawn)
		{
			Remove.Add(Pair.Key);
		}
	}
	for (const auto& Handle : Remove)
	{
		if (GetActiveGameplayEffect(Handle) && !RemoveActiveGameplayEffect(Handle))
		{
			return false;
		}
		OwnedEffects.Remove(Handle);
	}
	return true;
}

bool UNelaricAbilitySystemComponent::RestorePawnEffects(APawn* Pawn, const TArray<Nelaric::GAS::FEffectState>& Effects)
{
	for (const auto& Saved : Effects)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Saved.EndTime >= 0.0 && Saved.EndTime <= Now)
		{
			continue;
		}
		FGameplayEffectSpec Spec(Saved.Spec);
		const float Period = Spec.Period;
		// Container insertion skips OnApplied and instant execution. Temporarily
		// omit period scheduling so execute-on-application cannot fire again.
		Spec.Period = 0.0f;
		if (Saved.EndTime >= 0.0)
		{
			Spec.bDurationLocked = false;
			Spec.SetDuration(static_cast<float>(Saved.EndTime - Now), true);
		}
		FPredictionKey Key;
		bool bMerged = false;
		FActiveGameplayEffect* Active = ActiveGameplayEffects.ApplyGameplayEffectSpec(Spec, Key, bMerged);
		if (!Active || bMerged)
		{
			return false;
		}
		const auto Handle = Active->Handle;
		TrackEffect(Handle, Pawn, EGasStateOwnership::Pawn);
		// Remove transient non-periodic modifiers before restoring the period.
		SetActiveGameplayEffectInhibit(FActiveGameplayEffectHandle(Handle), true, false);
		Active = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
		if (!Active)
		{
			return false;
		}
		Active->Spec.Period = Period;
		// Preserve the original elapsed fraction, not a fresh cooldown start.
		Active->StartWorldTime = Saved.StartWorldTime;
		Active->StartServerWorldTime = Saved.StartServerWorldTime;
		Active->CachedStartServerWorldTime = Saved.StartServerWorldTime;
		Active->Spec.bDurationLocked = false;
		Active->Spec.SetDuration(Saved.Spec.GetDuration(), true);
		if (Saved.EndTime >= 0.0)
		{
			GetWorld()->GetTimerManager().SetTimer(
			    Active->DurationHandle,
			    FTimerDelegate::CreateUObject(this, &UNelaricAbilitySystemComponent::CheckDurationExpired, Handle),
			    static_cast<float>(Saved.EndTime - Now), false);
		}
		if (Period > 0.0f)
		{
			const float Delay = Saved.NextPeriodTime < 0.0
			                        ? Period
			                        : FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(Saved.NextPeriodTime - Now));
			GetWorld()->GetTimerManager().SetTimer(
			    Active->PeriodHandle,
			    FTimerDelegate::CreateUObject(this, &UNelaricAbilitySystemComponent::ExecutePeriodicEffect, Handle),
			    Period, true, Delay);
		}
		ActiveGameplayEffects.MarkItemDirty(*Active);
		SetActiveGameplayEffectInhibit(FActiveGameplayEffectHandle(Handle), Saved.bInhibited, true);
	}
	return true;
}
