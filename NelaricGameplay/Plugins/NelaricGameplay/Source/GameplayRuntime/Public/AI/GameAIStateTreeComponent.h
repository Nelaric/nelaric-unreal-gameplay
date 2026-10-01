// Copyright (c) 2026 Nelaric Contributors

/** @file GameAIStateTreeComponent.h */

#pragma once

#include "Components/StateTreeComponent.h"

#include "GameAIStateTreeComponent.generated.h"

/** @brief Runs GameAI Schema assets on a pawn or its controller.
 *
 * @details The owning actor controls component lifetime. Uses Unreal's
 * StateTree execution and event APIs on the game thread. Execution requires
 * possession and the selected world subsystem. Disable automatic startup
 * when possession occurs after BeginPlay, then call StartLogic when ready.
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
