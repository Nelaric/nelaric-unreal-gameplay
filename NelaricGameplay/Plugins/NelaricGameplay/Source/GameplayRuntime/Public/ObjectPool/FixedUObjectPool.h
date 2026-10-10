// Copyright (c) 2026 Nelaric Contributors

/** @file FixedUObjectPool.h
 * Provides fixed-capacity UObject leases with typed lifecycle policies.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectPool/FixedSlotPool.h"
#include "ObjectPool/PoolNetwork.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <concepts>

namespace Nelaric::ObjectPool
{
/// Compile-time lifetime strategy for the fixed object reference table.
enum class EReferenceMode : uint8
{
	/// Keeps ordinary UObjects reachable through a non-reflected FGCObject.
	Collector,
	/// Uses world ownership and weak references to detect actor destruction.
	WorldWeak,
	/// Uses raw actor pointers under a strict externally enforced lifetime.
	WorldRaw,
};

/** @brief Matches UObject types to a supported reference lifetime strategy.
 * @tparam T Complete UObject-derived type retained by the pool.
 * @tparam Mode Collector for UObjects, or world ownership for actors.
 */
template <class T, EReferenceMode Mode>
concept CPoolReference =
    std::derived_from<T, UObject> &&
    (Mode == EReferenceMode::Collector ||
     ((Mode == EReferenceMode::WorldWeak || Mode == EReferenceMode::WorldRaw) && std::derived_from<T, AActor>));

/** @brief Enables direct raw actor access only for the WorldRaw strategy.
 * @tparam T Complete actor type retained by the pool.
 * @tparam Mode Reference strategy selected by the pool.
 */
template <class T, EReferenceMode Mode>
concept CRawPoolReference = CPoolReference<T, Mode> && Mode == EReferenceMode::WorldRaw;

/** @brief Requires the factory and lifecycle calls used by a fixed pool.
 * @tparam T Complete UObject-derived type managed by the policy.
 * @tparam Policy Static factory and lifecycle provider compatible with T.
 */
template <class T, class Policy>
concept CPoolPolicy =
    std::derived_from<T, UObject> && requires(T& Object, const typename Policy::FCreateArgs& CreateArgs,
                                              const typename Policy::FAcquireArgs& AcquireArgs) {
	    { Policy::Create(CreateArgs) } -> std::convertible_to<T*>;
	    (!Policy::OnAcquire(Object, AcquireArgs)) ? true : false;
	    Policy::OnReturn(Object);
	    Policy::Destroy(Object);
    };

/** @brief Detects an optional identity supplied by pool creation arguments.
 * @tparam CreateArgs Factory argument type inspected without creating it.
 */
template <class CreateArgs>
concept CNamedPoolCreateArgs = requires(const CreateArgs& Args) { Args.NetworkName; };

/** @brief Requires world access and a compatible optional network identity.
 * @tparam CreateArgs Factory arguments for a replicated actor pool.
 */
template <class CreateArgs>
concept CReplicatedPoolCreateArgs = requires(const CreateArgs& Args) {
	(!Args.World) ? true : false;
	{ *Args.World } -> std::convertible_to<UWorld&>;
} && (!CNamedPoolCreateArgs<CreateArgs> || requires(const CreateArgs& Args) {
	                                    Args.NetworkName.IsNone() ? true : false;
	                                    { Args.NetworkName } -> std::convertible_to<FName>;
                                    });

/** @brief Accepts fixed pool parameters with valid storage and networking.
 * @tparam T Complete UObject-derived type retained by the pool.
 * @tparam Capacity Positive slot count with two reserved sentinel indices.
 * @tparam Policy Factory and lifecycle provider compatible with T.
 * @tparam Mode Supported reference strategy for T.
 * @tparam NetworkMode Replicated pools require actor types.
 */
