// Copyright (c) 2026 Nelaric

/** @file NelaricPawnComponent.h
 * Declares a base component for querying its owning pawn's context.
 */

#pragma once

#include "Components/ActorComponent.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/Casts.h"

#include "NelaricPawnComponent.generated.h"

class AController;
class APawn;
class APlayerController;
class APlayerState;

/** @brief Base for gameplay components owned directly by a pawn.
 *
 * @details The pawn owns the component and determines its lifetime.
 * Queries read current local state without caching or replication.
 * Call only on the game thread. Returned pointers are non-owning and
 * do not keep objects alive; revalidate references retained across frames.
 * Controller and player state availability is not guaranteed at BeginPlay.
 * Tick and component replication are disabled by default.
 */
UCLASS(Abstract, MinimalAPI, Blueprintable)
class UNelaricPawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** @brief Gets the pawn that directly owns this component.
	 *
	 * @details Call on the game thread. Does not traverse actor ownership.
	 * @return Non-owning pawn, or null for a missing, invalid, destroying,
	 * or non-pawn owner.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn")
	NELARICFOUNDATION_API APawn* GetPawn() const;

	/** @brief Gets the owning pawn as a specific pawn type.
	 *
	 * @details Call on the game thread. Uses the same lifetime as GetPawn().
	 * @tparam T Complete pawn type derived from APawn.
	 * @return Non-owning pawn, or null if unavailable or of another type.
	 */
	template <typename T> T* GetPawn() const
	{
		static_assert(TIsDerivedFrom<T, APawn>::IsDerived, "T must derive from APawn.");
		return Cast<T>(GetPawn());
	}

	/** @brief Gets the pawn's current controller on this machine.
	 *
	 * @details Call on the game thread. Unpossessed pawns and remote client
	 * pawns may have no controller. The result is never cached.
	 * @return Non-owning controller, or null if unavailable or destroying.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn")
	NELARICFOUNDATION_API AController* GetController() const;

	/** @brief Gets the current controller as a specific controller type.
	 *
	 * @details Call on the game thread. Follows GetController() lifetime.
	 * @tparam T Complete controller type derived from AController.
	 * @return Non-owning controller, or null if unavailable or mismatched.
	 */
	template <typename T> T* GetController() const
	{
		static_assert(TIsDerivedFrom<T, AController>::IsDerived, "T must derive from AController.");
		return Cast<T>(GetController());
	}

	/** @brief Gets the current controller when it is a player controller.
	 *
	 * @details Call on the game thread. Never searches by player index.
	 * @return Non-owning player controller, or null if unavailable,
	 * destroying, or controlled by a non-player controller.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn")
	NELARICFOUNDATION_API APlayerController* GetPlayerController() const;

	/** @brief Gets the player controller as a specific player controller.
	 *
	 * @details Call on the game thread. Follows GetController() lifetime.
	 * @tparam T Complete player controller type derived from APlayerController.
	 * @return Non-owning player controller, or null if unavailable or mismatched.
	 */
	template <typename T> T* GetPlayerController() const
	{
		static_assert(TIsDerivedFrom<T, APlayerController>::IsDerived, "T must derive from APlayerController.");
		return Cast<T>(GetPlayerController());
	}

	/** @brief Gets player state directly from the owning pawn.
	 *
	 * @details Call on the game thread. Does not require a local controller.
	 * State may be absent before replication arrives or after unpossession.
	 * @return Non-owning player state, or null if unavailable or destroying.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn")
	NELARICFOUNDATION_API APlayerState* GetPlayerState() const;

	/** @brief Gets the pawn's player state as a specific player state type.
	 *
	 * @details Call on the game thread. Follows GetPlayerState() lifetime.
	 * @tparam T Complete player state type derived from APlayerState.
	 * @return Non-owning player state, or null if unavailable or mismatched.
	 */
	template <typename T> T* GetPlayerState() const
	{
		static_assert(TIsDerivedFrom<T, APlayerState>::IsDerived, "T must derive from APlayerState.");
		return Cast<T>(GetPlayerState());
	}

	/** @brief Queries the pawn's native local-control semantics.
	 *
	 * @details Call on the game thread. Local control can include AI.
	 * @return The pawn's IsLocallyControlled() result, or false without a pawn.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn")
	NELARICFOUNDATION_API bool IsLocallyControlled() const;

	/** @brief Queries the pawn's native player-control semantics.
	 *
	 * @details Call on the game thread. Does not require a player controller
	 * to exist locally and does not imply local control or authority.
	 * @return The pawn's IsPlayerControlled() result, or false without a pawn.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn")
	NELARICFOUNDATION_API bool IsPlayerControlled() const;

public:
	NELARICFOUNDATION_API UNelaricPawnComponent(const FObjectInitializer& ObjectInitializer);
	NELARICFOUNDATION_API virtual void OnRegister() override;
};
