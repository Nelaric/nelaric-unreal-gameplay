// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoEquipmentDefinition.h"

#include "Equipment/DemoEquipmentInstance.h"

UDemoEquipmentDefinition::UDemoEquipmentDefinition()
{
	InstanceClass = UDemoEquipmentInstance::StaticClass();
}

UDemoWeaponDefinition::UDemoWeaponDefinition()
{
	InstanceClass = UDemoWeaponInstance::StaticClass();
}