template <class T, uint32 Capacity, class Policy, EReferenceMode Mode, ENetworkMode NetworkMode>
concept CPoolConfiguration =
    CPoolCapacity<Capacity> && CPoolReference<T, Mode> && CPoolPolicy<T, Policy> && CPoolNetworkMode<NetworkMode> &&
    (NetworkMode == ENetworkMode::Disabled ||
     (std::derived_from<T, AActor> && CReplicatedPoolCreateArgs<typename Policy::FCreateArgs>));

/// Selects weak world references for actors and GC collection for other types.
template <std::derived_from<UObject> T>
inline constexpr EReferenceMode DefaultReferenceMode =
    std::derived_from<T, AActor> ? EReferenceMode::WorldWeak : EReferenceMode::Collector;

/// Stable failure classifications shared by pool operations and leases.
enum class EPoolError : uint8
{
	/// The operation completed successfully.
	None,
	/// The pool has not completed prewarm or has already shut down.
	NotReady,
	/// This instance has already attempted prewarm or has shut down.
	AlreadyPrewarmed,
	/// A same-pool lifecycle operation is currently executing.
	InTransition,
	/// The creation policy returned an invalid object during prewarm.
	CreationFailed,
	/// The activation policy rejected the lease; its slot was returned.
	ActivationFailed,
	/// Every slot is currently leased.
	Full,
	/// The free-list head exhausted its generation and cannot be reused.
	GenerationExhausted,
	/// A prewarmed object disappeared and further acquisition is disabled.
	LostObject,
	/// The handle is empty, foreign, stale, or already released.
	InvalidHandle,
	/// Network registration failed or the active driver is unsupported.
	NetworkingUnavailable,
};

/** @brief Reports a synchronous pool operation without allocating memory.
 * @details Boolean conversion preserves existing success/failure call sites.
 * Inspect Error to distinguish failure paths. Results own no objects.
 */
struct FPoolResult
{
	/// None for success, otherwise the operation's domain-specific error.
	EPoolError Error = EPoolError::None;

	/// Returns true for success; usable in bool assignment or a condition.
	FORCEINLINE constexpr operator bool() const noexcept
	{
		return Error == EPoolError::None;
	}
};

namespace Private
{
template <class T, uint32 N, EReferenceMode Mode>
    requires CPoolReference<T, Mode> && CPoolCapacity<N>
class TReferenceTable;

/** @brief Bridges the fixed reference table to Unreal garbage collection.
 * @tparam T Complete UObject-derived type retained by this table.
 * @tparam N Fixed number of references managed by its owning pool.
 */
template <std::derived_from<UObject> T, uint32 N>
    requires CPoolCapacity<N>
class TReferenceTable<T, N, EReferenceMode::Collector> final : public FGCObject
{
public:
	/** @brief Writes one GC-visible reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 * @param Object UObject to retain, or null to clear this entry.
	 */
	FORCEINLINE void Set(uint32 Index, T* Object)
	{
		Objects[Index] = Object;
	}

	/** @brief Resolves one retained object on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 * @return The valid referenced object, or null after invalidation.
	 */
	FORCEINLINE T* Get(uint32 Index) const
	{
		T* Object = Objects[Index].Get();
		return IsValid(Object) ? Object : nullptr;
	}

	/** @brief Releases one retained reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 */
	FORCEINLINE void Clear(uint32 Index)
	{
		Objects[Index] = nullptr;
	}

public:
	FORCEINLINE virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (TObjectPtr<T>& Object : Objects)
		{
			Collector.AddReferencedObject(Object, nullptr, nullptr);
		}
	}
	FORCEINLINE virtual FString GetReferencerName() const override
	{
		return TEXT("Nelaric.FixedUObjectPool");
	}

private:
	TObjectPtr<T> Objects[N]{};
};

/** @brief Observes actors without extending their world-owned lifetime.
 * @tparam T Complete Actor-derived type stored as weak references.
 * @tparam N Fixed number of references managed by its owning pool.
 */
template <std::derived_from<AActor> T, uint32 N>
    requires CPoolCapacity<N>
class TReferenceTable<T, N, EReferenceMode::WorldWeak> final
{
public:
	/** @brief Writes one weak reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 * @param Object World-owned actor to observe.
	 */
	FORCEINLINE void Set(uint32 Index, T* Object)
	{
		Objects[Index] = Object;
	}

