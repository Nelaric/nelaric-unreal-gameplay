// Copyright (c) 2026 Nelaric Contributors

/** @file CharacterPool.h
 * Provides the authority character lifecycle policy and native pool alias.
 */

#pragma once

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "ObjectPool/FixedUObjectPool.h"
#include "ObjectPool/PoolableCharacter.h"

namespace Nelaric::ObjectPool
{
/** @brief Specializes object lifetime through a native pooling interface.
 * @details Runs on the game thread in standalone and server worlds.
 * @note Spawns into PersistentLevel; Destroy is a
 * shutdown operation.
 * @note Prepares a deferred spawn before construction and BeginPlay.
 * @note Replicated pools receive client actors and grant only server leases.
 * @tparam T Any ACharacter-derived type
 * implementing IPoolableCharacter.
 */
template <class T> struct TCharacterPoolPolicy
{
	static_assert(std::is_base_of_v<ACharacter, T>, "Character pool requires an ACharacter-derived type");
	static_assert(std::is_base_of_v<IPoolableCharacter, T>, "Character must implement IPoolableCharacter");

	/// Synchronous prewarm inputs; these pointers are not retained by the pool.
	struct FCreateArgs
	{
		/// Non-owning authority world; close before actor teardown or cleanup.
		UWorld* World = nullptr;
		/// Optional loaded subclass; null selects T::StaticClass().
		UClass* Class = nullptr;
		/// Initial world placement; prewarmed characters remain inactive.
		FTransform ParkTransform = FTransform::Identity;
		/// Optional identity for multiple replicated pools of the same type.
		FName NetworkName;
	};
	/// Per-lease world transform; the caller selects valid placement.
	using FAcquireArgs = FTransform;

	/** @brief Creates one inactive character during prewarm on the game thread.
	 * @param Args World, class, and initial transform used synchronously.
	 * @return A world-owned character, or null for invalid inputs or spawn.
	 */
	static T* Create(const FCreateArgs& Args)
	{
		if (!IsValid(Args.World) || Args.World->bIsTearingDown || Args.World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}
		UClass* Class = Args.Class ? Args.Class : T::StaticClass();
		if (!IsValid(Class) || !Class->IsChildOf(T::StaticClass()) ||
		    Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.OverrideLevel = Args.World->PersistentLevel;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Params.bDeferConstruction = true;
		T* Character = Args.World->template SpawnActor<T>(Class, Args.ParkTransform, Params);
		if (!IsValid(Character))
		{
			return nullptr;
		}
		static_cast<IPoolableCharacter&>(*Character).PrepareForPool();
		Character->FinishSpawning(Args.ParkTransform);
		if (!IsValid(Character) || Character->IsActorBeingDestroyed() || !Character->HasAuthority())
		{
			return nullptr;
		}
		return Character;
	}

	/** @brief Activates one character synchronously on the game thread.
	 * @param Character Valid inactive character preserved through callbacks.
	 * @param Args World transform for this lease.
	 * @return Whether native activation succeeded.
	 */
	static FORCEINLINE bool OnAcquire(T& Character, const FAcquireArgs& Args)
	{
		if (!Character.HasAuthority() || !Character.GetWorld() || Character.GetWorld()->GetNetMode() == NM_Client)
		{
			return false;
		}
		return static_cast<IPoolableCharacter&>(Character).ActivateFromPool(Args);
	}

	/** @brief Deactivates one character synchronously on the game thread.
	 * @param Character Valid character preserved through deactivation callbacks.
	 */
	static FORCEINLINE void OnReturn(T& Character)
	{
		static_cast<IPoolableCharacter&>(Character).DeactivateToPool();
	}

	/** @brief Requests actor destruction during shutdown on the game thread.
	 * @param Character World-owned character with a valid destruction contract.
	 * @details The world must still be alive and must permit actor destruction.
	 */
	static FORCEINLINE void Destroy(T& Character)
	{
		Character.Destroy();
	}
};

/** @brief Uses the shared slot algorithm with the native character policy.
 * @tparam T Native character implementing the authority lifecycle contract.
 * @tparam Capacity Positive compile-time
 * number of prewarmed characters.
 * @tparam Mode Reference mode; WorldRaw requires strict lifetime control.
 * @tparam NetworkMode Optional automatic actor-pool synchronization.
 */
template <class T, uint32 Capacity, EReferenceMode Mode = EReferenceMode::WorldWeak,
          ENetworkMode NetworkMode = ENetworkMode::Disabled>
using TCharacterPool = TFixedUObjectPool<T, Capacity, TCharacterPoolPolicy<T>, Mode, NetworkMode>;
} // namespace Nelaric::ObjectPool
