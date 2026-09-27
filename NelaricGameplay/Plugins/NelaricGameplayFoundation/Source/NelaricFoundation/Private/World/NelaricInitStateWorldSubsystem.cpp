// Copyright (c) 2026 Nelaric

#include "World/NelaricInitStateWorldSubsystem.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Pawn/NelaricPawnInitializationComponent.h"
#include "World/NelaricInitStateParticipantInterface.h"

void UNelaricInitStateWorldSubsystem::RegisterParticipant(UActorComponent* Component)
{
	if (!IsValid(Component) || Component->GetWorld() != GetWorld() ||
	    !Component->GetClass()->ImplementsInterface(UNelaricInitStateParticipantInterface::StaticClass()))
	{
		return;
	}

	RegisteredComponents.Add(Component);
	ProcessParticipants();
}

void UNelaricInitStateWorldSubsystem::UnregisterParticipant(UActorComponent* Component)
{
	RegisteredComponents.Remove(Component);
	InvalidateConfiguredDependents(Component);
	ProcessParticipants();
}

void UNelaricInitStateWorldSubsystem::ConfigureParticipant(UActorComponent* Component, FName ComponentId,
                                                           bool bRequiredForPawnReady,
                                                           const TArray<UActorComponent*>& Dependencies)
{
	if (IsValid(Component) && !ComponentId.IsNone())
	{
		FConfiguredParticipant Configuration;
		Configuration.ComponentId = ComponentId;
		Configuration.bRequiredForPawnReady = bRequiredForPawnReady;
		for (UActorComponent* Dependency : Dependencies)
		{
			Configuration.Dependencies.Add(Dependency);
		}
		ConfiguredComponents.Add(Component, MoveTemp(Configuration));
		if (RegisteredComponents.Contains(Component))
		{
			ProcessParticipants();
		}
	}
}

void UNelaricInitStateWorldSubsystem::UnconfigureParticipant(UActorComponent* Component)
{
	ConfiguredComponents.Remove(Component);
	ProcessParticipants();
}

bool UNelaricInitStateWorldSubsystem::AreRequiredParticipantsReady(const AActor* Owner) const
{
	if (!Owner)
	{
		return false;
	}
	for (const auto& Pair : ConfiguredComponents)
	{
		UActorComponent* Component = Pair.Key.Get();
		if (!IsValid(Component) || Component->GetOwner() != Owner || !Pair.Value.bRequiredForPawnReady)
		{
			continue;
		}
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		if (!RegisteredComponents.Contains(Component) || !Participant || !Participant->IsInitApplicable() ||
		    Participant->HasTerminalInitFailure() || Participant->GetInitState() != Nelaric::EInitState::Ready)
		{
			return false;
		}
	}
	return true;
}

void UNelaricInitStateWorldSubsystem::InvalidateConfiguredDependents(UActorComponent* Component)
{
	if (!IsValid(Component))
	{
		return;
	}
	const FConfiguredParticipant* Changed = ConfiguredComponents.Find(Component);
	if (!Changed)
	{
		return;
	}
	for (const auto& Pair : ConfiguredComponents)
	{
		UActorComponent* DependentComponent = Pair.Key.Get();
		if (!IsValid(DependentComponent) || !Pair.Value.Dependencies.Contains(FComponentPtr(Component)))
		{
			continue;
		}
		INelaricInitStateParticipantInterface* Dependent =
		    Cast<INelaricInitStateParticipantInterface>(DependentComponent);
		if (Dependent && Dependent->GetInitState() == Nelaric::EInitState::Ready)
		{
			Dependent->InvalidateInitGeneration();
		}
	}
}

bool UNelaricInitStateWorldSubsystem::TryCommitReadyGroup(UActorComponent* Root)
{
	TArray<UActorComponent*> Pending{Root};
	TSet<FComponentPtr> Seen;
	TSet<FComponentPtr> Queued;
	Queued.Add(Root);
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		UActorComponent* Component = Pending[Index];
		if (!IsValid(Component) || !RegisteredComponents.Contains(Component))
		{
			return false;
		}
		if (Seen.Contains(Component))
		{
			continue;
		}
		Seen.Add(Component);
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		if (!Participant || !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
		    Participant->GetInitState() != Nelaric::EInitState::DataInitialized)
		{
			return false;
		}
		if (const FConfiguredParticipant* Configuration = ConfiguredComponents.Find(Component))
		{
			for (const FComponentPtr& DependencyPtr : Configuration->Dependencies)
			{
				UActorComponent* Dependency = DependencyPtr.Get();
				const INelaricInitStateParticipantInterface* Required =
				    Cast<INelaricInitStateParticipantInterface>(Dependency);
				if (!IsValid(Dependency) || Dependency->GetOwner() != Component->GetOwner() || !Required ||
				    !RegisteredComponents.Contains(Dependency) || !Required->IsInitApplicable() ||
				    Required->HasTerminalInitFailure())
				{
					return false;
				}
				if (Required->GetInitState() != Nelaric::EInitState::Ready && !Queued.Contains(Dependency))
				{
					Pending.Add(Dependency);
					Queued.Add(Dependency);
				}
			}
		}
	}

	TArray<Nelaric::FInitStateSnapshot> BeforeReady;
	BeforeReady.Reserve(Seen.Num());
	for (UActorComponent* Component : Pending)
	{
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		const Nelaric::FInitStateSnapshot Before{Participant->GetInitGeneration(), Participant->GetInitState(),
		                                         Participant->HasTerminalInitFailure()};
		if (!Participant->CanEnterReady() || !(Before.Generation == Participant->GetInitGeneration()) ||
		    Before.State != Participant->GetInitState() || Participant->HasTerminalInitFailure())
		{
			return false;
		}
		BeforeReady.Add(Before);
	}
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Pending[Index]);
		if (!(BeforeReady[Index].Generation == Participant->GetInitGeneration()) ||
		    BeforeReady[Index].State != Participant->GetInitState() || Participant->HasTerminalInitFailure())
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Pending[Index]);
		if (!ensureMsgf(Participant->CommitReadyWithoutNotification(),
		                TEXT("Ready group member failed its silent commit.")))
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		Cast<INelaricInitStateParticipantInterface>(Pending[Index])->NotifyReadyCommitted(BeforeReady[Index]);
	}
	return true;
}

