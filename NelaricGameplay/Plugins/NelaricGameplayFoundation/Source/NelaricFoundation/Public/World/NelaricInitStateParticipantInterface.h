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
	 * @param OutDependencies Receives component and required stage pairs.
	 */
	virtual void GatherInitDependencies(TArray<Nelaric::FInitDependency>& OutDependencies) const = 0;

	/** @brief Attempts exactly one adjacent forward transition.
	 * @return True only if this call committed one state transition.
	 */
	virtual bool TryChangeInitState() = 0;

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
