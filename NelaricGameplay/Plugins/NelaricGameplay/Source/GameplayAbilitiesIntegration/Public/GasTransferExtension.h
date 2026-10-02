// Copyright (c) 2026 Nelaric Contributors

/** @file GasTransferExtension.h Declares domain-specific transfer hooks. */
#pragma once
#include "Player/ControlStateTransfer.h"
class UNelaricAbilitySystemComponent;
struct FGameplayAbilitySpec;
struct FActiveGameplayEffect;
namespace Nelaric::GAS
{
/** @brief Preserves custom ability, task or effect state across ASCs.
 * @details Registered before control operations. Export is read-only;
 * release and restore are retry-safe and must not change possession.
 * Snapshots must retain UObject payloads through GC-safe ownership.
 * Calls run on the authority game thread. Settle irreversible actions
 * before export; restore never repeats committed costs or executions.
 */
class ITransferExtension
{
public:
	/** @brief Declares whether this adapter owns an active ability checkpoint.
	 * @details Read-only. Covered abilities must be settled by Release and
	 * restored with committed costs and tasks accounted for by Restore.
	 * @param Spec Active ability on the source ASC.
	 * @return Whether this adapter supplies complete execution migration.
	 */
	virtual bool HandlesAbility(const FGameplayAbilitySpec& Spec) const
	{
		return false;
	}
	/** @brief Declares custom effect restoration instead of container replay.
	 * @details Use for custom component state, special capture retargeting,
	 * or effects with domain-specific stacking and restoration side effects.
	 * @param Effect Active effect on the source ASC.
	 * @return Whether this adapter owns complete export and restoration.
	 */
	virtual bool HandlesEffect(const FActiveGameplayEffect& Effect) const
	{
		return false;
	}
	/** @brief Checks custom restored state after bindings and Ready callbacks.
	 * @param ASC Current endpoint ASC.
	 * @param Pawn Pawn whose state must be present.
	 * @param State Original custom export.
	 * @return Whether custom state is complete and valid.
	 */
	virtual bool IsRestored(UNelaricAbilitySystemComponent* ASC, APawn* Pawn,
	                        const Control::FStateSnapshot& State) const = 0;
	/** @brief Publishes custom state after the entire transfer settles.
	 * @details Must not fail, change possession or repeat committed results.
	 * @param ASC Committed endpoint ASC.
	 * @param Pawn Pawn with settled state.
	 * @param State Original export used for the committed restoration.
	 */
	virtual void Commit(UNelaricAbilitySystemComponent* ASC, APawn* Pawn, const Control::FStateSnapshot& State)
	{
	}
	/// Releases this extension after registrations and snapshots end.
	virtual ~ITransferExtension() = default;
	/** @brief Exports complete custom state and validates the destination.
	 * @param Context Both endpoints and the pawn identity.
	 * @param Source Original ASC; may be null for a new pawn.
	 * @param Destination Proposed ASC; never null after custody preparation.
	 * @param OutState Immutable versioned payload, required on success.
	 * @return Whether complete state can be restored at both endpoints.
	 */
	virtual bool Export(const Control::FStateTransferContext& Context, UNelaricAbilitySystemComponent* Source,
	                    UNelaricAbilitySystemComponent* Destination,
	                    TSharedPtr<const Control::FStateSnapshot>& OutState) const = 0;
	/** @brief Removes custom state owned by this pawn from an endpoint.
	 * @param ASC Endpoint ASC, possibly null before initial association.
	 * @param Pawn Pawn whose state must be removed.
	 * @param State Original complete export.
	 * @return Whether removal completed without clearing unrelated state.
	 */
	virtual bool Release(UNelaricAbilitySystemComponent* ASC, APawn* Pawn, const Control::FStateSnapshot& State) = 0;
	/** @brief Restores state including remaining times and committed flags.
	 * @param ASC ASC that now represents this pawn.
	 * @param Pawn Unchanged pawn identity.
	 * @param State Original complete export.
	 * @param bRecovery Whether restoring the original endpoint.
	 * @return Whether all custom state was restored exactly once.
	 */
	virtual bool Restore(UNelaricAbilitySystemComponent* ASC, APawn* Pawn, const Control::FStateSnapshot& State,
	                     bool bRecovery) = 0;
};
} // namespace Nelaric::GAS
