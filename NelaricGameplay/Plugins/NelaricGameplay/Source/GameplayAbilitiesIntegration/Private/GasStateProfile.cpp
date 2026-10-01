// Copyright (c) 2026 Nelaric Contributors

#include "GasStateProfile.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

bool UGasStateProfile::IsValidProfile() const
{
	TSet<FGameplayAttribute> Seen;
	for (const auto& Rule : Attributes)
	{
		if (!Rule.Attribute.IsValid() || Seen.Contains(Rule.Attribute) || !FMath::IsFinite(Rule.InitialBase) ||
		    Rule.Ownership == EGasStateOwnership::Control)
		{
			return false;
		}
		Seen.Add(Rule.Attribute);
	}
	for (const auto& Grant : Abilities)
	{
		if (!Grant.Ability ||
		    Grant.Ability->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
		    Grant.Level < 1)
		{
			return false;
		}
	}
	TArray<TSubclassOf<UGameplayEffect>> Effects = ControlEffects;
	Effects.Append(PlayerControlEffects);
	Effects.Append(BotControlEffects);
	for (const auto& Effect : Effects)
	{
		if (!Effect || Effect->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
		    Effect->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Instant ||
		    Effect->GetDefaultObject<UGameplayEffect>()->StackingType != EGameplayEffectStackingType::None)
		{
			return false;
		}
	}
	return true;
}
