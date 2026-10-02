// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/PawnInitStateComponent.h"

#include "Engine/World.h"
#include "Pawn/InitStateWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricInitState, Log, All);

UPawnInitStateComponent::UPawnInitStateComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

bool UPawnInitStateComponent::IsInitApplicable() const
{
	return true;
}

bool UPawnInitStateComponent::TryChangeInitState()
{
	if (bLeavingWorld || bCommittingInitState || bTerminalInitFailure ||
	    InitState >= Nelaric::EInitState::DataInitialized)
	{
		return false;
	}
	const Nelaric::FInitGeneration Generation = InitGeneration;
	const Nelaric::EInitState PreviousState = InitState;
	bCommittingInitState = true;
	const bool bPrepared =
	    PreviousState == Nelaric::EInitState::Registered ? CanEntryDataAvailable() : CanEntryDataInitialized();
	bCommittingInitState = false;
	if (!bPrepared || !(Generation == InitGeneration) || PreviousState != InitState || bTerminalInitFailure ||
	    bLeavingWorld)
	{
		FlushDeferredInitRefresh();
		return false;
	}
	return CommitInitState(static_cast<Nelaric::EInitState>(static_cast<uint8>(PreviousState) + 1));
}

bool UPawnInitStateComponent::CanEntryDataAvailable()
{
	return true;
}

bool UPawnInitStateComponent::CanEntryDataInitialized()
{
	return true;
}

bool UPawnInitStateComponent::CanEntryReady()
{
	return true;
}

void UPawnInitStateComponent::OnInitReady()
{
}

void UPawnInitStateComponent::OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot&)
{
}

bool UPawnInitStateComponent::CanEnterReady()
{
	if (bLeavingWorld || bCommittingInitState || bTerminalInitFailure ||
	    InitState != Nelaric::EInitState::DataInitialized)
	{
		return false;
	}
	if (!bLocalReadyPrepared)
	{
		const Nelaric::FInitGeneration Generation = InitGeneration;
		bCommittingInitState = true;
		const bool bPrepared = CanEntryReady();
		bCommittingInitState = false;
		bLocalReadyPrepared = bPrepared && Generation == InitGeneration &&
		                      InitState == Nelaric::EInitState::DataInitialized && !bTerminalInitFailure &&
		                      !bLeavingWorld;
	}
	FlushDeferredInitRefresh();
	return bLocalReadyPrepared;
}

bool UPawnInitStateComponent::CommitReadyWithoutNotification()
{
	if (bLeavingWorld || bCommittingInitState || bTerminalInitFailure || bReadyNotificationPending ||
	    !bLocalReadyPrepared || InitState != Nelaric::EInitState::DataInitialized)
	{
		return false;
	}
	InitState = Nelaric::EInitState::Ready;
	bReadyNotificationPending = true;
	return true;
}

void UPawnInitStateComponent::NotifyReadyCommitted(const Nelaric::FInitStateSnapshot& Previous)
{
	if (!bReadyNotificationPending || bTerminalInitFailure || InitState != Nelaric::EInitState::Ready ||
	    !(Previous.Generation == InitGeneration) || Previous.State != Nelaric::EInitState::DataInitialized)
	{
		return;
	}
	bReadyNotificationPending = false;
	bCommittingInitState = true;
	OnInitReady();
	bCommittingInitState = false;
	FlushDeferredInitRefresh();
	if (!(Previous.Generation == InitGeneration) || InitState != Nelaric::EInitState::Ready || bTerminalInitFailure)
	{
		FlushDeferredInitRefresh();
		return;
	}
	NotifyInitChanged(Previous);
	FlushDeferredInitRefresh();
}

void UPawnInitStateComponent::InvalidateInitContext()
{
	if (bLeavingWorld)
	{
		return;
	}
	if (bCommittingInitState)
	{
		bContextInvalidationRequested = true;
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
		{
			Subsystem->InvalidateParticipantContext(this);
		}
	}
}

void UPawnInitStateComponent::RequestInitRefresh()
{
	if (bCommittingInitState)
	{
		bRefreshRequestedDuringTransition = true;
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
		{
			Subsystem->RequestParticipantRefresh(this);
		}
	}
}

Nelaric::EInitState UPawnInitStateComponent::GetInitState() const
{
	return InitState;
}

