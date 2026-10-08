// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCompanyCommandActor.h Declares virtual command lifecycle bridges. */
#pragma once

#include "AI/DemoCommandTypes.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "DemoCompanyCommandActor.generated.h"

class UStateTreeComponent;
class ADemoSquadCommandActor;

/// Single policy receiver for authoritative battlefront input.
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FDemoBattlefrontUpdate, AActor*, Publisher, const FString&, Snapshot);

/// Routes a platoon task into the runtime owning its coordinator.
DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(EStateTreeRunStatus, FDemoPlatoonStep, const FString&, Operation);

/// Actor-owned, persistent command snapshot; tasks never own this data.
UCLASS(MinimalAPI, BlueprintType)
class UDemoCompanyContextComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	/// Publishes a bounded same-world snapshot; authority game thread only.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool PublishState(const FString& Snapshot);
	/// Returns the committed command snapshot; no observation time is refreshed.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	FString GetState() const;
	/// Logical permission generation, unrelated to soldier bodies.
	UPROPERTY(BlueprintReadOnly, Category = "Company")
	int32 CommandEpoch = 1;
	/// Explicit workflow destination selected by the persistent coordinator.
	UPROPERTY(BlueprintReadOnly, Category = "Company")
	FName NextPhase;
	/// Monotonic committed lower-layer notification sequence.
	UPROPERTY(BlueprintReadOnly, Category = "Company")
	int32 InputRevision = 0;
	/// Logical decision authority policy, independent of possession.
	UPROPERTY(BlueprintReadOnly, Category = "Company")
	FName CommandMode;
	/// Number of actual native-to-TS workflow calls in this world execution.
	UPROPERTY(BlueprintReadOnly, Category = "Company|Diagnostics")
	int64 WorkflowCalls = 0;
	/// Accumulated game-thread workflow wall time, including the TS bridge.
	UPROPERTY(BlueprintReadOnly, Category = "Company|Diagnostics")
	double WorkflowSeconds = 0.0;
	/// Slowest observed native-to-TS workflow call in world seconds.
	UPROPERTY(BlueprintReadOnly, Category = "Company|Diagnostics")
	double MaximumWorkflowSeconds = 0.0;

public:
	UDemoCompanyContextComponent();

private:
	UPROPERTY(Transient)
	FString State;
};

