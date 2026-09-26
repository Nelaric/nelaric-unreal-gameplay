// Copyright (c) 2026 Nelaric

#include "Session/NelaricSessionTransitionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

namespace
{
constexpr double TransitionTimeoutSeconds = 30.0;

FURL MakeEndpointURL(const UE::Nelaric::FNetworkEndpoint& Endpoint)
{
	const FString AddressWithPort = FString::Printf(TEXT("%s:%d"), *Endpoint.Address, Endpoint.Port);
	return FURL(nullptr, *AddressWithPort, TRAVEL_Absolute);
}
} // namespace

void UNelaricSessionTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
	    FTickerDelegate::CreateUObject(this, &UNelaricSessionTransitionSubsystem::Tick), 0.1f);
}

void UNelaricSessionTransitionSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (Active)
	{
		FinishCancelled();
	}
	StartSourceApproval.Unbind();
	StartTargetApproval.Unbind();
	OnTerminated.Unbind();
	Super::Deinitialize();
}

void UNelaricSessionTransitionSubsystem::InternalConfigureApprovalTransport(
    const UE::Nelaric::FInternalAccessKey&, const UE::Nelaric::FStartTransitionApproval& Source,
    const UE::Nelaric::FStartTransitionApproval& Target, const UE::Nelaric::FOnTransitionTerminated& Terminated)
{
	StartSourceApproval = Source;
	StartTargetApproval = Target;
	OnTerminated = Terminated;
}

void UNelaricSessionTransitionSubsystem::InternalClearApprovalTransport(const UE::Nelaric::FInternalAccessKey&)
{
	if (Active)
	{
		Fail(UE::Nelaric::ETransitionError::AuthorityUnavailable);
	}
	StartSourceApproval.Unbind();
	StartTargetApproval.Unbind();
	OnTerminated.Unbind();
}

UE::Nelaric::FTransitionHandle
UNelaricSessionTransitionSubsystem::RequestTransition(ENetMode TargetMode,
                                                      const UE::Nelaric::FTransitionDestination& Destination,
                                                      const UE::Nelaric::FTransitionCallbacks& Callbacks)
{
	if (Active)
	{
		return {};
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || World->GetNetMode() != NM_Client || TargetMode != NM_Client || !Destination.GameEndpoint.IsValid() ||
	    !Destination.BeaconEndpoint.IsValid() || !StartSourceApproval.IsBound() || !StartTargetApproval.IsBound())
	{
		return {};
	}

	const FURL GameURL = MakeEndpointURL(Destination.GameEndpoint);
	const FURL BeaconURL = MakeEndpointURL(Destination.BeaconEndpoint);
	if (!GameURL.Valid || GameURL.Host.IsEmpty() || !BeaconURL.Valid || BeaconURL.Host.IsEmpty())
	{
		return {};
	}

	Active = MakeUnique<FActiveTransition>();
	Active->Handle.Id = NextRequestId++;
	Active->Callbacks = Callbacks;
	Active->Destination = Destination;
	Active->SourceWorld = World;
	Active->DeadlineSeconds = FPlatformTime::Seconds() + TransitionTimeoutSeconds;
	const UE::Nelaric::FTransitionHandle Handle = Active->Handle;

	if (!StartSourceApproval.Execute(Handle.Id, Destination))
	{
		Fail(UE::Nelaric::ETransitionError::AuthorityUnavailable);
		return Handle;
	}
	if (Active && !StartTargetApproval.Execute(Handle.Id, Destination))
	{
		Fail(UE::Nelaric::ETransitionError::AuthorityUnavailable);
	}
	return Handle;
}

bool UNelaricSessionTransitionSubsystem::CancelTransition(UE::Nelaric::FTransitionHandle Handle)
{
	if (!Active || Active->Handle.Id != Handle.Id || Active->bTravelStarted)
	{
		return false;
	}
	FinishCancelled();
	return true;
}

TOptional<ENetMode> UNelaricSessionTransitionSubsystem::GetNetMode() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? TOptional<ENetMode>(World->GetNetMode()) : TOptional<ENetMode>();
}

void UNelaricSessionTransitionSubsystem::InternalReportSourceApproval(const UE::Nelaric::FInternalAccessKey&,
                                                                      uint64 RequestId, bool bApproved)
{
	if (!Active || Active->Handle.Id != RequestId || Active->bTravelStarted)
	{
		return;
	}
	if (!bApproved)
	{
		Fail(UE::Nelaric::ETransitionError::AuthorityRejected);
		return;
	}
	Active->bSourceApproved = true;
}

