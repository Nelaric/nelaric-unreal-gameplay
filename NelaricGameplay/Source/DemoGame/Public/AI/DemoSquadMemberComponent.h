// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadMemberComponent.h Declares squad membership and reporting. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "Pawn/PawnInitStateComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "DemoSquadMemberComponent.generated.h"

class ADemoSquadCommandActor;
class UDemoSoldierComponent;
class AController;
class UDemoEquipmentManagerComponent;
class UAbilitySystemComponent;
class USceneComponent;
struct FOnAttributeChangeData;

/** @brief Connects a persistent member identity to its current character.
 * @details Install through the initialization DA. All operations run on the
 * authority game thread. Reports use changes plus a staggered heartbeat;
 * player possession changes capability without removing squad membership.
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable, ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class UDemoSquadMemberComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()
public:
	/** @brief Registers this body with a squad, assigning an identity if absent.
	 * @param Squad World-owned command actor in the same world and team.
	 * @param InUnitId Stable unit identity; invalid assigns a fresh identity.
	 * @return False for an occupied identity, roster limit or invalid context.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool JoinSquad(ADemoSquadCommandActor* Squad, FGuid InUnitId);
	/// Cancels owned intent and unregisters this body; authority game thread.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API void LeaveSquad();
	/// Returns the world-owned command actor, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API ADemoSquadCommandActor* GetSquad() const;
	/// Returns the persistent member identity; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API FGuid GetUnitId() const;
	/// Returns the current committed capability report; game thread only.
	DEMOGAME_API FDemoSquadMemberStatus CaptureStatus() const;
	/// Preferred capability used for assignments and commander succession.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	EDemoSquadRole Role = EDemoSquadRole::Rifleman;
	/// Lower priority wins within the same succession role.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	int32 SuccessionPriority = 100;
	/// Whether this member may be selected as a commander.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	bool bCanCommand = true;
	/// Whether losing this member invalidates a captured phase requirement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	bool bRequired = false;

public:
	UDemoSquadMemberComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void ReportNow();
	void NotifyNewLife();
	int32 GetBindingGeneration() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/// Reports revoked execution readiness without withdrawing membership.
	virtual void CancelInitGenerationWork() override;
	/// Refreshes observers after the new local initialization group commits.
	virtual void OnInitReady() override;

private:
	void HandleSoldierEvent(FGameplayTag Tag);
	void BindSoldier();
	void ClearSoldier();
	void HandleEquipmentChanged();
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	void HandleBodyTransform(USceneComponent* Component);
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
	TWeakObjectPtr<ADemoSquadCommandActor> CommandActor;
	TWeakObjectPtr<UDemoSoldierComponent> ObservedSoldier;
	FDelegateHandle SoldierHandle;
	TWeakObjectPtr<UDemoEquipmentManagerComponent> ObservedEquipment;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
	TWeakObjectPtr<USceneComponent> ObservedRoot;
	FDelegateHandle EquipmentHandle;
	FDelegateHandle HealthHandle;
	FDelegateHandle TransformHandle;
	FTimerHandle HeartbeatTimer;
	FGuid UnitId;
	int32 BindingGeneration = 1;
	bool bReporting = false;
	double LastReportedAt = -1.0;
	FDemoSquadMemberStatus LastStatus;
};
