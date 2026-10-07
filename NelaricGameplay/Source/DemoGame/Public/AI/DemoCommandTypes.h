// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCommandTypes.h Defines virtual command and objective contracts. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "Engine/DataAsset.h"
#include "DemoCommandTypes.generated.h"

/// Platoon intent, independent of company assignment role.
UENUM(BlueprintType)
enum class EDemoPlatoonMissionType : uint8
{
	Move,       ///< Reach an area.
	SecureArea, ///< Establish area control.
	Defend,     ///< Maintain area control.
	Search,     ///< Inspect assigned sectors.
	Withdraw,   ///< Reach a fallback area.
	Regroup,    ///< Gather available members.
};

/// Company weights and reserve preference; never a shared platoon action.
UENUM(BlueprintType)
enum class EDemoCompanyStrategy : uint8
{
	/// Favor unlocked objectives.
	Advance,
	/// Favor stable area duties.
	Maintain,
	/// Favor authorized recovery.
	Recover,
	/// Favor authorized withdrawal.
	Disengage,
};

/// Authoritative objective predicates; these never allocate forces.
UENUM(BlueprintType)
enum class EDemoObjectiveRuleType : uint8
{
	/// Reach the specified area.
	Arrive,
	/// Reach and gather.
	Regroup,
	/// Establish uncontested rule.
	Capture,
	/// Keep a current condition.
	Maintain,
	/// Cover specified sectors.
	Search,
	/// Protect designated soldiers.
	Escort,
	/// All referenced rules pass.
	All,
	/// Any referenced rule passes.
	Any,
};

/// Dependency lifetime; a start-only condition is latched per assignment.
UENUM(BlueprintType)
enum class EDemoCommandDependencyMode : uint8
{
	/// Required before activation.
	BeforeStartOnly,
	/// Required while executing.
	MaintainDuringExecution,
};

/// Cross-objective condition with explicitly selected evidence.
USTRUCT(BlueprintType)
struct FDemoCommandDependency
{
	GENERATED_BODY()
	/// Referenced objective identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString Id;
	/// Lifetime of the dependency.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	EDemoCommandDependencyMode Mode = EDemoCommandDependencyMode::BeforeStartOnly;
	/// True requires recorded completion; false requires current condition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	bool bCompletion = true;
};

/// Public platoon intent reused inside every company assignment.
USTRUCT(BlueprintType)
struct FDemoPlatoonMission
{
	GENERATED_BODY()
	/// Stable opaque task identity; empty requests a new identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Command")
	FString Id;
	/// Positive intent version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Command")
	int32 Revision = 1;
	/// Mission template executed by the platoon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	EDemoPlatoonMissionType Type = EDemoPlatoonMissionType::Move;
	/// Shared tactical area identity; empty uses only Goal geometry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString AreaId;
	/// Destination in the existing centimeter coordinate system.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FDemoSquadArea Goal;
	/// Optional conflict-free preparation area identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FString PreparationAreaId;
	/// Absolute deadline in world seconds; zero retains indefinite duties.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	double Deadline = 0.0;
	/// Existing firing policy, enforced by ordinary soldiers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	EDemoSquadEngagement Engagement = EDemoSquadEngagement::SelfDefense;
	/// Optional operating boundary applied by squad executors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FDemoSquadArea Boundary;
	/// Whether Boundary is an explicit constraint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	bool bHasBoundary = false;
	/// Minimum confirmed mobile soldiers for this task.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	int32 MinimumMobile = 1;
	/// Minimum armed fraction in the platoon aggregate, in [0, 1].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float MinimumAmmo = 0.0f;
};

/// Objective rule configuration; evaluated only by the rule authority.
USTRUCT(BlueprintType)
struct FDemoObjectiveRule
{
	GENERATED_BODY()
	/// Predicate type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	EDemoObjectiveRuleType Type = EDemoObjectiveRuleType::Arrive;
	/// Shared area used by this predicate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FString AreaId;
	/// Minimum eligible friendly occupants.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	int32 MinimumPresent = 1;
	/// Required fraction of the explicit participant set, in [0, 1].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float RequiredFraction = 1.0f;
	/// Stable required soldier identities; player-controlled soldiers count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	TArray<FString> Participants;
	/// Continuous world seconds needed to record completion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float HoldSeconds = 0.0f;
	/// Allowed condition-loss interval before resetting progress.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float GraceSeconds = 0.0f;
	/// Referenced objectives for All or Any.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	TArray<FString> Children;
	/// Shared area identities visited in order for Escort or Search.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	TArray<FString> Checkpoints;
	/// Whether current truth is required at mission settlement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	bool bContinuous = false;
	/// Whether the owning company may receive rule results.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	bool bPublicResult = true;
};

