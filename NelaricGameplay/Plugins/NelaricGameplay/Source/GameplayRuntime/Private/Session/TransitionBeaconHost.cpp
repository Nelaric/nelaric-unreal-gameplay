// Copyright (c) 2026 Nelaric

#include "Session/TransitionBeaconHost.h"

#include "Session/TransitionBeaconClient.h"

ATransitionBeaconHost::ATransitionBeaconHost(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ClientBeaconActorClass = ATransitionBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}
