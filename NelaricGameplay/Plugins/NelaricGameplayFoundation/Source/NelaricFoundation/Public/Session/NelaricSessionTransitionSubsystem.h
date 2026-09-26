// Copyright (c) 2026 Nelaric

/** @file NelaricSessionTransitionSubsystem.h
 * Declares requests for an observed game-world network mode.
 */

#pragma once

#include "CoreTypes.h"
#include "Containers/Ticker.h"
#include "Core/NetWork/NelaricNetworkEndpoint.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineBaseTypes.h"
#include "Internal/InternalAccessKey.h"
#include "Misc/Optional.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtr.h"

#include "NelaricSessionTransitionSubsystem.generated.h"

class UWorld;

namespace Nelaric
{
/** @brief Game and approval endpoints of one destination server.
 *
 * @details The caller supplies both endpoints for the same selected
 * destination. GameEndpoint is used for travel; BeaconEndpoint is used
 * for pre-travel target approval.
 */
struct FTransitionDestination
{
	/// Destination game server address and port.
	FNetworkEndpoint GameEndpoint;

	/// Destination approval beacon address and port.
	FNetworkEndpoint BeaconEndpoint;
};

/// Starts approval for a request and its destination endpoints.
DECLARE_DELEGATE_RetVal_TwoParams(bool, FStartTransitionApproval, uint64, const FTransitionDestination&);

/// Releases transport state for a terminal request.
DECLARE_DELEGATE_OneParam(FOnTransitionTerminated, uint64);

/// Stable identity of one accepted transition request.
struct FTransitionHandle
{
	/// Zero is not an active request identity.
	uint64 Id = 0;
};

/// Reason a network-mode request could not complete.
enum class ETransitionError : uint8
{
	/// The target mode or source-to-target route is unsupported.
	UnsupportedMode,

	/// No game world is available for the request.
	NoWorld,

	/// A required map or remote destination is unavailable.
	MissingDestination,

	/// Another transition is already active.
	Busy,

	/// The source or destination authority denied the request.
	AuthorityRejected,

	/// A required authority could not be reached for confirmation.
	AuthorityUnavailable,

	/// A confirmation expired or no longer matches this request.
	ConfirmationInvalid,

	/// Map travel did not establish the requested world.
	TravelFailed,

	/// Network setup or connection failed.
	NetworkFailed,

	/// The resulting world has a different network mode.
	UnexpectedNetMode,
};

/// Reports a verified destination mode on the game thread.
DECLARE_DELEGATE_TwoParams(FOnTransitionSucceeded, FTransitionHandle, ENetMode);

/// Reports cancellation on the game thread.
DECLARE_DELEGATE_OneParam(FOnTransitionCancelled, FTransitionHandle);

/// Reports timeout on the game thread.
DECLARE_DELEGATE_OneParam(FOnTransitionTimedOut, FTransitionHandle);

/// Reports a transition failure on the game thread.
DECLARE_DELEGATE_TwoParams(FOnTransitionFailed, FTransitionHandle, ETransitionError);

/** @brief Terminal callbacks for one asynchronous mode request.
 *
 * @details Exactly one matching bound callback runs on the game thread.
 * An unbound matching callback is skipped. Each field is optional.
 */
struct FTransitionCallbacks
{
	/// Called after the destination connection passes verification.
	FOnTransitionSucceeded OnSucceeded;

	/// Called when the request is cancelled before completion.
	FOnTransitionCancelled OnCancelled;

	/// Called when the request exceeds its bounded timeout.
	FOnTransitionTimedOut OnTimedOut;

	/// Called when the request cannot complete.
	FOnTransitionFailed OnFailed;
};
} // namespace Nelaric

/** @brief Coordinates requests for a game world's network role.
 *
 * @details Unreal creates one instance per game instance and destroys
 * it when that game instance shuts down. Requests and callbacks use the
 * game thread. The subsystem observes modes; it does not assign them.
 * A request is only an intent until both relevant authorities confirm it.
 */
