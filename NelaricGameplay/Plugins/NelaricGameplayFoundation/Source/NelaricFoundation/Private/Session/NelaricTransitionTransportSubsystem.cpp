// Copyright (c) 2026 Nelaric

#include "Session/NelaricTransitionTransportSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Internal/InternalAccess.h"
#include "Player/NelaricPlayerController.h"
#include "Session/NelaricSessionTransitionSubsystem.h"
#include "Session/NelaricTransitionBeaconClient.h"

void UNelaricTransitionTransportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UNelaricSessionTransitionSubsystem>();
	Super::Initialize(Collection);

	UNelaricSessionTransitionSubsystem* Coordinator =
	    GetGameInstance()->GetSubsystem<UNelaricSessionTransitionSubsystem>();
	UE::Nelaric::FStartTransitionApproval Source = UE::Nelaric::FStartTransitionApproval::CreateUObject(
	    this, &UNelaricTransitionTransportSubsystem::BeginSourceApproval);
	UE::Nelaric::FStartTransitionApproval Target = UE::Nelaric::FStartTransitionApproval::CreateUObject(
	    this, &UNelaricTransitionTransportSubsystem::BeginTargetApproval);
	UE::Nelaric::FOnTransitionTerminated Terminated = UE::Nelaric::FOnTransitionTerminated::CreateUObject(
	    this, &UNelaricTransitionTransportSubsystem::CleanupRequest);
	Coordinator->InternalConfigureApprovalTransport(UE::Nelaric::FInternalAccess::Key(), Source, Target, Terminated);
}

void UNelaricTransitionTransportSubsystem::Deinitialize()
{
	if (UNelaricSessionTransitionSubsystem* Coordinator =
	        GetGameInstance()->GetSubsystem<UNelaricSessionTransitionSubsystem>())
	{
		Coordinator->InternalClearApprovalTransport(UE::Nelaric::FInternalAccess::Key());
	}
	CleanupRequest(0);
	Super::Deinitialize();
}

bool UNelaricTransitionTransportSubsystem::BeginSourceApproval(uint64 RequestId,
                                                               const UE::Nelaric::FTransitionDestination& Destination)
{
	ANelaricPlayerController* Controller =
	    Cast<ANelaricPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (!Controller)
	{
		return false;
	}
	SourceController = Controller;
	const FString AddressWithPort =
	    FString::Printf(TEXT("%s:%d"), *Destination.GameEndpoint.Address, Destination.GameEndpoint.Port);
	const FURL GameURL(nullptr, *AddressWithPort, TRAVEL_Absolute);
	PendingTargetAddress = GameURL.ToString();
	SourceDecisionHandle = Controller->OnDepartureDecision().AddUObject(
	    this, &UNelaricTransitionTransportSubsystem::ReceiveSourceDecision);
	return Controller->RequestDepartureApproval(RequestId, PendingTargetAddress);
}

bool UNelaricTransitionTransportSubsystem::BeginTargetApproval(uint64 RequestId,
                                                               const UE::Nelaric::FTransitionDestination& Destination)
{
	UWorld* World = GetGameInstance()->GetWorld();
	ANelaricTransitionBeaconClient* Beacon = World ? World->SpawnActor<ANelaricTransitionBeaconClient>() : nullptr;
	if (!Beacon)
	{
		return false;
	}
	TargetBeacon = Beacon;
	TargetDecisionHandle =
	    Beacon->OnTargetDecision().AddUObject(this, &UNelaricTransitionTransportSubsystem::ReceiveTargetDecision);
	return Beacon->BeginTargetApproval(Destination.BeaconEndpoint, RequestId);
}

void UNelaricTransitionTransportSubsystem::ReceiveSourceDecision(uint64 RequestId, const FString& TargetAddress,
                                                                 bool bApproved)
{
	if (UNelaricSessionTransitionSubsystem* Coordinator =
	        GetGameInstance()->GetSubsystem<UNelaricSessionTransitionSubsystem>())
	{
		Coordinator->InternalReportSourceApproval(UE::Nelaric::FInternalAccess::Key(), RequestId,
		                                          bApproved && TargetAddress == PendingTargetAddress);
	}
}

void UNelaricTransitionTransportSubsystem::ReceiveTargetDecision(uint64 RequestId, bool bApproved,
                                                                 bool bAuthorityReplied)
{
	if (UNelaricSessionTransitionSubsystem* Coordinator =
	        GetGameInstance()->GetSubsystem<UNelaricSessionTransitionSubsystem>())
	{
		if (bAuthorityReplied)
		{
			Coordinator->InternalReportTargetApproval(UE::Nelaric::FInternalAccess::Key(), RequestId, bApproved);
		}
		else
		{
			Coordinator->InternalReportTargetUnavailable(UE::Nelaric::FInternalAccess::Key(), RequestId);
		}
	}
}

void UNelaricTransitionTransportSubsystem::CleanupRequest(uint64 RequestId)
{
	if (ANelaricPlayerController* Controller = SourceController.Get())
	{
		Controller->OnDepartureDecision().Remove(SourceDecisionHandle);
	}
	SourceController.Reset();
	SourceDecisionHandle.Reset();
	PendingTargetAddress.Reset();

	if (ANelaricTransitionBeaconClient* Beacon = TargetBeacon.Get())
	{
		Beacon->OnTargetDecision().Remove(TargetDecisionHandle);
		Beacon->DestroyBeacon();
	}
	TargetBeacon.Reset();
	TargetDecisionHandle.Reset();
}