void UNelaricInitStateWorldSubsystem::NotifyParticipantChanged(UActorComponent* Component,
                                                               const Nelaric::FInitStateSnapshot& Previous)
{
	if (!RegisteredComponents.Contains(Component))
	{
		return;
	}

	const INelaricInitStateParticipantInterface* Participant = Cast<INelaricInitStateParticipantInterface>(Component);
	if (!Participant)
	{
		return;
	}

	const bool bChanged = !(Previous.Generation == Participant->GetInitGeneration()) ||
	                      Previous.State != Participant->GetInitState() ||
	                      Previous.bTerminallyFailed != Participant->HasTerminalInitFailure();
	if (bChanged)
	{
		if (Previous.State == Nelaric::EInitState::Ready &&
		    (Participant->GetInitState() != Nelaric::EInitState::Ready || Participant->HasTerminalInitFailure() ||
		     !(Previous.Generation == Participant->GetInitGeneration())))
		{
			InvalidateConfiguredDependents(Component);
		}
		ProcessParticipants();
	}
}

void UNelaricInitStateWorldSubsystem::RequestParticipantRefresh(UActorComponent* Component)
{
	if (RegisteredComponents.Contains(Component))
	{
		ProcessParticipants();
	}
}

void UNelaricInitStateWorldSubsystem::ProcessParticipants()
{
	if (bProcessing)
	{
		bProcessRequested = true;
		return;
	}

	bProcessing = true;
	ON_SCOPE_EXIT
	{
		bProcessing = false;
	};

	// A successful step is bounded by four states. Revisit participants only
	// when a change can unlock another component.
	int32 RemainingPasses = RegisteredComponents.Num() * 3 + 1;
	do
	{
		bProcessRequested = false;
		const TArray<FComponentPtr> Participants = RegisteredComponents.Array();
		for (const FComponentPtr& ParticipantPtr : Participants)
		{
			UActorComponent* Component = ParticipantPtr.Get();
			if (!IsValid(Component) || Component->GetWorld() != GetWorld())
			{
				RegisteredComponents.Remove(ParticipantPtr);
				continue;
			}
			if (!RegisteredComponents.Contains(ParticipantPtr))
			{
				continue;
			}

			INelaricInitStateParticipantInterface* Participant = Cast<INelaricInitStateParticipantInterface>(Component);
			if (!Participant || !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
			    Participant->GetInitState() == Nelaric::EInitState::Ready)
			{
				continue;
			}

			const Nelaric::FInitGeneration Generation = Participant->GetInitGeneration();
			const Nelaric::EInitState PreviousState = Participant->GetInitState();
			bool bAdvanced = false;
			if (PreviousState == Nelaric::EInitState::DataInitialized)
			{
				bAdvanced = TryCommitReadyGroup(Component);
			}
			else
			{
				bAdvanced = Participant->TryChangeInitState();
			}
			const bool bExactlyOneStep =
			    Generation == Participant->GetInitGeneration() &&
			    static_cast<uint8>(Participant->GetInitState()) == static_cast<uint8>(PreviousState) + 1;
			if (bAdvanced)
			{
				ensureMsgf(bExactlyOneStep,
				           TEXT("Initialization transition must commit exactly one step in the same generation."));
				bProcessRequested |= bExactlyOneStep;
			}
			else
			{
				ensureMsgf(Generation == Participant->GetInitGeneration() &&
				               PreviousState == Participant->GetInitState(),
				           TEXT("Initialization transition returned false after changing state or generation."));
			}
		}
	} while (bProcessRequested && --RemainingPasses > 0);
	ensureMsgf(!bProcessRequested, TEXT("Initialization participants requested too many progress passes."));
	for (const FComponentPtr& ParticipantPtr : RegisteredComponents)
	{
		if (UActorComponent* Component = ParticipantPtr.Get())
		{
			if (AActor* Owner = Component->GetOwner())
			{
				if (UNelaricPawnInitializationComponent* Manager =
				        Owner->FindComponentByClass<UNelaricPawnInitializationComponent>())
				{
					Manager->TryInitializePawn();
				}
			}
		}
	}
}

void UNelaricInitStateWorldSubsystem::Deinitialize()
{
	RegisteredComponents.Empty();
	ConfiguredComponents.Empty();
	Super::Deinitialize();
}
