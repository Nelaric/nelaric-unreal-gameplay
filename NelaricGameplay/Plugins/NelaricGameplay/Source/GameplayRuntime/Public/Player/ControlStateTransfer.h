// Copyright (c) 2026 Nelaric Contributors

/** @file ControlStateTransfer.h Declares synchronous state export and
 * control association contracts.
 */

#pragma once

#include "Containers/Array.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AController;
class APawn;
class APlayerState;

namespace Nelaric::Control
{
/// Selects the original or proposed endpoint of a state transfer.
enum class EAssociationEndpoint : uint8
{
	/// The binding captured before possession changes.
	Source,

	/// The binding proposed by the validated control plan.
	Destination,
};

/// Non-owning controller and player state for one control association.
struct FStateAssociation
{
	/// Controller at this endpoint; explicitly null for unpossessed control.
	TWeakObjectPtr<AController> Controller;

	/// State at this endpoint; explicitly null when no state is associated.
	TWeakObjectPtr<APlayerState> PlayerState;
};

/** @brief Identifies one pawn's export and proposed association change.
 *
 * @details References are non-owning and must be revalidated before use.
 * The coordinator supplies this context on the authority game thread.
 */
struct FStateTransferContext
{
	/// Reservation identity shared by all pawns in this operation.
	FGuid TransitionId;

	/// Pawn whose state is exported; this identity never changes.
	TWeakObjectPtr<APawn> Pawn;

	/// Original controller and player state.
	FStateAssociation Source;

	/// Proposed controller and its existing player state.
	FStateAssociation Destination;
};

/** @brief Base for integration-owned, immutable exported state.
 *
 * @details Derive to store attributes or other typed domain data. The
 * coordinator retains snapshots through association callbacks and failed
 * recovery. Use weak UObject references or explicitly GC-safe ownership.
 * Access and release snapshots on the game thread, including retained
 * exports.
 */
struct FStateSnapshot
{
	/// Non-empty identity of the integration's payload layout.
	FName SchemaId;

	/// Positive payload layout version understood by this integration.
	uint32 SchemaVersion = 0;

	/// Destroys integration-owned data through its concrete snapshot type.
	virtual ~FStateSnapshot() = default;
};

/** @brief Exports pawn state and rebinds an optional gameplay
 * integration.
 *
 * @details Register with the pawn's control component before switching.
 * All calls run synchronously on the authority game thread under
 * complete reservations. Export is read-only. Association methods change
 * bindings, not attributes, effects or grants; state import is a
 * separate operation. Calls must not possess actors, replace
 * registrations or publish gameplay. Store UObject references weakly or
 * with GC-safe ownership. Failed calls may have partially changed a
 * binding and must support source restoration.
 */
class IStateTransferParticipant
{
public:
	/// Releases the integration when its registration and transfers end.
	virtual ~IStateTransferParticipant() = default;

	/** @brief Captures state while the original association is still intact.
	 *
	 * @details Must not mutate gameplay or retain the output reference.
	 * Return a non-null immutable snapshot, including when the payload is
	 * empty.
	 *
	 * @param Context Original and proposed bindings for this pawn.
	 *
	 * @param[out] OutSnapshot Independently owned state; ignored on failure.
	 *
	 * @return True if the complete export succeeded, false otherwise.
	 */
	virtual bool ExportState(const FStateTransferContext& Context,
	                         TSharedPtr<const FStateSnapshot>& OutSnapshot) const = 0;

	/** @brief Releases only bindings owned by the selected association.
	 *
	 * @details All source bindings detach before any destination attaches. A
	 * failed destination attach is detached before source recovery. Never
	 * clear a newer, unrelated binding. Must tolerate an absent owned
	 * binding.
	 *
	 * @param Context Fixed endpoints and transition identity.
	 *
	 * @param Endpoint Binding being released.
	 *
	 * @param Snapshot State exported by this participant.
	 *
	 * @return Whether the selected association was safely released.
	 */
	virtual bool DetachAssociation(const FStateTransferContext& Context, EAssociationEndpoint Endpoint,
	                               const FStateSnapshot& Snapshot) = 0;

	/** @brief Establishes a binding after native possession has changed.
	 *
	 * @details Runs before pawn Ready callbacks. Source selects recovery;
	 * destination selects forward execution. Explicitly null endpoints must
	 * be supported or rejected during export. Repeated restoration must be
	 * safe. This method must not import or clear gameplay state.
	 *
	 * @param Context Fixed endpoints and transition identity.
	 *
	 * @param Endpoint Binding to establish.
	 *
	 * @param Snapshot State exported by this participant.
	 *
	 * @return Whether the selected association was established.
	 */
	virtual bool AttachAssociation(const FStateTransferContext& Context, EAssociationEndpoint Endpoint,
	                               const FStateSnapshot& Snapshot) = 0;

	/** @brief Checks the integration binding without changing gameplay.
	 *
	 * @details Called after attach and Ready callbacks, and during explicit
	 * recovery resolution. Never invokes competing control operations.
	 *
	 * @param Context Fixed endpoints and transition identity.
	 *
	 * @param Endpoint Expected current association.
	 *
	 * @param Snapshot State exported by this participant.
	 *
	 * @return Whether the binding still matches the selected endpoint.
	 */
	virtual bool IsAssociationValid(const FStateTransferContext& Context, EAssociationEndpoint Endpoint,
	                                const FStateSnapshot& Snapshot) const = 0;
};

/// Named integration registered with a pawn's control component.
struct FStateParticipantRegistration
{
	/// Unique, non-empty identity within this pawn's registrations.
	FName Id;

	/// Shared integration lifetime; the component owns the registration.
	TSharedPtr<IStateTransferParticipant> Participant;
};

/// One participant's successful state export.
struct FParticipantStateExport
{
	/// Registration identity that produced this snapshot.
	FName Id;

	/// Immutable integration-owned state.
	TSharedPtr<const FStateSnapshot> Snapshot;
};

/** @brief Complete state export for one pawn in a control operation.
 *
 * @details Published only after every pawn exports successfully.
 * Available during association callbacks and failed recovery; not
 * replicated. Consumers may retain this value, but actor references
 * remain non-owning.
 */
struct FControlStateExport
{
	/// Original and proposed associations for the exported pawn.
	FStateTransferContext Context;

	/// Authority world time in seconds when this pawn's export began.
	double ExportWorldTime = 0.0;

	/// Successful snapshots in registration order; empty without integrations.
	TArray<FParticipantStateExport> States;
};
} // namespace Nelaric::Control