	/** @brief Resolves one weak reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 * @return The actor while valid, or null after invalidation.
	 */
	FORCEINLINE T* Get(uint32 Index) const
	{
		return Objects[Index].Get();
	}

	/** @brief Clears one weak reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 */
	FORCEINLINE void Clear(uint32 Index)
	{
		Objects[Index].Reset();
	}

private:
	TWeakObjectPtr<T> Objects[N]{};
};

/** @brief Stores raw actor references with an enforced lifetime contract.
 * @tparam T Complete Actor-derived type that
 * cannot be destroyed externally.
 * @tparam N Fixed number of references managed by its owning pool.
 */
template <std::derived_from<AActor> T, uint32 N>
    requires CPoolCapacity<N>
class TReferenceTable<T, N, EReferenceMode::WorldRaw> final
{
public:
	/** @brief Writes one non-owning actor reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 * @param Object Actor guaranteed alive until pool shutdown.
	 */
	FORCEINLINE void Set(uint32 Index, T* Object)
	{
		Objects[Index] = Object;
	}

	/** @brief Reads a raw reference without resolving object validity.
	 * @param Index Valid table index supplied by the owning pool.
	 * @return Stored pointer; the caller enforces the actor's lifetime.
	 */
	FORCEINLINE T* Get(uint32 Index) const
	{
		return Objects[Index];
	}

	/** @brief Clears one non-owning actor reference on the game thread.
	 * @param Index Valid table index supplied by the owning pool.
	 */
	FORCEINLINE void Clear(uint32 Index)
	{
		Objects[Index] = nullptr;
	}

private:
	T* Objects[N]{};
};
} // namespace Private

/** @brief Owns fixed-capacity logical leases over prewarmed UObjects.
 * @details A module, subsystem, or native owner keeps this pool at a stable
 * address. All access, construction, destruction, and policy callbacks use
 * the game thread. Close before world actors end play or engine cleanup.
 * The pool has no reflection, copy, move, or automatic lease return.
 * @par Policy contract
 * Policy supplies FCreateArgs, FAcquireArgs, Create, OnAcquire, OnReturn,
 * and Destroy as static native functions. OnReturn is infallible and must
 * accept an object after OnAcquire returns false. Callbacks cannot destroy
 * the pool or a transitioning object, or force garbage collection.
 * Only Destroy performs shutdown destruction.
 * Reentrant mutations fail.
 * @par Lifetime contract
 * Handles do not extend pool lifetime.
 * @par Network contract
 * Replicated requires actor
 * types and FCreateArgs::World. Optional
 * FCreateArgs::NetworkName distinguishes same-type pools in one world.
 *
 * @details Clients prewarm a non-owning view and cannot acquire or
 * return leases.
 * Actor construction and movement
 * use the standard UE replication driver.
 * @note Weak references protect objects; generations protect logical leases.
 *
 * @note WorldRaw requires live actors and Shutdown before their world ends.
 * @note External destruction and expiring
 * life spans are forbidden in WorldRaw.
 * @note Fixed storage does not bound engine allocations.
 * @tparam T Complete
 * UObject type with a valid lifecycle policy.
 * @tparam Capacity Positive compile-time object count created during prewarm.
 * @tparam Policy
 * Native factory and lifecycle contract for T.
 * @tparam Mode Compile-time reference retention or world ownership strategy.
 * @tparam NetworkMode Actor-pool synchronization; disabled by default.
 */
template <class T, uint32 Capacity, class Policy, EReferenceMode Mode = DefaultReferenceMode<T>,
          ENetworkMode NetworkMode = ENetworkMode::Disabled>
    requires CPoolConfiguration<T, Capacity, Policy, Mode, NetworkMode>
