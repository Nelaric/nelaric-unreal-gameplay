// Copyright (c) 2026 Nelaric

/** @file NelaricWorldConfig.h
 * Declares authored world and activity participation policy.
 */

#pragma once

#include "Engine/DataAsset.h"

#include "NelaricWorldConfig.generated.h"

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

/** @brief Authored player and activity policy for a gameplay world.
 *
 * @details A consuming project creates this asset and selects it for a
 * world. Activity fields provide defaults for local activities in that
 * world. The asset stores policy, not replicated runtime state. It does
 * not admit players, activate gameplay, or start activities by itself.
 * Authority-side gameplay code reads and applies it on the game thread.
 */
UCLASS(BlueprintType)
class NELARICFOUNDATION_API UNelaricWorldConfig : public UDataAsset
{
	GENERATED_BODY()

public:
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
	bool HasValidPlayerLimits() const;

	/** @brief Checks the authored activity participant-count limits.
	 *
	 * @details Call on the game thread before applying activity defaults.
	 * World admission and server capacity require separate validation.
	 *
	 * @return False for negative values or a positive maximum below the
	 * minimum; true otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Activity")
	bool HasValidActivityParticipantLimits() const;
};
