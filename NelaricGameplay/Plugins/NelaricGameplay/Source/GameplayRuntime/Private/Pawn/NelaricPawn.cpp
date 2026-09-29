// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawn.h"

#include "Pawn/PawnInitializationHelper.h"

ANelaricPawn::ANelaricPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PawnInitializationComponent = Nelaric::Pawn::FInitializationHelper::Create(this, ObjectInitializer);
}
