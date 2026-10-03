// Copyright (c) 2026 Nelaric Contributors

/** @file DemoAnimationDataInstance.h
 * Declares the native animation data base used by demo characters.
 */

#pragma once

#include "Animation/AnimationDataUpdater.h"
#include "Animation/AnimInstance.h"

#include "DemoAnimationDataInstance.generated.h"

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
	/** @brief Calculates this instance's data in its scheduled worker job.
	 * @details Native subclasses must implement calculation-only behavior.
	 * Synchronization and lifetime are guaranteed by the scheduler.
	 * @param DeltaSeconds Elapsed seconds, including skipped update frames.
	 */
	DEMOGAME_API virtual void UpdateAnimationData(float DeltaSeconds) override;
};
