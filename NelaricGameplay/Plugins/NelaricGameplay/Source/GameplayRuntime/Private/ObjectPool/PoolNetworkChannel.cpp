// Copyright (c) 2026 Nelaric Contributors

#include "ObjectPool/PoolNetworkChannel.h"

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "Net/DataBunch.h"

namespace Nelaric::ObjectPool::Private
{
namespace
{
constexpr uint8 ProtocolVersion = 1;
constexpr int32 MaxMessagesPerTick = 16;
enum class EPoolMessage : uint8
{
	Open,
	Slot,
	Close
};

void WriteHeader(FOutBunch& Bunch, EPoolMessage Message, FGuid Id)
{
	uint8 Version = ProtocolVersion;
	uint8 Kind = static_cast<uint8>(Message);
	Bunch.bReliable = true;
	Bunch << Version << Kind << Id;
}
} // namespace
} // namespace Nelaric::ObjectPool::Private

UPoolNetworkChannel::UPoolNetworkChannel()
{
	ChName = TEXT("NelaricPool");
}

void UPoolNetworkChannel::Tick()
{
	Super::Tick();
	using namespace Nelaric::ObjectPool::Private;
	UNetDriver* Driver = Connection ? Connection->Driver : nullptr;
	UWorld* World = Driver ? Driver->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown || !Driver->IsServer() || Driver->IsUsingIrisReplication() || Closing)
	{
		return;
	}
	const TSharedPtr<FNetGUIDCache>& Cache = Driver->GetNetGuidCache();
	if (!Cache)
	{
		return;
	}
	int32 Remaining = MaxMessagesPerTick;
	const TArray<TSharedPtr<FPoolNetworkStorage>> Pools = FPoolNetworkRuntime::GetPools(*World);
	for (auto It = SentPools.CreateIterator(); It; ++It)
	{
		if (!Pools.ContainsByPredicate([Id = It.Key()](const auto& Pool) { return Pool->Id == Id; }))
		{
			It.RemoveCurrent();
		}
	}
	const int32 StartPool = NextPool;
	for (int32 Offset = 0; Offset < Pools.Num() && Remaining > 0; ++Offset)
	{
		const int32 PoolIndex = (StartPool + Offset) % Pools.Num();
		NextPool = (PoolIndex + 1) % Pools.Num();
		const TSharedPtr<FPoolNetworkStorage>& Pool = Pools[PoolIndex];
		if (Pool->bClient || !Pool->bAnnounced || Remaining == 0 || NumOutRec >= 128 || !IsNetReady())
		{
			continue;
		}
		FSentPool& Sent = SentPools.FindOrAdd(Pool->Id);
		if (Pool->bClosed)
		{
			if (Sent.bOpened && !Sent.bClosed)
			{
				FOutBunch Bunch(this, false);
				WriteHeader(Bunch, EPoolMessage::Close, Pool->Id);
				SendBunch(&Bunch, false);
				Sent.bClosed = true;
				--Remaining;
			}
			continue;
		}
		if (!Sent.bOpened)
		{
			FOutBunch Bunch(this, false);
			WriteHeader(Bunch, EPoolMessage::Open, Pool->Id);
			uint32 Capacity = Pool->Slots.Num();
			const FTCHARToUTF8 Name(*Pool->Name.ToString());
			if (Name.Length() > MaxNetworkPoolNameBytes)
			{
				continue;
			}
			uint16 Length = Name.Length();
			Bunch << Capacity << Length;
			Bunch.Serialize(const_cast<ANSICHAR*>(Name.Get()), Length);
			SendBunch(&Bunch, false);
			Sent.bOpened = true;
			Sent.Revisions.SetNumZeroed(Capacity);
			--Remaining;
		}
		const int32 StartSlot = Sent.NextSlot;
		for (int32 SlotOffset = 0; SlotOffset < Pool->Slots.Num() && Remaining > 0 && NumOutRec < 128 && IsNetReady();
		     ++SlotOffset)
		{
			const int32 Index = (StartSlot + SlotOffset) % Pool->Slots.Num();
			Sent.NextSlot = (Index + 1) % Pool->Slots.Num();
			const FPoolNetworkSlot& Slot = Pool->Slots[Index];
			if (Slot.Revision == Sent.Revisions[Index])
			{
				continue;
			}
			AActor* Actor = Slot.Actor.Get();
			uint64 NetId = Actor ? Cache->GetOrAssignNetGUID(Actor).ObjectId : 0;
			if (Actor && NetId == 0)
			{
				continue;
			}
			FOutBunch Bunch(this, false);
			WriteHeader(Bunch, EPoolMessage::Slot, Pool->Id);
			uint32 SlotIndex = Index;
			uint64 Revision = Slot.Revision;
			uint8 Active = Slot.bActive && Actor;
			Bunch << SlotIndex << Revision << NetId << Active;
			SendBunch(&Bunch, false);
			Sent.Revisions[Index] = Revision;
			--Remaining;
		}
	}
}