UCLASS(MinimalAPI)
class UNelaricSessionTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Requests a move from one remote server to another.
	 *
	 * @details Call on the game thread. This implementation accepts only
	 * NM_Client to NM_Client with a destination address. The current and
	 * target servers must each reply before client travel starts. A
	 * request has a 30-second deadline and exactly one terminal outcome.
	 * Success verifies a new client world with an open connection whose
	 * host and port match the destination. Other modes and missing local
	 * prerequisites return a zero handle without a callback.
	 *
	 * @param TargetMode Must be NM_Client for this implementation.
	 * @param Destination Game and approval beacon endpoints.
	 * @param Callbacks Optional terminal callbacks for this request.
	 * @return Accepted request identity, or zero if locally rejected.
	 */
	NELARICFOUNDATION_API Nelaric::FTransitionHandle
	RequestTransition(ENetMode TargetMode, const Nelaric::FTransitionDestination& Destination,
	                  const Nelaric::FTransitionCallbacks& Callbacks);

	/** @brief Requests cancellation of an active transition.
	 *
	 * @details Call on the game thread. Before travel starts, cancellation
	 * prevents the transition. Once travel starts, it cannot be cancelled
	 * through this handle. A terminal request reports no further callback.
	 *
	 * @param Handle Identity returned by RequestTransition.
	 * @return True if cancellation was requested for an active operation.
	 */
	NELARICFOUNDATION_API bool CancelTransition(Nelaric::FTransitionHandle Handle);

	/** @brief Reads the current game world's observed network mode.
	 *
	 * @details Call on the game thread. A value may describe an old or
	 * intermediate world during travel; it is not transition completion.
	 *
	 * @return Current mode, or an empty value if no game world exists.
	 */
	NELARICFOUNDATION_API TOptional<ENetMode> GetNetMode() const;

public:
	/** @brief Binds Core's approval transport on the game thread.
	 * @param Key Access granted to the Core transport subsystem.
	 * @param Source Begins approval by the source authority.
	 * @param Target Begins approval by the target authority.
	 * @param Terminated Releases transport state after a terminal result.
	 */
	NELARICFOUNDATION_API void InternalConfigureApprovalTransport(const Nelaric::FInternalAccessKey& Key,
	                                                              const Nelaric::FStartTransitionApproval& Source,
	                                                              const Nelaric::FStartTransitionApproval& Target,
	                                                              const Nelaric::FOnTransitionTerminated& Terminated);

	/** @brief Removes Core's approval transport on the game thread.
	 * @param Key Access granted to the Core transport subsystem.
	 */
	NELARICFOUNDATION_API void InternalClearApprovalTransport(const Nelaric::FInternalAccessKey& Key);

	/** @brief Reports the source authority's decision.
	 * @param Key Access granted to the Core transport subsystem.
	 * @param RequestId Identity of the request being approved.
	 * @param bApproved Whether the source authority approved departure.
	 */
	NELARICFOUNDATION_API void InternalReportSourceApproval(const Nelaric::FInternalAccessKey& Key, uint64 RequestId,
	                                                        bool bApproved);

	/** @brief Reports the target authority's decision.
	 * @param Key Access granted to the Core transport subsystem.
	 * @param RequestId Identity of the request being approved.
	 * @param bApproved Whether the target authority approved arrival.
	 */
	NELARICFOUNDATION_API void InternalReportTargetApproval(const Nelaric::FInternalAccessKey& Key, uint64 RequestId,
	                                                        bool bApproved);

	/** @brief Reports that the target authority could not be reached.
	 * @param Key Access granted to the Core transport subsystem.
	 * @param RequestId Identity of the request awaiting approval.
	 */
	NELARICFOUNDATION_API void InternalReportTargetUnavailable(const Nelaric::FInternalAccessKey& Key,
	                                                           uint64 RequestId);

protected:
	/// Registers the coordinator's tick while its game instance lives.
	NELARICFOUNDATION_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/// Ends active work before the game instance subsystem is destroyed.
	NELARICFOUNDATION_API virtual void Deinitialize() override;

private:
	struct FActiveTransition
	{
		Nelaric::FTransitionHandle Handle;
		Nelaric::FTransitionCallbacks Callbacks;
		Nelaric::FTransitionDestination Destination;
		TWeakObjectPtr<UWorld> SourceWorld;
		double DeadlineSeconds = 0.0;
		bool bSourceApproved = false;
		bool bTargetApproved = false;
		bool bTravelStarted = false;
	};

	bool Tick(float DeltaTime);
	void TryStartTravel();
	void Fail(Nelaric::ETransitionError Error);
	void FinishSucceeded(ENetMode Mode);
	void FinishTimedOut();
	void FinishCancelled();

	Nelaric::FStartTransitionApproval StartSourceApproval;
	Nelaric::FStartTransitionApproval StartTargetApproval;
	Nelaric::FOnTransitionTerminated OnTerminated;
	TUniquePtr<FActiveTransition> Active;
	FTSTicker::FDelegateHandle TickHandle;
	uint64 NextRequestId = 1;
};
