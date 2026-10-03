// Copyright (c) 2026 Nelaric Contributors

/** @file DemoAnimationDataInstance.h
 * Declares the native animation data base used by demo characters.
 */

#pragma once

#include "Animation/AnimationDataUpdater.h"
#include "Animation/AnimInstance.h"

#include "DemoAnimationDataInstance.generated.h"

class ADemoCharacter;

/** @brief Provides the native calculation interface for demo animation.
 * @details Derive in native C++ and override
 * UpdateAnimationData. Unreal owns
 * each instance. Demo characters constrain their main animation class to
 * this
 * base; the scheduler owns worker synchronization and job lifetime.
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UDemoAnimationDataInstance : public UAnimInstance, public Nelaric::UnitAnimation::IAnimationDataUpdater
{
	GENERATED_BODY()

public:
public:
	/** @brief Calculates this instance's data in its scheduled worker job.
	 * @details Native subclasses must implement calculation-only behavior.
	 * Synchronization and lifetime are guaranteed by the scheduler.
	 * @param DeltaSeconds Elapsed seconds, including skipped update frames.
	 */
	DEMOGAME_API virtual void UpdateAnimationData(float DeltaSeconds) override;

	/** @brief Captures input values on the game thread before worker dispatch.
	 * @param Character Owning character protected by the scheduler.
	 * @note Called after mesh tasks finish. The default captures nothing.
	 * @note Overrides preserve ownership and retain value-only worker inputs.
	 */
	DEMOGAME_API virtual void PrepareAnimationData(const ADemoCharacter& Character);

	/** @brief Retains an actual layer change across skipped updates.
	 * @note Game-thread providers call this after linking or unlinking.
	 * @note Derived instances capture the revision before worker dispatch.
	 */
	DEMOGAME_API void NotifyAnimationLayerChanged();

protected:
	/// Returns the retained layer revision while preparing game-thread inputs.
	uint64 GetAnimationLayerChangeSerial() const
	{
		return AnimationLayerChangeSerial;
	}

private:
	uint64 AnimationLayerChangeSerial = 0;
};
