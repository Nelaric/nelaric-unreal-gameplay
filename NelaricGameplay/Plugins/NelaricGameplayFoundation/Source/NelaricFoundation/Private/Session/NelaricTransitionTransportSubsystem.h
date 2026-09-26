// Copyright (c) 2026 Nelaric

#pragma once

#include "Session/NelaricSessionTransitionSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "NelaricTransitionTransportSubsystem.generated.h"

class ANelaricPlayerController;
class ANelaricTransitionBeaconClient;

UCLASS(MinimalAPI)
class UNelaricTransitionTransportSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool BeginSourceApproval(uint64 RequestId, const UE::Nelaric::FTransitionDestination& Destination);
	bool BeginTargetApproval(uint64 RequestId, const UE::Nelaric::FTransitionDestination& Destination);
	void ReceiveSourceDecision(uint64 RequestId, const FString& TargetAddress, bool bApproved);
	void ReceiveTargetDecision(uint64 RequestId, bool bApproved, bool bAuthorityReplied);
	void CleanupRequest(uint64 RequestId);

	TWeakObjectPtr<ANelaricPlayerController> SourceController;
	TWeakObjectPtr<ANelaricTransitionBeaconClient> TargetBeacon;
	FDelegateHandle SourceDecisionHandle;
	FDelegateHandle TargetDecisionHandle;
	FString PendingTargetAddress;
};
