// Copyright (c) 2026 Nelaric

/** @file NelaricGameModeBase.h
 * Declares the project's default server-side game mode.
 */

#pragma once

#include "GameFramework/GameModeBase.h"

#include "NelaricGameModeBase.generated.h"

class AOnlineBeaconHost;
class ANelaricTransitionBeaconHost;

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
	NELARICFOUNDATION_API ANelaricGameModeBase();

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
	NELARICFOUNDATION_API bool CanAcceptTransition() const;

	/** @brief Starts the target approval beacon on an online server.
	 *
	 * @details Unreal calls this on the authoritative game thread.
	 */
	NELARICFOUNDATION_API virtual void StartPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AOnlineBeaconHost> BeaconHost;

	UPROPERTY(Transient)
	TObjectPtr<ANelaricTransitionBeaconHost> TransitionBeaconHost;
};
