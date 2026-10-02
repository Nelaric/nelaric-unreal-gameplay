// Copyright (c) 2026 Nelaric Contributors

#include "Session/TransitionTransportSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Internal/GameplayRuntimeInternalAccess.h"
#include "Player/NelaricPlayerController.h"
#include "Session/SessionTransitionSubsystem.h"
#include "Session/TransitionBeaconClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricTransport, Log, All);

void UTransitionTransportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<USessionTransitionSubsystem>();
	Super::Initialize(Collection);

	USessionTransitionSubsystem* Coordinator = GetGameInstance()->GetSubsystem<USessionTransitionSubsystem>();
	Nelaric::FStartTransitionApproval Source =
	    Nelaric::FStartTransitionApproval::CreateUObject(this, &UTransitionTransportSubsystem::BeginSourceApproval);
	Nelaric::FStartTransitionApproval Target =
	    Nelaric::FStartTransitionApproval::CreateUObject(this, &UTransitionTransportSubsystem::BeginTargetApproval);
	Nelaric::FOnTransitionTerminated Terminated =
	    Nelaric::FOnTransitionTerminated::CreateUObject(this, &UTransitionTransportSubsystem::CleanupRequest);
	Coordinator->InternalConfigureApprovalTransport(Nelaric::FGameplayRuntimeInternalAccess::Key(), Source, Target,
	                                                Terminated);
}

void UTransitionTransportSubsystem::Deinitialize()
{
	if (USessionTransitionSubsystem* Coordinator = GetGameInstance()->GetSubsystem<USessionTransitionSubsystem>())
	{
		Coordinator->InternalClearApprovalTransport(Nelaric::FGameplayRuntimeInternalAccess::Key());
	}
	CleanupRequest(0);
	Super::Deinitialize();
}

bool UTransitionTransportSubsystem::BeginSourceApproval(uint64 RequestId,
                                                        const Nelaric::FTransitionDestination& Destination)
{
	ANelaricPlayerController* Controller =
	    Cast<ANelaricPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (!Controller)
	{
		UE_LOG(LogNelaricTransport, Error,
		       TEXT("Source approval failed (request=%llu): local controller does not support departure approval."),
		       RequestId);
		return false;
	}
	SourceController = Controller;
	const FString AddressWithPort =
	    FString::Printf(TEXT("%s:%d"), *Destination.GameEndpoint.Address, Destination.GameEndpoint.Port);
	const FURL GameURL(nullptr, *AddressWithPort, TRAVEL_Absolute);
	PendingTargetAddress = GameURL.ToString();
	SourceDecisionHandle =
	    Controller->OnDepartureDecision().AddUObject(this, &UTransitionTransportSubsystem::ReceiveSourceDecision);
	return Controller->RequestDepartureApproval(RequestId, PendingTargetAddress);
}

bool UTransitionTransportSubsystem::BeginTargetApproval(uint64 RequestId,
                                                        const Nelaric::FTransitionDestination& Destination)
{
	UWorld* World = GetGameInstance()->GetWorld();
	ATransitionBeaconClient* Beacon = World ? World->SpawnActor<ATransitionBeaconClient>() : nullptr;
	if (!Beacon)
	{
		UE_LOG(LogNelaricTransport, Error,
		       TEXT("Target approval failed (request=%llu): could not spawn the target beacon client."), RequestId);
		return false;
	}
	TargetBeacon = Beacon;
	TargetDecisionHandle =
	    Beacon->OnTargetDecision().AddUObject(this, &UTransitionTransportSubsystem::ReceiveTargetDecision);
	return Beacon->BeginTargetApproval(Destination.BeaconEndpoint, RequestId);
}

void UTransitionTransportSubsystem::ReceiveSourceDecision(uint64 RequestId, const FString& TargetAddress,
                                                          bool bApproved)
{
	if (USessionTransitionSubsystem* Coordinator = GetGameInstance()->GetSubsystem<USessionTransitionSubsystem>())
	{
		Coordinator->InternalReportSourceApproval(Nelaric::FGameplayRuntimeInternalAccess::Key(), RequestId,
		                                          bApproved && TargetAddress == PendingTargetAddress);
	}
}

void UTransitionTransportSubsystem::ReceiveTargetDecision(uint64 RequestId, bool bApproved, bool bAuthorityReplied)
{
	if (USessionTransitionSubsystem* Coordinator = GetGameInstance()->GetSubsystem<USessionTransitionSubsystem>())
	{
		if (bAuthorityReplied)
		{
			Coordinator->InternalReportTargetApproval(Nelaric::FGameplayRuntimeInternalAccess::Key(), RequestId,
			                                          bApproved);
		}
		else
		{
			Coordinator->InternalReportTargetUnavailable(Nelaric::FGameplayRuntimeInternalAccess::Key(), RequestId);
		}
	}
}

void UTransitionTransportSubsystem::CleanupRequest(uint64 RequestId)
{
	if (ANelaricPlayerController* Controller = SourceController.Get())
	{
		Controller->OnDepartureDecision().Remove(SourceDecisionHandle);
	}
	SourceController.Reset();
	SourceDecisionHandle.Reset();
	PendingTargetAddress.Reset();

	if (ATransitionBeaconClient* Beacon = TargetBeacon.Get())
	{
		Beacon->OnTargetDecision().Remove(TargetDecisionHandle);
		Beacon->DestroyBeacon();
	}
	TargetBeacon.Reset();
	TargetDecisionHandle.Reset();
}
