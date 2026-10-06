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
	 * @return False for stale or malformed intent; previous mission is retained.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool SetMission(const FDemoSquadMission& Mission);
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
	/// Returns the current commander identity, or invalid during recovery.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FGuid GetLeaderUnitId() const;
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
	UPROPERTY(Transient)
	TArray<FDemoSquadMemberStatus> Members;
	UPROPERTY(Transient)
	TArray<FDemoSquadContact> Contacts;
	TMap<FGuid, TWeakObjectPtr<UDemoSquadMemberComponent>> MemberComponents;
	TWeakObjectPtr<UObject> ExecutionDriver;
	Nelaric::Squad::FChanged Changed;
	FTimerHandle UpdateTimer;
	FTimerHandle ChangedTimer;
	FGuid SquadId;
	FGuid LeaderUnitId;
	FGuid RouteRequestId;
	int64 CommandEpoch = 1;
	int32 RequestGeneration = 0;
	EDemoSquadCommandMode CommandMode = EDemoSquadCommandMode::Autonomous;
	EDemoSquadTactic ManualTactic = EDemoSquadTactic::Idle;
	double LeadershipLostAt = -1.0;
	bool bUpdating = false;
	bool bEnding = false;
	bool bMissionCompleted = false;
	bool bPhaseDispatched = false;
	FDemoSquadPhaseSettings PhaseSettings;
	bool bSplitGroups = false;
};
