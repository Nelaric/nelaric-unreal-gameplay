// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSoldierComponent.h Declares reusable authority soldier logic. */
#pragma once

#include "AI/DemoSoldierTypes.h"
#include "AITypes.h"
#include "Pawn/PawnInitStateComponent.h"
#include "Equipment/DemoWeaponTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "DemoSoldierComponent.generated.h"

class AAIController;
class ADemoCharacter;
class ADemoSoldierCoverPoint;
class UDemoWeaponInstance;
class UDemoEquipmentManagerComponent;
class USceneComponent;
class UAbilitySystemComponent;
class UPawnInitializationComponent;
struct FPathFollowingResult;
struct FOnAttributeChangeData;

namespace Nelaric::Soldier
{
/// Game-thread notification; listeners may query committed component data.
DECLARE_MULTICAST_DELEGATE_OneParam(FEvent, FGameplayTag);

/// Game-thread terminal result; identities isolate canceled action callbacks.
DECLARE_MULTICAST_DELEGATE_TwoParams(FActionFinished, FGuid, EDemoSoldierTreeResult);

/// Bounded reported danger; actor identity does not reveal its live position.
struct FGrenadeDanger
{
	/// Borrowed report identity; null denotes the anonymous report slot.
	TWeakObjectPtr<AActor> Source;
	/// Observed explosion center, in world centimeters.
	FVector Location = FVector::ZeroVector;
	/// Estimated dangerous radius in centimeters, excluding safety padding.
	float Radius = 0.0f;
	/// World-time deadline after which this observation no longer applies.
	double ExpiresAt = 0.0;
};
} // namespace Nelaric::Soldier

