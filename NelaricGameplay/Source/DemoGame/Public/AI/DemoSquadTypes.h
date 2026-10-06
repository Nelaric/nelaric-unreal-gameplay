// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadTypes.h Defines squad intent, authority and member reports. */
#pragma once

#include "CoreMinimal.h"
#include "DemoSquadTypes.generated.h"

class ADemoCharacter;

/// Source of tactical decisions, independent of pawn possession.
UENUM(BlueprintType)
enum class EDemoSquadCommandMode : uint8
{
	Autonomous,     ///< AI chooses tactics.
	PlayerAssisted, ///< Player assigns goals.
	PlayerManual,   ///< Player chooses tactics.
	NoCommander,    ///< Limited retained intent.
};

/// Capability preference used for assignment and succession.
UENUM(BlueprintType)
enum class EDemoSquadRole : uint8
{
	Leader,   ///< Primary commander.
	Deputy,   ///< Preferred successor.
	Support,  ///< Support preference.
	Rifleman, ///< General participant.
};

/// Long-lived objective retained across temporary encounters.
UENUM(BlueprintType)
enum class EDemoSquadMissionType : uint8
{
	None,     ///< No active objective.
	Move,     ///< Reach the goal area.
	Defend,   ///< Maintain an area.
	Control,  ///< Reach and secure an area.
	Search,   ///< Search a bounded area.
	Withdraw, ///< Reach a fallback area.
	Regroup,  ///< Gather and reorganize.
};

/// Feasible tactics selected independently of the mission.
UENUM(BlueprintType)
enum class EDemoSquadTactic : uint8
{
	Idle,     ///< Await an objective.
	Move,     ///< Travel in formation.
	Defend,   ///< Maintain sectors.
	Engage,   ///< Support and maneuver.
	Search,   ///< Inspect divided areas.
	Withdraw, ///< Disengage before regroup.
	Regroup,  ///< Gather participants.
};

/// Plan stages; continuous support survives stage changes.
UENUM(BlueprintType)
enum class EDemoSquadPhase : uint8
{
	None,        ///< No executing stage.
	Planning,    ///< Await route results.
	Travel,      ///< Reach formation slots.
	Deploy,      ///< Reach defensive sectors.
	Maintain,    ///< Maintain defense.
	Support,     ///< Establish ready support.
	Maneuver,    ///< Advance the group.
	Consolidate, ///< Check capabilities.
	Search,      ///< Execute divided searches.
	Withdrawal,  ///< Reach fallback.
	Regroup,     ///< Gather surviving members.
	Completed,   ///< Plan reached its goal.
	Failed,      ///< Plan cannot continue.
};

/// Intent submitted to a member rather than direct movement or weapon calls.
UENUM(BlueprintType)
enum class EDemoSquadOrderType : uint8
{
	MaintainFormation, ///< Follow the route anchor.
	MoveToArea,        ///< Reach an assigned area.
	HoldSector,        ///< Guard a bounded sector.
	SupportSector,     ///< Establish support.
	AdvanceToArea,     ///< Advance with constraints.
	SearchArea,        ///< Search a bounded sector.
	WithdrawToArea,    ///< Keep withdrawing.
	RegroupAt,         ///< Gather at the rally area.
};

/// Execution outcome; readiness is a separate, revocable flag.
UENUM(BlueprintType)
enum class EDemoSquadOrderState : uint8
{
	Received,  ///< Order received.
	Accepted,  ///< Validated and retained.
	Executing, ///< Member is executing.
	Suspended, ///< Temporarily interrupted.
	Succeeded, ///< Finite goal completed.
	Failed,    ///< Goal cannot be completed.
	Cancelled, ///< Explicitly withdrawn.
};

/// Structured reasons consumed by plan execution and debug displays.
UENUM(BlueprintType)
enum class EDemoSquadFailure : uint8
{
	None,                  ///< No failure.
	InvalidOrder,          ///< Invalid input contract.
	StaleAuthority,        ///< Authority was superseded.
	Unreachable,           ///< No complete route.
	Stuck,                 ///< Movement timed out.
	NoAmmo,                ///< No usable ammunition.
	NoValidFiringPosition, ///< Support cannot be ready.
	PositionUnavailable,   ///< Position lease denied.
	Suppressed,            ///< Support is ineffective.
	Incapacitated,         ///< Member cannot execute.
	PlayerControlled,      ///< Human drives the pawn.
	OrderExpired,          ///< Order lifetime ended.
	FeedbackTimeout,       ///< Member reports stopped.
	PhaseTimeout,          ///< Stage deadline ended.
	NoCommander,           ///< No eligible commander.
	Cancelled,             ///< Operation was cancelled.
};

