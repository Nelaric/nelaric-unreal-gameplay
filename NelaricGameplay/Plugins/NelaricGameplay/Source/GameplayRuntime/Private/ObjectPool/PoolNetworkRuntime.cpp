// Copyright (c) 2026 Nelaric Contributors

#include "ObjectPool/PoolNetworkRuntime.h"

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "ObjectPool/PoolNetworkChannel.h"

DEFINE_LOG_CATEGORY_STATIC(LogPoolNetwork, Log, All);

namespace Nelaric::ObjectPool::Private
{
namespace
{
using FWorldPools = TMap<FName, TSharedPtr<FPoolNetworkStorage>>;
TMap<TWeakObjectPtr<UWorld>, FWorldPools> WorldPools;
TMap<TWeakObjectPtr<AActor>, FPoolNetworkSlot*> ReplicaSlots;
FDelegateHandle DriverCreatedHandle;
FDelegateHandle WorldTickHandle;
FDelegateHandle WorldCleanupHandle;
const FName ChannelName(TEXT("NelaricPool"));

void RegisterDriver(UWorld*, UNetDriver* Driver)
{
	if (!Driver || Driver->IsKnownChannelName(ChannelName))
	{
		return;
	}
	FChannelDefinition Definition;
	Definition.ChannelName = ChannelName;
	Definition.ChannelClass = UPoolNetworkChannel::StaticClass();
	Definition.ClassName = FName(*Definition.ChannelClass->GetPathName());
	Definition.bServerOpen = true;
	Definition.bTickOnCreate = true;
	Driver->ChannelDefinitions.Add(Definition);
	Driver->ChannelDefinitionMap.Add(ChannelName, Definition);
}

void TickWorld(UWorld* World, ELevelTick TickType, float)
{
	if (!World || World->bIsTearingDown || TickType != LEVELTICK_All || !WorldPools.Contains(World))
	{
		return;
	}
	UNetDriver* Driver = World->GetNetDriver();
	if (!Driver || Driver->IsUsingIrisReplication())
	{
		return;
	}
	if (!Driver->IsServer())
	{
		FPoolNetworkRuntime::Resolve(*World, *Driver);
		return;
	}
	for (UNetConnection* Connection : Driver->ClientConnections)
	{
		if (!Connection || Connection->GetConnectionState() != USOCK_Open ||
		    !Connection->ClientHasInitializedLevel(World->PersistentLevel))
		{
			continue;
		}
		const bool bExists = Connection->OpenChannels.ContainsByPredicate(
		    [](const UChannel* Channel) { return Channel && Channel->ChName == ChannelName && !Channel->Closing; });
		if (!bExists)
		{
			Connection->CreateChannelByName(ChannelName, EChannelCreateFlags::OpenedLocally);
		}
	}
}

void CleanupWorld(UWorld* World, bool, bool)
{
	if (FWorldPools* Pools = WorldPools.Find(World))
	{
		for (auto& Pair : *Pools)
		{
			FPoolNetworkRuntime::Forget(*Pair.Value);
			Pair.Value->bClosed = true;
		}
		WorldPools.Remove(World);
	}
}
} // namespace

void FPoolNetworkRuntime::Start()
{
	DriverCreatedHandle = FWorldDelegates::OnNetDriverCreated.AddStatic(&RegisterDriver);
	WorldTickHandle = FWorldDelegates::OnWorldTickStart.AddStatic(&TickWorld);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&CleanupWorld);
}

