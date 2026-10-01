// Copyright (c) 2026 Nelaric Contributors

/** @file PawnControlComponent.h
 * Declares explicit pawn eligibility and a bot handback policy.
 */

#pragma once

#include "Containers/Array.h"
#include "Pawn/PawnInitStateComponent.h"
#include "Player/ControlStateTransfer.h"
#include "Templates/SubclassOf.h"

#include "PawnControlComponent.generated.h"

class AAIController;

/** @brief Opts an owning pawn into authority-validated control requests.
 *
 * @details Add this component to selectable pawns. The authority game mode
 * applies additional gameplay policy. Only the authority reads these
 * settings. Inherits reversible pawn initialization and context generations.
 * A remembered bot remains world-owned while the player controls
 * the pawn. This component destroys only idle replacement bots it spawned.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UPawnControlComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	/** @brief Checks whether this pawn is reserved for a control transition.
	 *
	 * @details Call on the game thread. Reads the authority coordinator.
	 * Includes failed recovery reservations. Returns false on clients.
	 *
	 * @return True while this pawn participates in a reserved transition.
	 */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Nelaric|Control")
	GAMEPLAYRUNTIME_API bool IsControlTransitionInProgress() const;

	/** @brief Adds an optional state export and association integration.
	 *
	 * @details Call on the game thread before control changes. This
	 * component shares ownership until removal or destruction. Duplicate IDs
	 * and changes during a reserved transition or registration callback are
	 * rejected.
	 *
	 * @param Id Unique, non-empty identity within this pawn.
	 *
	 * @param Participant Integration with lifetime-safe UObject references.
	 *
	 * @return Whether the participant was registered.
	 */
	GAMEPLAYRUNTIME_API bool
	RegisterStateTransferParticipant(FName Id, TSharedRef<Nelaric::Control::IStateTransferParticipant> Participant);

	/** @brief Removes an optional state transfer integration.
	 *
	 * @details Call on the game thread. Reserved transitions retain their
	 * registrations until completion or explicit recovery resolution.
	 *
	 * @param Id Identity supplied at registration.
	 *
	 * @return True if removed; false if absent or a change is prohibited.
	 */
	GAMEPLAYRUNTIME_API bool UnregisterStateTransferParticipant(FName Id);

	/// Whether authority requests may take control of this pawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nelaric|Control")
	bool bAllowPlayerControl = true;

	/// Whether the current player may release or switch away from this pawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nelaric|Control")
	bool bAllowReturnControl = true;

	/// Whether release must hand control to a prepared bot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nelaric|Control")
	bool bReturnToBot = true;

	/// Whether an AI-controlled Ready context starts its configured brains.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nelaric|Control")
	bool bStartBotLogicOnReady = true;

	/// Bot class used when no live, idle remembered controller is available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nelaric|Control")
	TSubclassOf<AAIController> ReturnControllerClass;

public:
	GAMEPLAYRUNTIME_API UPawnControlComponent(const FObjectInitializer& ObjectInitializer);
	GAMEPLAYRUNTIME_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	AAIController* FindReturnController() const;
	void TrackSpawnedController(AAIController* Controller);
	void ForgetSpawnedController(AAIController* Controller);
	void RememberController(AAIController* Controller);
	const TArray<Nelaric::Control::FStateParticipantRegistration>& GetStateTransferParticipants() const;
	uint64 GetStateTransferRevision() const;

protected:
	/** @brief Requires a usable player state for a locally present controller.
	 * @details Runs on the game thread. Unpossessed and remote AI pawns can
	 * initialize without a local controller. A present controller must have
	 * the same live player state as the pawn before control becomes Ready.
	 * @return Whether the local control context is available.
	 */
	GAMEPLAYRUNTIME_API virtual bool CanEntryDataAvailable() override;

	/** @brief Starts configured bot logic after the context group is Ready.
	 * @details Runs on the authority game thread. Human and client contexts
	 * do not start bot logic. Disable automatic brain startup when using this
	 * component to coordinate controller and pawn brain components.
	 */
	GAMEPLAYRUNTIME_API virtual void OnInitReady() override;

private:
	TArray<Nelaric::Control::FStateParticipantRegistration> StateTransferParticipants;
	uint64 StateTransferRevision = 0;
	bool bChangingStateTransferParticipants = false;
	bool bStateTransferRegistrationEnded = false;

	UPROPERTY(Transient)
	TObjectPtr<AAIController> RememberedController;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAIController>> SpawnedControllers;
};
