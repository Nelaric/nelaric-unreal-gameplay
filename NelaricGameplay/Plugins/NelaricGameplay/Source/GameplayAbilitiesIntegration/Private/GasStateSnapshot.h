// Copyright (c) 2026 Nelaric Contributors

#pragma once
#include "Player/ControlStateTransfer.h"
#include "GasTransferExtension.h"
#include "GasStateProfile.h"
#include "GameplayEffect.h"
#include "UObject/GCObject.h"
class UNelaricAbilitySystemComponent;
namespace Nelaric::GAS
{
struct FEffectState
{
	FGameplayEffectSpec Spec;
	double EndTime = -1.0;
	float StartWorldTime = 0.0f;
	float StartServerWorldTime = 0.0f;
	double NextPeriodTime = -1.0;
	bool bInhibited = false;
};
struct FAbilityState
{
	TSubclassOf<UGameplayAbility> Ability;
	FGameplayTagContainer ActionTags;
	TMap<FGameplayTag, float> SetByCaller;
	int32 Level = 1;
	int32 InputID = INDEX_NONE;
};
struct FAttributeState
{
	FGameplayAttribute Attribute;
	float Base = 0.0f;
};
struct FExtensionState
{
	TSharedPtr<ITransferExtension> Extension;
	TSharedPtr<const Control::FStateSnapshot> State;
};
struct FSnapshot : Control::FStateSnapshot, FGCObject
{
	TWeakObjectPtr<UNelaricAbilitySystemComponent> Source;
	TWeakObjectPtr<UNelaricAbilitySystemComponent> Destination;
	TArray<FAttributeState> Attributes;
	TArray<FAttributeState> DestinationAttributes;
	TArray<FEffectState> Effects;
	TArray<FAbilityState> Abilities;
	TArray<FExtensionState> Extensions;
	TObjectPtr<UGasStateProfile> Profile;
	bool bSourceHadPawnState = false;
	FSnapshot()
	{
		SchemaId = TEXT("Nelaric.GAS.ControlState");
		SchemaVersion = 1;
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Profile);
		for (auto& Ability : Abilities)
		{
			TObjectPtr<UClass> Class = Ability.Ability.Get();
			Collector.AddReferencedObject(Class);
			Ability.Ability = Class.Get();
		}
		for (auto& Effect : Effects)
		{
			Collector.AddPropertyReferencesWithStructARO(FGameplayEffectSpec::StaticStruct(), &Effect.Spec);
		}
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("Nelaric.GAS.ControlState");
	}
};
} // namespace Nelaric::GAS
