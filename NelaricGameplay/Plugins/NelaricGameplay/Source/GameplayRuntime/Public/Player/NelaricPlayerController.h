// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricPlayerController.h
 * Declares the project's player-owned network endpoint.
 */

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "GameFramework/PlayerController.h"

#include "NelaricPlayerController.generated.h"

class APawn;

/// Player intent submitted to the authority for a control change.
UENUM(BlueprintType)
enum class EControlSwitchAction : uint8
{
	/// Ask to take control of a selected pawn.
	TakeControl,

	/// Ask to hand back control of the player's current pawn.
	ReturnControl,
};

/// Authority decision for one player control request.
UENUM(BlueprintType)
enum class EControlSwitchResult : uint8
{
	/// The authority handler completed the requested control change.
	Succeeded,

	/// No control-change implementation handled the request.
	NotHandled,

	/// The request action or identity is invalid, or authority is missing.
	InvalidRequest,

	/// The target is missing, being destroyed, or in another world.
	InvalidTarget,

	/// The authority's world is unavailable or is being torn down.
	WorldUnavailable,

	/// This player already has a request executing in an authority callback.
	Busy,
};

/// Reports a player control decision on the owning client's game thread.
DECLARE_MULTICAST_DELEGATE_FourParams(FControlSwitchDecision, int32, EControlSwitchAction, APawn*,
                                      EControlSwitchResult);

/// Reports the current server's departure decision on the game thread.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDepartureDecision, uint64, const FString&, bool);

/** @brief Base player controller for NelaricGameplay worlds.
 *
 * @details The owning client and current server each have an instance
 * while connected. Game-specific controllers may derive from this class.
 * The current server approves a valid departure request immediately.
 * Destination admission uses a separate pre-travel connection.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** @brief Sends a player's request to take control of a selected pawn.
	 *
	 * @details Call on the game thread from the owning local player. The
	 * authority handles the intent and reports through OnControlSwitchDecision.
	 * Sending alone does not change possession. Requests cannot be cancelled
	 * through this transport API. Invalid local input sends no request.
	 *
	 * @param TargetPawn Valid selected pawn in this controller's world.
	 * @return Positive request ID, or zero if the request was not sent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API int32 RequestTakeControl(APawn* TargetPawn);

	/** @brief Sends a player's request to hand back current pawn control.
	 *
	 * @details Call on the owning local player's game thread. The authority
	 * resolves the current pawn; the caller cannot nominate another player.
	 * This only sends intent and does not assign an AI replacement.
	 * Requests cannot be cancelled through this transport API.
	 *
	 * @return Positive request ID, or zero if the request was not sent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API int32 RequestReturnControl();

	/** @brief Observes authority decisions for this player's requests.
	 *
	 * @details Bind on the owning client's game thread. The controller owns
	 * the delegate. Decisions include request ID, action, target and result.
	 * The target is null for return requests and may be null if destroyed.
	 * Bindings end with the controller. Delivery requires a live connection.
	 *
	 * @return Game-thread delegate reporting control request decisions.
	 */
	GAMEPLAYRUNTIME_API FControlSwitchDecision& OnControlSwitchDecision();

	/** @brief Asks the current server to approve leaving its session.
	 *
	 * @details Call on the game thread from the owning local controller.
	 * The server directly approves a valid request and returns its ID and
	 * destination through OnDepartureDecision on the game thread. Sending
	 * a request does not authorize travel or destination admission.
	 *
	 * @param RequestId Identity of the pending transition; must be nonzero.
	 * @param TargetAddress Destination server URL or host address.
	 * @return True if the request was sent, false for invalid local input.
	 */
	GAMEPLAYRUNTIME_API bool RequestDepartureApproval(uint64 RequestId, const FString& TargetAddress);

	/** @brief Observes decisions returned by the current server.
	 *
	 * @details Bind on the game thread. The controller owns the delegate;
	 * bindings cease to be useful when its connection or world is torn down.
	 *
	 * @return Delegate reporting identity, destination, and approval.
	 */
	GAMEPLAYRUNTIME_API FDepartureDecision& OnDepartureDecision();

public:
	// Native subclasses in other modules need these virtual definitions.
	GAMEPLAYRUNTIME_API virtual void ServerRequestDepartureApproval_Implementation(uint64 RequestId,
	                                                                               const FString& TargetAddress);
	GAMEPLAYRUNTIME_API virtual void
	ClientReceiveDepartureDecision_Implementation(uint64 RequestId, const FString& TargetAddress, bool bApproved);

protected:
	/** @brief Handles validated control intent from this controller's player.
	 *
	 * @details Runs synchronously on the authority's game thread. Override
	 * to apply gameplay approval and execute the control change. The player
	 * making the request is this controller. The default returns NotHandled
	 * and leaves control unchanged. Nested requests return Busy.
	 *
	 * @param Action Whether to take selected control or return current control.
	 * @param TargetPawn Valid target for TakeControl; null for ReturnControl.
	 * @return Authority decision delivered to the requesting player.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Nelaric|Control")
	EControlSwitchResult HandleControlSwitchRequest(EControlSwitchAction Action, APawn* TargetPawn);
	/// Implements the authority handler; defaults to NotHandled.
	GAMEPLAYRUNTIME_API virtual EControlSwitchResult
	HandleControlSwitchRequest_Implementation(EControlSwitchAction Action, APawn* TargetPawn);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestControlSwitch(int32 RequestId, EControlSwitchAction Action, APawn* TargetPawn);
	void ServerRequestControlSwitch_Implementation(int32 RequestId, EControlSwitchAction Action, APawn* TargetPawn);

	UFUNCTION(Client, Reliable)
	void ClientReceiveControlSwitchDecision(int32 RequestId, EControlSwitchAction Action, APawn* TargetPawn,
	                                        EControlSwitchResult Result);
	void ClientReceiveControlSwitchDecision_Implementation(int32 RequestId, EControlSwitchAction Action,
	                                                       APawn* TargetPawn, EControlSwitchResult Result);

	UFUNCTION(Server, Reliable)
	void ServerRequestDepartureApproval(uint64 RequestId, const FString& TargetAddress);

	UFUNCTION(Client, Reliable)
	void ClientReceiveDepartureDecision(uint64 RequestId, const FString& TargetAddress, bool bApproved);

	FDepartureDecision DepartureDecision;
	FControlSwitchDecision ControlSwitchDecision;
	int32 NextControlRequestId = 1;
	bool bHandlingControlRequest = false;
	int32 SendControlSwitchRequest(EControlSwitchAction Action, APawn* TargetPawn);
	EControlSwitchResult EvaluateControlSwitchRequest(int32 RequestId, EControlSwitchAction Action, APawn* TargetPawn);
};
