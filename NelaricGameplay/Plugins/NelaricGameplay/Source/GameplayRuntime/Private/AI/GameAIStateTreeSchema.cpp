// Copyright (c) 2026 Nelaric Contributors

#include "AI/GameAIStateTreeSchema.h"

#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "BrainComponent.h"
#include "Components/StateTreeComponent.h"
#include "Engine/World.h"
#include "AI/GameAIContextSubsystem.h"
#include "AI/GameAIStateTreeNodes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTypes.h"

UGameAIStateTreeSchema::UGameAIStateTreeSchema() : GameContextClass(UGameAIContextSubsystem::StaticClass())
{
	ContextDataDescs = {
	    {TEXT("OwnerActor"), AActor::StaticClass(), FGuid(0x7BE2AB01, 0x148A4AC0, 0xA27D5F61, 0x06432D91)},
	    {TEXT("Pawn"), APawn::StaticClass(), FGuid(0x7BE2AB02, 0x148A4AC0, 0xA27D5F61, 0x06432D91)},
	    {TEXT("Controller"), AController::StaticClass(), FGuid(0x7BE2AB03, 0x148A4AC0, 0xA27D5F61, 0x06432D91)},
	    {TEXT("GameContext"), GameContextClass.Get(), FGuid(0x7BE2AB04, 0x148A4AC0, 0xA27D5F61, 0x06432D91)},
	    {TEXT("StateTreeComponent"), UStateTreeComponent::StaticClass(),
	     FGuid(0x7BE2AB05, 0x148A4AC0, 0xA27D5F61, 0x06432D91)}};
}

void UGameAIStateTreeSchema::PostLoad()
{
	Super::PostLoad();
	ContextDataDescs[3].Struct = GameContextClass.Get();
}

#if WITH_EDITOR
void UGameAIStateTreeSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	ContextDataDescs[3].Struct = GameContextClass.Get();
}
#endif

bool UGameAIStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct && (InScriptStruct->IsChildOf(FGameAIStateTreeTaskBase::StaticStruct()) ||
	                          InScriptStruct->IsChildOf(FGameAIStateTreeEvaluatorBase::StaticStruct()) ||
	                          InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct()) ||
	                          InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct()) ||
	                          InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct()));
}

bool UGameAIStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return InClass && (InClass->IsChildOf(UGameAIStateTreeTaskBlueprintBase::StaticClass()) ||
	                   InClass->IsChildOf(UGameAIStateTreeEvaluatorBlueprintBase::StaticClass()) ||
	                   InClass->IsChildOf(UStateTreeConditionBlueprintBase::StaticClass()));
}

bool UGameAIStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// Context properties are the supported injection path. Prevent nodes from
	// bypassing the named context with arbitrary subsystem/component lookups.
	return false;
}

bool UGameAIStateTreeSchema::SetContextRequirements(UStateTreeComponent& Component, FStateTreeExecutionContext& Context,
                                                    bool bLogErrors) const
{
	if (!Context.IsValid())
	{
		if (bLogErrors)
		{
			UE_LOG(LogStateTree, Error, TEXT("Cannot set GameAI context for %s: execution context is invalid."),
			       *Component.GetName());
		}
		return false;
	}
	AActor* OwnerActor = Component.GetOwner();
	AController* Controller = Cast<AController>(OwnerActor);
	APawn* Pawn = Controller ? Controller->GetPawn().Get() : Cast<APawn>(OwnerActor);
	if (!Controller && IsValid(Pawn))
	{
		Controller = Pawn->GetController();
	}

	UWorld* World = Component.GetWorld();
	UWorldSubsystem* GameContext =
	    IsValid(World) && GameContextClass ? World->GetSubsystemBase(GameContextClass.Get()) : nullptr;

	// Actors can still be valid during EndPlay. Keep their context available
	// so the component can stop the tree and dispatch node exit callbacks.
	const bool bOwnerValid = IsValid(OwnerActor) && OwnerActor->IsA(GetContextActorClass());
	const bool bPawnValid = IsValid(Pawn);
	const bool bControllerValid = IsValid(Controller);
	const bool bContextValid =
	    bOwnerValid && bPawnValid && bControllerValid && IsValid(GameContext) && IsValid(&Component);
	if (!bContextValid && bLogErrors)
	{
		UE_LOG(LogStateTree, Error,
		       TEXT("GameAI context failed for %s: owner=%s ownerValid=%d pawn=%s pawnValid=%d controller=%s "
		            "controllerValid=%d gameContext=%s."),
		       *Component.GetName(), *GetNameSafe(OwnerActor), bOwnerValid, *GetNameSafe(Pawn), bPawnValid,
		       *GetNameSafe(Controller), bControllerValid, *GetNameSafe(GameContext));
	}

	Context.SetContextDataByName(TEXT("OwnerActor"), FStateTreeDataView(bOwnerValid ? OwnerActor : nullptr));
	Context.SetContextDataByName(TEXT("Pawn"), FStateTreeDataView(bPawnValid ? Pawn : nullptr));
	Context.SetContextDataByName(TEXT("Controller"), FStateTreeDataView(bControllerValid ? Controller : nullptr));
	Context.SetContextDataByName(TEXT("GameContext"), FStateTreeDataView(IsValid(GameContext) ? GameContext : nullptr));
	Context.SetContextDataByName(TEXT("StateTreeComponent"),
	                             FStateTreeDataView(IsValid(&Component) ? &Component : nullptr));
	const bool bViewsValid = bContextValid && Context.AreContextDataViewsValid();
	if (bContextValid && !bViewsValid && bLogErrors)
	{
		UE_LOG(LogStateTree, Error, TEXT("GameAI context data views are invalid for %s."), *Component.GetName());
	}
	return bViewsValid;
}
