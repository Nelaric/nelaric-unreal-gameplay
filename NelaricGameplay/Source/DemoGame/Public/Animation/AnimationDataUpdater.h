// Copyright (c) 2026 Nelaric Contributors

/** @file AnimationDataUpdater.h
 * Declares scheduler-protected native animation data calculations.
 */

#pragma once

namespace Nelaric::UnitAnimation
{
/** @brief Defines animation calculations protected by the animation scheduler.
 * @details Implement on a native animation instance. The scheduler preserves
 * target lifetime and excludes conflicting animation and game-thread access
 * until work completes. Implementations need no internal synchronization.
 */
class IAnimationDataUpdater
{
public:
	/** @brief Calculates and writes animation data on a scheduled worker thread.
	 * @details The scheduler provides stable target data and serializes updates
	 * for one target. Completed writes are visible before animation evaluation
	 * or other readers resume. Independent targets may update in parallel.
	 * Perform calculations on target-owned value fields only; no locks,
	 * atomics, scheduling, lifetime management, or engine operations are needed.
	 * @param DeltaSeconds Finite, non-negative elapsed seconds since this
	 * target's previous update, including skipped animation update frames.
	 */
	virtual void UpdateAnimationData(float DeltaSeconds) = 0;

public:
	DEMOGAME_API virtual ~IAnimationDataUpdater();
};
} // namespace Nelaric::UnitAnimation
