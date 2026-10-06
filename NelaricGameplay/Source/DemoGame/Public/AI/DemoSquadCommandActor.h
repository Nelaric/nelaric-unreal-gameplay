// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadCommandActor.h Declares a body-independent squad commander. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "GameFramework/Actor.h"
#include "DemoSquadCommandActor.generated.h"

class UDemoSquadContextComponent;
class UGameAIStateTreeComponent;

/** @brief World-owned squad executor independent of any leader character.
 * @details No model, collision or actor Tick. Register members and submit
 * a mission before starting. The authored GameAI StateTree owns all
 * tactical selection, stage sequencing and recovery decisions.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoSquadCommandActor : public AActor
{
	GENERATED_BODY()
public:
	/// Returns actor-owned squad context on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Squad")
	DEMOGAME_API UDemoSquadContextComponent* GetSquadContext() const;
	/** @brief Registers initial members before acquiring an execution driver.
	 * @return False on clients, without members or if execution cannot start.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API bool StartCommander();
	/// Stops the driver and releases active orders; authority game thread.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Squad")
	DEMOGAME_API void StopCommander();
	/// Initial members registered before commander execution begins.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Demo|Squad")
	TArray<TObjectPtr<ADemoCharacter>> InitialMembers;
	/// Automatically starts after the initial membership registration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo|Squad")
	bool bStartOnBeginPlay = true;

public:
	DEMOGAME_API ADemoSquadCommandActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Demo|Squad")
	TObjectPtr<UDemoSquadContextComponent> SquadContext;
	UPROPERTY(VisibleAnywhere, Category = "Demo|Squad")
	TObjectPtr<UGameAIStateTreeComponent> GameAITree;
};
