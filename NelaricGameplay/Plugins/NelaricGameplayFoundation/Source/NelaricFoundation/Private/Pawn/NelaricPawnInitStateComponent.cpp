// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawnInitStateComponent.h"

#include "Engine/World.h"
#include "World/NelaricInitStateWorldSubsystem.h"

UNelaricPawnInitStateComponent::UNelaricPawnInitStateComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UNelaricPawnInitStateComponent::IsInitApplicable() const
{
	return false;
}

bool UNelaricPawnInitStateComponent::TryChangeInitState()
{
	if (bCommittingInitState || bTerminalInitFailure || InitState >= Nelaric::EInitState::DataInitialized)
	{
		return false;
	}
	const Nelaric::FInitGeneration Generation = InitGeneration;
	const Nelaric::EInitState PreviousState = InitState;
	if (!CanAdvanceInitState() || !(Generation == InitGeneration) || PreviousState != InitState || bTerminalInitFailure)
	{
		return false;
	}
	return CommitInitState(static_cast<Nelaric::EInitState>(static_cast<uint8>(PreviousState) + 1));
}

bool UNelaricPawnInitStateComponent::CanAdvanceInitState()
{
	return false;
}

bool UNelaricPawnInitStateComponent::CanEnterReady() const
{
	return false;
}

bool UNelaricPawnInitStateComponent::CommitReadyWithoutNotification()
{
	if (bCommittingInitState || bTerminalInitFailure || bReadyNotificationPending ||
	    InitState != Nelaric::EInitState::DataInitialized)
	{
		return false;
	}
	InitState = Nelaric::EInitState::Ready;
	bReadyNotificationPending = true;
	return true;
}

void UNelaricPawnInitStateComponent::NotifyReadyCommitted(const Nelaric::FInitStateSnapshot& Previous)
{
	if (!bReadyNotificationPending || bTerminalInitFailure || InitState != Nelaric::EInitState::Ready ||
	    !(Previous.Generation == InitGeneration) || Previous.State != Nelaric::EInitState::DataInitialized)
	{
		return;
	}
	bReadyNotificationPending = false;
	NotifyInitChanged(Previous);
}

void UNelaricPawnInitStateComponent::RequestInitRefresh()
{
	if (UWorld* World = GetWorld())
	{
		if (UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>())
		{
			Subsystem->RequestParticipantRefresh(this);
		}
	}
}

Nelaric::EInitState UNelaricPawnInitStateComponent::GetInitState() const
{
	return InitState;
}

Nelaric::FInitGeneration UNelaricPawnInitStateComponent::GetInitGeneration() const
{
	return InitGeneration;
}

bool UNelaricPawnInitStateComponent::HasTerminalInitFailure() const
{
	return bTerminalInitFailure;
}

bool UNelaricPawnInitStateComponent::CanApplyInitResult(const UWorld* ExpectedWorld,
                                                        Nelaric::FInitGeneration ExpectedGeneration) const
{
	return IsInGameThread() && IsValid(this) && IsRegistered() && GetWorld() == ExpectedWorld &&
	       InitGeneration == ExpectedGeneration && !bTerminalInitFailure;
}

UNelaricPawnInitStateComponent*
UNelaricPawnInitStateComponent::ResolveInitResult(const TWeakObjectPtr<UNelaricPawnInitStateComponent>& WeakComponent,
                                                  const UWorld* ExpectedWorld,
                                                  Nelaric::FInitGeneration ExpectedGeneration)
{
	if (!IsInGameThread())
	{
		return nullptr;
	}
	UNelaricPawnInitStateComponent* Component = WeakComponent.Get();
	return Component && Component->CanApplyInitResult(ExpectedWorld, ExpectedGeneration) ? Component : nullptr;
}

bool UNelaricPawnInitStateComponent::CommitInitState(Nelaric::EInitState NextState)
{
	if (bCommittingInitState || bTerminalInitFailure || InitState == Nelaric::EInitState::Ready ||
	    NextState == Nelaric::EInitState::Ready || static_cast<uint8>(NextState) != static_cast<uint8>(InitState) + 1)
	{
		return false;
	}

	const Nelaric::FInitStateSnapshot Previous{InitGeneration, InitState, bTerminalInitFailure};
	bCommittingInitState = true;
	InitState = NextState;
	NotifyInitChanged(Previous);
	bCommittingInitState = false;
	return true;
}

void UNelaricPawnInitStateComponent::InvalidateInitGeneration()
{
	const Nelaric::FInitStateSnapshot Previous{InitGeneration, InitState, bTerminalInitFailure};
	++InitGeneration.Value;
	CancelInitGenerationWork();
	InitState = Nelaric::EInitState::Registered;
	bTerminalInitFailure = false;
	bReadyNotificationPending = false;
	NotifyInitChanged(Previous);
}

void UNelaricPawnInitStateComponent::MarkTerminalInitFailure()
{
	if (bTerminalInitFailure)
	{
		return;
	}

	const Nelaric::FInitStateSnapshot Previous{InitGeneration, InitState, bTerminalInitFailure};
	bTerminalInitFailure = true;
	NotifyInitChanged(Previous);
}

void UNelaricPawnInitStateComponent::CancelInitGenerationWork()
{
}

void UNelaricPawnInitStateComponent::NotifyInitChanged(const Nelaric::FInitStateSnapshot& Previous)
{
	if (UWorld* World = GetWorld())
	{
		if (UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>())
		{
			Subsystem->NotifyParticipantChanged(this, Previous);
		}
	}
}

void UNelaricPawnInitStateComponent::OnRegister()
{
	Super::OnRegister();
	if (UWorld* World = GetWorld())
	{
		if (UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>())
		{
			Subsystem->RegisterParticipant(this);
		}
	}
}

void UNelaricPawnInitStateComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>())
		{
			Subsystem->UnregisterParticipant(this);
		}
	}
	InvalidateInitGeneration();
	Super::OnUnregister();
}