class TFixedUObjectPool final : private Private::TPoolNetworkBinding<NetworkMode>
{
public:
	/// Shared fixed-capacity slot allocator used by this specialization.
	using FSlots = TFixedSlotPool<Capacity>;
	/// Non-owning logical lease token; expire it before destroying this pool.
	using FHandle = typename FSlots::FHandle;
	/// Synchronous prewarm arguments supplied by the typed creation policy.
	using FCreateArgs = typename Policy::FCreateArgs;
	/// Synchronous per-lease arguments supplied by the activation policy.
	using FAcquireArgs = typename Policy::FAcquireArgs;
	/// Compile-time number of prewarmed objects and slot entries.
	static constexpr uint32 CapacityValue = Capacity;

	/** @brief Returns an object and token for one lease, or its failure result.
	 * @details This is not an RAII owner. Copies identify the same lease and
	 * cannot be returned twice. Use Object immediately on the game thread;
	 * across frames retain Handle and resolve it through the still-live pool.
	 */
	struct FLease
	{
		/// Non-owning pointer for immediate game-thread use; null on failure.
		T* Object = nullptr;
		/// Token for later validation and explicit return to this live pool.
		FHandle Handle{};
		/// Shared operation result describing success or acquisition failure.
		FPoolResult Result{EPoolError::NotReady};

		/// Returns whether acquisition succeeded; does not revalidate the lease.
		FORCEINLINE explicit operator bool() const
		{
			return Object != nullptr && Result;
		}
	};

	/// Constructs the fixed tables on the game thread at a stable address.
	FORCEINLINE TFixedUObjectPool() = default;

