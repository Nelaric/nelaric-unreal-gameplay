// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadContextComponent.h Declares the squad mission executor. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "DemoSquadContextComponent.generated.h"

class UDemoSquadDefinition;
class UDemoSquadTactics;
class UDemoSquadMemberComponent;

/// Authority script policy for continuous objective movement.
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(FDemoSquadMemberOrder, FDemoSquadMemberOrderPolicy, ADemoCharacter*,
                                          Character, const FDemoSquadMemberOrder&, Order);

namespace Nelaric::Squad
{
/// Committed context notification; observers query snapshots on game thread.
DECLARE_MULTICAST_DELEGATE(FChanged);
} // namespace Nelaric::Squad

/** @brief Owns squad mission, intelligence, authority and phase execution.
 * @details The command actor owns this component. All APIs require the
 * authority game thread. The authored GameAI tree drives this executor;
 * only one may acquire it. It never drives member movement or weapons.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = AI)
class UDemoSquadContextComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	/** @brief Replaces mission intent without erasing shared intelligence.
	 * @param Mission Valid finite areas and an increasing same-ID revision.
	 * @return False for stale intent or an upstream lease; old intent remains.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool SetMission(const FDemoSquadMission& Mission);
	/** @brief Claims the single upstream mission source; authority game thread.
	 * @param Source Live same-world authority actor owning the coordination.
	 * @return Positive membership epoch, or zero for a conflicting source.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API int32 ClaimMissionSource(AActor* Source);
	/** @brief Releases a source and cancels only its intent; game thread.
	 * @param Source Actor that acquired the source lease.
	 * @param Epoch Exact membership epoch returned by the claim.
	 * @return False for an obsolete lease; no newer intent is changed.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool ReleaseMissionSource(AActor* Source, int32 Epoch);
	/** @brief Submits intent through the current lease; authority game thread.
	 * @param Source Actor that acquired the source lease.
	 * @param Epoch Exact membership epoch returned by the claim.
	 * @param Mission Valid intent with a strictly increasing same-ID revision.
	 * @return False for invalid intent, manual policy or an obsolete lease.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool SetMissionFromSource(AActor* Source, int32 Epoch, const FDemoSquadMission& Mission);
	/** @brief Cancels matching source intent; authority game thread only.
	 * @param Source Actor that acquired the source lease.
	 * @param Epoch Expected membership epoch.
	 * @param MissionId Expected mission identity.
	 * @param Revision Expected mission version.
	 * @return False for stale identity; unrelated intent remains intact.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool CancelMissionFromSource(AActor* Source, int32 Epoch, FGuid MissionId, int32 Revision);
	/// Returns the borrowed upstream source, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API AActor* GetMissionSource() const;
	/// Returns a dated aggregate without exposing raw members; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadSituationReport GetSituationReport() const;
	/// Actual nonempty lower-plan cancellations in this world execution.
	UPROPERTY(BlueprintReadOnly, Category = "Demo|Diagnostics")
	int32 PlanCancellationCount = 0;
	/// Actual route requests issued by this virtual squad in this world.
	UPROPERTY(BlueprintReadOnly, Category = "Demo|Diagnostics")
	int32 RouteRequestCount = 0;
	/** @brief Changes command policy and invalidates old asynchronous work.
	 * @param Mode Tactical authority policy, independent of body possession.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API void SetCommandMode(EDemoSquadCommandMode Mode);
	/** @brief Chooses an explicit tactic under player manual command.
	 * @param Tactic Player-selected feasible behavior for the retained mission.
	 * @return False without manual authority or a compatible goal.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool SetManualTactic(EDemoSquadTactic Tactic);
	/// Returns persistent mission intent on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadMission GetMission() const;
	/// Returns the current mission revision's explicit outcome; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadMissionResult GetMissionResult() const;
	/** @brief Confirms completion from an authoritative objective system.
	 * @param MissionId Expected current objective identity.
	 * @param Revision Expected current objective version.
	 * @return False for stale or terminal intent; authority game thread only.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool ConfirmMissionCompleted(FGuid MissionId, int32 Revision);
	/// Returns the committed plan and phase on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadPlan GetPlan() const;
	/// Returns the latest cheap tactical snapshot on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadSnapshot GetSnapshot() const;
	/// Returns registered status records, including non-guaranteed players.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API TArray<FDemoSquadMemberStatus> GetMembers() const;
	/// Returns intelligence with confidence and uncertainty aged to now.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API TArray<FDemoSquadContact> GetContacts() const;
	/// Returns the squad identity; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FGuid GetSquadId() const;
	/// Deprecated body identity query; virtual command always returns invalid.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FGuid GetLeaderUnitId() const;
	/** @brief Updates the exact source mission's execution gate; game thread.
	 * @param Source Current upstream authority actor.
	 * @param Epoch Exact membership lease.
	 * @param MissionId Current mission identity.
	 * @param Revision Current mission revision.
	 * @param GateVersion Increasing permission version; repeats are idempotent.
	 * @param bAllowed Whether ordinary task execution may proceed.
	 * @return False for stale authority, identity or permission version.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool SetExecutionPermitFromSource(AActor* Source, int32 Epoch, FGuid MissionId, int32 Revision,
	                                               int32 GateVersion, bool bAllowed);
	/// Returns the current execution permission on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API bool IsExecutionPermitted() const;
	/** @brief Validates or restores stable identity; authority game thread.
	 * @param Identity Valid saved or
	 * authored identity.
	 * @param bApply False validates without changing the local execution.
	 * @return False for
	 * invalid or conflicting world identity.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool RestoreIdentity(FGuid Identity, bool bApply = true);
	/// Returns current command policy on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API EDemoSquadCommandMode GetCommandMode() const;
	/// Returns player-selected tactical intent on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API EDemoSquadTactic GetManualTactic() const;
	/// Borrows committed change notifications; remove owned game-thread binds.
	DEMOGAME_API Nelaric::Squad::FChanged& OnChanged();
	/// Shared roster and succession configuration; null uses native defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	TObjectPtr<UDemoSquadDefinition> Definition;
	/// Shared tactical timing and thresholds; null uses native defaults.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	TObjectPtr<UDemoSquadTactics> Tactics;

public:
	/// Optional authority script adapter; unbound during world teardown.
	UPROPERTY(Transient)
	FDemoSquadMemberOrderPolicy MemberOrderHandler;
	bool UsesMemberOrderPolicy() const;
	UDemoSquadContextComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool RegisterMember(UDemoSquadMemberComponent* Member);
	void UnregisterMember(FGuid UnitId, int32 BindingGeneration);
	void ReportMember(const FDemoSquadMemberStatus& Status);
	void ReportContact(const FDemoSquadContact& Contact);
	bool IsCurrentIdentity(const FDemoSquadRequestIdentity& Identity) const;
	bool StartExecution(UObject* Driver);
	void StopExecution(UObject* Driver);
	bool ResolveLeadership();
	bool BuildPlan(EDemoSquadTactic Tactic, EDemoSquadGoalSource GoalSource, bool bSplitSupport,
	               int32 SupportGroupSize);
	bool BeginPhase(const FDemoSquadPhaseSettings& Settings);
	void MaintainPhase();
	bool IsPhaseReady() const;
	bool IsRosterReady() const;
	void CompletePlan();
	void ReportMissionFailure(EDemoSquadFailure Failure);
	void DegradeOrders();
	void CleanupSquad();
	bool IsLeadershipReady() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ApplyMission(const FDemoSquadMission& InMission);
	void ReconcileMissionSource();
	void CancelSourceMission();
	const UDemoSquadDefinition& GetDefinition() const;
	const UDemoSquadTactics& GetTactics() const;
	FDemoSquadMemberStatus* FindMember(FGuid UnitId);
	const FDemoSquadMemberStatus* FindMember(FGuid UnitId) const;
	void Wake();
	void Update();
	void CommitChanged();
	void RebuildSnapshot();
	void SetMissionOutcome(EDemoSquadMissionState State, EDemoSquadFailure Failure = EDemoSquadFailure::None);
	void LoseLeadership();
	void CancelPlan(bool bKeepSnapshot = false);
	void HandleRoute(FDemoSquadRequestIdentity Identity, EDemoSquadFailure Failure, const TArray<FVector>& Points);
	bool AssignPositions();
	bool UpdatePolicyOrder(FDemoSquadAssignment& Assignment);
	TMap<FGuid, FVector> PolicyGoals;
	double NextPolicyRefreshAt = 0.0;
	bool AssignPosition(FDemoSquadAssignment& Assignment, FVector Center);
	void PublishOrders(bool bOnlyChanged);
	void SetPhase(EDemoSquadPhase Phase);
	void FailPlan(EDemoSquadFailure Failure);
	bool PhaseSatisfied() const;
	bool RequiredMembersAvailable() const;
	void UpdateFormation();
	FVector ChooseRallyPosition() const;
	FDemoSquadArea ResolveGoal(EDemoSquadGoalSource Source) const;
	UPROPERTY(Transient)
	FDemoSquadMission Mission;
	UPROPERTY(Transient)
	FDemoSquadMissionResult MissionResult;
	UPROPERTY(Transient)
	FDemoSquadPlan Plan;
	UPROPERTY(Transient)
	FDemoSquadSnapshot Snapshot;
	TWeakObjectPtr<AActor> MissionSource;
	int32 MissionSourceEpoch = 0;
	bool bHasMissionSource = false;
	bool bMissionFromSource = false;
	int32 ReportSequence = 0;
	int32 TaskSequence = 0;
	int32 ConsecutivePlanFailures = 0;
	int32 ExecutionGateVersion = 0;
	bool bExecutionPermitted = true;
	double SnapshotObservedAt = -1.0;
	UPROPERTY(Transient)
	TArray<FDemoSquadMemberStatus> Members;
	UPROPERTY(Transient)
	TArray<FDemoSquadContact> Contacts;
	TMap<FGuid, TWeakObjectPtr<UDemoSquadMemberComponent>> MemberComponents;
	TWeakObjectPtr<UObject> ExecutionDriver;
	Nelaric::Squad::FChanged Changed;
	FTimerHandle UpdateTimer;
	FTimerHandle ChangedTimer;
	UPROPERTY(EditAnywhere, SaveGame, Category = "Demo|Squad")
	FGuid SquadId;
	FGuid RouteRequestId;
	int64 CommandEpoch = 1;
	int32 RequestGeneration = 0;
	EDemoSquadCommandMode CommandMode = EDemoSquadCommandMode::Autonomous;
	EDemoSquadTactic ManualTactic = EDemoSquadTactic::Idle;
	bool bUpdating = false;
	bool bEnding = false;
	bool bMissionCompleted = false;
	bool bPhaseDispatched = false;
	FDemoSquadPhaseSettings PhaseSettings;
	bool bSplitGroups = false;
};
