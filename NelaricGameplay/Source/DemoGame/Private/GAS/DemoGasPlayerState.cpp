// Copyright (c) 2026 Nelaric Contributors

#include "GAS/DemoGasPlayerState.h"
#include "GAS/DemoCombatAttributes.h"
ADemoGasPlayerState::ADemoGasPlayerState()
{
	AttributeSetClasses.Add(UDemoCombatAttributes::StaticClass());
}