Nelaric::FInitGeneration UPawnInitStateComponent::GetInitGeneration() const
{
	return InitGeneration;
}

bool UPawnInitStateComponent::HasTerminalInitFailure() const
{
	return bTerminalInitFailure;
}

bool UPawnInitStateComponent::CanApplyInitResult(const UWorld* ExpectedWorld,
                                                 Nelaric::FInitGeneration ExpectedGeneration) const
{
	return IsInGameThread() && IsValid(this) && IsRegistered() && !bLeavingWorld && GetWorld() == ExpectedWorld &&
	       InitGeneration == ExpectedGeneration && !bTerminalInitFailure;
}

UPawnInitStateComponent*
UPawnInitStateComponent::ResolveInitResult(const TWeakObjectPtr<UPawnInitStateComponent>& WeakComponent,
                                           const UWorld* ExpectedWorld, Nelaric::FInitGeneration ExpectedGeneration)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogNelaricInitState, Error, TEXT("Cannot resolve an initialization result outside the game thread."));
		return nullptr;
	}
	UPawnInitStateComponent* Component = WeakComponent.Get();
	return Component && Component->CanApplyInitResult(ExpectedWorld, ExpectedGeneration) ? Component : nullptr;
}

bool UPawnInitStateComponent::CommitInitState(Nelaric::EInitState NextState)
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
	FlushDeferredInitRefresh();
	return true;
}

void UPawnInitStateComponent::InvalidateInitGeneration()
{
	if (bInvalidatingGeneration)
	{
		return;
	}
	bInvalidatingGeneration = true;
	const Nelaric::FInitStateSnapshot Previous{InitGeneration, InitState, bTerminalInitFailure};
	const bool bWasCommitting = bCommittingInitState;
	bCommittingInitState = true;
	// Reject old asynchronous results before cancelling their work.
	++InitGeneration.Value;
	OnInitGenerationInvalidated(Previous);
	CancelInitGenerationWork();
	InitState = Nelaric::EInitState::Registered;
	bTerminalInitFailure = false;
	bReadyNotificationPending = false;
	bLocalReadyPrepared = false;
	bRefreshRequestedDuringTransition = !bLeavingWorld;
	NotifyInitChanged(Previous);
	bCommittingInitState = bWasCommitting;
	bInvalidatingGeneration = false;
	FlushDeferredInitRefresh();
}

void UPawnInitStateComponent::MarkTerminalInitFailure()
{
	if (bTerminalInitFailure)
	{
		return;
	}

	const Nelaric::FInitStateSnapshot Previous{InitGeneration, InitState, bTerminalInitFailure};
	UE_LOG(LogNelaricInitState, Error, TEXT("Terminal initialization failure: component=%s generation=%llu state=%d."),
	       *GetName(), InitGeneration.Value, static_cast<int32>(InitState));
	bTerminalInitFailure = true;
	NotifyInitChanged(Previous);
}

void UPawnInitStateComponent::CancelInitGenerationWork()
{
}

void UPawnInitStateComponent::NotifyInitChanged(const Nelaric::FInitStateSnapshot& Previous)
{
	if (UWorld* World = GetWorld())
	{
		if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
		{
			Subsystem->NotifyParticipantChanged(this, Previous);
		}
	}
}

void UPawnInitStateComponent::FlushDeferredInitRefresh()
{
	// A context reset supersedes a retry of the previous attempt.
	if (bContextInvalidationRequested && !bCommittingInitState)
	{
		bContextInvalidationRequested = false;
		bRefreshRequestedDuringTransition = false;
		InvalidateInitContext();
		return;
	}
	if (bRefreshRequestedDuringTransition && !bCommittingInitState)
	{
		bRefreshRequestedDuringTransition = false;
		RequestInitRefresh();
	}
}

void UPawnInitStateComponent::OnRegister()
{
	Super::OnRegister();
	bLeavingWorld = false;
	if (UWorld* World = GetWorld())
	{
		if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
		{
			Subsystem->RegisterParticipant(this);
		}
	}
	FlushDeferredInitRefresh();
}

void UPawnInitStateComponent::OnUnregister()
{
	bLeavingWorld = true;
	InvalidateInitGeneration();
	if (UWorld* World = GetWorld())
	{
		if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
		{
			Subsystem->UnregisterParticipant(this);
		}
	}
	Super::OnUnregister();
}
