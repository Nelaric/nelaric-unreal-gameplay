// Copyright (c) 2026 Nelaric Contributors

/** @file ControlSwitchSubsystem.h
 * Declares the authority world's control-request coordinator.
 */

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/Guid.h"
#include "Player/NelaricPlayerController.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "ControlSwitchSubsystem.generated.h"

class AActor;
class AController;

/** @brief Validates and executes pawn control changes on the authority.
 *
 * @details The world owns this subsystem. Structural checks and explicit
 * pawn eligibility precede the game mode's gameplay policy. Player-owned
 * pawns cannot be stolen. Operations are synchronous on the game thread;
 * nested changes are rejected across all participants in this world.
 * Pawns, controllers and player states are reserved before game callbacks.
 * Failed recovery keeps reservations until repaired control is verified.
 * Possession failure attempts recovery without stealing unrelated control.
 */
UCLASS(MinimalAPI)
class UControlSwitchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Checks whether an actor is reserved by a control transition.
	 *
	 * @details Call on the authority game thread. Includes preparation,
	 * possession, Ready callbacks and recovery. Failed recovery remains
	 * reserved. The query reads coordinator state and does not replicate it.
	 *
	 * @param Actor Pawn, controller or player state in this world.
	 * @return True while reserved; false for invalid or foreign actors.
	 */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API bool IsControlTransitionInProgress(const AActor* Actor) const;

	/** @brief Checks whether an actor's control transition needs repair.
	 *
	 * @details Call on the authority game thread. A failed restoration keeps
	 * surviving participants reserved until explicit recovery resolution.
	 *
	 * @param Actor Pawn, controller or player state in this world.
	 * @return True if its reserved transition failed to restore control.
	 */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API bool IsControlTransitionRecoveryRequired(const AActor* Actor) const;

	/** @brief Releases a failed transition after gameplay repairs control.
	 *
	 * @details Call on the authority game thread after repairing or removing
	 * affected actors. Checks surviving reserved actors, possession and
	 * pawn readiness before releasing the entire transition. Performs no
	 * possession. Active callbacks cannot resolve their transition.
	 *
	 * @param Actor Surviving actor reserved by the failed transition.
	 * @return True if control is settled and reservations are released.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API bool ResolveControlTransitionRecovery(const AActor* Actor);

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
	virtual void Deinitialize() override;

private:
	bool bExecutingControlSwitch = false;
	TMap<TWeakObjectPtr<const AActor>, FGuid> ControlTransitions;
	TSet<FGuid> RecoveryRequiredTransitions;
	bool HasConflictingTransition(const AActor* Actor, const FGuid& TransitionId) const;
	bool ReserveController(AController* Controller, const FGuid& TransitionId);
	void ReleaseTransition(const FGuid& TransitionId);
	void PruneDestroyedParticipants();
	EControlSwitchResult ValidateRequest(AController* Requester, EControlSwitchAction Action, APawn* TargetPawn,
	                                     const FGuid& TransitionId) const;
};
