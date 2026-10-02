// Copyright (c) 2026 Nelaric Contributors

#include "PawnGasBindingComponent.h"
#include "GasStateSnapshot.h"
#include "NelaricAbilitySystemComponent.h"
#include "NelaricGameplayAbility.h"
#include "NelaricGasPlayerState.h"
#include "Abilities/GameplayAbility.h"
#include "Pawn/PawnControlComponent.h"
#include "Player/ControlSwitchSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricGasBinding, Log, All);

namespace Nelaric::GAS
{
class FTransferParticipant : public Control::IStateTransferParticipant
{
	TWeakObjectPtr<UPawnGasBindingComponent> Binding;
	static const FSnapshot* Payload(const Control::FStateSnapshot& State)
	{
		return State.SchemaId == TEXT("Nelaric.GAS.ControlState") && State.SchemaVersion == 1
		           ? static_cast<const FSnapshot*>(&State)
		           : nullptr;
	}
	static UNelaricAbilitySystemComponent* Select(const FSnapshot& State, Control::EAssociationEndpoint Endpoint)
	{
		return Endpoint == Control::EAssociationEndpoint::Source ? State.Source.Get() : State.Destination.Get();
	}

public:
	virtual void GetReservationActors(APawn* Pawn, TArray<TWeakObjectPtr<const AActor>>& OutActors) const override
	{
		if (const auto* Component = Binding.Get())
		{
			if (Component->Custodian)
			{
				OutActors.AddUnique(Component->Custodian);
			}
			if (Component->StateOwner)
			{
				OutActors.AddUnique(Component->StateOwner);
			}
		}
	}
	explicit FTransferParticipant(UPawnGasBindingComponent* InBinding) : Binding(InBinding)
	{
	}
	virtual bool ExportState(const Control::FStateTransferContext& Context,
	                         TSharedPtr<const Control::FStateSnapshot>& OutSnapshot) const override
	{
		auto* Component = Binding.Get();
		if (!Component || !IsValid(Component->Custodian) ||
		    (Component->StateProfile && !Component->StateProfile->IsValidProfile()))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ExportState failed (pawn=%s): binding, custodian or profile is invalid."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		auto State = MakeShared<FSnapshot>();
		State->Profile = Component->StateProfile;
		State->Source = Component->GetAbilitySystem();
		const auto* Destination = Cast<ANelaricGasPlayerState>(Context.Destination.PlayerState.Get());
		if (!Destination && !Context.Destination.PlayerState.IsExplicitlyNull())
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ExportState failed (pawn=%s): destination PlayerState does not support GAS."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		State->Destination =
		    Destination ? Destination->GetNelaricAbilitySystem() : Component->Custodian->GetNelaricAbilitySystem();
		State->bSourceHadPawnState = Component->bPawnStateInstalled;
		auto* SourceASC = State->Source.Get();
		auto* DestinationASC = State->Destination.Get();
		if (!Component->HasLayout(SourceASC) || !Component->HasLayout(DestinationASC))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ExportState failed (pawn=%s): source or destination attribute layout is incompatible."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		if (State->Profile)
		{
			for (const auto& Rule : State->Profile->Attributes)
			{
				if (Rule.Ownership == EGasStateOwnership::Pawn)
				{
					const float SourceBase = SourceASC->GetNumericAttributeBase(Rule.Attribute);
					const float DestinationBase = DestinationASC->GetNumericAttributeBase(Rule.Attribute);
					if (!FMath::IsFinite(SourceBase) || !FMath::IsFinite(DestinationBase))
					{
						UE_LOG(LogNelaricGasBinding, Error,
						       TEXT("ExportState failed (pawn=%s): attribute base value is not finite."),
						       *GetNameSafe(Context.Pawn.Get()));
						return false;
					}
					State->Attributes.Add({Rule.Attribute, SourceBase});
					State->DestinationAttributes.Add({Rule.Attribute, DestinationBase});
				}
			}
		}
		// Validate all active avatar executions, including participant grants.
		for (const auto& Spec : SourceASC->GetActivatableAbilities())
		{
			if (!Spec.IsActive())
			{
				continue;
			}
			const auto* Ability = Cast<UNelaricGameplayAbility>(Spec.Ability);
			const bool bNeedsAdapter = Ability && Ability->ControlChangePolicy != EGasAbilityControlPolicy::Cancel;
			int32 Coverage = 0;
			for (const auto& Extension : Component->Extensions)
			{
				Coverage += Extension.Value->HandlesAbility(Spec) ? 1 : 0;
			}
			const bool bNonCancelable = Spec.GetAbilityInstances().ContainsByPredicate(
			    [](const UGameplayAbility* Instance)
			    { return Instance && Instance->IsActive() && !Instance->CanBeCanceled(); });
			if (Coverage > 1 || ((bNeedsAdapter || bNonCancelable) && Coverage != 1))
			{
				UE_LOG(
				    LogNelaricGasBinding, Error,
				    TEXT("ExportState failed (pawn=%s): active ability has missing or overlapping transfer adapters."),
				    *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
		}
		for (const auto& Handle : Component->GrantedAbilities)
		{
			const auto* Spec = SourceASC->FindAbilitySpecFromHandle(Handle);
			if (!Spec || !Spec->Ability)
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ExportState failed (pawn=%s): granted ability specification is missing."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
			State->Abilities.Add({Spec->Ability->GetClass(), Spec->GetDynamicSpecSourceTags(),
			                      Spec->SetByCallerTagMagnitudes, Spec->Level, Spec->InputID});
		}
		TArray<FActiveGameplayEffectHandle> CustomEffects;
		for (const auto& Effect : &SourceASC->GetActiveGameplayEffects())
		{
			int32 Coverage = 0;
			for (const auto& Extension : Component->Extensions)
			{
				Coverage += Extension.Value->HandlesEffect(Effect) ? 1 : 0;
			}
			if (Coverage > 1)
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ExportState failed (pawn=%s): active effect has overlapping transfer adapters."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
			if (Coverage == 1)
			{
				CustomEffects.Add(Effect.Handle);
			}
		}
		if (!SourceASC->ExportPawnEffects(Context.Pawn.Get(), State->Effects, CustomEffects) ||
		    !DestinationASC->CanReceiveEffects(State->Effects))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ExportState failed (pawn=%s): pawn effects could not be exported or received."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		// Domain adapters validate and capture active ability/task state before
		// the common grant cleanup. They may preserve executions independently.
		for (const auto& Entry : Component->Extensions)
		{
			TSharedPtr<const Control::FStateSnapshot> Export;
			if (!Entry.Value->Export(Context, SourceASC, DestinationASC, Export) || !Export ||
			    Export->SchemaId.IsNone() || Export->SchemaVersion == 0)
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ExportState failed (pawn=%s): transfer extension failed or returned an invalid snapshot."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
			State->Extensions.Add({Entry.Value, Export});
		}
		OutSnapshot = State;
		return true;
	}
	virtual bool DetachAssociation(const Control::FStateTransferContext& Context,
	                               Control::EAssociationEndpoint Endpoint,
	                               const Control::FStateSnapshot& Snapshot) override
	{
		auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		if (!Component || !State)
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("DetachAssociation failed (pawn=%s): binding or snapshot schema is invalid."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		auto* ASC = Select(*State, Endpoint);
		if (!ASC)
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("DetachAssociation failed (pawn=%s): selected ability system is unavailable."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		ASC->SetStateTransferBlocked(true);
		if (ASC->GetAvatarActor() == Context.Pawn.Get())
		{
			ASC->InitAbilityActorInfo(ASC->GetOwnerActor(), nullptr);
		}
		Component->bStateCommitted = false;
		return true;
	}
	virtual bool AttachAssociation(const Control::FStateTransferContext& Context,
	                               Control::EAssociationEndpoint Endpoint,
	                               const Control::FStateSnapshot& Snapshot) override
	{
		auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		if (!Component || !State)
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("AttachAssociation failed (pawn=%s): binding or snapshot schema is invalid."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		auto* ASC = Select(*State, Endpoint);
		if (!ASC || (ASC->GetAvatarActor() && ASC->GetAvatarActor() != Context.Pawn.Get()))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("AttachAssociation failed (pawn=%s): selected ability system is unavailable or has another "
			            "avatar."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		Component->StateOwner = Cast<ANelaricGasPlayerState>(ASC->GetOwnerActor());
		ASC->InitAbilityActorInfo(ASC->GetOwnerActor(), Context.Pawn.Get());
		if (!Component->StateOwner)
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("Cannot attach GAS association for %s: ASC owner is not a GAS PlayerState."),
			       *GetNameSafe(Context.Pawn.Get()));
		}
		return Component->StateOwner != nullptr;
	}
	virtual bool IsAssociationValid(const Control::FStateTransferContext& Context,
	                                Control::EAssociationEndpoint Endpoint,
	                                const Control::FStateSnapshot& Snapshot) const override
	{
		const auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		const auto* ASC = State ? Select(*State, Endpoint) : nullptr;
		return Component && ASC && Component->GetAbilitySystem() == ASC &&
		       ASC->GetAvatarActor() == Context.Pawn.Get() && ASC->GetOwnerActor() == Component->StateOwner;
	}
	virtual bool ReleaseState(const Control::FStateTransferContext& Context, Control::EAssociationEndpoint Endpoint,
	                          const Control::FStateSnapshot& Snapshot) override
	{
		auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		auto* ASC = State ? Select(*State, Endpoint) : nullptr;
		if (!Component || !ASC)
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ReleaseState failed (pawn=%s): binding or selected ability system is unavailable."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		ASC->SetStateTransferBlocked(true);
		for (const auto& Extension : State->Extensions)
		{
			if (!Extension.Extension->Release(ASC, Context.Pawn.Get(), *Extension.State))
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ReleaseState failed (pawn=%s): transfer extension failed to release state."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
		}
		TArray<FGameplayAbilitySpecHandle> Active;
		for (const auto& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive())
			{
				Active.Add(Spec.Handle);
			}
		}
		for (const auto& Handle : Active)
		{
			ASC->CancelAbilityHandle(Handle);
			const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			if (Spec && Spec->IsActive())
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ReleaseState failed (pawn=%s): ability remained active after cancellation."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
		}
		if (!Component->RemoveGrants(ASC) || !ASC->RemovePawnEffects(Context.Pawn.Get()))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ReleaseState failed (pawn=%s): grants or pawn effects could not be removed."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		const auto& Defaults =
		    Endpoint == Control::EAssociationEndpoint::Source ? State->Attributes : State->DestinationAttributes;
		for (const auto& Attribute : Defaults)
		{
			// Source cleanup clears its role slot; recovery imports its export.
			ASC->SetNumericAttributeBase(Attribute.Attribute,
			                             Endpoint == Control::EAssociationEndpoint::Source ? 0.0f : Attribute.Base);
		}
		Component->bPawnStateInstalled = false;
		return true;
	}
	virtual bool ImportState(const Control::FStateTransferContext& Context, Control::EAssociationEndpoint Endpoint,
	                         const Control::FStateSnapshot& Snapshot) override
	{
		auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		auto* ASC = State ? Select(*State, Endpoint) : nullptr;
		if (!Component || !ASC || !Component->HasLayout(ASC))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ImportState failed (pawn=%s): binding or selected ability system layout is invalid."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		ASC->SetStateTransferBlocked(true);
		// Idempotent recovery removes any partial role import first.
		if (!Component->RemoveGrants(ASC) || !ASC->RemovePawnEffects(Context.Pawn.Get()))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ImportState failed (pawn=%s): partial imported grants or effects could not be removed."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		for (const auto& Attribute : State->Attributes)
		{
			ASC->SetNumericAttributeBase(Attribute.Attribute, Attribute.Base);
		}
		if (!ASC->RestorePawnEffects(Context.Pawn.Get(), State->Effects) || !Component->InstallGrants(ASC, false))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("ImportState failed (pawn=%s): effects or control grants could not be restored."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		Component->GrantsOwner = ASC;
		for (const auto& Ability : State->Abilities)
		{
			FGameplayAbilitySpec Spec(Ability.Ability, Ability.Level, Ability.InputID, Context.Pawn.Get());
			Spec.GetDynamicSpecSourceTags() = Ability.ActionTags;
			Spec.SetByCallerTagMagnitudes = Ability.SetByCaller;
			const auto Handle = ASC->GiveAbility(Spec);
			if (!Handle.IsValid())
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ImportState failed (pawn=%s): saved ability grant could not be restored."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
			Component->GrantedAbilities.Add(Handle);
		}
		for (const auto& Extension : State->Extensions)
		{
			if (!Extension.Extension->Restore(ASC, Context.Pawn.Get(), *Extension.State,
			                                  Endpoint == Control::EAssociationEndpoint::Source))
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("ImportState failed (pawn=%s): transfer extension failed to restore state."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
		}
		Component->bPawnStateInstalled = true;
		return true;
	}
	virtual bool IsStateValid(const Control::FStateTransferContext& Context, Control::EAssociationEndpoint Endpoint,
	                          const Control::FStateSnapshot& Snapshot) const override
	{
		const auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		auto* ASC = State ? Select(*State, Endpoint) : nullptr;
		if (!Component || !State || !ASC || !Component->bPawnStateInstalled ||
		    !IsAssociationValid(Context, Endpoint, Snapshot))
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("IsStateValid failed (pawn=%s): binding, snapshot, installed state or association is invalid."),
			       *GetNameSafe(Context.Pawn.Get()));
			return false;
		}
		for (const auto& Extension : State->Extensions)
		{
			if (!Extension.Extension->IsRestored(ASC, Context.Pawn.Get(), *Extension.State))
			{
				UE_LOG(LogNelaricGasBinding, Error,
				       TEXT("IsStateValid failed (pawn=%s): transfer extension state was not restored."),
				       *GetNameSafe(Context.Pawn.Get()));
				return false;
			}
		}
		return true;
	}
	virtual void CommitState(const Control::FStateTransferContext& Context, Control::EAssociationEndpoint Endpoint,
	                         const Control::FStateSnapshot& Snapshot) override
	{
		auto* Component = Binding.Get();
		const auto* State = Payload(Snapshot);
		if (!Component || !State)
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("Cannot commit GAS state for %s: binding or snapshot schema is invalid."),
			       *GetNameSafe(Context.Pawn.Get()));
			return;
		}
		for (const auto& Extension : State->Extensions)
		{
			Extension.Extension->Commit(Select(*State, Endpoint), Context.Pawn.Get(), *Extension.State);
		}
		Component->bStateCommitted = true;
		++Component->BindingRevision;
		if (State->Source.IsValid())
		{
			State->Source->SetStateTransferBlocked(false);
		}
		if (State->Destination.IsValid())
		{
			State->Destination->SetStateTransferBlocked(false);
		}
		if (Component->StateOwner)
		{
			Component->StateOwner->ForceNetUpdate();
		}
		if (Context.Pawn.IsValid())
		{
			Context.Pawn->ForceNetUpdate();
		}
	}
};
} // namespace Nelaric::GAS

UPawnGasBindingComponent::UPawnGasBindingComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UPawnGasBindingComponent::BeginPlay()
{
	Super::BeginPlay();
	// Registration can run before actor roles and the authority game mode
	// are initialized, especially in PIE. Dynamic control components may
	// also be created during BeginPlay. Retry after that entire phase ends.
	GetWorld()->GetTimerManager().SetTimerForNextTick(
	    FTimerDelegate::CreateWeakLambda(this,
	                                     [this]()
	                                     {
		                                     if (IsRegistered() && GetWorld() && !GetWorld()->bIsTearingDown)
		                                     {
			                                     RequestInitRefresh();
		                                     }
	                                     }));
}

UNelaricAbilitySystemComponent* UPawnGasBindingComponent::GetAbilitySystem() const
{
	return IsValid(StateOwner) && !StateOwner->IsActorBeingDestroyed() ? StateOwner->GetNelaricAbilitySystem()
	                                                                   : nullptr;
}

bool UPawnGasBindingComponent::HasCommittedState() const
{
	const auto* ASC = GetAbilitySystem();
	return bStateCommitted && ASC && ASC->GetAvatarActor() == GetPawn() &&
	       (!GetController() || GetPlayerState() == StateOwner);
}

bool UPawnGasBindingComponent::RestoreReservedState()
{
	if (!GetPawn() || !GetPawn()->HasAuthority())
	{
		UE_LOG(LogNelaricGasBinding, Error,
		       TEXT("Cannot restore reserved GAS state on %s: pawn is unavailable or has no authority."), *GetName());
		return false;
	}
	auto* Coordinator = GetWorld()->GetSubsystem<UControlSwitchSubsystem>();
	const auto Export = Coordinator->GetExportedControlState(GetPawn());
	if (!Export || !Coordinator->IsControlTransitionRecoveryRequired(GetPawn()) || !Participant)
	{
		UE_LOG(LogNelaricGasBinding, Error,
		       TEXT("Cannot restore reserved GAS state on %s: retained export, recovery reservation or transfer "
		            "participant is unavailable."),
		       *GetName());
		return false;
	}
	const auto* State = Export->States.FindByPredicate([](const auto& Entry) { return Entry.Id == TEXT("GAS"); });
	if (!State)
	{
		UE_LOG(LogNelaricGasBinding, Error,
		       TEXT("Cannot restore reserved GAS state on %s: retained GAS snapshot is missing."), *GetName());
		return false;
	}
	const auto& Context = Export->Context;
	auto Endpoint = Nelaric::Control::EAssociationEndpoint::Destination;
	if (GetController() == Context.Source.Controller.Get() && GetPlayerState() == Context.Source.PlayerState.Get())
	{
		Endpoint = Nelaric::Control::EAssociationEndpoint::Source;
	}
	else if (GetController() != Context.Destination.Controller.Get() ||
	         GetPlayerState() != Context.Destination.PlayerState.Get())
	{
		UE_LOG(LogNelaricGasBinding, Error,
		       TEXT("Cannot restore reserved GAS state on %s: native association matches neither endpoint."),
		       *GetName());
		return false;
	}
	return Participant->AttachAssociation(Context, Endpoint, *State->Snapshot) &&
	       Participant->ImportState(Context, Endpoint, *State->Snapshot) &&
	       Participant->IsStateValid(Context, Endpoint, *State->Snapshot);
}

bool UPawnGasBindingComponent::IsReadyForActions() const
{
	const auto* ASC = GetAbilitySystem();
	return bStateCommitted && GetInitState() == Nelaric::EInitState::Ready && ASC &&
	       ASC->GetAvatarActor() == GetPawn() && !ASC->IsTransferringState();
}

bool UPawnGasBindingComponent::RegisterTransferExtension(FName Id,
                                                         TSharedRef<Nelaric::GAS::ITransferExtension> Extension)
{
	check(IsInGameThread());
	const auto* Policy = GetPawn() ? GetPawn()->FindComponentByClass<UPawnControlComponent>() : nullptr;
	if (Id.IsNone() || Extensions.Contains(Id) || (Policy && Policy->IsControlTransitionInProgress()))
	{
		UE_LOG(LogNelaricGasBinding, Error,
		       TEXT("Cannot register GAS transfer extension on %s (id=%s): ID is empty, duplicated or a control "
		            "transition is active."),
		       *GetName(), *Id.ToString());
		return false;
	}
	Extensions.Add(Id, Extension);
	return true;
}

bool UPawnGasBindingComponent::UnregisterTransferExtension(FName Id)
{
	check(IsInGameThread());
	const auto* Policy = GetPawn() ? GetPawn()->FindComponentByClass<UPawnControlComponent>() : nullptr;
	return !(Policy && Policy->IsControlTransitionInProgress()) && Extensions.Remove(Id) != 0;
}

bool UPawnGasBindingComponent::EnsureCustodian()
{
	UWorld* World = GetWorld();
	APawn* Pawn = GetPawn();
	if (!World || !World->IsGameWorld() || World->bIsTearingDown || !Pawn || !Pawn->HasAuthority() ||
	    Pawn->IsActorBeingDestroyed())
	{
		return false;
	}
	if (IsValid(Custodian))
	{
		return true;
	}
	const auto* Mode = World->GetAuthGameMode();
	if (!Mode)
	{
		return false;
	}
	UClass* Class = Mode->PlayerStateClass.Get();
	if (!Class || !Class->IsChildOf(ANelaricGasPlayerState::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		ReportInitErrorOnce(TEXT("CustodianClass"),
		                    TEXT("GameMode must configure a concrete GAS PlayerState class for state custody."));
		return false;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = Pawn;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Custodian = World->SpawnActor<ANelaricGasPlayerState>(Class, Parameters);
	if (Custodian)
	{
		Custodian->SetIsABot(true);
	}
	if (!Custodian)
	{
		ReportInitErrorOnce(TEXT("CustodianSpawn"), TEXT("Could not spawn the state custodian."));
	}
	return Custodian != nullptr;
}

bool UPawnGasBindingComponent::HasLayout(UNelaricAbilitySystemComponent* ASC) const
{
	const auto* ParticipantOwner = ASC ? Cast<ANelaricGasPlayerState>(ASC->GetOwnerActor()) : nullptr;
	if (!IsValid(ASC) || (ParticipantOwner && ParticipantOwner->ParticipantProfile &&
	                      !ParticipantOwner->ParticipantProfile->IsValidProfile()))
	{
		return false;
	}
	if (StateProfile)
	{
		for (const auto& Rule : StateProfile->Attributes)
		{
			const auto* Owner = Cast<ANelaricGasPlayerState>(ASC->GetOwnerActor());
			if (Owner && Owner->ParticipantProfile)
			{
				for (const auto& ParticipantRule : Owner->ParticipantProfile->Attributes)
				{
					if (ParticipantRule.Attribute == Rule.Attribute && ParticipantRule.Ownership != Rule.Ownership)
					{
						ReportInitErrorOnce(
						    TEXT("AttributeOwnership"),
						    TEXT("Pawn and participant profiles assign conflicting attribute ownership."));
						return false;
					}
				}
			}
			if (!ASC->HasAttributeSetForAttribute(Rule.Attribute))
			{
				ReportInitErrorOnce(TEXT("AttributeLayout"),
				                    TEXT("The ability system is missing a configured attribute set."));
				return false;
			}
		}
	}
	return true;
}

bool UPawnGasBindingComponent::CanEntryDataAvailable()
{
	APawn* Pawn = GetPawn();
	if (!Pawn || !Pawn->HasActorBegunPlay() || (StateProfile && !StateProfile->IsValidProfile()))
	{
		return false;
	}
	if (!Pawn->HasAuthority())
	{
		return StateOwner && HasLayout(GetAbilitySystem());
	}
	auto* Policy = Pawn->FindComponentByClass<UPawnControlComponent>();
	if (!Policy || !EnsureCustodian())
	{
		if (!Policy)
		{
			ReportInitErrorOnce(TEXT("ControlPolicy"), TEXT("The pawn has no control policy component."));
		}
		return false;
	}
	if (!bParticipantRegistered)
	{
		Participant = MakeShared<Nelaric::GAS::FTransferParticipant>(this);
		bParticipantRegistered = Policy->RegisterStateTransferParticipant(TEXT("GAS"), Participant.ToSharedRef());
		if (!bParticipantRegistered)
		{
			return false;
		}
	}
	if (!StateOwner)
	{
		StateOwner = Cast<ANelaricGasPlayerState>(Pawn->GetPlayerState());
		if (!StateOwner && !Pawn->GetController())
		{
			StateOwner = Custodian;
		}
	}
	return HasLayout(GetAbilitySystem());
}

bool UPawnGasBindingComponent::CanEntryDataInitialized()
{
	auto* ASC = GetAbilitySystem();
	APawn* Pawn = GetPawn();
	if (!ASC || !Pawn)
	{
		return false;
	}
	// Spawned player pawns register before GameMode's initial Possess. Give
	// that same-frame possession a chance to choose its participant before
	// installing state in custody. Truly unpossessed pawns initialize next tick.
	if (Pawn->HasAuthority() && !bPawnStateInstalled && !Pawn->GetController() && !bInitialBindingAllowed)
	{
		if (!bInitialBindingQueued)
		{
			bInitialBindingQueued = true;
			GetWorld()->GetTimerManager().SetTimerForNextTick(
			    FTimerDelegate::CreateWeakLambda(this,
			                                     [this]()
			                                     {
				                                     if (IsRegistered() && GetWorld() && !GetWorld()->bIsTearingDown)
				                                     {
					                                     bInitialBindingAllowed = true;
					                                     RequestInitRefresh();
				                                     }
			                                     }));
		}
		return false;
	}
	const auto* Policy = Pawn->FindComponentByClass<UPawnControlComponent>();
	if (Pawn->HasAuthority() && Policy && Policy->IsControlTransitionInProgress())
	{
		return bPawnStateInstalled && ASC->GetAvatarActor() == Pawn;
	}
	if (Pawn->HasAuthority() && Pawn->GetPlayerState() != StateOwner &&
	    (Pawn->GetController() || StateOwner != Custodian))
	{
		if (bPawnStateInstalled)
		{
			// Only the world coordinator may migrate an initialized avatar.
			// Preserve its old ASC state rather than performing a partial swap.
			ReportInitErrorOnce(TEXT("UncoordinatedControl"),
			                    TEXT("Initialized GAS state changed owner outside the control coordinator."));
			return false;
		}
		StateOwner = Pawn->GetController() ? Cast<ANelaricGasPlayerState>(Pawn->GetPlayerState()) : Custodian.Get();
		ASC = GetAbilitySystem();
		if (!HasLayout(ASC))
		{
			return false;
		}
	}
	if (Pawn->GetController() && Pawn->GetPlayerState() != StateOwner)
	{
		return false;
	}
	if (ASC->GetAvatarActor() && ASC->GetAvatarActor() != Pawn)
	{
		ReportInitErrorOnce(TEXT("AvatarConflict"), TEXT("The ability system is already bound to another pawn."));
		return false;
	}
	ASC->InitAbilityActorInfo(StateOwner, Pawn);
	if (!Pawn->HasAuthority())
	{
		ClientBoundASC = ASC;
		if (bStateCommitted)
		{
			GetWorld()->GetTimerManager().ClearTimer(ReplicationRetry);
		}
		if (bStateCommitted)
		{
			ReportedInitErrors.Reset();
		}
		return bStateCommitted;
	}
	if (!bPawnStateInstalled)
	{
		if (StateProfile)
		{
			for (const auto& Rule : StateProfile->Attributes)
			{
				if (Rule.Ownership == EGasStateOwnership::Pawn)
				{
					ASC->SetNumericAttributeBase(Rule.Attribute, Rule.InitialBase);
				}
			}
		}
		if (!InstallGrants(ASC))
		{
			return false;
		}
		bPawnStateInstalled = true;
	}
	bStateCommitted = true;
	ReportedInitErrors.Reset();
	return true;
}

bool UPawnGasBindingComponent::InstallGrants(UNelaricAbilitySystemComponent* ASC, bool bIncludeAbilities)
{
	if (!StateProfile)
	{
		return true;
	}
	GrantsOwner = ASC;
	for (const auto& Grant : StateProfile->Abilities)
	{
		if (!bIncludeAbilities)
		{
			break;
		}
		FGameplayAbilitySpec Spec(Grant.Ability, Grant.Level, INDEX_NONE, GetPawn());
		if (Grant.ActionTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Grant.ActionTag);
		}
		const auto Handle = ASC->GiveAbility(Spec);
		if (!Handle.IsValid())
		{
			ReportInitErrorOnce(TEXT("AbilityGrant"), TEXT("Could not grant a configured pawn ability."));
			return false;
		}
		GrantedAbilities.Add(Handle);
	}
	TArray<TSubclassOf<UGameplayEffect>> Effects;
	if (StateOwner && !StateOwner->IsStateCustodian())
	{
		Effects = StateProfile->ControlEffects;
		Effects.Append(StateOwner->IsABot() ? StateProfile->BotControlEffects : StateProfile->PlayerControlEffects);
	}
	for (const auto& Effect : Effects)
	{
		auto Context = ASC->MakeEffectContext();
		Context.AddSourceObject(GetPawn());
		const auto Handle = ASC->ApplyGameplayEffectToSelf(Effect->GetDefaultObject<UGameplayEffect>(), 1.0f, Context);
		if (!Handle.IsValid())
		{
			ReportInitErrorOnce(TEXT("ControlEffect"), TEXT("Could not apply a configured control effect."));
			return false;
		}
		ControlEffects.Add(Handle);
		ASC->TrackEffect(Handle, GetPawn(), EGasStateOwnership::Control);
	}
	return true;
}

bool UPawnGasBindingComponent::RemoveGrants(UNelaricAbilitySystemComponent* ASC)
{
	if (GrantsOwner.IsValid() && GrantsOwner.Get() != ASC)
	{
		return true;
	}
	for (const auto& Handle : GrantedAbilities)
	{
		ASC->CancelAbilityHandle(Handle);
		const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		if (Spec && Spec->IsActive())
		{
			UE_LOG(LogNelaricGasBinding, Error,
			       TEXT("Cannot remove GAS grants on %s: granted ability remained active after cancellation."),
			       *GetName());
			return false;
		}
		ASC->ClearAbility(Handle);
	}
	GrantedAbilities.Reset();
	for (const auto& Handle : ControlEffects)
	{
		if (ASC->GetActiveGameplayEffect(Handle) && !ASC->RemoveActiveGameplayEffect(Handle))
		{
			UE_LOG(LogNelaricGasBinding, Error, TEXT("Cannot remove GAS grants on %s: control effect removal failed."),
			       *GetName());
			return false;
		}
	}
	ControlEffects.Reset();
	GrantsOwner.Reset();
	return true;
}

void UPawnGasBindingComponent::OnRep_StateOwner()
{
	if (auto* Previous = ClientBoundASC.Get();
	    Previous && Previous != GetAbilitySystem() && Previous->GetAvatarActor() == GetPawn())
	{
		Previous->ClearActionInput();
		Previous->InitAbilityActorInfo(Previous->GetOwnerActor(), nullptr);
	}
	InvalidateInitContext();
	RequestInitRefresh();
	if (GetInitState() != Nelaric::EInitState::Ready && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		GetWorld()->GetTimerManager().SetTimer(
		    ReplicationRetry,
		    FTimerDelegate::CreateWeakLambda(this,
		                                     [this]()
		                                     {
			                                     if (GetInitState() == Nelaric::EInitState::Ready ||
			                                         HasTerminalInitFailure())
			                                     {
				                                     GetWorld()->GetTimerManager().ClearTimer(ReplicationRetry);
			                                     }
			                                     else
			                                     {
				                                     RequestInitRefresh();
			                                     }
		                                     }),
		    0.1f, true);
	}
}

void UPawnGasBindingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPawnGasBindingComponent, StateOwner);
	DOREPLIFETIME(UPawnGasBindingComponent, BindingRevision);
	DOREPLIFETIME(UPawnGasBindingComponent, bStateCommitted);
}

