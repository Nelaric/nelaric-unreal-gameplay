// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricGameModeBase.h
 * Declares the project's default server-side game mode.
 */

#pragma once

#include "GameFramework/GameModeBase.h"
#include "Player/NelaricPlayerController.h"

#include "NelaricGameModeBase.generated.h"

class AOnlineBeaconHost;
class ATransitionBeaconHost;
class AController;

/** @brief Base game mode for NelaricGameplay worlds.
 *
 * @details The server owns this actor for its world lifetime. It selects
 * ANelaricPlayerController by default and hosts destination approval
 * requests. Access game mode state on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	/// Selects the project's player controller for this game mode.
	GAMEPLAYRUNTIME_API ANelaricGameModeBase();

	/// Port on which this server listens for transition approval beacons.
	UPROPERTY(EditDefaultsOnly, Category = "Nelaric|Transition", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 TransitionBeaconListenPort = 15000;

	/** @brief Reports whether this server can accept a transition.
	 *
	 * @details Call on the authoritative game thread. This temporary
	 * implementation returns true until the active WorldStartupConfig
	 * can be retrieved and its MaxPlayers policy applied.
	 *
	 * @return True while destination capacity integration is pending.
	 */
	GAMEPLAYRUNTIME_API bool CanAcceptTransition() const;

	/** @brief Applies game-specific approval to a pawn control request.
	 *
	 * @details Called synchronously on the authority game thread after the
	 * coordinator checks pawn eligibility and participant state. Override in
	 * C++ or Blueprint for team, distance, life-state, or permission rules.
	 * The default approves. This predicate must not change possession or
	 * destroy participants. Structural checks run again after it returns.
	 *
	 * @param Requester Controller authenticated by the request transport.
	 * @param Action Take or return control.
	 * @param TargetPawn Selected pawn, or current pawn for a return request.
	 * @return Whether gameplay rules approve this change.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API bool CanChangePawnControl(AController* Requester, EControlSwitchAction Action,
	                                              APawn* TargetPawn) const;

public:
	GAMEPLAYRUNTIME_API virtual bool
	CanChangePawnControl_Implementation(AController* Requester, EControlSwitchAction Action, APawn* TargetPawn) const;

	GAMEPLAYRUNTIME_API virtual void StartPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AOnlineBeaconHost> BeaconHost;

	UPROPERTY(Transient)
	TObjectPtr<ATransitionBeaconHost> TransitionBeaconHost;
};
