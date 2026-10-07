// Copyright (c) 2026 Nelaric Contributors

/** @file DemoPlayerController.h
 * Declares the demo's overview and character-control presentation.
 */

#pragma once

#include "NelaricGasPlayerController.h"
#include "AI/DemoCommandTypes.h"
#include "Engine/EngineTypes.h"
#include "Equipment/DemoEquipmentTypes.h"

#include "DemoPlayerController.generated.h"

class ADemoOverviewPawn;
class UCameraComponent;

/// Local presentation state; authority remains with the control coordinator.
UENUM(BlueprintType)
enum class EDemoControlMode : uint8
{
	/// Shows the overview camera and accepts takeover requests.
	Overview,
	/// Waits for authority approval and local character readiness.
	TakingControl,
	/// Shows and operates the locally possessed character.
	ControllingCharacter,
	/// Waits for authority approval and local overview pawn readiness.
	ReturningControl,
};

/** @brief Coordinates demo control changes and camera presentation.
 * @details The world owns this controller.
 * It retains its overview pawn
 * while operating characters and destroys that pawn on authority teardown.
 * All
 * methods and Blueprint events run on the game thread. The existing
 * coordinator handles both character takeover and
 * return to the overview.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoPlayerController : public ANelaricGasPlayerController
{
	GENERATED_BODY()

public:
	/** @brief Requests control of a selectable demo character.
	 * @details Local-player only, on the game thread. Pending requests prevent
	 * another send. Decisions and readiness update the local mode later.
	 * The inherited request cannot be cancelled after sending.
	 * @param TargetPawn Character in this world; the reference is borrowed.
	 * @return Whether a request was sent, not whether it was approved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Control")
	DEMOGAME_API bool TakeControlOfBot(APawn* TargetPawn);

	/** @brief Hands the current character back and restores the overview.
	 * @details Local-player only. Also works after approval while character
	 * input is still initializing. The transport cannot cancel a sent request.
	 * @return Whether a return request was sent; failures retain actual control.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Control")
	DEMOGAME_API bool ReturnToOverview();

	/** @brief Requests primary-weapon selection for the current character.
	 * @details Call on the game thread for
	 * the owning local player.
	 * Authority resolves its current pawn and changes equipment there.
	 *
	 * @par Result
	 * OnRifleActiveResult
	 * reports the outcome to the owning client.
	 * Equipment snapshots replicate presentation. Pending control
	 * changes reject the local send.
	 * @param bActive True selects PrimaryWeapon; false restores
	 * unarmed state.
	 * @return Whether a request was sent, not whether equipment changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment")
	DEMOGAME_API bool RequestRifleActive(bool bActive);

	/** @brief Sends company intent through the owning controller's UE RPC.
	 * @param Mission Typed intent validated again on authority.
	 * @return True when sent; OnCompanyCommandResult reports acceptance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool RequestCompanyMission(const FDemoCompanyMission& Mission);
	/// Requests a local platoon scope lock; authority validates team and roster.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool RequestPlatoonManualScope(const FString& PlatoonId, bool bLocked);
	/// Requests a typed player task through the company constraints and scope.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool RequestManualPlatoonMission(const FString& PlatoonId, const FDemoPlatoonMission& Mission);
	/// Requests the allowed command snapshot through the owning controller RPC.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool RequestCompanySnapshot();
	/// Authored server-side permission; independent of the possessed body.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Company")
	bool bCanCommandCompany = true;
	/// Authored command team; clients cannot supply an alternative team.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Company")
	uint8 CompanyCommandTeamId = 0;
	/// bAllowed platoon IDs; empty authorizes this team's complete company scope.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Company")
	TArray<FString> CompanyPlatoonScope;
	/// Returns the owning player's current presentation mode.
	UFUNCTION(BlueprintPure, Category = "Demo|Control")
	EDemoControlMode GetDemoControlMode() const
	{
		return ControlMode;
	}

	/// Returns the borrowed selected character, or null after its destruction.
	UFUNCTION(BlueprintPure, Category = "Demo|Control")
	DEMOGAME_API APawn* GetSelectedBot() const;

	/// Reports an outstanding request or a local readiness wait.
	UFUNCTION(BlueprintPure, Category = "Demo|Control")
	bool IsWaitingForControl() const
	{
		return ControlMode == EDemoControlMode::TakingControl || ControlMode == EDemoControlMode::ReturningControl;
	}

	/// Returns the retained overview pawn, or null before spawning replicates.
	UFUNCTION(BlueprintPure, Category = "Demo|Camera")
	DEMOGAME_API ADemoOverviewPawn* GetOverviewPawn() const;

	/// Returns the borrowed overview camera component, or null if unavailable.
	UFUNCTION(BlueprintPure, Category = "Demo|Camera")
	DEMOGAME_API UCameraComponent* GetOverviewCamera() const;

	/// Camera transition time in seconds; zero switches immediately.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Camera", meta = (ClampMin = "0.0"))
	float CameraBlendTime = 0.35f;

	/** @brief Seconds before reporting a delayed decision or readiness.
	 * @details Reporting does not cancel authority work. The controller keeps
	 * reconciling actual possession and blocks a second outstanding request.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Control", meta = (ClampMin = "0.1"))
	float ControlWaitTimeout = 10.0f;

public:
	DEMOGAME_API ADemoPlayerController();
	DEMOGAME_API virtual void BeginPlay() override;
	DEMOGAME_API virtual void ReceivedPlayer() override;
	DEMOGAME_API virtual void PlayerTick(float DeltaTime) override;
	DEMOGAME_API virtual void TickActor(float DeltaTime, ELevelTick TickType,
	                                    FActorTickFunction& ThisTickFunction) override;
	DEMOGAME_API virtual void OnPossess(APawn* InPawn) override;
	DEMOGAME_API virtual void OnUnPossess() override;
	DEMOGAME_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	DEMOGAME_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** @brief Delivers a company command decision and an allowed snapshot.
	 * @param bAccepted Whether authority accepted the requested operation.
	 * @param Snapshot Friendly command facts; empty on authorization failure.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Company")
	void OnCompanyCommandResult(bool bAccepted, const FString& Snapshot);
	/** @brief Presents the authority result on the owning client.
	 * @details Runs on the game thread. Success may
	 * repeat an existing state;
	 * the equipment manager owns replicated model and animation presentation.
	 *
	 * @param bActive Requested primary-weapon selection.
	 * @param Result Equipment operation outcome.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Equipment")
	void OnRifleActiveResult(bool bActive, EDemoEquipmentResult Result);

	/** @brief Maps return intent to a coordinated switch to the owned overview.
	 * @details Runs on authority. The
	 * character's bot handback and GAS transfer
	 * remain part of the same transaction as possessing the overview
	 * pawn.
	 * @param Action Take a character or return to the overview.
	 * @param TargetPawn Selected character;
	 * null for a return request.
	 * @return Coordinator result; missing overview preparation rejects return.
	 */
	DEMOGAME_API virtual EControlSwitchResult HandleControlSwitchRequest_Implementation(EControlSwitchAction Action,
	                                                                                    APawn* TargetPawn) override;

	/** @brief Updates UI when the owning player's presentation state changes.
	 * @param PreviousMode State before the update.
	 * @param NewMode State after camera presentation is updated.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Presentation")
	void OnDemoModeChanged(EDemoControlMode PreviousMode, EDemoControlMode NewMode);

	/** @brief Updates selection highlighting on the owning client.
	 * @param PreviousBot Previous target, possibly null.
	 * @param NewBot New target, possibly null.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Presentation")
	void OnSelectedTargetChanged(APawn* PreviousBot, APawn* NewBot);

	/** @brief Presents a local rejection or an authority failure.
	 * @param Action Requested operation.
	 * @param Result Domain error; Succeeded is never reported here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Presentation")
	void OnControlRequestFailed(EControlSwitchAction Action, EControlSwitchResult Result);

	/** @brief Reports a wait once per phase without cancelling the operation.
	 * @param Action Operation whose response or local setup is delayed.
	 * @param bWaitingForDecision True before the authority decision arrives.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Presentation")
	void OnControlWaitTimedOut(EControlSwitchAction Action, bool bWaitingForDecision);

	/** @brief Presents a possessed character after GAS and input become ready.
	 * @param ControlledPawn Borrowed character ready for player input.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Presentation")
	void OnControlledCharacterReady(APawn* ControlledPawn);

	/// Reports a missing overview pawn once until its camera becomes available.
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Presentation")
	void OnOverviewCameraUnavailable();

private:
	UFUNCTION(Server, Reliable)
	void ServerCompanyMission(const FDemoCompanyMission& Mission);
	UFUNCTION(Server, Reliable)
	void ServerCompanyScope(const FString& PlatoonId, bool bLocked);
	UFUNCTION(Server, Reliable)
	void ServerManualPlatoonMission(const FString& PlatoonId, const FDemoPlatoonMission& Mission);
	UFUNCTION(Server, Reliable)
	void ServerCompanySnapshot();
	UFUNCTION(Client, Reliable)
	void ClientCompanyResult(bool bAccepted, const FString& Snapshot);
	UFUNCTION(Server, Reliable)
	void ServerSetRifleActive(bool bActive);
	UFUNCTION(Client, Reliable)
	void ClientReportRifleResult(bool bActive, EDemoEquipmentResult Result);
	void ReportRifleResult(bool bActive, EDemoEquipmentResult Result);

	UPROPERTY(Transient, Replicated)
	TObjectPtr<ADemoOverviewPawn> OverviewPawn;
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> SelectedBot;
	TWeakObjectPtr<APawn> PendingTarget;
	TWeakObjectPtr<APawn> PresentedPawn;
	FDelegateHandle DecisionHandle;
	EDemoControlMode ControlMode = EDemoControlMode::Overview;
	EControlSwitchAction PendingAction = EControlSwitchAction::TakeControl;
	EControlSwitchResult PendingDecision = EControlSwitchResult::InvalidRequest;
	int32 PendingRequestId = 0;
	double WaitStartedAt = 0.0;
	bool bLocalInitialized = false;
	bool bEndingPlay = false;
	bool bSendingRequest = false;
	bool bPendingRequest = false;
	bool bHasDecision = false;
	bool bWaitTimeoutReported = false;
	bool bRestoreOverviewRequested = false;
	bool bCameraUnavailableReported = false;
	bool bPresentationApplied = false;

	void InitializeLocalDemo();
	bool EnsureAuthorityOverviewPawn();
	void RestoreAuthorityOverview();
	void SetMode(EDemoControlMode NewMode);
	void SetSelectedBot(APawn* NewBot);
	bool IsSelectableBot(APawn* ControlledPawn) const;
	bool IsOverviewReady(APawn* ControlledPawn) const;
	bool IsPawnInputReady(APawn* ControlledPawn) const;
	bool IsCharacterReady(APawn* ControlledPawn) const;
	bool BeginControlRequest(EControlSwitchAction Action, APawn* Target);
	void HandleControlDecision(int32 RequestId, EControlSwitchAction Action, APawn* Target,
	                           EControlSwitchResult Result);
	void ClearPendingRequest();
	void UpdateLocalControl();
	void ReconcilePossession();
	void ReportWaitTimeout();
};