void UPoolNetworkChannel::ReceivedBunch(FInBunch& Bunch)
{
	using namespace Nelaric::ObjectPool::Private;
	UNetDriver* Driver = Connection ? Connection->Driver : nullptr;
	UWorld* World = Driver ? Driver->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown || Driver->IsServer() || Driver->IsUsingIrisReplication())
	{
		Bunch.SetError();
		return;
	}
	uint8 Version = 0;
	uint8 Kind = 0;
	FGuid Id;
	Bunch << Version << Kind << Id;
	if (Bunch.IsError() || Version != ProtocolVersion || !Id.IsValid())
	{
		Bunch.SetError();
		return;
	}
	if (Kind == static_cast<uint8>(EPoolMessage::Open))
	{
		uint32 Capacity = 0;
		uint16 Length = 0;
		Bunch << Capacity << Length;
		if (Bunch.IsError() || Length == 0 || Length > MaxNetworkPoolNameBytes)
		{
			Bunch.SetError();
			return;
		}
		TArray<ANSICHAR, TInlineAllocator<MaxNetworkPoolNameBytes>> Bytes;
		Bytes.SetNumUninitialized(Length);
		Bunch.Serialize(Bytes.GetData(), Length);
		if (Bunch.IsError() || Bytes.Contains(static_cast<ANSICHAR>(0)))
		{
			Bunch.SetError();
			return;
		}
		const FUTF8ToTCHAR Name(Bytes.GetData(), Length);
		if (Name.Length() == 0 || Name.Length() >= NAME_SIZE)
		{
			Bunch.SetError();
			return;
		}
		if (!FPoolNetworkRuntime::ReceiveOpen(*World, Id, FName(FString(Name.Length(), Name.Get())), Capacity))
		{
			Bunch.SetError();
		}
		return;
	}
	TSharedPtr<FPoolNetworkStorage> Pool = FPoolNetworkRuntime::Find(*World, Id);
	if (!Pool)
	{
		Bunch.SetError();
		return;
	}
	if (Kind == static_cast<uint8>(EPoolMessage::Close))
	{
		FPoolNetworkRuntime::Forget(*Pool);
		Pool->bClosed = true;
		return;
	}
	if (Kind != static_cast<uint8>(EPoolMessage::Slot))
	{
		Bunch.SetError();
		return;
	}
	uint32 Index = 0;
	uint64 Revision = 0;
	uint64 NetId = 0;
	uint8 Active = 0;
	Bunch << Index << Revision << NetId << Active;
	if (Bunch.IsError() || Index >= static_cast<uint32>(Pool->Slots.Num()) || Revision == 0 || Active > 1 ||
	    Pool->bClosed || (Active && !NetId))
	{
		Bunch.SetError();
		return;
	}
	FPoolNetworkSlot& Slot = Pool->Slots[Index];
	if (Revision > Slot.Revision)
	{
		Slot.NetId = NetId;
		Slot.Revision = Revision;
		Slot.bActive = Active != 0;
	}
}
