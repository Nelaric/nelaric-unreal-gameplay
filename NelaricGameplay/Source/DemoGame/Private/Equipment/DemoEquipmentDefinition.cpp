// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoEquipmentDefinition.h"

#include "Equipment/DemoEquipmentInstance.h"
#include "GAS/DemoWeaponDamageEffect.h"
#include "GAS/DemoWeaponTags.h"

UDemoEquipmentDefinition::UDemoEquipmentDefinition()
{
	InstanceClass = UDemoEquipmentInstance::StaticClass();
}

UDemoWeaponDefinition::UDemoWeaponDefinition()
{
	InstanceClass = UDemoWeaponInstance::StaticClass();
	DamageEffect = UDemoWeaponDamageEffect::StaticClass();
	FireGameplayCue = Nelaric::DemoWeaponTags::RifleFireCue;
	ImpactGameplayCue = Nelaric::DemoWeaponTags::RifleImpactCue;
}

bool UDemoWeaponDefinition::IsCombatConfigurationValid() const
{
	check(IsInGameThread());
	return MagazineCapacity > 0 && InitialReserveAmmo >= 0 && FMath::IsFinite(FireInterval) && FireInterval >= 0.02f &&
	       FMath::IsFinite(Range) && Range >= 1.0f && FMath::IsFinite(DamagePerShot) && DamagePerShot >= 0.0f &&
	       FMath::IsFinite(ReloadDuration) && ReloadDuration >= 0.05f && TraceChannel < ECC_MAX && DamageEffect &&
	       !DamageEffect->HasAnyClassFlags(CLASS_Abstract) &&
	       DamageEffect->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Instant;
}