/// Firing policy applied by the soldier's final weapon executor.
UENUM(BlueprintType)
enum class EDemoSquadEngagement : uint8
{
	HoldFire,    ///< No autonomous shooting.
	SelfDefense, ///< React to close threats.
	FireAtWill,  ///< Engage observed hostiles.
};

/// Explicit objective outcome; tactic failure alone does not end a mission.
UENUM(BlueprintType)
enum class EDemoSquadMissionState : uint8
{
	None,      ///< No objective.
	Running,   ///< Objective is retained.
	Succeeded, ///< Objective completed.
	Failed,    ///< Objective cannot finish.
	Cancelled, ///< Objective withdrawn.
};

/// Terminal or active outcome of a specific mission revision.
USTRUCT(BlueprintType)
struct FDemoSquadMissionResult
{
	GENERATED_BODY()
	/// Objective identity this result describes.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid MissionId;
	/// Objective version this result describes.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 Revision = 0;
	/// Outcome, independent of the current tactic phase.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadMissionState State = EDemoSquadMissionState::None;
	/// Structured terminal reason when the objective fails.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadFailure Failure = EDemoSquadFailure::None;
	/// Absolute world seconds of the last outcome change.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double ReportedAt = 0.0;
};

/// Circular world area; all lengths are centimeters.
USTRUCT(BlueprintType)
struct FDemoSquadArea
{
	GENERATED_BODY()
	/// Center chosen from observed or navigation-validated information.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FVector Center = FVector::ZeroVector;
	/// Positive horizontal radius in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad", meta = (ClampMin = "1"))
	float Radius = 500.0f;
};

/// Persistent mission semantics; zero deadlines mean no mission deadline.
USTRUCT(BlueprintType)
struct FDemoSquadMission
{
	GENERATED_BODY()
	/// Stable objective identity; assigned when absent on submission.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FGuid MissionId;
	/// Positive version; revisions of the same identity must increase.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	int32 Revision = 1;
	/// Objective that survives temporary tactical changes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	EDemoSquadMissionType Type = EDemoSquadMissionType::None;
	/// Primary objective area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FDemoSquadArea Goal;
	/// Authored fallback area; used only when explicitly enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FDemoSquadArea Fallback;
	/// Whether the fallback is a deliberate mission input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	bool bHasFallback = false;
	/// Optional circular operating boundary.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FDemoSquadArea Boundary;
	/// Whether all ordinary assigned positions must remain in Boundary.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	bool bHasBoundary = false;
	/// Desired horizontal direction; zero leaves selection to the StateTree.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FVector Facing = FVector::ZeroVector;
	/// Autonomous firing policy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	EDemoSquadEngagement Engagement = EDemoSquadEngagement::FireAtWill;
	/// Absolute world seconds; zero disables the mission deadline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	double Deadline = 0.0;
};

/// Identity carried by queued planning work and asynchronous callbacks.
USTRUCT(BlueprintType)
struct FDemoSquadRequestIdentity
{
	GENERATED_BODY()
	/// Authority generation, changed on leadership loss or mode changes.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int64 CommandEpoch = 0;
	/// Objective identity captured when the request was submitted.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid MissionId;
	/// Objective revision captured when the request was submitted.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 MissionRevision = 0;
	/// Plan that owns the result.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid PlanId;
	/// Additional identity invalidated on request cancellation.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 Generation = 0;
};

/// Versioned member intent; receiver stores this even during player control.
USTRUCT(BlueprintType)
struct FDemoSquadMemberOrder
{
	GENERATED_BODY()
	/// Owning squad identity.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid SquadId;
	/// Intended member's stable identity.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid UnitId;
	/// Rejects callbacks and submissions from obsolete command authority.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadRequestIdentity Identity;
	/// Stable order identity; repeated same-version submissions are harmless.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid OrderId;
	/// Positive order revision; changes replace the previous intent.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 Revision = 1;
	/// Desired behavior without direct access to pawn execution resources.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadOrderType Type = EDemoSquadOrderType::MoveToArea;
	/// Zero maneuvers, one supports, and two maintains coordination.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 Group = 0;
	/// Assigned goal region; Center is the preferred navigation position.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadArea Goal;
	/// Circular local reposition boundary.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadArea MovementArea;
	/// Observed support/search focus; never an unseen live actor position.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FVector FocusLocation = FVector::ZeroVector;
	/// Whether the focus comes from a valid observation or assigned sector.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bHasFocus = false;
	/// Desired horizontal heading; zero is unspecified.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FVector Facing = FVector::ZeroVector;
	/// Stable formation slot; -1 means unassigned.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 SlotId = INDEX_NONE;
	/// Firing policy enforced by the soldier.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadEngagement Engagement = EDemoSquadEngagement::FireAtWill;
	/// Whether ordinary combat may stop progress toward this goal.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bAllowStopToFight = true;
	/// Whether bounded pursuit and local cover selection are permitted.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bAllowLocalReposition = true;
	/// Arrival tolerance in centimeters.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float AcceptanceRadius = 100.0f;
	/// Absolute world seconds when submitted.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double IssuedAt = 0.0;
	/// Absolute arrival deadline; zero disables it.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double Deadline = 0.0;
	/// Absolute intent expiry; zero retains the order until cancellation.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double ExpiresAt = 0.0;
};

