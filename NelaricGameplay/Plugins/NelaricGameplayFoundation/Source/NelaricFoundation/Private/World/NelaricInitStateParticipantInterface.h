// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateParticipantInterface.h
 * Declares the native participant contract for initialization.
 */

#pragma once

#include "Containers/Array.h"
#include "World/NelaricInitStateTypes.h"

namespace Nelaric
{
/** @brief Native contract for a world initialization state participant.
 *
 * @details Implement on a component that participates in initialization.
 * Calls are made on the game thread. The component owner controls lifetime.
 */
class INelaricInitStateParticipantInterface
{
public:
	/// Releases this native interface without owning its component.
	virtual ~INelaricInitStateParticipantInterface() = default;

	/// Reports whether this participant applies in its current context.
	virtual bool IsInitApplicable() const = 0;

	/// Reports whether pawn readiness depends on this participant.
	virtual bool IsRequiredForPawnReady() const = 0;

	/** @brief Appends the participant's initialization dependencies.
	 * @param OutDependencies Receives required components and stages.
	 */
	virtual void GatherInitDependencies(TArray<FInitDependency>& OutDependencies) const = 0;

	/// Tries to move the participant to its next initialization state.
	virtual bool TryChangeInitState() = 0;
};
} // namespace Nelaric
