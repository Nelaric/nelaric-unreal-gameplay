// Copyright (c) 2026 Nelaric Contributors

/** @file FixedObjectPoolWorldSubsystem.h
 * Coordinates native fixed pools with the world lifecycle.
 */

#pragma once

#include "Delegates/Delegate.h"
#include "ObjectPool/FixedUObjectPool.h"
#include "Subsystems/WorldSubsystem.h"

#include "FixedObjectPoolWorldSubsystem.generated.h"

/** @brief Prewarms native pools and closes them before world actor teardown.
 * @details Unreal owns one instance of each concrete subclass per world.
 * @note Concrete subclasses hold their typed pools as native members.
 * @note Supports Game and PIE worlds; all calls run on the game thread.
 * @note Pool callbacks must not end the world or deinitialize this service.
 */
UCLASS(MinimalAPI, Abstract)
class UFixedObjectPoolWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Reads the result of the world's single prewarm attempt.
	 * @details Game thread only; returns NotReady before prewarm starts.
	 * @note The result stays available after shutdown for diagnostics.
	 * @note Prewarm success does not imply that actor BeginPlay has finished.
	 * @return The startup outcome; query the concrete pool for current state.
	 */
	FORCEINLINE Nelaric::ObjectPool::FPoolResult GetPrewarmResult() const
	{
		check(IsInGameThread());
		return PrewarmResult;
	}

public:
	GAMEPLAYRUNTIME_API UFixedObjectPoolWorldSubsystem();
	GAMEPLAYRUNTIME_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	GAMEPLAYRUNTIME_API virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	GAMEPLAYRUNTIME_API virtual void Deinitialize() override;

protected:
	/** @brief Creates every object in the concrete subsystem's typed pools.
	 * @details Called once on the game thread before actor BeginPlay.
	 * @note Failure must roll back created objects; there is no retry.
	 * @param World Owning world, valid throughout this synchronous operation.
	 * @return The prewarm outcome reported by the owned pools.
	 */
	virtual Nelaric::ObjectPool::FPoolResult PrewarmPools(UWorld& World)
	    PURE_VIRTUAL(UFixedObjectPoolWorldSubsystem::PrewarmPools, return {Nelaric::ObjectPool::EPoolError::NotReady};);

	/** @brief Invalidates leases and closes every owned pool.
	 * @details Called on the game thread at world teardown, or deinitialize.
	 * @note Must be safe before prewarm and outside lifecycle callbacks.
	 * @return Success; failure indicates a lifecycle contract violation.
	 */
	virtual Nelaric::ObjectPool::FPoolResult ShutdownPools()
	    PURE_VIRTUAL(UFixedObjectPoolWorldSubsystem::ShutdownPools,
	                 return {Nelaric::ObjectPool::EPoolError::NotReady};);

	/** @brief Restricts pool owners to Game and PIE worlds.
	 * @param WorldType Engine world category considered during creation.
	 * @return Whether this world type supports gameplay pool ownership.
	 */
	GAMEPLAYRUNTIME_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleWorldBeginTearDown(UWorld* World);
	void ShutdownOwnedPools();

	FDelegateHandle WorldBeginTearDownHandle;
	Nelaric::ObjectPool::FPoolResult PrewarmResult{Nelaric::ObjectPool::EPoolError::NotReady};
	bool bPrewarmAttempted = false;
	bool bShuttingDown = false;
};