void UNelaricSessionTransitionSubsystem::InternalReportTargetApproval(const UE::Nelaric::FInternalAccessKey&,
                                                                      uint64 RequestId, bool bApproved)
{
	if (!Active || Active->Handle.Id != RequestId || Active->bTravelStarted)
	{
		return;
	}
	if (!bApproved)
	{
		Fail(UE::Nelaric::ETransitionError::AuthorityRejected);
		return;
	}
	Active->bTargetApproved = true;
}

void UNelaricSessionTransitionSubsystem::InternalReportTargetUnavailable(const UE::Nelaric::FInternalAccessKey&,
                                                                         uint64 RequestId)
{
	if (Active && Active->Handle.Id == RequestId && !Active->bTravelStarted)
	{
		Fail(UE::Nelaric::ETransitionError::AuthorityUnavailable);
	}
}

void UNelaricSessionTransitionSubsystem::TryStartTravel()
{
	if (!Active || !Active->bSourceApproved || !Active->bTargetApproved || Active->bTravelStarted)
	{
		return;
	}
	if (FPlatformTime::Seconds() >= Active->DeadlineSeconds)
	{
		FinishTimedOut();
		return;
	}
	APlayerController* PlayerController =
	    GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		Fail(UE::Nelaric::ETransitionError::NoWorld);
		return;
	}
	Active->bTravelStarted = true;
	PlayerController->ClientTravel(MakeEndpointURL(Active->Destination.GameEndpoint).ToString(), TRAVEL_Absolute);
}

bool UNelaricSessionTransitionSubsystem::Tick(float DeltaTime)
{
	if (!Active)
	{
		return true;
	}
	if (FPlatformTime::Seconds() >= Active->DeadlineSeconds)
	{
		FinishTimedOut();
		return true;
	}
	if (!Active->bTravelStarted)
	{
		TryStartTravel();
		return true;
	}
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World != Active->SourceWorld.Get() && World->GetNetMode() == NM_Client && World->GetNetDriver() &&
	    World->GetNetDriver()->ServerConnection &&
	    World->GetNetDriver()->ServerConnection->GetConnectionState() == USOCK_Open)
	{
		const FURL ExpectedURL = MakeEndpointURL(Active->Destination.GameEndpoint);
		const FURL& ConnectedURL = World->GetNetDriver()->ServerConnection->URL;
		if (ExpectedURL.Valid && ConnectedURL.Host.Equals(ExpectedURL.Host, ESearchCase::IgnoreCase) &&
		    ConnectedURL.Port == ExpectedURL.Port)
		{
			FinishSucceeded(NM_Client);
		}
	}
	return true;
}

void UNelaricSessionTransitionSubsystem::Fail(UE::Nelaric::ETransitionError Error)
{
	const UE::Nelaric::FTransitionHandle Handle = Active->Handle;
	const UE::Nelaric::FTransitionCallbacks Callbacks = Active->Callbacks;
	Active.Reset();
	OnTerminated.ExecuteIfBound(Handle.Id);
	Callbacks.OnFailed.ExecuteIfBound(Handle, Error);
}

void UNelaricSessionTransitionSubsystem::FinishSucceeded(ENetMode Mode)
{
	const UE::Nelaric::FTransitionHandle Handle = Active->Handle;
	const UE::Nelaric::FTransitionCallbacks Callbacks = Active->Callbacks;
	Active.Reset();
	OnTerminated.ExecuteIfBound(Handle.Id);
	Callbacks.OnSucceeded.ExecuteIfBound(Handle, Mode);
}

void UNelaricSessionTransitionSubsystem::FinishTimedOut()
{
	const UE::Nelaric::FTransitionHandle Handle = Active->Handle;
	const UE::Nelaric::FTransitionCallbacks Callbacks = Active->Callbacks;
	Active.Reset();
	OnTerminated.ExecuteIfBound(Handle.Id);
	Callbacks.OnTimedOut.ExecuteIfBound(Handle);
}

void UNelaricSessionTransitionSubsystem::FinishCancelled()
{
	const UE::Nelaric::FTransitionHandle Handle = Active->Handle;
	const UE::Nelaric::FTransitionCallbacks Callbacks = Active->Callbacks;
	Active.Reset();
	OnTerminated.ExecuteIfBound(Handle.Id);
	Callbacks.OnCancelled.ExecuteIfBound(Handle);
}
