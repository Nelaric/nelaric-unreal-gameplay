// Copyright (c) 2026 Nelaric

#include "NelaricGameModeBase.h"

#include "OnlineBeaconHost.h"
#include "Player/NelaricPlayerController.h"
#include "Session/NelaricTransitionBeaconHost.h"

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

void ANelaricGameModeBase::StartPlay()
{
	Super::StartPlay();

	if ((GetNetMode() != NM_DedicatedServer && GetNetMode() != NM_ListenServer) || TransitionBeaconListenPort < 1 ||
	    TransitionBeaconListenPort > 65535)
	{
		return;
	}

	BeaconHost = GetWorld()->SpawnActor<AOnlineBeaconHost>();
	if (!BeaconHost)
	{
		return;
	}

	BeaconHost->ListenPort = TransitionBeaconListenPort;
	if (!BeaconHost->InitHost())
	{
		BeaconHost->Destroy();
		BeaconHost = nullptr;
		return;
	}

	TransitionBeaconHost = GetWorld()->SpawnActor<ANelaricTransitionBeaconHost>();
	if (!TransitionBeaconHost)
	{
		BeaconHost->Destroy();
		BeaconHost = nullptr;
		return;
	}

	BeaconHost->RegisterHost(TransitionBeaconHost);
	BeaconHost->PauseBeaconRequests(false);
}
