// Copyright (c) 2026 Nelaric Contributors

#include "GAS/DemoWeaponDamageEffect.h"

#include "GAS/DemoCombatAttributes.h"
#include "GAS/DemoWeaponTags.h"

UDemoWeaponDamageEffect::UDemoWeaponDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UDemoCombatAttributes::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = Nelaric::DemoWeaponTags::Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