/** @brief Executes one soldier's orders using observed combat information.
 * @details The pawn initialization DA creates this non-replicated component.
 * All APIs and events
 * run on the game thread; mutations require authority. Native brains and
 * GameAI tasks share this executor, with only one execution owner at a time.
 * Health, ammunition and shots remain with the existing character and item.
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable, ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class UDemoSoldierComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	/** @brief Accepts or replaces an order; None withdraws the current intent.
	 * @details Authority game thread only. Invalid data leaves intent intact.
	 * A valid order receives an identity. Combat interruptions retain it.
	 * @param Order Finite coordinates and positive movement tolerances.
	 * @return Whether the intent was accepted.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API bool IssueOrder(const FDemoSoldierOrder& Order);

	/// Returns the current intent on the game thread, including its identity.
	UFUNCTION(BlueprintPure, Category = "Demo|Soldier")
	DEMOGAME_API FDemoSoldierOrder GetOrder() const;

	/// Returns the current intent's outcome on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Soldier")
	DEMOGAME_API EDemoSoldierOrderStatus GetOrderStatus() const;

	/// Returns the retained order's structured failure; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Soldier")
	DEMOGAME_API EDemoSoldierOrderFailure GetOrderFailure() const;

	/// Returns observed contacts with actual observation times; game thread.
	DEMOGAME_API TArray<FDemoSoldierContact> GetObservedContacts() const;

	/** @brief Cancels intent only when its identity still matches.
	 * @param OrderId Retained intent identity expected by the caller.
	 * @return False for a stale identity; authority game thread only.
	 */
	DEMOGAME_API bool CancelOrder(FGuid OrderId);

	/** @brief Updates a retained moving goal without replacing its action.
	 * @param OrderId Retained intent identity expected by the caller.
	 * @param Location Finite goal inside the existing movement boundary.
	 * @return False for stale, terminal or incompatible intent; game thread.
	 */
	DEMOGAME_API bool UpdateOrderGoal(FGuid OrderId, FVector Location);

	/** @brief Updates observation heading without replacing the owned action.
	 * @param OrderId Expected retained intent identity.
	 * @param Direction Finite nonzero horizontal observation direction.
	 * @return False for stale or invalid intent; authority game thread only.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API bool UpdateOrderFacing(FGuid OrderId, FVector Direction);

	/// Returns a copy of observed combat information on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Soldier")
	DEMOGAME_API FDemoSoldierMemory GetMemory() const;

	/// Returns the current native action on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Soldier")
	DEMOGAME_API EDemoSoldierBehavior GetBehavior() const;

	/** @brief Clears orders and observations for an explicitly new soldier.
	 * @details Authority game thread only; cancels current actions. Does not
	 * reset health or ammunition. Control handbacks preserve orders instead.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API void ResetSoldierState();

	/** @brief Reports visual information from a perception adapter.
	 * @details Authority game thread only. Losing sight preserves location.
	 * @param Actor Observed identity; only hostile actors are remembered.
	 * @param Location Observed world position, never an unseen live query.
	 * @param bVisible Whether the visual observation succeeded.
	 */
	DEMOGAME_API void ReportSight(AActor* Actor, FVector Location, bool bVisible);

	/** @brief Records an audible cue without creating visual contact.
	 * @details Authority game thread only; invalid coordinates are ignored.
	 * @param Location Observed sound location, in world centimeters.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API void ReportSound(FVector Location);

	/** @brief Records committed damage without granting unseen target data.
	 * @details Authority game thread only. Only existing contacts gain threat.
	 * @param Source Optional damage identity, borrowed without ownership.
	 * @param Amount Positive damage magnitude, used for suppression.
	 * @param TowardSource Directional cue; zero means unknown direction.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API void ReportDamage(AActor* Source, float Amount, FVector TowardSource);

	/** @brief Adds transient pressure without modifying health.
	 * @details Authority game thread only; intensity is clamped to [0, 1].
	 * @param Intensity Additional suppression from an observed near miss.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API void ReportNearMiss(float Intensity);

	/** @brief Reports or updates one grenade's estimated danger region.
	 * @details Authority game thread only. At most eight reports are retained;
	 * source identity updates its existing report. Null uses one anonymous slot.
	 * @param Source Non-owning identity; its transform is never queried.
	 * @param Location Reported explosion center in world centimeters.
	 * @param Radius Estimated danger radius in centimeters; must be positive.
	 * @param RemainingSeconds Report lifetime in world seconds; positive only.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API void ReportGrenade(AActor* Source, FVector Location, float Radius, float RemainingSeconds);

	/** @brief Removes a reported grenade after disarming or cancellation.
	 * @details Authority game thread only; missing identities are harmless.
	 * @param Source Borrowed identity, or null for the anonymous report slot.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Soldier")
	DEMOGAME_API void ClearGrenade(AActor* Source);

	/** @brief Resolves hostility using pawn soldier teams or UE team agents.
	 * @details Game thread only. No-team and non-team actors are neutral.
	 * Override in Blueprint for game-specific faction rules.
	 * @param Actor Candidate identity, which must differ from the owner.
	 * @return Whether this soldier may autonomously engage the actor.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Demo|Soldier")
	bool IsHostile(AActor* Actor) const;

	/// Borrows next-tick game-thread notifications; remove owned bindings.
	DEMOGAME_API Nelaric::Soldier::FEvent& OnEvent();

	/// Returns the injected team, or 255 without a demo owner; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Soldier")
	DEMOGAME_API uint8 GetTeamId() const;

	/// Authored decision and action tuning; sampled on the game thread.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Soldier")
	FDemoSoldierSettings Settings;

public:
	UDemoSoldierComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void RequestExecutionStart();
	bool StartExecution(AAIController* Controller, UObject* Driver, bool bNativePlanner = true);
	void StopExecution(UObject* Driver);
	bool TestTreeCondition(EDemoSoldierTreeTest Test) const;
	bool BeginTreeAction(EDemoSoldierTreeAction Action, UObject* Driver, FGuid& Id);
	EDemoSoldierTreeResult GetTreeActionResult(FGuid Id) const;
	void EndTreeAction(FGuid Id);
	void EnsureInitialTreeOrder(EDemoSoldierOrderType Type, float Radius);
	Nelaric::Soldier::FActionFinished& OnTreeActionFinished();
	void NotifyOwnerDeath();
	void NotifyTeamChanged();
	bool IsExecutingFor(const UObject* Driver) const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsHostile_Implementation(AActor* Actor) const;

protected:
	/** @brief Cancels actions and queued events for an invalidated context.
	 * @details Game thread only. Retains orders for control handback; removal
	 * of the DA-created component ends its owned state.
	 */
	virtual void CancelInitGenerationWork() override;

