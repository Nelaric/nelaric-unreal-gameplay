// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSoldierTypes.h Defines soldier intent and observed combat data. */
#pragma once

#include "CoreMinimal.h"
#include "DemoSoldierTypes.generated.h"

/// Long-lived intent supplied by a player or another gameplay system.
UENUM(BlueprintType)
enum class EDemoSoldierOrderType : uint8
{
	None,   ///< No external intent.
	Move,   ///< Reach a destination.
	Hold,   ///< Guard a bounded area.
	Attack, ///< Attack an assigned goal.
	Follow, ///< Follow a friendly actor.
	Defend, ///< Guard a bounded area.
};

/// Outcome of an accepted order; interruptions retain Running.
UENUM(BlueprintType)
enum class EDemoSoldierOrderStatus : uint8
{
	None,      ///< No accepted order.
	Running,   ///< Intent remains active.
	Completed, ///< Goal was reached.
	Failed,    ///< Goal cannot be reached.
	Canceled,  ///< Intent was withdrawn.
};

/// Atomic actions selected by the authored soldier StateTree.
UENUM(BlueprintType)
enum class EDemoSoldierTreeAction : uint8
{
	Idle,              ///< Wait for observations.
	AvoidGrenade,      ///< Escape reported danger.
	TakeCover,         ///< Reach authored cover.
	Reload,            ///< Reload the active weapon.
	Aim,               ///< Establish aim once.
	FireBurst,         ///< Fire a finite burst.
	Observe,           ///< Pause between actions.
	ApproachTarget,    ///< Enter weapon range.
	MoveToMemory,      ///< Visit observed position.
	Search,            ///< Observe the memory point.
	ForgetTarget,      ///< Finish an empty search.
	InvestigateDamage, ///< Examine a damage cue.
	InvestigateSound,  ///< Examine a sound cue.
	ExecuteOrder,      ///< Execute retained intent.
	OutOfAmmo,         ///< Await a usable weapon.
	Dead,              ///< Terminal for this life.
};

/// Cheap predicates used by state selection and priority interruptions.
UENUM(BlueprintType)
enum class EDemoSoldierTreeTest : uint8
{
	Alive,            ///< The character is alive.
	Grenade,          ///< Immediate grenade danger.
	Combat,           ///< Combat or weapon upkeep.
	NeedsReload,      ///< A safe reload is needed.
	NeedsCover,       ///< Cover retry is eligible.
	OutOfAmmo,        ///< No usable weapon ammo.
	TargetVisible,    ///< Visual target confirmed.
	CanApproach,      ///< Pursuit can enter range.
	CanSearchMove,    ///< May visit memory point.
	Alert,            ///< Unconsumed sensory cue.
	DamageCue,        ///< Damage is the latest cue.
	HasOrder,         ///< An order is running.
	MoveOrder,        ///< A move order is running.
	HoldOrder,        ///< A hold order is running.
	AttackOrder,      ///< Attack intent is running.
	FollowOrder,      ///< Follow intent is running.
	DefendOrder,      ///< Defend intent is running.
	ReturnToArea,     ///< Guard boundary exceeded.
	ShouldReconsider, ///< Higher priority changed.
};

/// Result of one identity-scoped StateTree action.
UENUM(BlueprintType)
enum class EDemoSoldierTreeResult : uint8
{
	Running,   ///< The action is active.
	Succeeded, ///< The action finished.
	Failed,    ///< The action cannot finish.
};

/// Current native action, exposed for debugging and StateTree bindings.
UENUM(BlueprintType)
enum class EDemoSoldierBehavior : uint8
{
	Idle,           ///< Observe without a task.
	ExecuteOrder,   ///< Move toward the goal.
	Hold,           ///< Remain inside the area.
	Aim,            ///< React and align the view.
	ApproachTarget, ///< Enter weapon range.
	FireBurst,      ///< Execute a finite burst.
	Observe,        ///< Pause before deciding.
	Reload,         ///< Await an owned reload.
	MoveToMemory,   ///< Reach an observed point.
	Search,         ///< Look for a lost target.
	Investigate,    ///< Examine a sensory cue.
	AvoidGrenade,   ///< Escape reported danger.
	TakeCover,      ///< Reach authored cover.
	OutOfAmmo,      ///< Await a usable weapon.
	Dead,           ///< Terminal for this life.
};

/// Authority-only intent; actor references never own their targets.
USTRUCT(BlueprintType)
struct FDemoSoldierOrder
{
	GENERATED_BODY()

