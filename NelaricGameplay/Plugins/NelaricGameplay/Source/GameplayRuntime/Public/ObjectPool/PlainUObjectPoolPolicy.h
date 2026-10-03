// Copyright (c) 2026 Nelaric Contributors

/** @file PlainUObjectPoolPolicy.h
 * Provides a lifecycle policy for transient custom reusable UObjects.
 */

#pragma once

#include "Components/ActorComponent.h"
#include "ObjectPool/FixedUObjectPool.h"

namespace Nelaric::ObjectPool
{
/** @brief Creates new transient objects with native pool lifecycle methods.
 * @details T provides OnPoolAcquire and an idempotent OnPoolReturn. All calls
 * run synchronously on the game thread. This is not an asset, component,
 * actor, widget, or world factory. Shutdown releases collector references;
 * Unreal GC reclaims the object when no other references keep it reachable.
 * @tparam T Custom UObject whose native lifecycle supports repeated leases.
 */
template <class T> struct TPlainUObjectPoolPolicy
{
	static_assert(std::is_base_of_v<UObject, T>, "Requires UObject");
	static_assert(!std::is_base_of_v<AActor, T>, "Actors require SpawnActor");
	static_assert(!std::is_base_of_v<UActorComponent, T>, "Components need an owner/registration policy");

	/// Synchronous creation inputs; the pool does not retain the arguments.
	struct FCreateArgs
	{
		/// Valid non-owning outer; its lifetime is guaranteed by the caller.
		UObject* Outer = nullptr;
	};
	/// This baseline has no additional per-lease inputs.
	struct FAcquireArgs
	{
	};

	/** @brief Creates a new transient object on the game thread.
	 * @param Args Valid outer for this object's lifetime.
	 * @return A new object, or null for an invalid outer or abstract class.
	 */
	static FORCEINLINE T* Create(const FCreateArgs& Args)
	{
		if (!IsValid(Args.Outer) || T::StaticClass()->HasAnyClassFlags(CLASS_Abstract))
		{
			return nullptr;
		}
		return NewObject<T>(Args.Outer, NAME_None, RF_Transient);
	}

	/** @brief Runs native activation synchronously on the game thread.
	 * @param Object Valid inactive object that survives its callback.
	 * @param Args Empty activation arguments for this baseline.
	 * @return True after the infallible native activation callback.
	 */
	static FORCEINLINE bool OnAcquire(T& Object, const FAcquireArgs& Args)
	{
		(void)Args;
		Object.OnPoolAcquire();
		return true;
	}

	/** @brief Runs native deactivation synchronously on the game thread.
	 * @param Object Valid object whose OnPoolReturn is idempotent.
	 */
	static FORCEINLINE void OnReturn(T& Object)
	{
		Object.OnPoolReturn();
	}

	/** @brief Deactivates the object before its pool reference is released.
	 * @param Object Valid object whose native reset runs on the game thread.
	 * @details The native reset removes external registrations and cancels
	 * owned work. Memory reclamation remains with GC; do not delete UObjects.
	 */
	static FORCEINLINE void Destroy(T& Object)
	{
		Object.OnPoolReturn();
	}
};
} // namespace Nelaric::ObjectPool
