// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawnInitializationHelper.h"

#include "GameFramework/Pawn.h"
#include "Pawn/NelaricPawnInitializationComponent.h"
#include "UObject/UObjectGlobals.h"

UNelaricPawnInitializationComponent*
Nelaric::Pawn::FInitializationHelper::Create(APawn* Owner, const FObjectInitializer& ObjectInitializer)
{
	check(Owner);
	return ObjectInitializer.CreateDefaultSubobject<UNelaricPawnInitializationComponent>(
	    Owner, TEXT("PawnInitializationComponent"));
}
