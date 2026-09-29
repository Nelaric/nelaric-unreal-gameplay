// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawn.h"
#include "Pawn/PawnInitStateComponent.h"

#include "PawnInitializationNetworkTestTypes.generated.h"

class APlayerController;
class APlayerState;

UCLASS(MinimalAPI)
class UInitNetworkTestObject final : public UObject
{
	GENERATED_BODY()
};

UCLASS(Abstract, MinimalAPI)
class UInitNetworkTestPeer : public UPawnInitStateComponent
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
			PreparedObject = NewObject<UInitNetworkTestObject>(this);
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
		const FName PeerName = GetFName() == TEXT("Init_A") ? TEXT("Init_B") : TEXT("Init_A");
		UInitNetworkTestPeer* Peer = FindObject<UInitNetworkTestPeer>(GetOwner(), *PeerName.ToString());
		if (Peer)
		{
			Peer->EnsurePreparedObject();
		}
		bPeerObjectExistedBeforePreparation = Peer && Peer->PreparedObject != nullptr;
		return bPeerObjectExistedBeforePreparation;
	}

	virtual void OnInitReady() override
	{
		const FName PeerName = GetFName() == TEXT("Init_A") ? TEXT("Init_B") : TEXT("Init_A");
		UInitNetworkTestPeer* Peer = FindObject<UInitNetworkTestPeer>(GetOwner(), *PeerName.ToString());
		bPeerCallSucceededAfterReady = Peer && Peer->TouchPreparedObject();
	}

	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot&) override
	{
		PreparedObject = nullptr;
		PreparedObjectCallCount = 0;
		bPeerObjectExistedBeforePreparation = false;
		bPeerCallSucceededAfterReady = false;
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> PreparedObject;
};

UCLASS(MinimalAPI)
class UInitNetworkTestA final : public UInitNetworkTestPeer
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class UInitNetworkTestB final : public UInitNetworkTestPeer
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class UInitNetworkTestC final : public UPawnInitStateComponent
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
	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot&) override
	{
		ReadyController.Reset();
		ReadyPlayerState.Reset();
		ReadyCallCount = 0;
		bControllerAndPlayerStateUsableAfterReady = false;
	}
};

UCLASS(MinimalAPI)
class UInitNetworkTestD final : public UPawnInitStateComponent
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
	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot&) override
	{
		ReadyCallCount = 0;
		bAllDependenciesUsableAfterReady = false;
	}
};

UCLASS(MinimalAPI)
class UInitNetworkTestGraphNode final : public UPawnInitStateComponent
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
			PreparedObject = NewObject<UInitNetworkTestObject>(this);
		}
		return PreparedObject != nullptr;
	}

	virtual void OnInitReady() override;
	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot&) override
	{
		PreparedObject = nullptr;
		bReadyCallbackRan = false;
		bAllTargetCallsSucceeded = false;
		CalledTargetIds.Empty();
		ReceivedCallerIds.Empty();
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UInitNetworkTestObject> PreparedObject;
};

UCLASS(MinimalAPI)
class AInitNetworkTestPawn final : public ANelaricPawn
{
	GENERATED_BODY()

public:
	AInitNetworkTestPawn()
	{
		bReplicates = true;
		bAlwaysRelevant = true;
	}
};
