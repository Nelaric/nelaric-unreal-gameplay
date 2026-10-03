// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCharacterPoolSubsystem.h
 * Declares world ownership and automatic prewarm for demo character leases.
 */

#pragma once

#include "Character/DemoCharacter.h"
#include "ObjectPool/CharacterPool.h"
#include "ObjectPool/FixedObjectPoolWorldSubsystem.h"
#include "Templates/SubclassOf.h"

#include "DemoCharacterPoolSubsystem.generated.h"

class UPawnInitializationComponent;

/** @brief Owns the demo's fixed character pool in an authority world.
 * @details Unreal creates this service in Game
 * and PIE worlds.
 * @note Prewarms before actor BeginPlay and closes at world teardown.
 * @note Gameplay access starts after the world has begun play.
 * @note Clients expose this service but cannot acquire
 * authority leases.
 * @note All access runs on the game thread; leases never retain the world.
 * @note WorldRaw forbids external destruction and expiring actor life spans.
 * @note Native transitions retain the demo's existing GAS state contract.
 */
UCLASS(MinimalAPI)
class UDemoCharacterPoolSubsystem final : public UFixedObjectPoolWorldSubsystem
{
	GENERATED_BODY()

public:
	/// Compile-time number of demo characters created during world prewarm.
	static constexpr uint32 Capacity = 200;
	/// Raw fixed storage; the subsystem exclusively controls actor destruction.
	using FPool =
	    Nelaric::ObjectPool::TCharacterPool<ADemoCharacter, Capacity, Nelaric::ObjectPool::EReferenceMode::WorldRaw>;
	/// Non-owning object and generation handle; does not return automatically.
	using FLease = FPool::FLease;
	/// Non-owning lease token; invalid after return or world teardown.
	using FHandle = FPool::FHandle;

	/** @brief Activates one prewarmed demo character on the game thread.
	 * @param Transform World placement chosen by the caller without a sweep.
	 * @return Lease, or an error; NotReady on clients or outside world play.
	 * @note Acquisition never grows the
	 * pool or replaces missing characters.
	 */
	[[nodiscard]] FORCEINLINE FLease TryAcquire(const FTransform& Transform)
	{
		if (!CanUsePool())
		{
			return {nullptr, {}, {Nelaric::ObjectPool::EPoolError::NotReady}};
		}
		return Pool.TryAcquire(Transform);
	}

	/** @brief Returns a current lease to the world pool on the game thread.
	 * @param Handle Token belonging to this subsystem's current pool instance.
	 * @return Pool result; NotReady on clients or outside world play.
	 * @note Foreign, stale, and duplicate returns
	 * fail without deactivation.
	 */
	[[nodiscard]] FORCEINLINE Nelaric::ObjectPool::FPoolResult Release(FHandle Handle)
	{
		if (!CanUsePool())
		{
			return {Nelaric::ObjectPool::EPoolError::NotReady};
		}
		return Pool.Release(Handle);
	}

	/** @brief Resolves a lease while its world remains in play.
	 * @details Game thread only; the pointer is for immediate access.
	 * @param Handle Non-owning token from this still-live subsystem.
	 * @return Character, or null for an invalid lease or unavailable world.
	 */
	FORCEINLINE ADemoCharacter* Get(FHandle Handle) const
	{
		return CanUsePool() ? Pool.Get(Handle) : nullptr;
	}

	/** @brief Reads a prewarmed demo slot directly on the game thread.
	 * @details The caller guarantees a ready world and pool, live characters,
	 * and no lifecycle transition. Access performs no runtime validation.
	 * @param Index Slot in [0, Capacity); unchecked.
	 * @return Non-owning character, including characters in free slots.
	 * @note Does not acquire a lease or verify its generation.
	 */
	FORCEINLINE ADemoCharacter* GetByIndexUnchecked(uint32 Index) const noexcept
	{
		return Pool.GetByIndexUnchecked(Index);
	}

	/// Returns authority pool readiness; always false on clients; game thread.
	FORCEINLINE bool IsReady() const
	{
		return CanUsePool() && Pool.IsReady();
	}

	/// Returns free slots, or zero when unavailable; game thread only.
	FORCEINLINE uint32 NumFree() const
	{
		return CanUsePool() ? Pool.NumFree() : 0;
	}

public:
	UDemoCharacterPoolSubsystem();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	/** @brief Creates Capacity inactive instances of the demo Blueprint.
	 * @param World Owning game world before
	 * actor BeginPlay.
	 * @return Prewarm outcome; clients succeed without
	 * spawning local actors.
	 */
	virtual Nelaric::ObjectPool::FPoolResult PrewarmPools(UWorld& World) override;

	/// Invalidates all leases and closes the native pool on the game thread.
	FORCEINLINE virtual Nelaric::ObjectPool::FPoolResult ShutdownPools() override
	{
		return Pool.Shutdown();
	}

private:
	friend class ADemoInitialCharacterSpawnPoint;
	bool AreInitialCharactersReady(UPawnInitializationComponent*& PendingInitialization);

	FORCEINLINE bool CanUsePool() const
	{
		check(IsInGameThread());
		const UWorld* World = GetWorld();
		return World && World->HasBegunPlay() && !World->bIsTearingDown && World->GetNetMode() != NM_Client;
	}

	// Keep the Blueprint class reachable and referenced by the subsystem CDO.
	UPROPERTY()
	TSubclassOf<ADemoCharacter> CharacterClass;

	FPool Pool;
	bool bInitialCharactersReady = false;
};
