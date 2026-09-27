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
		if (Dependency.IsValid())
		{
			OutDependencies.Add({Dependency, Nelaric::EInitState::Ready});
		}
	}
	virtual bool TryChangeInitState() override
	{
		if (!bAllowAdvance)
		{
			return false;
		}
		return CommitInitState(static_cast<Nelaric::EInitState>(static_cast<uint8>(GetInitState()) + 1));
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
