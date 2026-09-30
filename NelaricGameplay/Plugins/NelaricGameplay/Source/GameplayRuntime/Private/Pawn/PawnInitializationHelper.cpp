// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/PawnInitializationHelper.h"

#include "GameFramework/Pawn.h"
#include "Pawn/PawnInitializationComponent.h"
#include "UObject/UObjectGlobals.h"

UPawnInitializationComponent* Nelaric::Pawn::FInitializationHelper::Create(APawn* Owner,
                                                                           const FObjectInitializer& ObjectInitializer)
{
	check(Owner);
	return ObjectInitializer.CreateDefaultSubobject<UPawnInitializationComponent>(Owner,
	                                                                              TEXT("PawnInitializationComponent"));
}
