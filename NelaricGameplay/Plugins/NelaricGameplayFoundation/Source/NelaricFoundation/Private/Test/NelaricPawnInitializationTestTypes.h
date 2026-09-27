// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawnInitializationComponent.h"

#include "NelaricPawnInitializationTestTypes.generated.h"

UCLASS(MinimalAPI, NotBlueprintable, Transient)
class UNelaricPawnInitializationTestComponent final : public UNelaricPawnInitializationComponent
{
	GENERATED_BODY()

public:
	bool bReady = false;
	mutable int32 ReadinessChecks = 0;
	int32 InitializationEvents = 0;

	UFUNCTION()
	void RecordInitialization(UNelaricPawnInitializationComponent* Component)
	{
		if (Component == this)
		{
			++InitializationEvents;
		}
	}

protected:
	virtual bool CanInitializePawn_Implementation() const override
	{
		++ReadinessChecks;
		return bReady;
	}
};
