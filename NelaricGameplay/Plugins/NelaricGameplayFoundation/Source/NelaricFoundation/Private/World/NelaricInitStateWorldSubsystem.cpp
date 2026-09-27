// Copyright (c) 2026 Nelaric

#include "World/NelaricInitStateWorldSubsystem.h"

#include "Components/ActorComponent.h"
#include "Misc/ScopeExit.h"
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
	ProcessParticipants();
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

			TArray<Nelaric::FInitDependency> Dependencies;
			Participant->GatherInitDependencies(Dependencies);
			bool bDependenciesReady = true;
			for (const Nelaric::FInitDependency& Dependency : Dependencies)
			{
				UActorComponent* RequiredComponent = Dependency.Component.Get();
				const INelaricInitStateParticipantInterface* Required =
				    Cast<INelaricInitStateParticipantInterface>(RequiredComponent);
				if (!Required || !RegisteredComponents.Contains(RequiredComponent) ||
				    Required->HasTerminalInitFailure() || !Required->IsInitApplicable() ||
				    Required->GetInitState() < Dependency.RequiredState)
				{
					bDependenciesReady = false;
					break;
				}
			}
			if (!bDependenciesReady)
			{
				continue;
			}

			const Nelaric::FInitGeneration Generation = Participant->GetInitGeneration();
			const Nelaric::EInitState PreviousState = Participant->GetInitState();
			const bool bAdvanced = Participant->TryChangeInitState();
			const bool bExactlyOneStep =
			    Generation == Participant->GetInitGeneration() &&
			    static_cast<uint8>(Participant->GetInitState()) == static_cast<uint8>(PreviousState) + 1;
			if (bAdvanced)
			{
				ensureMsgf(bExactlyOneStep,
				           TEXT("TryChangeInitState must commit exactly one step in the same generation."));
				bProcessRequested |= bExactlyOneStep;
			}
			else
			{
				ensureMsgf(Generation == Participant->GetInitGeneration() &&
				               PreviousState == Participant->GetInitState(),
				           TEXT("TryChangeInitState returned false after changing state or generation."));
			}
		}
	} while (bProcessRequested && --RemainingPasses > 0);
	ensureMsgf(!bProcessRequested, TEXT("Initialization participants requested too many progress passes."));
}

void UNelaricInitStateWorldSubsystem::Deinitialize()
{
	RegisteredComponents.Empty();
	Super::Deinitialize();
}
