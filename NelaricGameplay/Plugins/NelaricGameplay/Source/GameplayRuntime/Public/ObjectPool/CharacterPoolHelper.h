// Copyright (c) 2026 Nelaric Contributors

/** @file CharacterPoolHelper.h
 * Declares shared native movement and presentation transitions for pooling.
 */

#pragma once

#include "CoreTypes.h"
#include "Math/Transform.h"

class ACharacter;

namespace Nelaric::ObjectPool
{
namespace Private
{
struct FPoolNetworkRuntime;
}
/** @brief Stores preparation and activation inside the owning character.
 * @details Keep this state for the actor's lifetime on the game thread.
 * @note Ordinary spawns may start active; pool preparation clears the flag.
 * @note Independent of pool handles and pawn initialization.
 */
struct FCharacterPoolState
{
	/// Whether the actor currently participates in native character gameplay.
	bool bActive = false;
	/// Whether pool preparation bound this state to a pooled actor.
	bool bPrepared = false;
};

/** @brief Shares basic character transitions across unrelated native bases.
 * @details Uses the existing movement and mesh on the game thread.
 * @note Call from a type's IPoolableCharacter implementation.
 * @note Owns no actor and allocates no pool management storage.
 * @note The type manages AI, GAS, timers, and initialization.
 * @note Replicated pools apply client transitions through their channel.
 */
struct FCharacterPoolHelper
{
public:
	/** @brief Reads local or automatically received activity on the game thread.
	 * @param Character Local character whose pool state is queried.
	 * @param State Authority or ordinary-spawn state retained by the character.
	 * @return Applied replica activity, or the supplied local activity.
	 */
	GAMEPLAYRUNTIME_API static bool IsActive(const ACharacter& Character, const FCharacterPoolState& State);

	/** @brief Reads whether pool preparation applies on the game thread.
	 * @param Character Local character whose membership is queried.
	 * @param State Local native state retained by the character.
	 * @return True for native preparation or framework replica membership.
	 */
	GAMEPLAYRUNTIME_API static bool IsPrepared(const ACharacter& Character, const FCharacterPoolState& State);
	/** @brief Activates native walking, display, collision, and component ticks.
	 * @details Call after BeginPlay with inactive state on the game thread.
	 * @note Uses no sweep; the caller supplies valid placement.
	 * @note Collision is enabled last; callbacks must preserve actor and pool.
	 * @param Character World-owned character with its native components.
	 * @param State Character-owned state, retained across leases.
	 * @param Transform Valid world placement; unit scale is preferred.
	 * @return False if unready, active, unauthorized, or placement fails.
	 * @note Clients only activate through the
	 * private replication adapter.
	 * @note A blocked authority uncrouch also rejects activation.
	 */
	GAMEPLAYRUNTIME_API static bool Activate(ACharacter& Character, FCharacterPoolState& State,
	                                         const FTransform& Transform);

	/** @brief Clears movement and disables native collision, display, and ticks.
	 * @details Game thread only; safe to repeat and clears native motion.
	 * @note Direct client calls are ignored;
	 * replication applies client state.
	 * @note Clears jump, input, velocity, forces, and pending launch.
	 * @note Clears character movement prediction
	 * data between leases.
	 * @note Idle actors must stay untouched by gameplay code.
	 * @note Does not reset type-specific systems.
	 * @param Character World-owned character preserved through callbacks.
	 * @param State Character-owned state; marked inactive before callbacks.
	 */
	GAMEPLAYRUNTIME_API static void Deactivate(ACharacter& Character, FCharacterPoolState& State);

public:
	// Authority preparation during deferred spawn; needs no client callback.
	GAMEPLAYRUNTIME_API static void PrepareForPool(ACharacter& Character, FCharacterPoolState& State);

private:
	friend struct Private::FPoolNetworkRuntime;
	static bool ActivateInternal(ACharacter& Character, FCharacterPoolState& State, const FTransform& Transform,
	                             bool bFromReplication);
	static void DeactivateInternal(ACharacter& Character, FCharacterPoolState& State, bool bFromReplication);
};
} // namespace Nelaric::ObjectPool
