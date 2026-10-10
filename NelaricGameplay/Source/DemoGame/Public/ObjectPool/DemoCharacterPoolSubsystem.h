// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCharacterPoolSubsystem.h
 * Declares world ownership and automatic prewarm for demo character leases.
 */

#pragma once

#include "Character/DemoCharacter.h"
#include "Containers/StaticArray.h"
#include "ObjectPool/DemoCharacterPoolPolicy.h"
#include "ObjectPool/FixedObjectPoolWorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"

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
	static constexpr uint32 Capacity = 512;
	/// Raw fixed storage; the subsystem exclusively controls actor destruction.
	using FPool = Nelaric::ObjectPool::TFixedUObjectPool<ADemoCharacter, Capacity, Nelaric::Demo::FCharacterPoolPolicy,
	                                                     Nelaric::ObjectPool::EReferenceMode::WorldRaw,
	                                                     Nelaric::ObjectPool::ENetworkMode::Replicated>;
	/// Non-owning object and generation handle; does not return automatically.
	using FLease = FPool::FLease;
	/// Non-owning lease token; invalid after return or world teardown.
	using FHandle = FPool::FHandle;

	/** @brief Activates one prewarmed demo character on the game thread.
	 * @param Transform World placement chosen by the caller without a sweep.
	 * @param TeamId Faction assigned before activation; 255 is neutral.
	 * @return Lease, or an error; NotReady on clients or outside world play.
	 * @note Acquisition never grows the
	 * pool or replaces missing characters.
	 */
	[[nodiscard]] FORCEINLINE FLease TryAcquire(const FTransform& Transform, uint8 TeamId = 255)
	{
		if (!CanUsePool())
		{
			return {nullptr, {}, {Nelaric::ObjectPool::EPoolError::NotReady}};
		}
		const FLease Lease = Pool.TryAcquire({Transform, TeamId});
		if (Lease)
		{
			ActiveHandles[Lease.Handle.Index] = Lease.Handle;
		}
		return Lease;
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
		const Nelaric::ObjectPool::FPoolResult Result = Pool.Release(Handle);
		if (Result || Result.Error == Nelaric::ObjectPool::EPoolError::LostObject)
		{
			ActiveHandles[Handle.Index] = {};
		}
		return Result;
	}

	/** @brief Returns a dead character's current lease on the authority thread.
	 * @details Uses the recorded
	 * generation; player control must be released.
	 * No actor destruction occurs. New acquisitions reset dead
	 * combat state.
	 * @param Character Dead character belonging to this world and pool.
	 * @return InvalidHandle
	 * for a live, controlled, foreign or unleased actor;
	 * otherwise the normal lease release result.
	 */
	DEMOGAME_API Nelaric::ObjectPool::FPoolResult ReleaseDeadCharacter(ADemoCharacter* Character);

	/** @brief Resolves a lease while its world remains in play.
	 * @details Game thread only; the pointer is for immediate access.
	 * @param Handle Non-owning token from this still-live subsystem.
	 * @return Character, or null for an invalid lease or unavailable world.
	 */
	FORCEINLINE ADemoCharacter* Get(FHandle Handle) const
	{
		return CanUsePool() ? Pool.Get(Handle) : nullptr;
	}

	/** @brief Reads a local demo slot or an automatically received replica.
	 * @details Game thread only; clients may have unresolved empty slots.
	 * @param Index Slot index; out-of-range indices return null.
	 * @return Non-owning character, or null while unavailable or unresolved.
	 * @note Does not acquire a lease or verify its generation.
	 */
	FORCEINLINE ADemoCharacter* At(uint32 Index) const
	{
		return IsReady() ? Pool.At(Index) : nullptr;
	}

	/** @brief Reads a demo slot with the native pool's raw-access contract.
	 * @details Authority callers guarantee
	 * a ready pool and valid index.
	 * Client access resolves a weak replica and may return null.
	 * @param Index
	 * Slot in [0, Capacity); unchecked on authority.
	 * @return Non-owning character; does not acquire or verify a
	 * lease.
	 */
	FORCEINLINE ADemoCharacter* operator[](uint32 Index) const noexcept
	{
		return Pool[Index];
	}

	/// Returns local storage or client-view availability on the game thread.
	FORCEINLINE bool IsReady() const
	{
		const UWorld* World = GetWorld();
		return World && World->HasBegunPlay() && !World->bIsTearingDown && Pool.IsReady();
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
	friend class ADemoRuntimeCharacterSpawnPoint;
	bool AreInitialCharactersReady(UPawnInitializationComponent*& PendingInitialization);

	FORCEINLINE bool CanUsePool() const
	{
		check(IsInGameThread());
		const UWorld* World = GetWorld();
		return World && World->HasBegunPlay() && !World->bIsTearingDown && World->GetNetMode() != NM_Client;
	}

	// Reference the Blueprint for cooking without loading it during CDO creation.
	UPROPERTY()
	TSoftClassPtr<ADemoCharacter> CharacterClass;

	FPool Pool;
	TStaticArray<FHandle, Capacity> ActiveHandles{};
	bool bInitialCharactersReady = false;
};
