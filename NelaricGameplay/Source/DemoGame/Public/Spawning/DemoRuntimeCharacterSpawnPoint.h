// Copyright (c) 2026 Nelaric Contributors

/** @file DemoRuntimeCharacterSpawnPoint.h
 * Declares an on-demand spawn area for world-owned demo character leases.
 */

#pragma once

#include "Engine/EngineTypes.h"
#include "Engine/TargetPoint.h"
#include "ObjectPool/DemoCharacterPoolSubsystem.h"
#include "TimerManager.h"
#include "UObject/ObjectPtr.h"

#include "DemoRuntimeCharacterSpawnPoint.generated.h"

class UBoxComponent;

/// Outcome of one synchronous runtime character spawn request.
UENUM(BlueprintType)
enum class EDemoRuntimeCharacterSpawnResult : uint8
{
	/// A new character lease was activated at a valid random area location.
	Spawned,
	/// The existing lease was resolved without acquiring another slot.
	AlreadySpawned,
	/// The request came from a client or an actor without authority.
	NotAuthority,
	/// World play, marker lifetime, pool storage, or startup GAS is not ready.
	NotReady,
	/// Every slot in the shared fixed character pool is leased.
	PoolFull,
	/// This marker or its pool is already performing a lifecycle operation.
	Busy,
	/// Character activation failed and its slot was returned to the pool.
	ActivationFailed,
	/// The pool cannot acquire a usable slot; inspect the diagnostic log.
	PoolUnavailable,
	/// The area, capsule, or ground clearance settings are invalid.
	InvalidConfiguration,
	/// No navigation data supports the character's agent settings.
	NavigationUnavailable,
	/// No points were cached, or every cached point is blocked.
	NoValidLocation,
};

/** @brief Acquires one current character at a randomly selected cached point.
 * @details Authority caches up to 32 valid
 * area locations during startup.
 * @note BeginPlay starts bounded initialization after Pawn/GAS readiness.
 * @note
 * Startup waiting expires after ten seconds of world time.
 * @note Spawn requests draw from the fixed cache and check
 * current blocking.
 * @note Call SpawnCharacter on the game thread after world startup.
 * @note Ground must be walkable and the full capsule must fit inside the box.
 * @note
 * BeginPlay does not acquire a character.
 * @note Repeated calls resolve the same lease until gameplay returns it.
 * @note Characters remain upright at unit scale, facing the area's world yaw.
 *
 * @note Clients receive characters through normal actor replication.
 * @note World-owned leases may outlive this area;
 * requests do not queue.
 */
UCLASS(MinimalAPI, Blueprintable, meta = (DisplayName = "Demo Runtime Character Spawn Area"))
class ADemoRuntimeCharacterSpawnPoint : public ATargetPoint
{
	GENERATED_BODY()

public:
	/// Maximum distinct valid locations retained by each initialized area.
	static constexpr int32 MaxSpawnLocationCount = 32;

	/** @brief Box volume containing the full capsule at a valid spawn location.
	 * @details Half-extents default to 500, 500, 250 centimeters.
	 * @note Edit Box Extent, relative transform, or
	 * actor scale in the editor.
	 * @note The box neither collides nor changes navigation.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo|Spawning")
	TObjectPtr<UBoxComponent> SpawnArea;

	/** @brief Extra capsule clearance above the support plane in centimeters.
	 * @details Valid range is 0 through 50; defaults to 2.
	 * @note Placement accounts for floor slope and
	 * validates the full capsule.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo|Spawning",
	          meta = (ClampMin = "0.0", ClampMax = "50.0", Units = "cm"))
	float GroundClearance = 2.0f;

	/** @brief Team injected before a newly acquired character is activated.
	 * @details Equal IDs are friendly,
	 * different IDs hostile, and 255 is
	 * neutral. Defaults to 0, matching initial character spawn points.
	 *
	 * @note Changes apply to later acquisitions, not an already active lease.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo|Spawning", meta = (ExposeOnSpawn = "true"))
	uint8 TeamId = 0;

	/// Non-owning generation token used by the owning world's character pool.
	using FHandle = UDemoCharacterPoolSubsystem::FHandle;

	/** @brief Resolves or acquires this area's character on the game thread.
	 * @param[out] OutCharacter Borrowed
	 * character on Spawned or
	 * AlreadySpawned; null for every failure.
	 * @return Spawned for a new lease,
	 * AlreadySpawned for the current lease,
	 * or a failure code without a new lease.
	 * @note Call on authority after startup. NotReady and Busy may be retried.
	 * @note New leases draw from the available cached points in random order.
	 * @note Each point is checked for
	 * current blocking before acquisition.
	 * @note NoValidLocation never falls back to the actor origin.
	 * @note
	 * Existing dead characters remain the current lease until return.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Spawning", meta = (ExpandEnumAsExecs = "ReturnValue"))
	DEMOGAME_API EDemoRuntimeCharacterSpawnResult SpawnCharacter(ADemoCharacter*& OutCharacter);

	/** @brief Copies this area's fixed spawn locations on the game thread.
	 * @return Up to 32 world-space capsule
	 * centers, or an empty array before
	 * successful initialization, on clients, or after EndPlay.
	 * @note
	 * Editing the area after initialization does not rebuild its cache.
	 */
	UFUNCTION(BlueprintPure, Category = "Demo|Spawning")
	DEMOGAME_API TArray<FVector> GetSpawnLocations() const;

	/** @brief Resolves this marker's current lease on the game thread.
	 * @return Borrowed character, or null
	 * before acquisition, after return,
	 * on clients, after marker EndPlay, or while the pool is unavailable.
	 *
	 * @note A stale generation never resolves another marker's later lease.
	 */
	UFUNCTION(BlueprintPure, Category = "Demo|Spawning")
	DEMOGAME_API ADemoCharacter* GetSpawnedCharacter() const;

	/** @brief Returns this marker's token for gameplay-managed pool return.
	 * @details Game thread only. Empty
	 * before acquisition and after EndPlay.
	 * @note Validate with the world pool; return and shutdown expire
	 * tokens.
	 */
	FORCEINLINE FHandle GetSpawnedHandle() const
	{
		return SpawnedHandle;
	}

public:
	DEMOGAME_API ADemoRuntimeCharacterSpawnPoint(const FObjectInitializer& ObjectInitializer);
	DEMOGAME_API virtual void BeginPlay() override;
	DEMOGAME_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class ADemoGrandWarfrontGameMode;
	void InitializeSpawnLocations();
	void FinishSpawnLocationInitialization(EDemoRuntimeCharacterSpawnResult Result);
	EDemoRuntimeCharacterSpawnResult BuildSpawnLocations(const ADemoCharacter& CharacterTemplate);
	EDemoRuntimeCharacterSpawnResult FindSpawnTransform(FTransform& OutTransform) const;

	TArray<FVector> CachedSpawnLocations;
	FQuat CachedSpawnRotation = FQuat::Identity;
	FCollisionResponseContainer CachedCollisionResponses;
	ECollisionChannel CachedCollisionChannel = ECC_Pawn;
	float CachedCapsuleRadius = 0.0f;
	float CachedCapsuleHalfHeight = 0.0f;
	FTimerHandle LocationInitializationTimer;
	double LocationInitializationDeadline = 0.0;
	EDemoRuntimeCharacterSpawnResult LocationInitializationResult = EDemoRuntimeCharacterSpawnResult::NotReady;
	bool bLocationInitializationFinished = false;
	FHandle SpawnedHandle;
	bool bSpawning = false;
	bool bEndingPlay = false;
};