	/// Operation identity; an invalid identity is assigned on acceptance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier")
	FGuid Id;
	/// Requested intent; None cancels the current order.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier")
	EDemoSoldierOrderType Type = EDemoSoldierOrderType::None;
	/// World destination or guard-area center in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier")
	FVector Location = FVector::ZeroVector;
	/// Optional attack target or required follow target; non-owning.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier")
	TWeakObjectPtr<AActor> Actor;
	/// Arrival tolerance in centimeters; must be positive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier", meta = (ClampMin = "1"))
	float AcceptanceRadius = 100.0f;
	/// Hold and Defend movement boundary in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier", meta = (ClampMin = "1"))
	float HoldRadius = 500.0f;
	/// Permit combat pursuit; Hold and Defend always enforce their boundary.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soldier")
	bool bAllowPursuit = true;
};

/// Known enemy data; unseen enemies never refresh their world positions.
USTRUCT(BlueprintType)
struct FDemoSoldierContact
{
	GENERATED_BODY()

	/// Non-owning enemy identity; invalid entries are discarded.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	TWeakObjectPtr<AActor> Actor;
	/// Latest visual observation, in world centimeters.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	FVector Location = FVector::ZeroVector;
	/// Last confirmed visual observation in world seconds; -1 means none.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	double LastSeenTime = -1.0;
	/// Last damage from this known actor in world seconds; -1 means none.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	double LastDamageTime = -1.0;
	/// Visual sense result; hearing and damage do not set this flag.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	bool bVisible = false;
};

/// Read-only decision snapshot; health and ammunition remain with GAS/items.
USTRUCT(BlueprintType)
struct FDemoSoldierMemory
{
	GENERATED_BODY()

	/// Selected enemy, borrowed from the bounded contact list.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	TWeakObjectPtr<AActor> Target;
	/// Last known target position, in world centimeters.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	FVector TargetLocation = FVector::ZeroVector;
	/// Last sound position, in world centimeters.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	FVector HeardLocation = FVector::ZeroVector;
	/// Last reported direction toward incoming fire; zero means unknown.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	FVector DamageDirection = FVector::ZeroVector;
	/// Last sound time in world seconds; -1 means none.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	double LastHeardTime = -1.0;
	/// Last incoming fire time in world seconds; -1 means none.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	double LastDamageTime = -1.0;
	/// Whether the selected target is currently visually known.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	bool bTargetVisible = false;
	/// Recent incoming fire; expires without further damage or near misses.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	bool bUnderFire = false;
	/// Decaying pressure in [0, 1]; no health or ammo state is duplicated.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	float Suppression = 0.0f;
	/// Whether an authored cover point was reached by this execution.
	UPROPERTY(BlueprintReadOnly, Category = "Soldier")
	bool bInCover = false;
};

/// Tuning shared by native actions and the authored soldier StateTree.
USTRUCT(BlueprintType)
struct FDemoSoldierSettings
{
	GENERATED_BODY()

	/// Native planner interval in seconds; authored trees use event callbacks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.05"))
	float DecisionInterval = 0.2f;
	/// Reaction delay range in seconds; sampled once per aim action.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	FVector2D ReactionSeconds = FVector2D(0.15, 0.7);
	/// Observation delay range in seconds between bursts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	FVector2D ObserveSeconds = FVector2D(0.15, 0.4);
	/// Inclusive rounds per burst; tune higher for machine guns.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FIntPoint BurstRounds = FIntPoint(2, 6);
	/// Maximum view error in degrees before a shot may be requested.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.1", ClampMax = "45"))
	float AimToleranceDegrees = 5.0f;
	/// Minimum rounds that permit opportunistic early reload.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	int32 ReloadThreshold = 3;
	/// Lost visual memories expire after this many world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory", meta = (ClampMin = "1"))
	float MemorySeconds = 8.0f;
	/// Seconds spent looking at the last known position before forgetting.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory", meta = (ClampMin = "0.1"))
	float SearchSeconds = 2.5f;
	/// Sound investigation lifetime, including movement, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory", meta = (ClampMin = "0.1"))
	float AlertSeconds = 4.0f;
	/// Minimum target retention in seconds unless visual contact is lost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory", meta = (ClampMin = "0"))
	float TargetLockSeconds = 1.5f;
	/// Required score advantage when switching between visible targets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Memory", meta = (ClampMin = "1"))
	float TargetSwitchRatio = 1.3f;
	/// Incoming fire remains recent for this many seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival", meta = (ClampMin = "0.1"))
	float UnderFireSeconds = 2.0f;
	/// Suppression lost per world second without additional stimuli.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival", meta = (ClampMin = "0.01"))
	float SuppressionDecay = 0.2f;
	/// Radius for searching authored cover, in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival", meta = (ClampMin = "1"))
	float CoverSearchRadius = 1200.0f;
	/// Minimum seconds between cover queries after a failed attempt.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival", meta = (ClampMin = "0.1"))
	float CoverRetrySeconds = 3.0f;
	/// Maximum duration of one movement request, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1"))
	float MoveTimeoutSeconds = 15.0f;
	/// Maximum run speed during reported grenade danger, in cm/s.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1"))
	float SprintSpeed = 650.0f;
};
