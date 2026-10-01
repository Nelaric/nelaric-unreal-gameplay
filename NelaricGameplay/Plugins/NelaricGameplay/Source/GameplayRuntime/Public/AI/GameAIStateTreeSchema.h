// Copyright (c) 2026 Nelaric Contributors

/** @file GameAIStateTreeSchema.h */

#pragma once

#include "Components/StateTreeComponentSchema.h"
#include "Templates/SubclassOf.h"

#include "GameAIStateTreeSchema.generated.h"

class UGameAIContextSubsystem;
class UStateTreeComponent;

/** @brief Declares required AI context and restricts authored nodes.
 *
 * @details Assets own their Schema. Context is resolved on the game thread
 * before each component execution. OwnerActor, Pawn, Controller, GameContext
 * and StateTreeComponent are all required. Use GameAI node base types;
 * common conditions, considerations and property functions remain available.
 */
UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Game AI", CommonSchema))
class UGameAIStateTreeSchema : public UStateTreeComponentSchema
{
	GENERATED_BODY()

public:
public:
	GAMEPLAYRUNTIME_API UGameAIStateTreeSchema();
	GAMEPLAYRUNTIME_API bool SetContextRequirements(UStateTreeComponent& Component, FStateTreeExecutionContext& Context,
	                                                bool bLogErrors = false) const;
	GAMEPLAYRUNTIME_API virtual void PostLoad() override;
#if WITH_EDITOR
	GAMEPLAYRUNTIME_API virtual void
	PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
	GAMEPLAYRUNTIME_API virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	GAMEPLAYRUNTIME_API virtual bool IsClassAllowed(const UClass* InClass) const override;
	GAMEPLAYRUNTIME_API virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

private:
	UPROPERTY(EditAnywhere, Category = Defaults, NoClear)
	TSubclassOf<UGameAIContextSubsystem> GameContextClass;
};
