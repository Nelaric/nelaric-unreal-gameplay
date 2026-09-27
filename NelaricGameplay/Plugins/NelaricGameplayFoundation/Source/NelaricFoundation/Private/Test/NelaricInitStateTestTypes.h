// Copyright (c) 2026 Nelaric

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "Pawn/NelaricPawnInitStateComponent.h"
#include "World/NelaricInitStateWorldSubsystem.h"

#include "NelaricInitStateTestTypes.generated.h"

UCLASS(MinimalAPI)
class UNelaricInitStateTestPawnComponent : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bAllowAdvance = false;
	bool bInternalReady = false;
	int32 CancelCount = 0;
	Nelaric::FInitGeneration GenerationAtCancel;

	virtual bool IsInitApplicable() const override
	{
		return true;
	}
	virtual bool CanEnterReady() const override
	{
		return bInternalReady && GetInitState() == Nelaric::EInitState::DataInitialized;
	}

protected:
	virtual bool CanAdvanceInitState() override
	{
		return bAllowAdvance;
	}

	virtual void CancelInitGenerationWork() override
	{
		GenerationAtCancel = GetInitGeneration();
		++CancelCount;
	}
};

UCLASS(MinimalAPI)
class UNelaricInitStateTestDirectComponent : public UActorComponent, public INelaricInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	bool bAllowAdvance = false;
	bool bInternalReady = false;
	int32 NotificationCount = 0;
	int32 CancelCount = 0;
	Nelaric::FInitGeneration GenerationAtCancel;
	TWeakObjectPtr<UActorComponent> ObservedDependency;
	bool bDependencyReadyAtNotification = false;
	int32 DependencyNotificationsAtReady = 0;

	virtual bool IsInitApplicable() const override
	{
		return true;
	}
	virtual bool TryChangeInitState() override
	{
		if (!bAllowAdvance || bFailed || State >= Nelaric::EInitState::DataInitialized)
		{
			return false;
		}
		const Nelaric::FInitStateSnapshot Previous{Generation, State, bFailed};
		State = static_cast<Nelaric::EInitState>(static_cast<uint8>(State) + 1);
		NotifyChanged(Previous);
		return true;
	}
	virtual bool CanEnterReady() const override
	{
		return bInternalReady && !bFailed && State == Nelaric::EInitState::DataInitialized;
	}
	virtual bool CommitReadyWithoutNotification() override
	{
		if (bFailed || bReadyNotificationPending || State != Nelaric::EInitState::DataInitialized)
		{
			return false;
		}
		State = Nelaric::EInitState::Ready;
		bReadyNotificationPending = true;
		return true;
	}
	virtual void NotifyReadyCommitted(const Nelaric::FInitStateSnapshot& Previous) override
	{
		if (!bReadyNotificationPending || bFailed || State != Nelaric::EInitState::Ready ||
		    !(Previous.Generation == Generation) || Previous.State != Nelaric::EInitState::DataInitialized)
		{
			return;
		}
		bReadyNotificationPending = false;
		NotifyChanged(Previous);
	}
	virtual Nelaric::EInitState GetInitState() const override
	{
		return State;
	}
	virtual Nelaric::FInitGeneration GetInitGeneration() const override
	{
		return Generation;
	}
	virtual bool HasTerminalInitFailure() const override
	{
		return bFailed;
	}
	virtual void InvalidateInitGeneration() override
	{
		const Nelaric::FInitStateSnapshot Previous{Generation, State, bFailed};
		++Generation.Value;
		GenerationAtCancel = Generation;
		++CancelCount;
		State = Nelaric::EInitState::Registered;
		bFailed = false;
		bReadyNotificationPending = false;
		NotifyChanged(Previous);
	}
	virtual void MarkTerminalInitFailure() override
	{
		if (bFailed)
		{
			return;
		}
		const Nelaric::FInitStateSnapshot Previous{Generation, State, bFailed};
		bFailed = true;
		NotifyChanged(Previous);
	}

private:
	void NotifyChanged(const Nelaric::FInitStateSnapshot& Previous)
	{
		++NotificationCount;
		if (State == Nelaric::EInitState::Ready && ObservedDependency.IsValid())
		{
			const INelaricInitStateParticipantInterface* Dependency =
			    Cast<INelaricInitStateParticipantInterface>(ObservedDependency.Get());
			bDependencyReadyAtNotification = Dependency && Dependency->GetInitState() == Nelaric::EInitState::Ready;
			if (const UNelaricInitStateTestDirectComponent* DirectDependency =
			        Cast<UNelaricInitStateTestDirectComponent>(ObservedDependency.Get()))
			{
				DependencyNotificationsAtReady = DirectDependency->NotificationCount;
			}
		}
		if (UWorld* World = GetWorld())
		{
			if (UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>())
			{
				Subsystem->NotifyParticipantChanged(this, Previous);
			}
		}
	}
	Nelaric::EInitState State = Nelaric::EInitState::Registered;
	Nelaric::FInitGeneration Generation{1};
	bool bFailed = false;
	bool bReadyNotificationPending = false;
};
