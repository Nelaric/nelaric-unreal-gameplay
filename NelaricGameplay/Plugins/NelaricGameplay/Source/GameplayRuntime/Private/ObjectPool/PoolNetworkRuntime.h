// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/CharacterPoolHelper.h"
#include "ObjectPool/PoolNetwork.h"

class UNetDriver;

namespace Nelaric::ObjectPool::Private
{
inline constexpr uint32 MaxNetworkPoolCapacity = 4096;
inline constexpr int32 MaxNetworkPools = 128;
inline constexpr uint16 MaxNetworkPoolNameBytes = 1024;

struct FPoolNetworkSlot
{
	TWeakObjectPtr<AActor> Actor;
	uint64 NetId = 0;
	uint64 Revision = 0;
	uint64 AppliedRevision = 0;
	bool bActive = false;
	FCharacterPoolState CharacterState;
};

struct FPoolNetworkStorage
{
	TWeakObjectPtr<UWorld> World;
	FGuid Id;
	FName Name;
	TArray<FPoolNetworkSlot> Slots;
	bool bClient = false;
	bool bAnnounced = false;
	bool bClosed = false;
	bool bLoggedResolution = false;
};

struct FPoolNetworkRuntime
{
	static void Start();
	static void Stop();
	static TArray<TSharedPtr<FPoolNetworkStorage>> GetPools(UWorld& World);
	static TSharedPtr<FPoolNetworkStorage> ReceiveOpen(UWorld& World, FGuid Id, FName Name, uint32 Capacity);
	static TSharedPtr<FPoolNetworkStorage> Find(UWorld& World, FGuid Id);
	static void Resolve(UWorld& World, UNetDriver& Driver);
	static void Forget(FPoolNetworkStorage& Pool);
	static void ForgetSlot(FPoolNetworkSlot& Slot, bool bClient);
};
} // namespace Nelaric::ObjectPool::Private
