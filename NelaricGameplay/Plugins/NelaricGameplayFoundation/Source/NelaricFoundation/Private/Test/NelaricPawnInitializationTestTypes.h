// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawnInitializationComponent.h"
#include "Pawn/NelaricPawnInitializationConfig.h"
#include "Test/NelaricInitStateTestTypes.h"
#include "UObject/UObjectGlobals.h"

#include "NelaricPawnInitializationTestTypes.generated.h"

UCLASS(MinimalAPI, NotBlueprintable, Transient)
class UNelaricPawnInitializationTestComponent final : public UNelaricPawnInitializationComponent
{
	GENERATED_BODY()

public:
	bool bReady = false;
	mutable int32 ReadinessChecks = 0;
	int32 InitializationEvents = 0;
	void SetConfig(UNelaricPawnInitializationConfig* Config)
	{
		InitializationConfig = Config;
	}

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

UCLASS(MinimalAPI)
class UNelaricConfiguredInitStateTestComponent : public UNelaricInitStateTestPawnComponent
{
	GENERATED_BODY()

public:
	bool bPeerExistsOnRegister = false;

	UNelaricConfiguredInitStateTestComponent()
	{
		bAllowAdvance = true;
		bInternalReady = true;
	}

protected:
	virtual void OnRegister() override
	{
		const FName PeerName = GetFName() == TEXT("NelaricInit_First") ? TEXT("NelaricInit_Second")
		                                                              : TEXT("NelaricInit_First");
		bPeerExistsOnRegister = FindObject<UActorComponent>(GetOwner(), *PeerName.ToString()) != nullptr;
		Super::OnRegister();
	}
};
