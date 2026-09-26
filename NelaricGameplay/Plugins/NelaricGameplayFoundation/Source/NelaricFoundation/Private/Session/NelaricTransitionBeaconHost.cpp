// Copyright (c) 2026 Nelaric

#include "Session/NelaricTransitionBeaconHost.h"

#include "Session/NelaricTransitionBeaconClient.h"

ANelaricTransitionBeaconHost::ANelaricTransitionBeaconHost(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	ClientBeaconActorClass = ANelaricTransitionBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}
