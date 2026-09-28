// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricCharacter.h"

#include "Pawn/NelaricPawnInitializationHelper.h"

ANelaricCharacter::ANelaricCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PawnInitializationComponent = Nelaric::Pawn::FInitializationHelper::Create(this, ObjectInitializer);
}
