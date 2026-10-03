// Copyright (c) 2026 Nelaric Contributors

/** @file DemoInitialCharacterSpawnPoint.h
 * Declares a placed marker that acquires one demo character at startup.
 */

#pragma once

#include "Engine/TargetPoint.h"
#include "ObjectPool/DemoCharacterPoolSubsystem.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtr.h"

#include "DemoInitialCharacterSpawnPoint.generated.h"

class UPawnInitializationComponent;

/** @brief Acquires one world-owned demo character at this marker's transform.
 * @details Authority waits for initial pawn and GAS readiness, then tries
 * one acquisition outside initialization callbacks.
 * @note Startup waiting times out after ten seconds of world time.
 * @note Clients receive the character through normal actor replication.
 * @note Failure logs a warning and leaves this marker empty without retry.
 * @note Gameplay returns the lease; world teardown closes the owning pool.
 * @note All reads and lifecycle operations run on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoInitialCharacterSpawnPoint : public ATargetPoint
{
	GENERATED_BODY()

public:
	/// Native token type used by the owning world's fixed character pool.
	using FHandle = UDemoCharacterPoolSubsystem::FHandle;

	/** @brief Resolves this marker's current character lease on the game thread.
	 * @return Borrowed character, or
	 * null before acquisition, after return,
	 * on clients, or while the owning world pool is unavailable.
	 */
	UFUNCTION(BlueprintPure, Category = "Demo|Spawning")
	DEMOGAME_API ADemoCharacter* GetSpawnedCharacter() const;

	/** @brief Returns this point's token for gameplay-managed pool return.
	 * @details Game thread only. Empty
	 * until successful acquisition.
	 * @note Validate with the world pool; tokens expire at world shutdown.
	 */
	FORCEINLINE FHandle GetSpawnedHandle() const
	{
		return SpawnedHandle;
	}

public:
	DEMOGAME_API ADemoInitialCharacterSpawnPoint(const FObjectInitializer& ObjectInitializer);
	DEMOGAME_API virtual void BeginPlay() override;
	DEMOGAME_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SpawnInitialCharacter();
	void WaitForInitialCharacters(UPawnInitializationComponent* Initialization);
	void ClearInitializationSubscription();
	void StopWaitingForInitialCharacters();
	void HandleReadinessTimeout();
	UFUNCTION()
	void HandleCharacterInitialized(UPawnInitializationComponent* Initialization);

	FTimerHandle SpawnTimer;
	FTimerHandle ReadinessTimeoutTimer;
	TWeakObjectPtr<UPawnInitializationComponent> PendingInitialization;
	FHandle SpawnedHandle;
	bool bSpawnAttempted = false;
	bool bEndingPlay = false;
	bool bReadinessWaitStarted = false;
};