/// Company objective and the minimum complete force group it requires.
USTRUCT(BlueprintType)
struct FDemoCompanyObjective
{
	GENERATED_BODY()
	/// Stable objective identity unique within the mission.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FString Id;
	/// Authored utility priority; higher values win after hard constraints.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	float Priority = 1.0f;
	/// Whether mission success requires this objective.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	bool bMandatory = true;
	/// Number of platoons required before the group may activate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	int32 MinimumPlatoons = 1;
	/// Aggregate mobile capability required across the complete group.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	int32 MinimumMobile = 1;
	/// Reused platoon intent; each selected platoon receives its own identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FDemoPlatoonMission Mission;
	/// Authoritative completion and current-condition predicate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FDemoObjectiveRule Rule;
	/// Start and sustained dependencies on other objectives.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	TArray<FDemoCommandDependency> Dependencies;
};

/// Authorized response to an infeasible objective force group.
UENUM(BlueprintType)
enum class EDemoCompanyFallback : uint8
{
	Hold,     ///< Keep allowed duties.
	Recover,  ///< Regroup available force.
	Withdraw, ///< Withdraw available force.
};

/// Long-lived company intent; local repairs do not change its revision.
USTRUCT(BlueprintType)
struct FDemoCompanyMission
{
	GENERATED_BODY()
	/// Stable opaque mission identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Id;
	/// Overall weighting policy; platoons still execute different task types.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	EDemoCompanyStrategy StrategicPolicy = EDemoCompanyStrategy::Maintain;
	/// Positive externally supplied intent version.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 Revision = 1;
	/// Objectives and their dependency graph.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	TArray<FDemoCompanyObjective> Objectives;
	/// Whether the configured operating boundary constrains every assignment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	bool bHasOperatingBoundary = false;
	/// Shared allowed operating area, using existing world geometry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FDemoSquadArea OperatingBoundary;
	/// Maximum publicly known risk permitted for new or continuing execution.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	float MaximumKnownRisk = 1.0f;
	/// Company-authorized engagement behavior applied to platoon intents.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	EDemoSquadEngagement RulesOfEngagement = EDemoSquadEngagement::SelfDefense;
	/// Optional explicit success objective set; empty uses mandatory objectives.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	TArray<FString> SuccessCriteria;
	/// Optional rule conditions whose current truth fails the company mission.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	TArray<FString> FailureCriteria;
	/// Authorized infeasibility response; no force is created by this policy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	EDemoCompanyFallback FallbackPolicy = EDemoCompanyFallback::Hold;
	/// Existing shared area used by an authorized recovery or withdrawal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FString FallbackAreaId;
	/// Absolute deadline; zero retains the mission until explicit replacement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	double Deadline = 0.0;
};

/// Complete, versioned publication identity used by every permission gate.
USTRUCT(BlueprintType)
struct FDemoExecutionPermit
{
	GENERATED_BODY()
	/// Owning virtual company.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	FString CompanyId;
	/// Current world execution identity.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	FString RunId;
	/// Logical publication authority generation.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	int32 CommandEpoch = 1;
	/// Exact recipient membership generation.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	int32 MembershipRevision = 1;
	/// Exact assignment identity.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	FString AssignmentId;
	/// Exact assignment revision.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	int32 AssignmentRevision = 1;
	/// Increasing permission revision, independent of global plan revision.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	int32 GateVersion = 1;
	/// Exact authorized phase; current tasks use Execute.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	FName PhaseId;
	/// Versions of conditions used to authorize this phase.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	TMap<FString, int32> DependencyVersions;
	/// Whether this exact task may execute.
	UPROPERTY(BlueprintReadWrite, Category = "Command")
	bool bAllowed = true;
};

/// Logical command authority, independent of soldier possession.
UENUM(BlueprintType)
enum class EDemoCommandMode : uint8
{
	Autonomous,     ///< Select objective tasks.
	PlayerAssisted, ///< Follow player objectives.
	PlayerManual,   ///< Follow explicit orders.
	Suspended,      ///< Preserve allowed duties.
};

/// Assignment acceptance and execution; readiness is separate.
UENUM(BlueprintType)
enum class EDemoCommandTaskState : uint8
{
	Accepted,  ///< Candidate accepted.
	Preparing, ///< Prepare authorized work.
	Executing, ///< Task is executing.
	Succeeded, ///< Finite success recorded.
	Failed,    ///< Task recorded failure.
	Cancelled, ///< Exact task was withdrawn.
};

