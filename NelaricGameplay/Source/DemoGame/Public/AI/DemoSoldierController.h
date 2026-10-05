// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSoldierController.h Declares perception and native soldier AI. */
#pragma once

#include "AI/NelaricBotController.h"
#include "Perception/AIPerceptionTypes.h"
#include "DemoSoldierController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Damage;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UDemoSoldierBrainComponent;
class ADemoCharacter;

/** @brief Adapts UE perception and Ready lifecycle to one soldier executor.
 * @details Authority owns this controller and its participant PlayerState.
 * Use with a DemoCharacter carrying Soldier and PawnControl components.
 * PawnControl starts the brain after initialization; possession alone does
 * not start gameplay. Unpossess stops actions before losing pawn context.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoSoldierController : public ANelaricBotController
{
	GENERATED_BODY()

public:
	/// Use the native brain; disable when adding a GameAI StateTree component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Soldier")
	bool bUseNativeBrain = true;

public:
	DEMOGAME_API ADemoSoldierController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;

private:
	void ClearTeamSubscriptions();
	void HandleObservedTeamChanged();
	TMap<TWeakObjectPtr<ADemoCharacter>, FDelegateHandle> TeamSubscriptions;
	UFUNCTION()
	void HandlePerception(AActor* Actor, FAIStimulus Stimulus);
	UPROPERTY(VisibleAnywhere, Category = "Demo|Soldier")
	TObjectPtr<UAIPerceptionComponent> SoldierPerception;
	UPROPERTY(VisibleAnywhere, Category = "Demo|Soldier")
	TObjectPtr<UAISenseConfig_Sight> Sight;
	UPROPERTY(VisibleAnywhere, Category = "Demo|Soldier")
	TObjectPtr<UAISenseConfig_Hearing> Hearing;
	UPROPERTY(VisibleAnywhere, Category = "Demo|Soldier")
	TObjectPtr<UAISenseConfig_Damage> Damage;
	UPROPERTY(VisibleAnywhere, Category = "Demo|Soldier")
	TObjectPtr<UDemoSoldierBrainComponent> SoldierBrain;
};