void UPawnGasBindingComponent::EndPlay(EEndPlayReason::Type Reason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ReplicationRetry);
	}
	auto* ASC = GetAbilitySystem();
	if (ASC && ASC->GetAvatarActor() == GetPawn())
	{
		ASC->ClearActionInput();
		if (GetPawn()->HasAuthority())
		{
			RemoveGrants(ASC);
			ASC->RemovePawnEffects(GetPawn());
		}
		ASC->InitAbilityActorInfo(ASC->GetOwnerActor(), nullptr);
	}
	if (auto* Policy = GetPawn() ? GetPawn()->FindComponentByClass<UPawnControlComponent>() : nullptr)
	{
		Policy->UnregisterStateTransferParticipant(TEXT("GAS"));
	}
	if (Custodian && !Custodian->IsActorBeingDestroyed())
	{
		Custodian->Destroy();
	}
	Super::EndPlay(Reason);
}

void UPawnGasBindingComponent::ReportInitErrorOnce(FName ErrorId, const TCHAR* Reason) const
{
	if (!ReportedInitErrors.Contains(ErrorId))
	{
		ReportedInitErrors.Add(ErrorId);
		UE_LOG(LogNelaricGasBinding, Error, TEXT("GAS initialization failed on %s: %s (profile=%s stateOwner=%s)."),
		       *GetNameSafe(GetPawn()), Reason, *GetNameSafe(StateProfile), *GetNameSafe(StateOwner));
	}
}
