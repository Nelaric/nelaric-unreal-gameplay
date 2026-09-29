// Copyright (c) 2026 Nelaric

/** @file InitStateParticipantInterface.h
 * Declares the reflected component initialization contract.
 */

#pragma once

#include "UObject/Interface.h"
#include "Pawn/InitStateTypes.h"

#include "InitStateParticipantInterface.generated.h"

/// UE type used to discover initialization participants on components.
UINTERFACE(MinimalAPI)
class UInitStateParticipantInterface : public UInterface
{
	GENERATED_BODY()
};

/** @brief Contract implemented by an actor component in world initialization.
 *
 * @details The component owns its state, generation, and failure flag. The
 * world subsystem discovers this interface through UE reflection. All calls
 * and notifications occur on the game thread.
 */
class IInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	/// Whether this component participates in its current context.
	virtual bool IsInitApplicable() const = 0;

	/** @brief Attempts one adjacent transition through DataInitialized.
	 * @details Check only this component's data and context. This method must
	 * never enter Ready or rely on dependency gameplay behavior.
	 * @return True only if this call committed exactly one adjacent transition.
	 */
	virtual bool TryChangeInitState() = 0;

	/** @brief Checks this component's internal preparation for Ready.
	 * @details Called at DataInitialized after the coordinator checks external
	 * dependencies. Preparation may perform retry-safe local work but must not
	 * change init state or start
	 * gameplay. Component gameplay requires Ready; work
	 * requiring the complete pawn must additionally wait for
	 * pawn Ready.
	 * @return Whether this component may enter Ready now.
	 */
	virtual bool CanEnterReady() = 0;

	/** @brief Writes Ready without callbacks after group checks pass.
	 * @details The coordinator calls this once per member, then notifies every
	 * member after all commits. Implementations must not recheck dependencies or
	 * broadcast, invoke delegates, or request coordinator progress here.
	 * @return True only if DataInitialized changed to Ready in this call.
	 */
	virtual bool CommitReadyWithoutNotification() = 0;

	/** @brief Announces an already committed Ready transition.
	 * @details Called after the entire readiness group is Ready. Notify the
	 * world coordinator and any component observers only in this phase.
	 * @param Previous Snapshot captured before the Ready commit.
	 */
	virtual void NotifyReadyCommitted(const Nelaric::FInitStateSnapshot& Previous) = 0;

	/// Current component-owned state of this attempt.
	virtual Nelaric::EInitState GetInitState() const = 0;

	/// Current component-owned attempt identity.
	virtual Nelaric::FInitGeneration GetInitGeneration() const = 0;

	/// Whether this attempt has a terminal failure.
	virtual bool HasTerminalInitFailure() const = 0;

	/** @brief Invalidates old work and returns to Registered.
	 * @details Increment the generation before removing listeners or
	 * cancelling work. Notify the world after resetting state.
	 */
	virtual void InvalidateInitGeneration() = 0;

	/** @brief Records a terminal failure without adding a state.
	 * @details Notify the world so dependents can stop awaiting this attempt.
	 */
	virtual void MarkTerminalInitFailure() = 0;
};