void FPoolNetworkRuntime::Stop()
{
	FWorldDelegates::OnNetDriverCreated.Remove(DriverCreatedHandle);
	FWorldDelegates::OnWorldTickStart.Remove(WorldTickHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	ReplicaSlots.Reset();
	WorldPools.Reset();
}

TArray<TSharedPtr<FPoolNetworkStorage>> FPoolNetworkRuntime::GetPools(UWorld& World)
{
	TArray<TSharedPtr<FPoolNetworkStorage>> Result;
	if (FWorldPools* Pools = WorldPools.Find(&World))
	{
		Pools->GenerateValueArray(Result);
	}
	return Result;
}

TSharedPtr<FPoolNetworkStorage> FPoolNetworkRuntime::ReceiveOpen(UWorld& World, FGuid Id, FName Name, uint32 Capacity)
{
	FWorldPools& Pools = WorldPools.FindOrAdd(&World);
	if (!Id.IsValid() || Name.IsNone() || Capacity == 0 || Capacity > MaxNetworkPoolCapacity ||
	    (!Pools.Contains(Name) && Pools.Num() >= MaxNetworkPools))
	{
		return nullptr;
	}
	TSharedPtr<FPoolNetworkStorage>& Pool = Pools.FindOrAdd(Name);
	if (Pool && Pool->bAnnounced && Pool->Id != Id)
	{
		Forget(*Pool);
		// Existing local bindings continue to observe the replacement pool.
		Pool->Slots.Reset();
	}
	if (!Pool)
	{
		Pool = MakeShared<FPoolNetworkStorage>();
		Pool->World = &World;
		Pool->Name = Name;
		Pool->bClient = true;
	}
	if (Pool->Slots.Num() != 0 && Pool->Slots.Num() != static_cast<int32>(Capacity))
	{
		return nullptr;
	}
	Pool->Slots.SetNum(Capacity);
	Pool->Id = Id;
	Pool->bAnnounced = true;
	Pool->bClosed = false;
	Pool->bLoggedResolution = false;
	UE_LOG(LogPoolNetwork, Log, TEXT("Client pool received: %s (%u slots)."), *Name.ToString(), Capacity);
	return Pool;
}

TSharedPtr<FPoolNetworkStorage> FPoolNetworkRuntime::Find(UWorld& World, FGuid Id)
{
	for (const TSharedPtr<FPoolNetworkStorage>& Pool : GetPools(World))
	{
		if (Pool->Id == Id)
		{
			return Pool;
		}
	}
	return nullptr;
}

void FPoolNetworkRuntime::Forget(FPoolNetworkStorage& Pool)
{
	for (FPoolNetworkSlot& Slot : Pool.Slots)
	{
		ForgetSlot(Slot, Pool.bClient);
	}
}

void FPoolNetworkRuntime::ForgetSlot(FPoolNetworkSlot& Slot, bool bClient)
{
	if (AActor* Actor = Slot.Actor.Get())
	{
		UWorld* World = Actor->GetWorld();
		if (bClient && World && !World->bIsTearingDown && Actor->HasActorBegunPlay())
		{
			Slot.CharacterState.bActive = false;
			if (ACharacter* Character = Cast<ACharacter>(Actor))
			{
				FCharacterPoolHelper::DeactivateInternal(*Character, Slot.CharacterState, true);
			}
			else
			{
				Actor->SetActorHiddenInGame(true);
				Actor->SetActorTickEnabled(false);
				Actor->SetActorEnableCollision(false);
			}
		}
	}
	ReplicaSlots.Remove(Slot.Actor);
	Slot.Actor.Reset();
	Slot.AppliedRevision = 0;
	Slot.CharacterState = {};
}

void FPoolNetworkRuntime::Resolve(UWorld& World, UNetDriver& Driver)
{
	const TSharedPtr<FNetGUIDCache>& Cache = Driver.GetNetGuidCache();
	if (!Cache)
	{
		return;
	}
	for (const TSharedPtr<FPoolNetworkStorage>& Pool : GetPools(World))
	{
		if (!Pool->bClient || !Pool->bAnnounced || Pool->bClosed)
		{
			continue;
		}
		for (FPoolNetworkSlot& Slot : Pool->Slots)
		{
			if (!Slot.Revision)
			{
				continue;
			}
			if (!Slot.NetId)
			{
				if (Slot.AppliedRevision != Slot.Revision)
				{
					ForgetSlot(Slot, true);
				}
				Slot.AppliedRevision = Slot.Revision;
				continue;
			}
			FNetworkGUID Guid;
			Guid.ObjectId = Slot.NetId;
			AActor* Actor = Cast<AActor>(Cache->GetObjectFromNetGUID(Guid, true));
			if (!IsValid(Actor) || Actor->GetWorld() != &World || !Actor->HasActorBegunPlay())
			{
				continue;
			}
			if (Slot.Actor.Get() != Actor)
			{
				ForgetSlot(Slot, true);
				Slot.Actor = Actor;
			}
			if (Slot.AppliedRevision == Slot.Revision)
			{
				continue;
			}
			ReplicaSlots.Add(Actor, &Slot);
			Slot.CharacterState.bPrepared = true;
			if (ACharacter* Character = Cast<ACharacter>(Actor))
			{
				FCharacterPoolHelper::DeactivateInternal(*Character, Slot.CharacterState, true);
				if (Slot.bActive && !FCharacterPoolHelper::ActivateInternal(*Character, Slot.CharacterState,
				                                                            Character->GetActorTransform(), true))
				{
					continue;
				}
			}
			else
			{
				Slot.CharacterState.bActive = Slot.bActive;
				Actor->SetActorHiddenInGame(!Slot.bActive);
				Actor->SetActorTickEnabled(Slot.bActive);
				Actor->SetActorEnableCollision(Slot.bActive);
			}
			Slot.AppliedRevision = Slot.Revision;
		}
		if (!Pool->bLoggedResolution &&
		    !Pool->Slots.ContainsByPredicate([](const FPoolNetworkSlot& Slot)
		                                     { return Slot.Revision == 0 || Slot.AppliedRevision != Slot.Revision; }))
		{
			Pool->bLoggedResolution = true;
			int32 ActiveCount = 0;
			for (const FPoolNetworkSlot& Slot : Pool->Slots)
			{
				ActiveCount += Slot.CharacterState.bActive;
			}
			UE_LOG(LogPoolNetwork, Log, TEXT("Client pool synchronized: %s (%d slots, %d active)."),
			       *Pool->Name.ToString(), Pool->Slots.Num(), ActiveCount);
		}
	}
	for (auto It = ReplicaSlots.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

bool FPoolNetworkBinding::Begin(UWorld& World, FName Name, uint32 Capacity)
{
	check(IsInGameThread());
	if (World.bIsTearingDown || Name.IsNone() || FTCHARToUTF8(*Name.ToString()).Length() > MaxNetworkPoolNameBytes ||
	    Capacity == 0 || Capacity > MaxNetworkPoolCapacity ||
	    (World.GetNetDriver() && World.GetNetDriver()->IsUsingIrisReplication()))
	{
		UE_LOG(LogPoolNetwork, Error, TEXT("Pool networking requires a live world and the standard UE NetDriver."));
		return false;
	}
	FWorldPools& Pools = WorldPools.FindOrAdd(&World);
	if (!Pools.Contains(Name) && Pools.Num() >= MaxNetworkPools)
	{
		return false;
	}
	TSharedPtr<FPoolNetworkStorage>& Pool = Pools.FindOrAdd(Name);
	const bool bClient = World.GetNetMode() == NM_Client;
	if (Pool && !Pool->bClosed && (!bClient || Pool->Slots.Num() != static_cast<int32>(Capacity)))
	{
		return false;
	}
	if (!Pool || Pool->bClosed)
	{
		Pool = MakeShared<FPoolNetworkStorage>();
		Pool->World = &World;
		Pool->Name = Name;
		Pool->Id = bClient ? FGuid() : FGuid::NewGuid();
		Pool->Slots.SetNum(Capacity);
		Pool->bClient = bClient;
		Pool->bAnnounced = !bClient;
	}
	Storage = Pool;
	return true;
}

void FPoolNetworkBinding::Publish(uint32 Index, AActor* Actor, bool bActive)
{
	check(IsInGameThread());
	check(Storage && !Storage->bClient && Index < static_cast<uint32>(Storage->Slots.Num()));
	FPoolNetworkSlot& Slot = Storage->Slots[Index];
	Slot.Actor = Actor;
	Slot.bActive = bActive;
	check(Slot.Revision != MAX_uint64);
	++Slot.Revision;
	if (IsValid(Actor))
	{
		Actor->SetReplicates(true);
		Actor->SetReplicateMovement(true);
		Actor->bAlwaysRelevant = true;
		Actor->bOnlyRelevantToOwner = false;
		Actor->bNetUseOwnerRelevancy = false;
		Actor->SetNetDormancy(DORM_Awake);
		Actor->ForceNetUpdate();
	}
}

void FPoolNetworkBinding::Close()
{
	check(IsInGameThread());
	if (Storage && !Storage->bClient)
	{
		Storage->bClosed = true;
	}
	Storage.Reset();
}

bool FPoolNetworkBinding::IsClient() const
{
	return Storage && Storage->bClient;
}

bool FPoolNetworkBinding::IsReady() const
{
	return Storage && Storage->World.IsValid() && Storage->bAnnounced && !Storage->bClosed;
}

AActor* FPoolNetworkBinding::Get(uint32 Index) const
{
	if (!IsReady() || Index >= static_cast<uint32>(Storage->Slots.Num()))
	{
		return nullptr;
	}
	const FPoolNetworkSlot& Slot = Storage->Slots[Index];
	return !Storage->bClient || Slot.AppliedRevision != 0 ? Slot.Actor.Get() : nullptr;
}
} // namespace Nelaric::ObjectPool::Private

bool Nelaric::ObjectPool::QueryPoolReplica(const AActor& Actor, bool& bActive)
{
	check(IsInGameThread());
	if (Private::FPoolNetworkSlot* const* Slot = Private::ReplicaSlots.Find(&Actor))
	{
		bActive = (*Slot)->CharacterState.bActive;
		return true;
	}
	return false;
}
