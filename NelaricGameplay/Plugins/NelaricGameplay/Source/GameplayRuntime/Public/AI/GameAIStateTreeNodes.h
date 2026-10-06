// Copyright (c) 2026 Nelaric Contributors

/** @file GameAIStateTreeNodes.h */

#pragma once

#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"

#include "GameAIStateTreeNodes.generated.h"

class AActor;
class AController;
class APawn;
class UGameAIContextSubsystem;
class UStateTreeComponent;

/** @brief Common instance data for native GameAI nodes.
 *
 * @details The StateTree compiler automatically binds these Context fields.
 * Derive custom instance data from this type and override GetInstanceDataType
 * in the node when adding fields. References belong to the current execution;
 * do not retain them across frames or use them outside the game thread.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYRUNTIME_API FGameAIStateTreeContext
{
	GENERATED_BODY()

	/// Actual actor owning the executing component; may be a controller.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<AActor> OwnerActor = nullptr;

	/// Pawn resolved from possession; may be null for actor-only trees.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<APawn> Pawn = nullptr;

	/// Current controller; may be null for an actor-only authored tree.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<AController> Controller = nullptr;

	/// World-owned AI service selected by the asset's Schema.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<UGameAIContextSubsystem> GameContext = nullptr;

	/// Executing component supplied directly, without an actor search.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<UStateTreeComponent> StateTreeComponent = nullptr;
};

/// Native task extension point accepted by the GameAI Schema.
USTRUCT(meta = (Hidden))
struct GAMEPLAYRUNTIME_API FGameAIStateTreeTaskBase : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

public:
	/// Default instance data; derive it when adding task-specific fields.
	using FInstanceDataType = FGameAIStateTreeContext;

public:
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
};

/// Native evaluator extension point accepted by the GameAI Schema.
USTRUCT(meta = (Hidden))
struct GAMEPLAYRUNTIME_API FGameAIStateTreeEvaluatorBase : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

public:
	/// Default instance data; derive it when adding evaluator-specific fields.
	using FInstanceDataType = FGameAIStateTreeContext;

public:
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
};

/** @brief Blueprint task extension point accepted by the GameAI Schema.
 *
 * @details Context fields are automatically bound by the compiler.
 * Access on the game thread during StateTree callbacks only.
 */
UCLASS(Abstract, MinimalAPI, Blueprintable)
class UGameAIStateTreeTaskBlueprintBase : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

public:
	/// Actual owner of the executing component.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<AActor> OwnerActor = nullptr;

	/// Current controlled pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<APawn> Pawn = nullptr;

	/// Current controller of the pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<AController> Controller = nullptr;

	/// World-owned AI service selected by the Schema.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<UGameAIContextSubsystem> GameContext = nullptr;

	/// Component executing this task.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<UStateTreeComponent> StateTreeComponent = nullptr;
};

/** @brief Blueprint evaluator extension point accepted by the GameAI Schema.
 *
 * @details Context fields are automatically bound by the compiler.
 * Access on the game thread during StateTree callbacks only.
 */
UCLASS(Abstract, MinimalAPI, Blueprintable)
class UGameAIStateTreeEvaluatorBlueprintBase : public UStateTreeEvaluatorBlueprintBase
{
	GENERATED_BODY()

public:
	/// Actual owner of the executing component.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<AActor> OwnerActor = nullptr;

	/// Current controlled pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<APawn> Pawn = nullptr;

	/// Current controller of the pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<AController> Controller = nullptr;

	/// World-owned AI service selected by the Schema.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<UGameAIContextSubsystem> GameContext = nullptr;

	/// Component executing this evaluator.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Context)
	TObjectPtr<UStateTreeComponent> StateTreeComponent = nullptr;
};