	/** @brief Attempts exactly one complete prewarm on the game thread.
	 * @param Args Synchronous factory inputs; the pool does not retain them.
	 * @return Success, or a readiness, factory, or network-registration error.
	 * @details A failed factory rolls
	 * back created objects. This instance
	 * cannot be prewarmed again after an attempt or Shutdown.
	 * Client
	 * success binds a view; IsReady waits for the authority announcement.
	 */
	[[nodiscard]] FPoolResult Prewarm(const FCreateArgs& Args)
	{
		check(IsInGameThread());
		if (bBusy)
		{
			return {EPoolError::InTransition};
		}
		if (bAttemptedPrewarm)
		{
			return {EPoolError::AlreadyPrewarmed};
		}
		FOperationGuard Guard(bBusy);
		bAttemptedPrewarm = true;
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			FName Name = T::StaticClass()->GetFName();
			if constexpr (CNamedPoolCreateArgs<FCreateArgs>)
			{
				if (!Args.NetworkName.IsNone())
				{
					Name = Args.NetworkName;
				}
			}
			if (!Args.World || !this->Begin(*Args.World, Name, Capacity))
			{
				return {EPoolError::NetworkingUnavailable};
			}
			if (this->IsClient())
			{
				bReady = true;
				return {};
			}
		}
		for (uint32 I = 0; I < Capacity; ++I)
		{
			T* Object = Policy::Create(Args);
			if (!IsValid(Object))
			{
				DestroyCreated();
				if constexpr (NetworkMode == ENetworkMode::Replicated)
				{
					this->Close();
				}
				return {EPoolError::CreationFailed};
			}
			References.Set(I, Object);
			++CreatedCount;
			Policy::OnReturn(*Object);
			if constexpr (NetworkMode == ENetworkMode::Replicated)
			{
				this->Publish(I, Object, false);
			}
		}
		bReady = true;
		return {};
	}

	/** @brief Activates one prewarmed object on the game thread.
	 * @param Args Synchronous inputs for this lease's activation policy.
	 * @return Object and handle on success; Result.Error on ordinary failure.
	 * @details Never expands or respawns. Failure includes unavailable state,
	 * reentry, exhaustion, a lost object, or rejected activation. Failed
	 * activation is deactivated before its slot returns to the free list.
	 */
	[[nodiscard]] FLease TryAcquire(const FAcquireArgs& Args)
	{
		check(IsInGameThread());
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			if (this->IsClient())
			{
				return {nullptr, {}, {EPoolError::NotReady}};
			}
		}
		if (bBusy)
		{
			return {nullptr, {}, {EPoolError::InTransition}};
		}
		if (!bReady)
		{
			return {nullptr, {}, {EPoolError::NotReady}};
		}
		if (bFaulted)
		{
			return {nullptr, {}, {EPoolError::LostObject}};
		}
		FOperationGuard Guard(bBusy);
		const FHandle Handle = Slots.TryAcquire();
		if (!Handle)
		{
			return {nullptr, {}, {Slots.NumFree() == 0 ? EPoolError::Full : EPoolError::GenerationExhausted}};
		}
		T* Object = References.Get(Handle.Index);
		if (!Object)
		{
			bFaulted = true;
			if constexpr (NetworkMode == ENetworkMode::Replicated)
			{
				this->Publish(Handle.Index, nullptr, false);
			}
			const bool bReturned = Slots.Release(Handle);
			check(bReturned);
			return {nullptr, {}, {EPoolError::LostObject}};
		}
		if (!Policy::OnAcquire(*Object, Args))
		{
			Policy::OnReturn(*Object);
			const bool bReturned = Slots.Release(Handle);
			check(bReturned);
			return {nullptr, {}, {EPoolError::ActivationFailed}};
		}
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			this->Publish(Handle.Index, Object, true);
		}
		return {Object, Handle, {}};
	}

	/** @brief Deactivates and returns one current lease on the game thread.
	 * @param Handle Token for the lease to return.
	 * @return Success, NotReady, InTransition, InvalidHandle, or LostObject.
	 * @details The slot becomes free after deactivation finishes. LostObject
	 * releases the logical slot but returns failure and disables acquisition.
	 * Other live leases can still be returned after the pool loses an object.
	 */
	[[nodiscard]] FPoolResult Release(FHandle Handle)
	{
		check(IsInGameThread());
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			if (this->IsClient())
			{
				return {EPoolError::NotReady};
			}
		}
		if (bBusy)
		{
			return {EPoolError::InTransition};
		}
		if (!bReady)
		{
			return {EPoolError::NotReady};
		}
		if (!Slots.IsLive(Handle))
		{
			return {EPoolError::InvalidHandle};
		}
		FOperationGuard Guard(bBusy);
		T* Object = References.Get(Handle.Index);
		if (!Object)
		{
			bFaulted = true;
			if constexpr (NetworkMode == ENetworkMode::Replicated)
			{
				this->Publish(Handle.Index, nullptr, false);
			}
			const bool bReturned = Slots.Release(Handle);
			check(bReturned);
			return {EPoolError::LostObject};
		}
		Policy::OnReturn(*Object);
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			this->Publish(Handle.Index, Object, false);
		}
		const bool bReturned = Slots.Release(Handle);
		check(bReturned);
		return {};
	}

	/** @brief Resolves a current lease on the game thread.
	 * @param Handle Token to validate against this still-live pool.
	 * @return Non-owning pointer, or null for an unavailable or invalid lease.
	 * @details During transitions Get returns null. Object lifetime validation
	 * follows Mode and does not change the pool's lost-object fault flag.
	 */
	FORCEINLINE T* Get(FHandle Handle) const
	{
		check(IsInGameThread());
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			if (this->IsClient())
			{
				return nullptr;
			}
		}
		if (!bReady || bBusy || !Slots.IsLive(Handle))
		{
			return nullptr;
		}
		return References.Get(Handle.Index);
	}

	/** @brief Reads a local slot or an already resolved client actor.
	 * @details Game thread only. Client slots may be null while replicating.
	 * @param Index Slot index; out-of-range indices return null.
	 * @return Non-owning object while this pool is available, otherwise null.
	 */
	FORCEINLINE T* At(uint32 Index) const
	{
		check(IsInGameThread());
		if (!IsReady() || bBusy || Index >= Capacity)
		{
			return nullptr;
		}
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			if (this->IsClient())
			{
				return Cast<T>(Private::FPoolNetworkBinding::Get(Index));
			}
		}
		return References.Get(Index);
	}

	/** @brief Reads a raw actor slot directly on the game thread.
	 * @details Available only with WorldRaw. The caller guarantees successful
	 * prewarm, a live pool and actor, and no active lifecycle transition.
	 * No bounds, thread, world, readiness, or lease checks run at access time.
	 * @note Replicated clients resolve weak actor references and can
	 * return null before receipt, after
	 * destruction, or after closure.
	 * @param Index Slot in [0, Capacity); unchecked on authority.
	 * @return
	 * Non-owning stored pointer, whether its slot is free or leased.
	 * @note Reading a slot does not acquire it or validate an earlier lease.
	 */
	FORCEINLINE T* operator[](uint32 Index) const noexcept
	    requires CRawPoolReference<T, Mode>
	{
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			if (this->IsClient())
			{
				return Cast<T>(Private::FPoolNetworkBinding::Get(Index));
			}
		}
		return References.Get(Index);
	}

	/** @brief Ends this instance's lifetime contract on the game thread.
	 * @return Success, or InTransition when a lifecycle operation is active.
	 * @details Invalidates outstanding leases before policy destruction.
	 * Safe to repeat, but permanently prevents prewarm.
	 * @note Close before actor teardown or engine cleanup.
	 * @note Policy destruction must complete its native contract.
	 */
	[[nodiscard]] FPoolResult Shutdown()
	{
		check(IsInGameThread());
		if (bBusy)
		{
			return {EPoolError::InTransition};
		}
		FOperationGuard Guard(bBusy);
		bReady = false;
		bAttemptedPrewarm = true;
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			this->Close();
		}
		DestroyCreated();
		return {};
	}

	/// Returns whether prewarm succeeded without object loss; game thread only.
	FORCEINLINE bool IsReady() const
	{
		check(IsInGameThread());
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			return bReady && !bFaulted && Private::FPoolNetworkBinding::IsReady();
		}
		else
		{
			return bReady && !bFaulted;
		}
	}
	/// Returns whether acquire or release observed object loss; game thread only.
	FORCEINLINE bool HasLostObject() const
	{
		check(IsInGameThread());
		return bFaulted;
	}
	/// Returns whether a lifecycle operation is active; game thread only.
	FORCEINLINE bool IsInTransition() const
	{
		check(IsInGameThread());
		return bBusy;
	}
	/// Returns free slots, or zero when unavailable or faulted; game thread only.
	FORCEINLINE uint32 NumFree() const
	{
		check(IsInGameThread());
		if constexpr (NetworkMode == ENetworkMode::Replicated)
		{
			if (this->IsClient())
			{
				return 0;
			}
		}
		return bReady && !bFaulted ? Slots.NumFree() : 0;
	}

