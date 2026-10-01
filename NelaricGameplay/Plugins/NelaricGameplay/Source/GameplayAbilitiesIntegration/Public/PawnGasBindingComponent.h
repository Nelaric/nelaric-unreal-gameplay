// Copyright (c) 2026 Nelaric Contributors

/** @file PawnGasBindingComponent.h Declares transactional ASC binding. */
#pragma once
#include "Pawn/PawnInitStateComponent.h"
#include "GasStateProfile.h"
#include "GasTransferExtension.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "PawnGasBindingComponent.generated.h"
class ANelaricGasPlayerState;
class UNelaricAbilitySystemComponent;
namespace Nelaric::GAS
{
class FTransferParticipant;
struct FSnapshot;
} // namespace Nelaric::GAS

/** @brief Binds a pawn to its participant ASC and transfers owned state.
 * @details Add one component alongside PawnControlComponent. Register
 * extensions before control requests. The world owns actors; custody
 * retains attributes in a PlayerState ASC when no controller is present.
 * All public operations use the game thread.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UPawnGasBindingComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()
public:
	/** @brief Returns this pawn's currently bound participant or custody ASC.
	 * @details Borrowed during the owner's lifetime. May be null while
	 * replicated references are unresolved. Check IsReadyForActions first.
	 * @return Bound ASC, never a pawn-owned duplicate.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API UNelaricAbilitySystemComponent* GetAbilitySystem() const;
	/** @brief Checks committed input readiness on this machine.
	 * @details Authority also rejects reserved or failed control operations.
	 * @return Whether this pawn can submit ability actions.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API bool IsReadyForActions() const;
	/** @brief Adds a custom state migration adapter before control requests.
	 * @details Shared ownership until removal or component teardown. Changes
	 * while reserved are rejected. Adapter must own its payload safely.
	 * @param Id Unique non-empty identity on this pawn.
	 * @param Extension Domain-specific effect, ability or task migration.
	 * @return Whether the adapter was registered.
	 */
	GAMEPLAYABILITIESINTEGRATION_API bool
	RegisterTransferExtension(FName Id, TSharedRef<Nelaric::GAS::ITransferExtension> Extension);
	/** @brief Removes a custom adapter outside control transitions.
	 * @param Id Identity supplied during registration.
	 * @return Whether a matching adapter was removed.
	 */
	GAMEPLAYABILITIESINTEGRATION_API bool UnregisterTransferExtension(FName Id);
	/** @brief Checks whether the replicated state transaction has committed.
	 * @details Read on the game thread; does not query ASC input gating.
	 * @return Whether actor info matches a committed state owner.
	 */
	GAMEPLAYABILITIESINTEGRATION_API bool HasCommittedState() const;
	/** @brief Restores retained state after gameplay repairs native control.
	 * @details Authority game thread only. Uses the coordinator's retained
	 * export. Call ResolveControlTransitionRecovery after every pawn repairs.
	 * @return Whether state and association were repaired at the current end.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API bool RestoreReservedState();
	/// Immutable pawn attributes, abilities and control effect configuration.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|GAS")
	TObjectPtr<UGasStateProfile> StateProfile;

public:
	GAMEPLAYABILITIESINTEGRATION_API UPawnGasBindingComponent(const FObjectInitializer& ObjectInitializer);
	GAMEPLAYABILITIESINTEGRATION_API virtual void EndPlay(EEndPlayReason::Type Reason) override;
	GAMEPLAYABILITIESINTEGRATION_API virtual void
	GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** @brief Waits for policy registration and participant attribute sets.
	 * @return Whether state references and the transfer registration exist.
	 */
	GAMEPLAYABILITIESINTEGRATION_API virtual bool CanEntryDataAvailable() override;
	/** @brief Initializes actor info and installs new pawn state once.
	 * @return Whether the complete pawn state is bound.
	 */
	GAMEPLAYABILITIESINTEGRATION_API virtual bool CanEntryDataInitialized() override;

private:
	friend class Nelaric::GAS::FTransferParticipant;
	UPROPERTY(ReplicatedUsing = OnRep_StateOwner)
	TObjectPtr<ANelaricGasPlayerState> StateOwner;
	UPROPERTY(ReplicatedUsing = OnRep_StateOwner)
	uint32 BindingRevision = 0;
	UPROPERTY(ReplicatedUsing = OnRep_StateOwner)
	bool bStateCommitted = false;
	UPROPERTY(Transient)
	TObjectPtr<ANelaricGasPlayerState> Custodian;
	TMap<FName, TSharedPtr<Nelaric::GAS::ITransferExtension>> Extensions;
	TSharedPtr<Nelaric::Control::IStateTransferParticipant> Participant;
	TWeakObjectPtr<UNelaricAbilitySystemComponent> GrantsOwner;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;
	TArray<FActiveGameplayEffectHandle> ControlEffects;
	bool bPawnStateInstalled = false;
	bool bInitialBindingQueued = false;
	bool bInitialBindingAllowed = false;
	bool bParticipantRegistered = false;
	FTimerHandle ReplicationRetry;
	TWeakObjectPtr<UNelaricAbilitySystemComponent> ClientBoundASC;
	UFUNCTION()
	void OnRep_StateOwner();
	bool EnsureCustodian();
	bool HasLayout(UNelaricAbilitySystemComponent* ASC) const;
	bool InstallGrants(UNelaricAbilitySystemComponent* ASC, bool bIncludeAbilities = true);
	bool RemoveGrants(UNelaricAbilitySystemComponent* ASC);
};
