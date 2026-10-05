// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCharacterPoolPolicy.h
 * Injects per-lease team identity before native character activation.
 */

#pragma once

#include "Character/DemoCharacter.h"
#include "ObjectPool/CharacterPool.h"

namespace Nelaric::Demo
{
/** @brief Adds external team assignment to the shared character lifecycle.
 * @details Authority game thread only. Every acquisition assigns its team
 * before collision or bot startup; dead leases receive a new combat life.
 * Returning clears the lease's identity.
 *
 * @note Shared creation and destruction retain the existing world ownership.
 */
struct FCharacterPoolPolicy : ObjectPool::TCharacterPoolPolicy<ADemoCharacter>
{
	/// Inputs consumed synchronously for one character lease.
	struct FAcquireArgs
	{
		/// Caller-selected world placement; native activation does not sweep.
		FTransform Transform = FTransform::Identity;
		/// Externally assigned faction; 255 denotes a neutral character.
		uint8 TeamId = 255;
	};

	/** @brief Assigns the lease's team before enabling character gameplay.
	 * @param Character Valid inactive authority character retained by the pool.
	 * @param Args Placement and team consumed on the game thread.
	 * @return False if assignment or native activation is rejected.
	 */
	static bool OnAcquire(ADemoCharacter& Character, const FAcquireArgs& Args)
	{
		return Character.SetTeamId(Args.TeamId) && (Character.IsAlive() || Character.ResetCombatState()) &&
		       Character.ActivateFromPool(Args.Transform);
	}

	/** @brief Deactivates a lease before clearing its team identity.
	 * @details Game thread only; also accepts failed activation attempts.
	 * @param Character Valid authority character retained through callbacks.
	 */
	static void OnReturn(ADemoCharacter& Character)
	{
		Character.DeactivateToPool();
		Character.SetTeamId(255);
	}
};
} // namespace Nelaric::Demo