public:
	~TFixedUObjectPool()
	{
		check(!bBusy);
		(void)Shutdown();
	}
	TFixedUObjectPool(const TFixedUObjectPool&) = delete;
	TFixedUObjectPool& operator=(const TFixedUObjectPool&) = delete;
	TFixedUObjectPool(TFixedUObjectPool&&) = delete;
	TFixedUObjectPool& operator=(TFixedUObjectPool&&) = delete;

private:
	struct FOperationGuard
	{
		bool& Flag;
		FORCEINLINE explicit FOperationGuard(bool& InFlag) : Flag(InFlag)
		{
			Flag = true;
		}
		FORCEINLINE ~FOperationGuard()
		{
			Flag = false;
		}
		FOperationGuard(const FOperationGuard&) = delete;
		FOperationGuard& operator=(const FOperationGuard&) = delete;
	};

	FORCEINLINE void DestroyCreated()
	{
		for (uint32 I = 0; I < CreatedCount; ++I)
		{
			if (T* Object = References.Get(I))
			{
				Policy::Destroy(*Object);
			}
			References.Clear(I);
		}
		CreatedCount = 0;
	}

	Private::TReferenceTable<T, Capacity, Mode> References;
	FSlots Slots;
	uint32 CreatedCount = 0;
	bool bAttemptedPrewarm = false;
	bool bReady = false;
	bool bBusy = false;
	bool bFaulted = false;
};
} // namespace Nelaric::ObjectPool