/// Company-plan purpose; never a lower tactical state.
UENUM(BlueprintType)
enum class EDemoAssignmentRole : uint8
{
	MainObjective, ///< Execute an objective.
	AreaSecurity,  ///< Maintain area security.
	Support,       ///< Support an objective.
	Reserve,       ///< Keep authorized reserve.
	Recovery,      ///< Recover available force.
};

/// Whether the rule authority has sufficient current evidence.
UENUM(BlueprintType)
enum class EDemoObjectiveEvaluation : uint8
{
	Valid,   ///< Known current facts.
	Unknown, ///< Insufficient facts.
};

/// Rule-authority lifecycle, independent of command execution.
UENUM(BlueprintType)
enum class EDemoObjectiveResultState : uint8
{
	Running,   ///< No terminal result.
	Succeeded, ///< Stage success recorded.
	Failed,    ///< Explicit failure.
};

/// Versioned platoon intent envelope, without runtime object references.
USTRUCT(BlueprintType)
struct FDemoCompanyAssignment
{
	GENERATED_BODY()
	/// Stable assignment identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Id;
	/// Exact assignment revision.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 Revision = 1;
	/// Authorized virtual company identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString CompanyId;
	/// Current world callback generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString RunId;
	/// Logical publication authority generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 CommandEpoch = 1;
	/// Stable receiving platoon identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString PlatoonId;
	/// Exact receiving membership generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 MembershipRevision = 1;
	/// Plan identity used only for tracking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString CompanyPlanId;
	/// Selected objective identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString ObjectiveId;
	/// Plan role independent of platoon tactics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	EDemoAssignmentRole Role = EDemoAssignmentRole::MainObjective;
	/// Reused platoon mission contents.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FDemoPlatoonMission Mission;
	/// Start and sustained permission conditions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	TArray<FDemoCommandDependency> Dependencies;
	/// Exact execution authorization.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FDemoExecutionPermit Permit;
	/// Execution lifecycle separate from readiness.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	EDemoCommandTaskState State = EDemoCommandTaskState::Accepted;
	/// Current preparation or continuing-duty readiness.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bReady = false;
	/// World time when this assignment started.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double StartedAt = 0.0;
	/// Start conditions already satisfied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	TArray<FString> LatchedDependencies;
};

/// Platoon summary with independently ordered capability and task facts.
USTRUCT(BlueprintType)
struct FDemoPlatoonSituationReport
{
	GENERATED_BODY()
	/// Stable logical report source.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString PlatoonId;
	/// Current company membership generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 MembershipRevision = 1;
	/// Current world callback generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString RunId;
	/// Current publication authority generation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 CommandEpoch = 1;
	/// Monotonic capability sample sequence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 ReportSequence = 0;
	/// Monotonic task update sequence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 TaskSequence = 0;
	/// Actual oldest required capability observation time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double ObservedAt = -1.0;
	/// Actual task-state observation time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double TaskObservedAt = -1.0;
	/// Virtual command and required bindings are available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bCommandAvailable = false;
	/// Authorized friendly team; 255 is unknown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	uint8 TeamId = 255;
	/// Available soldier bodies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 Effective = 0;
	/// Bodies capable of the requested movement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 Mobile = 0;
	/// Aggregate armed readiness fraction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	float Ammo = 0.0f;
	/// Formation integrity fraction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	float Integrity = 0.0f;
	/// Aggregate recovery need.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	float Recovery = 1.0f;
	/// Risk from permitted known reports.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	float KnownRisk = 0.0f;
	/// Aggregate friendly reference location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FVector Location = FVector::ZeroVector;
	/// Exact task currently reported.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString AssignmentId;
	/// Exact task revision currently reported.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 AssignmentRevision = 0;
	/// Exact execution gate for which current readiness was observed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 GateVersion = 0;
	/// Task status independent of the capability sample.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	EDemoCommandTaskState State = EDemoCommandTaskState::Accepted;
	/// Current readiness may be withdrawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bReady = false;
	/// Candidate preparation is currently valid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bPreparationReady = false;
	/// Structured failure or hold reason.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Failure;
};

