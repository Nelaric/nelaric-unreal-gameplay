// Copyright (c) 2026 Nelaric Contributors

/** @file FixedUObjectPool.h
 * Provides fixed-capacity UObject leases with typed lifecycle policies.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectPool/FixedSlotPool.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

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

/// Selects weak world references for actors and GC collection for other types.
template <class T>
inline constexpr EReferenceMode DefaultReferenceMode =
    std::is_base_of_v<AActor, T> ? EReferenceMode::WorldWeak : EReferenceMode::Collector;

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
template <class T, uint32 N, EReferenceMode Mode> class TReferenceTable;

/** @brief Bridges the fixed reference table to Unreal garbage collection.
 * @tparam T Complete UObject-derived type retained by this table.
 * @tparam N Fixed number of references managed by its owning pool.
 */
template <class T, uint32 N> class TReferenceTable<T, N, EReferenceMode::Collector> final : public FGCObject
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
template <class T, uint32 N> class TReferenceTable<T, N, EReferenceMode::WorldWeak> final
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
	static_assert(std::is_base_of_v<AActor, T>, "World ownership requires an Actor");
	TWeakObjectPtr<T> Objects[N]{};
};

/** @brief Stores raw actor references with an enforced lifetime contract.
 * @tparam T Complete Actor-derived type that
 * cannot be destroyed externally.
 * @tparam N Fixed number of references managed by its owning pool.
 */
template <class T, uint32 N> class TReferenceTable<T, N, EReferenceMode::WorldRaw> final
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
	static_assert(std::is_base_of_v<AActor, T>, "World ownership requires an Actor");
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
 */
template <class T, uint32 Capacity, class Policy, EReferenceMode Mode = DefaultReferenceMode<T>>
class TFixedUObjectPool final
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
	 * @return Success or AlreadyPrewarmed, InTransition, or CreationFailed.
	 * @details A failed factory rolls back created objects. This instance
	 * cannot be prewarmed again after an attempt or Shutdown.
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
		for (uint32 I = 0; I < Capacity; ++I)
		{
			T* Object = Policy::Create(Args);
			if (!IsValid(Object))
			{
				DestroyCreated();
				return {EPoolError::CreationFailed};
			}
			References.Set(I, Object);
			++CreatedCount;
			Policy::OnReturn(*Object);
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
			const bool bReturned = Slots.Release(Handle);
			check(bReturned);
			return {EPoolError::LostObject};
		}
		Policy::OnReturn(*Object);
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
		if (!bReady || bBusy || !Slots.IsLive(Handle))
		{
			return nullptr;
		}
		return References.Get(Handle.Index);
	}

	/** @brief Reads a raw actor slot directly on the game thread.
	 * @details Available only with WorldRaw. The caller guarantees successful
	 * prewarm, a live pool and actor, and no active lifecycle transition.
	 * No bounds, thread, world, readiness, or lease checks run at access time.
	 * @param Index Slot in [0, Capacity); unchecked.
	 * @return Non-owning stored pointer, whether its slot is free or leased.
	 * @note Reading a slot does not acquire it or validate an earlier lease.
	 */
	FORCEINLINE T* GetByIndexUnchecked(uint32 Index) const noexcept
	{
		static_assert(Mode == EReferenceMode::WorldRaw, "Unchecked index access requires WorldRaw actor references");
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
		DestroyCreated();
		return {};
	}

	/// Returns whether prewarm succeeded without object loss; game thread only.
	FORCEINLINE bool IsReady() const
	{
		check(IsInGameThread());
		return bReady && !bFaulted;
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
		return bReady && !bFaulted ? Slots.NumFree() : 0;
	}

public:
	FORCEINLINE ~TFixedUObjectPool()
	{
		check(!bBusy);
		(void)Shutdown();
	}
	TFixedUObjectPool(const TFixedUObjectPool&) = delete;
	TFixedUObjectPool& operator=(const TFixedUObjectPool&) = delete;
	TFixedUObjectPool(TFixedUObjectPool&&) = delete;
	TFixedUObjectPool& operator=(TFixedUObjectPool&&) = delete;

private:
	static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");

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