/// Native company assembly. A PuerTS subclass implements coordination policy.
UCLASS(MinimalAPI, Blueprintable)
class ADemoCompanyCommandActor : public AActor
{
	GENERATED_BODY()
public:
	/// Acquires the world publication lease and starts one tree; authority only.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Demo|Company")
	bool StartCommander();
	/// Stops owned execution and releases its exact lease; authority game thread.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Demo|Company")
	void StopCommander();
	/// Submits typed company intent through the sole coordinator; game thread.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Demo|Company")
	bool SubmitCompanyMission(const FDemoCompanyMission& Mission);
	/** @brief Publishes shared battlefront goals from the authority GameMode.
	 * @param Publisher Current world's GameMode; never a client request.
	 * @param Snapshot Bounded JSON matching the battlefront snapshot contract.
	 * @return False for invalid, stale, or conflicting input; game thread only.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Company")
	bool UpdateBattlefrontState(AActor* Publisher, const FString& Snapshot);
	/// Sets a recipient's player scope without invalidating unrelated tasks.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Demo|Company")
	bool SetPlatoonManualScope(const FString& PlatoonId, bool bLocked);
	/// Submits a player-authorized platoon task through the same constraints.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Demo|Company")
	bool SubmitManualPlatoonMission(const FString& PlatoonId, const FDemoPlatoonMission& Mission);
	/// Returns the actor-owned persistent snapshot on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	UDemoCompanyContextComponent* GetCompanyContext() const;
	/// Returns the sole generic actor StateTree component on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	UStateTreeComponent* GetCommandTree() const;
	/// Stable identity and allowed platoon roster.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	TObjectPtr<UDemoCompanyDefinition> Definition;
	/// Configurable bounded command scheduling.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	TObjectPtr<UDemoCompanyPolicy> Policy;
	/// Optional initial typed mission.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	TObjectPtr<UDemoCompanyMissionAsset> InitialMission;
	/// Explicit same-team platoon command actors; never soldier references.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Company")
	TArray<TObjectPtr<AActor>> Platoons;
	/// Whether the uniform level bootstrap starts command at BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Company")
	bool bStartOnBeginPlay = true;

public:
	ADemoCompanyCommandActor();
	UFUNCTION(BlueprintNativeEvent, Category = "Demo|Company")
	EStateTreeRunStatus StepCompany(FName Phase);
	virtual bool StartCommander_Implementation();
	virtual void StopCommander_Implementation();
	virtual bool SubmitCompanyMission_Implementation(const FDemoCompanyMission& Mission);
	UPROPERTY(Transient)
	FDemoBattlefrontUpdate BattlefrontUpdateHandler;
	virtual bool SetPlatoonManualScope_Implementation(const FString& PlatoonId, bool bLocked);
	virtual bool SubmitManualPlatoonMission_Implementation(const FString& PlatoonId,
	                                                       const FDemoPlatoonMission& Mission);
	virtual EStateTreeRunStatus StepCompany_Implementation(FName Phase);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDemoCompanyContextComponent> Context;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStateTreeComponent> Tree;
};

/// Generic Actor-schema task context; the authored operation stays in TS.
UCLASS(Abstract, MinimalAPI, Blueprintable)
class UDemoCompanyStateTreeTask : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()
public:
	/// Actual virtual company supplied by the generic Actor schema.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<ADemoCompanyCommandActor> Actor;
	/// Workflow operation selected by this leaf state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameter)
	FName Operation;

public:
};

/// Explicit destination condition for transitions; not an implicit selector.
USTRUCT()
struct FDemoCompanyNextPhaseData
{
	GENERATED_BODY()
	/// Virtual command actor bound by the generic schema.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ADemoCompanyCommandActor> Actor;
	/// Required destination in the committed context.
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName Phase;
};

/// Tests the persistent workflow destination on the game thread.
USTRUCT(meta = (DisplayName = "Company Next Phase"))
struct FDemoCompanyNextPhase : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
public:
	/// Reflected context bound by the StateTree compiler.
	using FInstanceDataType = FDemoCompanyNextPhaseData;

public:
	virtual const UStruct* GetInstanceDataType() const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/// One workflow operation with the generic Actor-schema context.
USTRUCT()
struct FDemoCompanyWorkflowData
{
	GENERATED_BODY()
	/// Actual virtual command owner supplied by the schema.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ADemoCompanyCommandActor> Actor;
	/// Explicit operation selected by the authored leaf state.
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName Operation;
};

/// Calls the TS policy through a native bridge; no teardown of assignments.
USTRUCT(meta = (DisplayName = "Company Workflow"))
struct FDemoCompanyWorkflowTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
public:
	/// Reflected persistent-owner binding and atomic operation.
	using FInstanceDataType = FDemoCompanyWorkflowData;

public:
	FDemoCompanyWorkflowTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

/// Native receiving mailbox enforces scope and versions before TS activation.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class UDemoCompanyMembershipComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	/// Executes one task operation in its owning script runtime; authority only.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	EStateTreeRunStatus StepPlatoon(const FString& Operation);
	/// Claims the exact registered company source; authority game thread only.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	int32 ClaimCompany(AActor* Source, const FString& PlatoonId);
	/// Releases the matching source membership; obsolete callers do no work.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool ReleaseCompany(AActor* Source, int32 Revision);
	/// Validates and retains one candidate; old active intent remains intact.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool StageAssignment(AActor* Source, const FString& Assignment, bool bPlayerAuthorized = false);
	/// Rebinds saved intent without granting execution or invoking lower actions.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool ReconcileAssignment(AActor* Source, const FString& Assignment);
	/// Validates an exact permission before activating retained intent.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool ActivateAssignment(AActor* Source, const FDemoExecutionPermit& Permit);
	/// Cancels only the exact active/candidate task identity and revision.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool CancelAssignment(AActor* Source, const FString& Id, int32 Revision);
	/// Updates a manual scope lock from the current registered company.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool SetManualScope(AActor* Source, bool bLocked);
	/// Returns the exact membership generation; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	int32 GetMembershipRevision() const;
	/// Returns the current borrowed logical source, or null after teardown.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	AActor* GetCompanySource() const;
	/// Registers one owned squad and forwards committed changes; game thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool WatchSquad(ADemoSquadCommandActor* Squad);
	/// Removes only this component's previous squad subscriptions; game thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	void ReleaseSquadWatches();
	/// Returns the exact registered squad roster for rule evidence validation.
	const TArray<TWeakObjectPtr<ADemoSquadCommandActor>>& GetSquads() const;
	/// Local lower-layer update sequence used by the platoon task.
	UPROPERTY(BlueprintReadOnly, Category = "Company")
	int32 InputRevision = 0;

public:
	UPROPERTY(Transient)
	FDemoPlatoonStep StepHandler;
	UDemoCompanyMembershipComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool ValidSource(AActor* Source) const;
	void HandleSquadChanged(TWeakObjectPtr<ADemoSquadCommandActor> Squad);
	TArray<TWeakObjectPtr<ADemoSquadCommandActor>> Squads;
	TArray<FDelegateHandle> SquadHandles;
	TMap<TWeakObjectPtr<ADemoSquadCommandActor>, uint32> SquadSignatures;
	TWeakObjectPtr<AActor> Company;
	FString Identity;
	FString Candidate;
	FString ActiveId;
	int32 ActiveRevision = 0;
	int32 MembershipRevision = 0;
	bool bManualScope = false;
	bool bCandidatePlayerAuthorized = false;
};