private:
	UFUNCTION()
	void HandlePawnInitialized(UPawnInitializationComponent* Initialization);
	UFUNCTION()
	void HandlePawnInitializationRevoked(UPawnInitializationComponent* Initialization);
	void StartReadyBrain();
	bool AcceptsObservation() const;
	void Wake();
	void Schedule(float Delay);
	void Update();
	void UpdateTreeAction(double Now);
	void ScheduleTreeWake(double Now);
	void NotifyTreeActionResult();
	void RefreshTreeObservers();
	void ClearTreeObservers();
	void HandleObservedTransform(USceneComponent* Component);
	void HandleObservedHealth(const FOnAttributeChangeData& Change);
	void HandleEquipmentChanged();
	bool IsAimAligned() const;
	UFUNCTION()
	void HandleObservedEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	void RefreshPressure(double Now);
	void UpdateObservations(double Now);
	void SelectTarget(double Now);
	float ScoreContact(const FDemoSoldierContact& Contact, double Now) const;
	void ForgetTarget();
	void SetBehavior(EDemoSoldierBehavior Next, double Now);
	void CancelAction();
	void RestoreMovement();
	void SetObservationFocus(FVector Location);
	bool StartMove(FVector Destination, float Radius, bool bEmergency = false);
	void HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result);
	bool FinishMovement(double Now);
	bool IsInsideOrderArea(FVector Location) const;
	bool CanPursue() const;
	bool MustKeepMoving() const;
	void ExecuteMovingFire(double Now);
	bool IsDangerous(FVector Location) const;
	bool FindEscape(FVector& Location) const;
	bool TryCover(double Now);
	bool HasClearFiringLine() const;
	bool FindObservationPosition(FVector ObservedPoint, FVector& Location) const;
	bool CanShoot();
	bool WantsReload() const;
	bool StartReload(double Now);
	void HandleReloadFinished(FGuid Id, EDemoWeaponResult Result);
	void HandleWeaponCanceled();
	void ExecuteCombat(double Now);
	void ExecuteOrder(double Now);
	UDemoWeaponInstance* GetWeapon() const;
	void Emit(FGameplayTag Tag);
	void FlushEvents();
	void CompleteOrder(EDemoSoldierOrderStatus Status);

	UPROPERTY(Transient)
	FDemoSoldierOrder CurrentOrder;
	UPROPERTY(Transient)
	FDemoSoldierMemory Memory;
	UPROPERTY(Transient)
	TArray<FDemoSoldierContact> Contacts;
	TArray<Nelaric::Soldier::FGrenadeDanger> Grenades;
	TWeakObjectPtr<AAIController> Controller;
	TWeakObjectPtr<UPawnInitializationComponent> PawnInitialization;
	TWeakObjectPtr<UObject> ExecutionDriver;
	TWeakObjectPtr<UDemoWeaponInstance> ActionWeapon;
	TWeakObjectPtr<ADemoSoldierCoverPoint> Cover;
	TWeakObjectPtr<AActor> ActionTarget;
	TWeakObjectPtr<UDemoEquipmentManagerComponent> ObservedEquipment;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedTargetAbilitySystem;
	TMap<TWeakObjectPtr<USceneComponent>, FDelegateHandle> TransformHandles;
	TSet<TWeakObjectPtr<AActor>> ObservedActors;
	FDelegateHandle EquipmentChangedHandle;
	FDelegateHandle TargetHealthHandle;
	FTimerHandle UpdateTimer;
	FTimerHandle EventTimer;
	FTimerHandle BrainStartTimer;
	FDelegateHandle MoveHandle;
	FDelegateHandle ReloadHandle;
	FDelegateHandle WeaponCanceledHandle;
	FAIRequestID MoveId;
	FGuid ReloadId;
	FVector MoveGoal = FVector::ZeroVector;
	FVector LastSelfLocation = FVector::ZeroVector;
	EDemoSoldierOrderStatus OrderStatus = EDemoSoldierOrderStatus::None;
	EDemoSoldierOrderFailure OrderFailure = EDemoSoldierOrderFailure::None;
	EDemoSoldierBehavior Behavior = EDemoSoldierBehavior::Idle;
	double ActionDeadline = 0.0;
	double MoveDeadline = 0.0;
	double NextShotTime = 0.0;
	double LastUpdateTime = 0.0;
	double TargetSelectedTime = 0.0;
	double NextCoverTime = 0.0;
	double NextReloadTime = 0.0;
	double NextRepositionTime = 0.0;
	double ConsumedAlertTime = -1.0;
	double InvestigatedAlertTime = -1.0;
	double SearchDeadline = 0.0;
	float OriginalWalkSpeed = 0.0f;
	int32 RoundsRemaining = 0;
	int32 EscapeAttempts = 0;
	bool bMoveFinished = false;
	bool bMoveSucceeded = false;
	bool bSavedRotationYaw = false;
	bool bSavedOrientMovement = false;
	bool bSavedCrouched = false;
	bool bUpdating = false;
	bool bSuppressed = false;
	bool bNativePlanner = true;
	bool bInitialTreeOrderApplied = false;
	bool bTreeResultNotified = false;
	bool bAimWakeRequested = false;
	bool bTreeReturningToArea = false;
	FGuid TreeActionId;
	uint64 OrderRevision = 0;
	uint64 TreeOrderRevision = 0;
	EDemoSoldierTreeAction TreeAction = EDemoSoldierTreeAction::Idle;
	EDemoSoldierTreeResult TreeResult = EDemoSoldierTreeResult::Failed;
	uint64 ActionRevision = 0;
	TArray<FGameplayTag> PendingEvents;
	Nelaric::Soldier::FEvent Events;
	Nelaric::Soldier::FActionFinished TreeActionFinished;
};
