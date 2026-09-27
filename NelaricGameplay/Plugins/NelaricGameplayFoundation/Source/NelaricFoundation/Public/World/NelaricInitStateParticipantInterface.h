// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateParticipantInterface.h
 * Declares the reflected component initialization contract.
 */

#pragma once

#include "Containers/Array.h"
#include "UObject/Interface.h"
#include "World/NelaricInitStateTypes.h"

#include "NelaricInitStateParticipantInterface.generated.h"

/// UE type used to discover initialization participants on components.
UINTERFACE(MinimalAPI)
class UNelaricInitStateParticipantInterface : public UInterface
{
	GENERATED_BODY()
};

/** @brief Contract implemented by an actor component in world initialization.
 *
 * @details The component owns its state, generation, and failure flag. The
 * world subsystem discovers this interface through UE reflection. All calls
 * and notifications occur on the game thread.
 */
class INelaricInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	/// Whether this component participates in its current context.
	virtual bool IsInitApplicable() const = 0;

	/// Whether pawn readiness requires this component to reach Ready.
	virtual bool IsRequiredForPawnReady() const = 0;

	/** @brief Appends dependencies for the current attempt.
	 * @details Declare each runtime dependency by stable identity even when
	 * its component reference is temporarily null. Request a refresh when a
	 * missing reference becomes available.
	 * @param OutDependencies Receives runtime component dependencies.
	 */
	virtual void GatherInitDependencies(TArray<Nelaric::FInitDependency>& OutDependencies) const = 0;

	/** @brief Attempts one adjacent transition through DataInitialized.
	 * @details Check only this component's data and context. This method must
	 * never enter Ready or rely on dependency gameplay behavior.
	 * @return True only if this call committed one state transition.
	 */
	virtual bool TryChangeInitState() = 0;

	/** @brief Checks internal preparation and runtime dependency readiness.
	 * @details Called at DataInitialized. Dependencies outside the current
	 * readiness group must be Ready. This query must not mutate state, start
	 * gameplay, or broadcast events. Component gameplay requires Ready; work
	 * requiring the complete pawn must additionally wait for pawn Ready.
	 * @return Whether this component may enter Ready now.
	 */
	virtual bool CanEnterReady() const = 0;

	/** @brief Commits the final transition after CanEnterReady succeeds.
	 * @return True only if this call committed DataInitialized to Ready.
	 */
	virtual bool EnterReady() = 0;

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
