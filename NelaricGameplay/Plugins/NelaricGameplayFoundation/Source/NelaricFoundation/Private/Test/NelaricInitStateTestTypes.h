// Copyright (c) 2026 Nelaric

#pragma once

#include "Components/ActorComponent.h"
#include "Pawn/NelaricPawnInitStateComponent.h"

#include "NelaricInitStateTestTypes.generated.h"

UCLASS(MinimalAPI)
class UNelaricInitStateTestPawnComponent : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bAllowAdvance = false;
	bool bInternalReady = false;
	bool bDeclareDependency = false;
	TWeakObjectPtr<UActorComponent> Dependency;
	int32 CancelCount = 0;

	virtual bool IsInitApplicable() const override
	{
		return true;
	}
	virtual bool IsRequiredForPawnReady() const override
	{
		return true;
	}
	virtual void GatherInitDependencies(TArray<Nelaric::FInitDependency>& OutDependencies) const override
	{
		if (bDeclareDependency)
		{
			OutDependencies.Add({FName(TEXT("RuntimeComponent")), Dependency});
		}
	}
	virtual bool TryChangeInitState() override
	{
		if (!bAllowAdvance || GetInitState() >= Nelaric::EInitState::DataInitialized)
		{
			return false;
		}
		return CommitInitState(static_cast<Nelaric::EInitState>(static_cast<uint8>(GetInitState()) + 1));
	}
	virtual bool CanEnterReady() const override
	{
		if (!bInternalReady || GetInitState() != Nelaric::EInitState::DataInitialized)
		{
			return false;
		}
		if (!bDeclareDependency)
		{
			return true;
		}
		const INelaricInitStateParticipantInterface* Required =
		    Cast<INelaricInitStateParticipantInterface>(Dependency.Get());
		return Required && !Required->HasTerminalInitFailure() &&
		       Required->GetInitState() == Nelaric::EInitState::Ready;
	}
	void SetDependency(UActorComponent* Component)
	{
		Dependency = Component;
		RequestInitRefresh();
	}
	bool TestCommit(Nelaric::EInitState NextState)
	{
		return CommitInitState(NextState);
	}

protected:
	virtual void CancelInitGenerationWork() override
	{
		++CancelCount;
	}
};

UCLASS(MinimalAPI)
class UNelaricInitStateTestDirectComponent : public UActorComponent, public INelaricInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	virtual bool IsInitApplicable() const override
	{
		return true;
	}
	virtual bool IsRequiredForPawnReady() const override
	{
		return false;
	}
	virtual void GatherInitDependencies(TArray<Nelaric::FInitDependency>&) const override
	{
	}
	virtual bool TryChangeInitState() override
	{
		return false;
	}
	virtual bool CanEnterReady() const override
	{
		return false;
	}
	virtual bool EnterReady() override
	{
		return false;
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
		++Generation.Value;
		State = Nelaric::EInitState::Registered;
		bFailed = false;
	}
	virtual void MarkTerminalInitFailure() override
	{
		bFailed = true;
	}

private:
	Nelaric::EInitState State = Nelaric::EInitState::Registered;
	Nelaric::FInitGeneration Generation{1};
	bool bFailed = false;
};