/// Committed execution feedback; readiness does not complete support intent.
USTRUCT(BlueprintType)
struct FDemoSquadMemberFeedback
{
	GENERATED_BODY()
	/// Member producing this report.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid UnitId;
	/// Order this report describes.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid OrderId;
	/// Version this report describes.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 Revision = 0;
	/// Current execution state.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadOrderState State = EDemoSquadOrderState::Received;
	/// Revocable readiness; meaningful only for active AI execution.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bReady = false;
	/// Failure or temporary suspension reason.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadFailure Failure = EDemoSquadFailure::None;
	/// Goal progress in [0, 1].
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float Progress = 0.0f;
	/// Absolute world seconds when this state was checked.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double ReportedAt = 0.0;
};

/// Latest member capability report, retained separately from phase rosters.
USTRUCT(BlueprintType)
struct FDemoSquadMemberStatus
{
	GENERATED_BODY()
	/// Persistent identity; never an array index or actor pointer.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid UnitId;
	/// Non-owning current body; pooling may bind a different unit later.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	TWeakObjectPtr<ADemoCharacter> Character;
	/// Body binding generation; rejects reports from previous pool leases.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 BindingGeneration = 0;
	/// Preferred capability and succession role.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadRole Role = EDemoSquadRole::Rifleman;
	/// Lower values win succession after role preference.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 SuccessionPriority = 100;
	/// Whether this member is eligible for command authority.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bCanCommand = true;
	/// Required participation; loss fails rather than shrinking the roster.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bRequired = false;
	/// Latest committed alive and active-pool state.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bAlive = false;
	/// Full pawn/GAS readiness and usable movement capability.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bMobile = false;
	/// Whether a human currently drives this body.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bPlayerControlled = false;
	/// Registered team identity; a team change withdraws squad membership.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	uint8 TeamId = 255;
	/// Latest friendly position; not an enemy observation.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FVector Location = FVector::ZeroVector;
	/// Current magazine rounds from the equipment owner.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 MagazineAmmo = 0;
	/// Current reserve rounds from the equipment owner.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 ReserveAmmo = 0;
	/// Current reload state from the equipment owner.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bReloading = false;
	/// Weapon range in centimeters; zero without a usable definition.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float WeaponRange = 0.0f;
	/// Current local suppression in [0, 1].
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float Suppression = 0.0f;
	/// Latest order feedback; no StateTree node inspection is needed.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadMemberFeedback Feedback;
	/// Absolute world seconds of the capability report.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double ReportedAt = 0.0;
};

/// Observed squad intelligence; forwarding does not create visual contact.
USTRUCT(BlueprintType)
struct FDemoSquadContact
{
	GENERATED_BODY()
	/// Stable contact identity independent of whether the actor still exists.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid ContactId;
	/// Actual observing member; a relay must preserve this identity.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid ObserverUnitId;
	/// Optional borrowed identity, used only for association.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	TWeakObjectPtr<AActor> Actor;
	/// Last actually observed world position.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FVector Location = FVector::ZeroVector;
	/// Actual observation time, distinct from local memory expiry.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double ObservedAt = -1.0;
	/// Observation confidence in [0, 1], decayed from ObservedAt.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float Confidence = 1.0f;
	/// Initial spatial uncertainty in centimeters, expanding over time.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float UncertaintyRadius = 0.0f;
};

/// Captured phase participant; its identity remains in the roster.
USTRUCT(BlueprintType)
struct FDemoSquadAssignment
{
	GENERATED_BODY()
	/// Captured participant identity.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid UnitId;
	/// Captured body binding identity.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 BindingGeneration = 0;
	/// Captured automatic participation; later loss cannot shrink the roster.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bGuaranteed = true;
	/// Whether phase completion requires this participant.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	bool bRequired = false;
	/// Current persistent order for this assignment.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadMemberOrder Order;
	/// World service position lease; released when the plan is replaced.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FGuid PositionId;
	/// Bounded local reassignment count; does not restart other members.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 RetryCount = 0;
};

