// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawn.h"
#include "Pawn/NelaricPawnInitStateComponent.h"

#include "NelaricPawnInitializationNetworkTestTypes.generated.h"

class APlayerController;
class APlayerState;

UCLASS(MinimalAPI)
class UNelaricInitNetworkTestObject final : public UObject
{
	GENERATED_BODY()
};

UCLASS(Abstract, MinimalAPI)
class UNelaricInitNetworkTestPeer : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bPeerObjectExistedBeforePreparation = false;
	bool bPeerCallSucceededAfterReady = false;
	int32 PreparedObjectCallCount = 0;

	void EnsurePreparedObject()
	{
		if (!PreparedObject)
		{
			PreparedObject = NewObject<UNelaricInitNetworkTestObject>(this);
		}
	}

	bool TouchPreparedObject()
	{
		if (!PreparedObject || GetInitState() != Nelaric::EInitState::Ready)
		{
			return false;
		}
		++PreparedObjectCallCount;
		return true;
	}

protected:
	virtual bool CanEntryDataAvailable() override
	{
		EnsurePreparedObject();
		const FName PeerName = GetFName() == TEXT("NelaricInit_A") ? TEXT("NelaricInit_B") : TEXT("NelaricInit_A");
		UNelaricInitNetworkTestPeer* Peer = FindObject<UNelaricInitNetworkTestPeer>(GetOwner(), *PeerName.ToString());
		if (Peer)
		{
			Peer->EnsurePreparedObject();
		}
		bPeerObjectExistedBeforePreparation = Peer && Peer->PreparedObject != nullptr;
		return bPeerObjectExistedBeforePreparation;
	}

	virtual void OnInitReady() override
	{
		const FName PeerName = GetFName() == TEXT("NelaricInit_A") ? TEXT("NelaricInit_B") : TEXT("NelaricInit_A");
		UNelaricInitNetworkTestPeer* Peer = FindObject<UNelaricInitNetworkTestPeer>(GetOwner(), *PeerName.ToString());
		bPeerCallSucceededAfterReady = Peer && Peer->TouchPreparedObject();
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> PreparedObject;
};

UCLASS(MinimalAPI)
class UNelaricInitNetworkTestA final : public UNelaricInitNetworkTestPeer
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class UNelaricInitNetworkTestB final : public UNelaricInitNetworkTestPeer
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class UNelaricInitNetworkTestC final : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bControllerAndPlayerStateUsableAfterReady = false;
	TWeakObjectPtr<APlayerController> ReadyController;
	TWeakObjectPtr<APlayerState> ReadyPlayerState;
	int32 ReadyCallCount = 0;

	bool TouchPlayerContext();

protected:
	virtual bool CanEntryReady() override;
	virtual void OnInitReady() override;
};

UCLASS(MinimalAPI)
class UNelaricInitNetworkTestD final : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bAllDependenciesUsableAfterReady = false;
	int32 ReadyCallCount = 0;

	bool TouchDependencies()
	{
		if (GetInitState() != Nelaric::EInitState::Ready || !bAllDependenciesUsableAfterReady)
		{
			return false;
		}
		++ReadyCallCount;
		return true;
	}

protected:
	virtual void OnInitReady() override;
};

UCLASS(MinimalAPI)
class UNelaricInitNetworkTestGraphNode final : public UNelaricPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bReadyCallbackRan = false;
	bool bAllTargetCallsSucceeded = false;
	TArray<FName> CalledTargetIds;
	TArray<FName> ReceivedCallerIds;

	bool ReceiveReadyCall(FName CallerId)
	{
		if (GetInitState() != Nelaric::EInitState::Ready || !PreparedObject)
		{
			return false;
		}
		ReceivedCallerIds.Add(CallerId);
		return true;
	}

protected:
	virtual bool CanEntryDataAvailable() override
	{
		if (!PreparedObject)
		{
			PreparedObject = NewObject<UNelaricInitNetworkTestObject>(this);
		}
		return PreparedObject != nullptr;
	}

	virtual void OnInitReady() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNelaricInitNetworkTestObject> PreparedObject;
};

UCLASS(MinimalAPI)
class ANelaricInitNetworkTestPawn final : public ANelaricPawn
{
	GENERATED_BODY()

public:
	ANelaricInitNetworkTestPawn()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
	}
};
