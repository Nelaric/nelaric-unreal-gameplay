// Copyright (c) 2026 Nelaric Contributors

#include "GAS/DemoCombatAttributes.h"
#include "AI/DemoSoldierComponent.h"
#include "Character/DemoCharacter.h"
#include "AbilitySystemComponent.h"
#include "NelaricAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

FGameplayAttribute UDemoCombatAttributes::GetHealthAttribute()
{
	return FGameplayAttribute(
	    FindFieldChecked<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UDemoCombatAttributes, Health)));
}
FGameplayAttribute UDemoCombatAttributes::GetMaxHealthAttribute()
{
	return FGameplayAttribute(
	    FindFieldChecked<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UDemoCombatAttributes, MaxHealth)));
}
FGameplayAttribute UDemoCombatAttributes::GetAttackAttribute()
{
	return FGameplayAttribute(
	    FindFieldChecked<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UDemoCombatAttributes, Attack)));
}
void UDemoCombatAttributes::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UDemoCombatAttributes, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDemoCombatAttributes, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDemoCombatAttributes, Attack, COND_None, REPNOTIFY_Always);
}
void UDemoCombatAttributes::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	const auto* ASC = Cast<UNelaricAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	if (ASC && ASC->IsTransferringState())
	{
		return;
	}
	if (Attribute == GetHealthAttribute())
	{
		const auto* Character = ASC ? Cast<ADemoCharacter>(ASC->GetAvatarActor()) : nullptr;
		if (Character && Character->IsBattlefrontFrozen())
			NewValue = FMath::Max(NewValue, Health.GetCurrentValue());
		NewValue = FMath::Clamp(NewValue, 0.0f, MaxHealth.GetCurrentValue());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}
bool UDemoCombatAttributes::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	const auto* Character = Cast<ADemoCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor());
	if (Character && Character->IsBattlefrontFrozen() && Data.EvaluatedData.Attribute == GetHealthAttribute() &&
	    Data.EvaluatedData.Magnitude < 0)
		return false;
	return Super::PreGameplayEffectExecute(Data);
}
void UDemoCombatAttributes::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetHealthAttribute() || Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		GetOwningAbilitySystemComponent()->SetNumericAttributeBase(
		    GetHealthAttribute(), FMath::Clamp(Health.GetCurrentValue(), 0.0f, MaxHealth.GetCurrentValue()));
		if (ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor()))
		{
			FVector IncomingDamageDirection = FVector::ZeroVector;
			AActor* Source = Data.EffectSpec.GetContext().GetOriginalInstigator();
			const bool bHealthDamage =
			    Data.EvaluatedData.Attribute == GetHealthAttribute() && Data.EvaluatedData.Magnitude < 0.0f;
			if (bHealthDamage)
			{
				const FHitResult* Hit = Data.EffectSpec.GetContext().GetHitResult();
				const FVector TowardSource = IsValid(Source)
				                                 ? Source->GetActorLocation() - Character->GetActorLocation()
				                             : Hit ? Hit->TraceStart - Character->GetActorLocation()
				                                   : FVector::ZeroVector;
				IncomingDamageDirection = -TowardSource;
				if (UDemoSoldierComponent* Soldier = Character->FindComponentByClass<UDemoSoldierComponent>())
				{
					Soldier->ReportDamage(Source, -Data.EvaluatedData.Magnitude, TowardSource);
				}
			}
			Character->NotifyCombatHealthChanged(Source, bHealthDamage ? &IncomingDamageDirection : nullptr);
		}
	}
}
void UDemoCombatAttributes::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDemoCombatAttributes, Health, OldValue);
	if (ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor()))
	{
		Character->NotifyCombatHealthChanged();
	}
}
void UDemoCombatAttributes::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDemoCombatAttributes, MaxHealth, OldValue);
	if (ADemoCharacter* Character = Cast<ADemoCharacter>(GetOwningAbilitySystemComponent()->GetAvatarActor()))
	{
		Character->NotifyCombatHealthChanged();
	}
}
void UDemoCombatAttributes::OnRep_Attack(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDemoCombatAttributes, Attack, OldValue);
}
