// Copyright (c) 2026 Nelaric Contributors

/** @file DemoUnitAnimationSubsystem.h
 * Declares the demo's world-scoped unit animation data subsystem.
 */

#pragma once

#include "Delegates/Delegate.h"
#include "ObjectPool/DemoCharacterPoolSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtr.h"

#include "DemoUnitAnimationSubsystem.generated.h"

class UAnimInstance;
class USkeletalMeshComponentBudgeted;

namespace Nelaric::UnitAnimation
{
/// Number of update levels; indices 0 through 5 represent levels 1 to 6.
inline constexpr int32 NumLevels = 6;

/** @brief Animation data update intervals in frames, ordered by level.
 * @details Indices 0 through 5 represent levels 1 to 6. Zero disables
 * updates for level 6 and must not be used as a divisor.
 */
inline constexpr int32 UpdateIntervalFrames[NumLevels] = {1, 3, 5, 10, 30, 0};

/** @brief Inclusive distance upper bounds in centimeters, ordered by level.
 * @details Distances beyond the last bound remain in level 6.
 */
inline constexpr float DistanceCm[NumLevels] = {1000.0f, 3000.0f, 5000.0f, 10000.0f, 20000.0f, 30000.0f};

/// Interval between distance updates, in milliseconds.
inline constexpr int32 DistanceUpdateIntervalMs = 1000;
static_assert(
    []() constexpr
    {
	    for (int32 Level = 0; Level < NumLevels; ++Level)
	    {
		    if (!(DistanceCm[Level] > 0.0f))
		    {
			    return false;
		    }
	    }
	    return true;
    }(),
    "Animation level distances must be greater than zero.");

static_assert(
    []() constexpr
    {
	    for (int32 Level = 1; Level < NumLevels; ++Level)
	    {
		    if (!(DistanceCm[Level] > DistanceCm[Level - 1]))
		    {
			    return false;
		    }
	    }
	    return true;
    }(),
    "Animation level distances must be strictly increasing.");

static_assert(DistanceUpdateIntervalMs > 0, "Distance update interval must be greater than zero.");

/** @brief Returns the shared distance level for animation data and budgets.
 * @param DistanceSquared Squared distance
 * to the character, in cm squared.
 * @return Index from zero through five; upper bounds are inclusive.
 */
FORCEINLINE int32 GetDistanceLevel(double DistanceSquared)
{
	for (int32 Level = 0; Level < NumLevels; ++Level)
	{
		const double DistanceLimit = DistanceCm[Level];
		if (DistanceSquared <= DistanceLimit * DistanceLimit)
		{
			return Level;
		}
	}
	return NumLevels - 1;
}
} // namespace Nelaric::UnitAnimation

/** @brief Stores animation update levels and unit indices in a demo world.
 * @details Unreal owns this subsystem in Game and PIE worlds. A game-thread
 * timer groups active character-pool slots by distance to the first local
 * player's controlled character. No reference character leaves groups empty.
 * Due animation instances update on workers before actor ticks resume.
 * Demo characters supply native updater
 * references from typed instances.
 * @note The scheduler completes mesh tasks and joins its own jobs.
 * @note Client views expose received slots without authority leases.
 * @note The same pool slots supply meshes to the world animation budget.
 * Hidden, inactive, level 6 and off-view
 * meshes do not contribute demand.
 */
UCLASS(MinimalAPI)
class UDemoUnitAnimationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void UpdateDistanceLevels();
	void HandleWorldPreActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void ResetUpdateState();

	FORCEINLINE void ResetLevelCounts()
	{
		for (int32& Count : LevelCounts)
		{
			Count = 0;
		}
	}

	static constexpr int32 NumLevels = Nelaric::UnitAnimation::NumLevels;
	static constexpr int32 MaxUnits = static_cast<int32>(UDemoCharacterPoolSubsystem::Capacity);

	// Only entries in [0, LevelCounts[Level]) contain stored unit indices.
	int32 LevelIndices[NumLevels][MaxUnits] = {};
	int32 LevelCounts[NumLevels] = {};
	FTimerHandle DistanceUpdateTimer;
	FDelegateHandle WorldPreActorTickHandle;
	FDelegateHandle WorldTickStartHandle;
	TWeakObjectPtr<UAnimInstance> SlotAnimationInstances[MaxUnits];
	float ElapsedUpdateSeconds[MaxUnits] = {};
	uint64 FrameCounter = 0;
	bool bDistanceLevelsReady = false;
	TWeakObjectPtr<USkeletalMeshComponentBudgeted> BudgetMeshes[MaxUnits];
	bool bShuttingDown = false;
	bool bReportedClientUpdate = false;
};
