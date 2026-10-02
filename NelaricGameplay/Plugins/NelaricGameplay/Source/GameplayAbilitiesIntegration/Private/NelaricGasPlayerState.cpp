// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGasPlayerState.h"
#include "NelaricAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricGasPlayerState, Log, All);

ANelaricGasPlayerState::ANelaricGasPlayerState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UNelaricAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Full);
}

UAbilitySystemComponent* ANelaricGasPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
UNelaricAbilitySystemComponent* ANelaricGasPlayerState::GetNelaricAbilitySystem() const
{
	return AbilitySystem;
}

void ANelaricGasPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, nullptr);
	if (!HasAuthority())
	{
		return;
	}
	TSet<UClass*> Seen;
	for (const auto& Class : AttributeSetClasses)
	{
		if (Class && !Seen.Contains(Class.Get()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Seen.Add(Class.Get());
			AbilitySystem->AddAttributeSetSubobject(NewObject<UAttributeSet>(this, Class));
		}
		else
		{
			UE_LOG(LogNelaricGasPlayerState, Error,
			       TEXT("Invalid attribute set configuration on %s: class=%s is null, abstract or duplicated."),
			       *GetName(), *GetNameSafe(Class.Get()));
		}
	}
	if (ParticipantProfile && ParticipantProfile->IsValidProfile())
	{
		for (const auto& Rule : ParticipantProfile->Attributes)
		{
			if (Rule.Ownership == EGasStateOwnership::Participant &&
			    AbilitySystem->HasAttributeSetForAttribute(Rule.Attribute))
			{
				AbilitySystem->SetNumericAttributeBase(Rule.Attribute, Rule.InitialBase);
			}
			else if (Rule.Ownership == EGasStateOwnership::Participant)
			{
				UE_LOG(LogNelaricGasPlayerState, Error,
				       TEXT("Cannot initialize participant attribute on %s: attribute=%s profile=%s; attribute set is "
				            "missing."),
				       *GetName(), *Rule.Attribute.GetName(), *GetNameSafe(ParticipantProfile));
			}
		}
		for (const auto& Grant : ParticipantProfile->Abilities)
		{
			FGameplayAbilitySpec Spec(Grant.Ability, Grant.Level);
			if (Grant.ActionTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(Grant.ActionTag);
			}
			if (!AbilitySystem->GiveAbility(Spec).IsValid())
			{
				UE_LOG(LogNelaricGasPlayerState, Error,
				       TEXT("Cannot grant participant ability on %s: ability=%s level=%d profile=%s."), *GetName(),
				       *GetNameSafe(Grant.Ability.Get()), Grant.Level, *GetNameSafe(ParticipantProfile));
			}
		}
	}
}

bool ANelaricGasPlayerState::IsStateCustodian() const
{
	return Cast<APawn>(GetOwner()) != nullptr;
}
