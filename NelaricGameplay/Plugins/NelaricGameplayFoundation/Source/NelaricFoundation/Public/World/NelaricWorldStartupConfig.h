// Copyright (c) 2026 Nelaric

/** @file NelaricWorldStartupConfig.h
 * Declares the map and authored policy selected before starting a world.
 */

#pragma once

#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"

#include "NelaricWorldStartupConfig.generated.h"

class UWorld;

/// Determines how a local activity begins after its player gate is met.
UENUM(BlueprintType)
enum class ENelaricActivityStartPolicy : uint8
{
	/// Gameplay code explicitly starts the activity.
	Manual,

	/// The activity starts when its minimum participant count is met.
	WhenMinimumReached,
};

/// Determines whether a running local activity accepts participants.
UENUM(BlueprintType)
enum class ENelaricActivityJoinInProgressPolicy : uint8
{
	/// New participants cannot enter after the activity starts.
	Reject,

	/// New participants can enter while the activity is running.
	Participate,
};

/** @brief Map and authored player and activity policy for a gameplay world.
 *
 * @details A consuming project creates this asset and selects it before
 * loading the referenced map. Activity fields provide defaults for local
 * activities. The asset stores startup policy, not replicated runtime
 * state. Authority-side gameplay code validates and applies it on the
 * game thread. The startup owner retains the asset for the world lifetime.
 */
UCLASS(MinimalAPI, BlueprintType)
class UNelaricWorldStartupConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/// Map to load after selecting and validating this startup config.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World|Startup", meta = (AssetBundles = "World"))
	TSoftObjectPtr<UWorld> WorldMap;

	/** @brief Players required to activate world gameplay; 0 has no gate.
	 *
	 * @details The world can load and admit players before this threshold.
	 * Falling below it later does not deactivate gameplay automatically.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World|Players", meta = (ClampMin = "0"))
	int32 MinPlayersToActivate = 0;

	/// Maximum admitted players; 0 adds no framework limit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World|Players", meta = (ClampMin = "0"))
	int32 MaxPlayers = 0;

	/// Whether the world initially accepts new players.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World|Players")
	bool bInitiallyAcceptingPlayers = true;

	/// Whether players may join after world gameplay activates.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World|Players")
	bool bAllowJoinAfterActivation = true;

	/** @brief Default participants required to start an activity.
	 *
	 * @details Zero removes the start gate. This does not define what
	 * happens if participants leave after the activity starts.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity|Participants", meta = (ClampMin = "0"))
	int32 MinParticipantsToStart = 0;

	/// Default activity capacity; 0 adds no activity-specific limit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity|Participants", meta = (ClampMin = "0"))
	int32 MaxParticipants = 0;

	/// Whether gameplay code or the minimum count triggers activity start.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity|Lifecycle")
	ENelaricActivityStartPolicy StartPolicy = ENelaricActivityStartPolicy::Manual;

	/// Whether a running activity admits new participants by default.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity|Participants")
	ENelaricActivityJoinInProgressPolicy JoinInProgressPolicy = ENelaricActivityJoinInProgressPolicy::Reject;

	/** @brief Checks the authored world player-count limits.
	 *
	 * @details Call on the game thread before applying this asset. Server
	 * capacity requires separate validation.
	 *
	 * @return False for negative values or a positive maximum below the
	 * minimum; true otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|World")
	NELARICFOUNDATION_API bool HasValidPlayerLimits() const;

	/** @brief Checks the authored activity participant-count limits.
	 *
	 * @details Call on the game thread before applying activity defaults.
	 * World admission and server capacity require separate validation.
	 *
	 * @return False for negative values or a positive maximum below the
	 * minimum; true otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Activity")
	NELARICFOUNDATION_API bool HasValidActivityParticipantLimits() const;

	/** @brief Checks the complete authored startup configuration.
	 *
	 * @details Call on the game thread before loading the map. This checks
	 * the map reference and policy values, but not whether the map can be
	 * loaded or whether server capacity can accommodate the policy.
	 *
	 * @return True if the map reference and authored policy are valid.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|World")
	NELARICFOUNDATION_API bool HasValidStartupConfig() const;
};
