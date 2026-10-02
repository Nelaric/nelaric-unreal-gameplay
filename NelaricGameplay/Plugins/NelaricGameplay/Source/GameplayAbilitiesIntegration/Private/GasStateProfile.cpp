// Copyright (c) 2026 Nelaric Contributors

#include "GasStateProfile.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricGasProfile, Log, All);

bool UGasStateProfile::IsValidProfile() const
{
	auto Invalid = [this](const TCHAR* Reason, const FString& Entry)
	{
		if (!bInvalidProfileReported)
		{
			bInvalidProfileReported = true;
			UE_LOG(LogNelaricGasProfile, Error, TEXT("Invalid GAS profile %s: %s (entry=%s)."), *GetName(), Reason,
			       *Entry);
		}
		return false;
	};
	TSet<FGameplayAttribute> Seen;
	for (const auto& Rule : Attributes)
	{
		if (!Rule.Attribute.IsValid() || Seen.Contains(Rule.Attribute) || !FMath::IsFinite(Rule.InitialBase) ||
		    Rule.Ownership == EGasStateOwnership::Control)
		{
			return Invalid(TEXT("attribute is invalid, duplicated, non-finite or control-owned"),
			               Rule.Attribute.GetName());
		}
		Seen.Add(Rule.Attribute);
	}
	for (const auto& Grant : Abilities)
	{
		if (!Grant.Ability ||
		    Grant.Ability->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
		    Grant.Level < 1)
		{
			return Invalid(TEXT("ability class is missing, non-concrete or its level is below one"),
			               GetNameSafe(Grant.Ability.Get()));
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
			return Invalid(TEXT("control effect is missing, non-concrete, instant or stacked"),
			               GetNameSafe(Effect.Get()));
		}
	}
	bInvalidProfileReported = false;
	return true;
}
