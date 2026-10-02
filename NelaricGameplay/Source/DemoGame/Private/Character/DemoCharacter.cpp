// Copyright (c) 2026 Nelaric Contributors

#include "Character/DemoCharacter.h"
#include "GAS/DemoCombatAttributes.h"
#include "GAS/DemoJumpAbility.h"
#include "PawnGasBindingComponent.h"
#include "GasStateProfile.h"
#include "NativeGameplayTags.h"

namespace Nelaric::DemoActions
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(Jump, "Action.Jump");
}

ADemoCharacter::ADemoCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultStateProfile = CreateDefaultSubobject<UGasStateProfile>(TEXT("DefaultStateProfile"));
	DefaultStateProfile->Attributes = {
	    {UDemoCombatAttributes::GetMaxHealthAttribute(), EGasStateOwnership::Pawn, 100.0f},
	    {UDemoCombatAttributes::GetHealthAttribute(), EGasStateOwnership::Pawn, 100.0f},
	    {UDemoCombatAttributes::GetAttackAttribute(), EGasStateOwnership::Pawn, 10.0f}};
	DefaultStateProfile->Abilities.Add({UDemoJumpAbility::StaticClass(), Nelaric::DemoActions::Jump, 1});
	GetGasBinding()->StateProfile = DefaultStateProfile;
}