/// Public rule-authority result with historical and current truth separated.
USTRUCT(BlueprintType)
struct FDemoObjectiveResult
{
	GENERATED_BODY()
	/// Stable objective identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Id;
	/// A configured stage result was achieved.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bCompletionRecorded = false;
	/// Current sustained condition is valid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bCurrentConditionValid = false;
	/// Evidence availability for current truth.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	EDemoObjectiveEvaluation EvaluationState = EDemoObjectiveEvaluation::Unknown;
	/// Stage or terminal result.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	EDemoObjectiveResultState State = EDemoObjectiveResultState::Running;
	/// Configured progress fraction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	float Progress = 0.0f;
	/// Monotonic authority result sequence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 ResultSequence = 0;
	/// Changes only when current truth, evidence or stage status changes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 ConditionVersion = 0;
	/// Actual evaluation time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double EvaluatedAt = -1.0;
};

/// Resource responsibility owned by one assignment revision.
USTRUCT(BlueprintType)
struct FDemoSharedResourceClaim
{
	GENERATED_BODY()
	/// Shared area or passage identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Key;
	/// Assignment identity and revision.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Owner;
	/// Claimed capacity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 Amount = 1;
	/// Controlled reservation expiry time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double ExpiresAt = 0.0;
	/// Candidate reservation awaiting activation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	bool bProvisional = true;
};

/// Stable allocation semantics; changes do not invalidate unrelated tasks.
USTRUCT(BlueprintType)
struct FDemoCompanyPlan
{
	GENERATED_BODY()
	/// Stable plan identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString Id;
	/// Tracking revision, independent of assignment validity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 Revision = 1;
	/// External mission identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	FString SourceMissionId;
	/// External mission revision.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	int32 SourceMissionRevision = 1;
	/// Exact active task by platoon identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	TMap<FString, FDemoCompanyAssignment> Assignments;
	/// Currently uncommitted authorized platoons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	TArray<FString> ReservePlatoonIds;
	/// Explicit shared resource responsibilities.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	TArray<FDemoSharedResourceClaim> SharedResourceClaims;
	/// Plan creation world time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double StartedAt = 0.0;
	/// Last allocation world time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	double LastReplannedAt = 0.0;
};

/// Only permitted public rule results, never hidden world state.
USTRUCT(BlueprintType)
struct FDemoCompanyKnowledge
{
	GENERATED_BODY()
	/// Latest public result for each configured objective.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Company")
	TMap<FString, FDemoObjectiveResult> ObjectiveResults;
};

/// Native asset carrying typed company intent for Blueprint authors.
UCLASS(MinimalAPI, BlueprintType)
class UDemoCompanyMissionAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	/// Configured company mission; runtime copies do not modify this asset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	FDemoCompanyMission Mission;

public:
};

/// Bounded, configurable scheduling and reassignment defaults.
UCLASS(MinimalAPI, BlueprintType)
class UDemoCompanyPolicy : public UDataAsset
{
	GENERATED_BODY()
public:
	/// Capability report maximum age in world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float MaxReportAge = 3.0f;
	/// Active objective reassessment interval in world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float ReassessmentSeconds = 4.0f;
	/// Stable hold reassessment interval in world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float HoldSeconds = 8.0f;
	/// Ordinary full reassignment cooldown in world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float ReassignmentSeconds = 15.0f;
	/// Maximum changed platoon publications per frame.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	int32 PublishBudget = 2;
	/// Candidate acceptance and preparation timeout in world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float AcceptanceSeconds = 30.0f;
	/// Registration timeout in world seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float RegistrationSeconds = 30.0f;
	/// Maximum attempts to repair an objective before holding it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	int32 MaximumRepairs = 2;
	/// Authored objective priority weight.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float ValueWeight = 1.0f;
	/// Deadline urgency weight.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float UrgencyWeight = 2.0f;
	/// Dependent-objective unlock weight.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float UnlockWeight = 1.0f;
	/// Already observed risk penalty.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float RiskWeight = 2.0f;
	/// Force opportunity-cost penalty.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float OpportunityWeight = 1.0f;
	/// Minimum utility gain before an optional duty can be replaced.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	float ReassignmentGain = 0.1f;

public:
};

/// Authored logical identity, command scope and explicit registration roster.
UCLASS(MinimalAPI, BlueprintType)
class UDemoCompanyDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	/// Stable company identity, retained across save/load and tree restarts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	FString CompanyId;
	/// Only this team can be registered for command.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	uint8 TeamId = 0;
	/// Configured platoon identities; an empty list is not registration complete.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	TArray<FString> ExpectedPlatoonIds;

public:
	UDemoCompanyDefinition();
};
