// Copyright (c) 2026 Nelaric Contributors

#include "AI/GameAIStateTreeComponent.h"

#include "AI/GameAIStateTreeSchema.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricAI, Log, All);

TSubclassOf<UStateTreeSchema> UGameAIStateTreeComponent::GetSchema() const
{
	return UGameAIStateTreeSchema::StaticClass();
}

TValueOrError<void, FString> UGameAIStateTreeComponent::HasValidStateTreeReference() const
{
	if (TValueOrError<void, FString> Result = Super::HasValidStateTreeReference(); Result.HasError())
	{
		return Result;
	}
	if (!StateTreeRef.GetStateTree()->GetSchema()->IsA<UGameAIStateTreeSchema>())
	{
		return MakeError(TEXT("The StateTree asset requires the GameAI Schema."));
	}
	return MakeValue();
}

bool UGameAIStateTreeComponent::SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors)
{
	if (!Context.IsValid())
	{
		if (bLogErrors)
		{
			UE_LOG(LogNelaricAI, Error, TEXT("Cannot set StateTree context for %s: execution context is invalid."),
			       *GetName());
		}
		return false;
	}
	const UGameAIStateTreeSchema* Schema = Cast<UGameAIStateTreeSchema>(Context.GetStateTree()->GetSchema());
	if (!Schema)
	{
		if (bLogErrors)
		{
			UE_LOG(LogNelaricAI, Error,
			       TEXT("Cannot set StateTree context for %s: StateTree does not use the GameAI schema."), *GetName());
		}
		return false;
	}
	Context.SetLinkedStateTreeOverrides(LinkedStateTreeOverrides);
	return Schema->SetContextRequirements(*this, Context, bLogErrors);
}
