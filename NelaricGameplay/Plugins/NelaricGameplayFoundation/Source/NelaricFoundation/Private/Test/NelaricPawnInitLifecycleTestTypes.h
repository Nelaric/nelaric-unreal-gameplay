// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawnInitStateComponent.h"

#include "NelaricPawnInitLifecycleTestTypes.generated.h"

UCLASS(MinimalAPI)
class UNelaricInitLifecycleDefaultTestComponent : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class UNelaricInitLifecycleGateTestComponent : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bDataAvailable = false;
	bool bDataInitialized = false;
	bool bReadyPrepared = false;
	int32 RegisteredCalls = 0;
	int32 AvailableCalls = 0;
	int32 InitializedCalls = 0;
	int32 ReadyCalls = 0;

protected:
	virtual bool CanEntryDataAvailable() override
	{
		++RegisteredCalls;
		return bDataAvailable;
	}

	virtual bool CanEntryDataInitialized() override
	{
		++AvailableCalls;
		return bDataInitialized;
	}

	virtual bool CanEntryReady() override
	{
		++InitializedCalls;
		return bReadyPrepared;
	}

	virtual void OnInitReady() override
	{
		++ReadyCalls;
	}
};
