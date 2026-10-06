// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadOrderReceiverComponent.h Defines the member order boundary. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "Pawn/PawnInitStateComponent.h"
#include "DemoSquadOrderReceiverComponent.generated.h"

class UDemoSquadMemberComponent;

/** @brief Retains versioned squad intent and reports actual execution.
 * @details Install through the pawn initialization DA with the soldier and
 * member components. All calls require the authority game thread. Orders
 * survive player possession, but do not drive player movement or weapons.
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable, ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class UDemoSquadOrderReceiverComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()
public:
	/** @brief Validates current authority and accepts a member intent.
	 * @details Same identity and revision are idempotent. Stale, foreign and
	 * malformed submissions leave current intent intact; returns false.
	 * @param Order Intent issued by this member's current squad.
	 * @return Whether the intent was accepted or already retained.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool ReceiveOrder(const FDemoSquadMemberOrder& Order);
	/** @brief Cancels matching intent and retains bounded local self-defense.
	 * @param OrderId Expected order identity.
	 * @return Whether the matching intent was cancelled.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool CancelOrder(FGuid OrderId);
	/// Returns retained squad intent on the game thread, including HUD goals.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadMemberOrder GetOrder() const;
	/// Returns committed feedback on the game thread; Ready is revocable.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FDemoSquadMemberFeedback GetFeedback() const;
	/// Returns whether a nonterminal order remains retained; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API bool HasActiveOrder() const;

public:
	UDemoSquadOrderReceiverComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void RefreshExecution();
	void LimitRetainedOrder(double ExpiresAt);
	void ResetReceiver();
	void EnterDegradedHold();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool SubmitSoldierIntent();
	bool HasSupportSolution() const;
	void SetFeedback(EDemoSquadOrderState State, EDemoSquadFailure Failure, bool bReady);
	UPROPERTY(Transient)
	FDemoSquadMemberOrder CurrentOrder;
	UPROPERTY(Transient)
	FDemoSquadMemberFeedback Feedback;
	FGuid SoldierOrderId;
	FVector SearchBaseFacing = FVector::ZeroVector;
	bool bHasOrder = false;
	bool bSubmitted = false;
	bool bDegradedHold = false;
};
