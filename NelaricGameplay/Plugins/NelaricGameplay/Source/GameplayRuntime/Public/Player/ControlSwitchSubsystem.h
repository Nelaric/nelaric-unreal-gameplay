// Copyright (c) 2026 Nelaric Contributors

/** @file ControlSwitchSubsystem.h
 * Declares the authority world's control-request coordinator.
 */

#pragma once

#include "Player/NelaricPlayerController.h"
#include "Subsystems/WorldSubsystem.h"

#include "ControlSwitchSubsystem.generated.h"

class AController;

/** @brief Validates and executes pawn control changes on the authority.
 *
 * @details The world owns this subsystem. Structural checks and explicit
 * pawn eligibility precede the game mode's gameplay policy. Player-owned
 * pawns cannot be stolen. Operations are synchronous on the game thread;
 * nested changes are rejected across all participants in this world.
 * Possession failure attempts recovery without stealing unrelated control.
 */
UCLASS(MinimalAPI)
class UControlSwitchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Validates policy and changes a controller's pawn on authority.
	 *
	 * @details Call on the authority game thread. The controller is the
	 * requesting participant; never use a client-supplied requester identity.
	 * TakeControl switches from the current pawn when necessary. ReturnControl
	 * uses the current pawn. Pawn components configure bot handback. Failure
	 * before execution keeps existing possession. Callbacks must not perform
	 * competing direct possession changes during this operation.
	 *
	 * @param Requester Live controller and player state in this world.
	 * @param Action Requested control operation.
	 * @param TargetPawn Selected pawn for TakeControl; null for ReturnControl.
	 * @return Structural, policy, execution, or recovery result.
	 */
	GAMEPLAYRUNTIME_API EControlSwitchResult ExecuteControlSwitch(AController* Requester, EControlSwitchAction Action,
	                                                              APawn* TargetPawn);

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	bool bExecutingControlSwitch = false;
	EControlSwitchResult ValidateRequest(AController* Requester, EControlSwitchAction Action, APawn* TargetPawn) const;
};
