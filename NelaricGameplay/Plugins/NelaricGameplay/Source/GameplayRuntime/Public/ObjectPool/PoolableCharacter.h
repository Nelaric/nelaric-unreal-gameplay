// Copyright (c) 2026 Nelaric Contributors

/** @file PoolableCharacter.h
 * Declares a native pooling contract independent of character inheritance.
 */

#pragma once

#include "Math/Transform.h"

namespace Nelaric::ObjectPool
{
/** @brief Defines native pooling without constraining the character base.
 * @details Implement alongside ACharacter on the game thread.
 * @note The world owns the actor; callbacks must preserve actor and pool.
 * @note Implementations manage reset and cancellation of their own state.
 * @note This interface uses no Unreal reflection.
 */
class IPoolableCharacter
{
public:
	/** @brief Activates an inactive actor for one logical lease.
	 * @details Call after BeginPlay on the game thread.
	 * @note Failure must leave the actor valid for DeactivateToPool.
	 * @param Transform World transform chosen by the caller.
	 * @return Whether placement and type-specific activation succeeded.
	 */
	virtual bool ActivateFromPool(const FTransform& Transform) = 0;

	/** @brief Stops the current lease and leaves the actor inactive.
	 * @details Game thread only; infallible and safe to repeat.
	 * @note Also accepts prepared actors and failed activations.
	 * @note Cancel lease-owned work and bindings before slot reuse.
	 */
	virtual void DeactivateToPool() = 0;

	/// Returns the native active state on the game thread.
	virtual bool IsPoolActive() const = 0;

public:
	/** @brief Prepares a deferred spawn to enter play as an inactive actor.
	 * @details The pool calls this before FinishSpawning on the game thread.
	 * @note Disable automatic possession and lease gameplay before BeginPlay.
	 * @note Construction and initialization must preserve inactivity.
	 */
	virtual void PrepareForPool() = 0;
	virtual FORCEINLINE ~IPoolableCharacter() = default;
};
} // namespace Nelaric::ObjectPool
