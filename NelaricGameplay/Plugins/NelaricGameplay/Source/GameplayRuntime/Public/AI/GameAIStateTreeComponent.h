// Copyright (c) 2026 Nelaric Contributors

/** @file GameAIStateTreeComponent.h */

#pragma once

#include "Components/StateTreeComponent.h"

#include "GameAIStateTreeComponent.generated.h"

/** @brief Runs the shared GameAI Schema on actors, pawns or controllers.
 *
 * @details The owning actor controls component lifetime. Uses Unreal's
 * StateTree execution and event APIs on the game thread. The asset selects
 * whether possession is required; actor-only trees use optional pawn and
 * controller context. The selected world subsystem is always required.
 * Call StartLogic after all required context and authored inputs are ready.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class UGameAIStateTreeComponent : public UStateTreeComponent
{
	GENERATED_BODY()

public:
public:
	GAMEPLAYRUNTIME_API virtual TSubclassOf<UStateTreeSchema> GetSchema() const override;

protected:
	GAMEPLAYRUNTIME_API virtual TValueOrError<void, FString> HasValidStateTreeReference() const override;
	GAMEPLAYRUNTIME_API virtual bool SetContextRequirements(FStateTreeExecutionContext& Context,
	                                                        bool bLogErrors = false) override;
};
