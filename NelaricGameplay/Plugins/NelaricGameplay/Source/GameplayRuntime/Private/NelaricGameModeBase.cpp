// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGameModeBase.h"

#include "OnlineBeaconHost.h"
#include "Player/NelaricPlayerController.h"
#include "Session/TransitionBeaconHost.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricGameMode, Log, All);

ANelaricGameModeBase::ANelaricGameModeBase()
{
	PlayerControllerClass = ANelaricPlayerController::StaticClass();
}

bool ANelaricGameModeBase::CanAcceptTransition() const
{
	// TODO(NELARIC-TRANSITION-CAPACITY-INTEGRATION): Fetch the active
	// WorldStartupConfig and enforce MaxPlayers, including pending joins.
	return true;
}

bool ANelaricGameModeBase::CanChangePawnControl_Implementation(AController* Requester, EControlSwitchAction Action,
                                                               APawn* TargetPawn) const
{
	return HasAuthority();
}

void ANelaricGameModeBase::StartPlay()
{
	Super::StartPlay();

	if (GetNetMode() != NM_DedicatedServer && GetNetMode() != NM_ListenServer)
	{
		return;
	}
	if (TransitionBeaconListenPort < 1 || TransitionBeaconListenPort > 65535)
	{
		UE_LOG(LogNelaricGameMode, Error, TEXT("Transition beacon startup failed on %s: invalid listen port %d."),
		       *GetName(), TransitionBeaconListenPort);
		return;
	}

	BeaconHost = GetWorld()->SpawnActor<AOnlineBeaconHost>();
	if (!BeaconHost)
	{
		UE_LOG(LogNelaricGameMode, Error,
		       TEXT("Transition beacon startup failed on %s (port=%d): could not spawn the beacon listener."),
		       *GetName(), TransitionBeaconListenPort);
		return;
	}

	BeaconHost->ListenPort = TransitionBeaconListenPort;
	if (!BeaconHost->InitHost())
	{
		UE_LOG(LogNelaricGameMode, Error,
		       TEXT("Transition beacon startup failed on %s (port=%d): could not initialize the beacon listener."),
		       *GetName(), TransitionBeaconListenPort);
		BeaconHost->Destroy();
		BeaconHost = nullptr;
		return;
	}

	TransitionBeaconHost = GetWorld()->SpawnActor<ATransitionBeaconHost>();
	if (!TransitionBeaconHost)
	{
		UE_LOG(LogNelaricGameMode, Error,
		       TEXT("Transition beacon startup failed on %s (port=%d): could not spawn the transition beacon host."),
		       *GetName(), TransitionBeaconListenPort);
		BeaconHost->Destroy();
		BeaconHost = nullptr;
		return;
	}

	BeaconHost->RegisterHost(TransitionBeaconHost);
	BeaconHost->PauseBeaconRequests(false);
}
