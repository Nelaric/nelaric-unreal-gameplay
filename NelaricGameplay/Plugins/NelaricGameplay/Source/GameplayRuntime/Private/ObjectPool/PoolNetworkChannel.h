// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Engine/Channel.h"
#include "ObjectPool/PoolNetworkRuntime.h"

#include "PoolNetworkChannel.generated.h"

UCLASS(Transient, MinimalAPI)
class UPoolNetworkChannel final : public UChannel
{
	GENERATED_BODY()

public:
	UPoolNetworkChannel();
	virtual void Tick() override;
	virtual void ReceivedBunch(FInBunch& Bunch) override;
	virtual bool CanStopTicking() const override
	{
		return false;
	}

private:
	struct FSentPool
	{
		bool bOpened = false;
		bool bClosed = false;
		int32 NextSlot = 0;
		TArray<uint64> Revisions;
	};
	TMap<FGuid, FSentPool> SentPools;
	int32 NextPool = 0;
};