/// Current execution plan; retained as a snapshot when authority is lost.
USTRUCT(BlueprintType)
struct FDemoSquadPlan
{
	GENERATED_BODY()
	/// Identity validated by all planning callbacks and receiver submissions.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadRequestIdentity Identity;
	/// Selected tactic.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadTactic Tactic = EDemoSquadTactic::Idle;
	/// Frozen tactical goal validated by this plan's route request.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FDemoSquadArea Goal;
	/// Current stage.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadPhase Phase = EDemoSquadPhase::None;
	/// Captured assignments; casualties do not silently shrink this array.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	TArray<FDemoSquadAssignment> Assignments;
	/// Shared route intent; members still perform their own MoveTo requests.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	TArray<FVector> Route;
	/// Route-following anchor independent of leader body rotation.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FVector FormationAnchor = FVector::ZeroVector;
	/// Heading derived from the route or assigned sector.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FVector FormationDirection = FVector::ForwardVector;
	/// Next shared route point.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 RouteIndex = 0;
	/// Plan creation time in world seconds.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double StartedAt = 0.0;
	/// Current phase start in world seconds.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double PhaseStartedAt = 0.0;
	/// Current phase deadline; zero means a continuous phase.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	double PhaseDeadline = 0.0;
	/// Explicit cause for failure or last rebuild.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	EDemoSquadFailure Failure = EDemoSquadFailure::None;
	/// Human-readable reason, only changed on meaningful plan decisions.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	FString Reason;
};

/// Cheap capability summaries used by tactical selection.
USTRUCT(BlueprintType)
struct FDemoSquadSnapshot
{
	GENERATED_BODY()
	/// Alive, ready AI participants; player bodies are not guaranteed actors.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 EffectiveMemberCount = 0;
	/// AI members with usable movement.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 MobileMemberCount = 0;
	/// Active support orders with revocable Ready feedback.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	int32 ReadySupportCount = 0;
	/// Suppressed fraction among effective participants.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float SuppressedMemberRatio = 0.0f;
	/// Fraction with usable weapon ammunition.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float AmmoReadiness = 0.0f;
	/// Fraction near the independent formation anchor.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float FormationIntegrity = 0.0f;
	/// Decayed observed threat pressure in [0, 1].
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float KnownThreatPressure = 0.0f;
	/// One for a validated route; zero after failure.
	UPROPERTY(BlueprintReadOnly, Category = "Squad")
	float RouteConfidence = 0.0f;
};

/// Data source explicitly selected by an authored planning task.
UENUM(BlueprintType)
enum class EDemoSquadGoalSource : uint8
{
	Mission,  ///< Submitted mission area.
	Fallback, ///< Submitted fallback area.
	Contact,  ///< Last observed contact.
	Rally,    ///< Mobile roster medoid.
};

/// Completion measurement selected by a phase task, without sequencing.
UENUM(BlueprintType)
enum class EDemoSquadReadiness : uint8
{
	All,        ///< Captured phase roster.
	Support,    ///< Assigned support group.
	Maneuver,   ///< Assigned movement group.
	Continuous, ///< Await tree transition.
};

/// Atomic phase execution inputs; the tree owns every policy choice.
USTRUCT(BlueprintType)
struct FDemoSquadPhaseSettings
{
	GENERATED_BODY()
	/// Debug label; does not select behavior or a subsequent phase.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadPhase Phase = EDemoSquadPhase::Travel;
	/// Order submitted to ordinary participants.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadOrderType OrderType = EDemoSquadOrderType::MoveToArea;
	/// Order used by the explicit support group.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadOrderType SupportOrderType = EDemoSquadOrderType::SupportSector;
	/// Order used by the explicit coordination group.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadOrderType CoordinationOrderType = EDemoSquadOrderType::HoldSector;
	/// Order used while the movement group waits at the anchor.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadOrderType HoldingOrderType = EDemoSquadOrderType::HoldSector;
	/// Feedback aggregation rule for this wait.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadReadiness Readiness = EDemoSquadReadiness::All;
	/// Duration in world seconds; zero permits continuous execution.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0"))
	float TimeoutSeconds = 30.0f;
	/// Fraction of captured participants required at their assigned areas.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1", ClampMax = "1"))
	float ArrivalRatio = 0.75f;
	/// Support participants required to report valid firing readiness.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1"))
	int32 MinimumReadySupport = 2;
	/// Advance route slots rather than moving directly to the final area.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bFollowRoute = false;
	/// Leave existing support and coordination orders in place.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bPreserveSupport = false;
	/// Keep the movement group at the anchor during support preparation.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bHoldMovement = false;
	/// Constrain ordinary participants to their assigned local sector.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bLocalSector = false;
	/// Local movement radius in centimeters, separate from arrival tolerance.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "50"))
	float LocalSectorRadius = 150.0f;
	/// Permit local cover changes inside the assigned movement area.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bAllowLocalReposition = false;
	/// Permit the soldier to pause movement for combat.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bAllowStopToFight = false;
};
