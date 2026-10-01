// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Containers/Array.h"
#include "Misc/Guid.h"
#include "Pawn/InitStateTypes.h"
#include "Player/NelaricPlayerController.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class AAIController;
class AController;
class ANelaricGameModeBase;
class APawn;
class APlayerState;
class UClass;
class UPawnControlComponent;

namespace Nelaric::Control
{
// Capture policy identity as well as settings: a replacement must not inherit
// approval granted to a different component or initialization generation.
struct FPolicySnapshot
{
	TWeakObjectPtr<UPawnControlComponent> Component;
	FInitGeneration Generation;
	TWeakObjectPtr<UClass> ReturnControllerClass;
	bool bAllowPlayerControl = false;
	bool bAllowReturnControl = false;
	bool bReturnToBot = false;
	bool bStartBotLogicOnReady = false;
};

// Weak references preserve the validated identities without extending actor
// lifetime. Preparation may add a replacement, but cannot retarget the plan.
struct FSwitchPlan
{
	FGuid TransitionId;
	EControlSwitchAction Action = EControlSwitchAction::TakeControl;
	TWeakObjectPtr<ANelaricGameModeBase> GameMode;
	TWeakObjectPtr<UClass> PlayerStateClass;
	TWeakObjectPtr<AController> Requester;
	TWeakObjectPtr<APlayerState> RequesterState;
	TWeakObjectPtr<APawn> OldPawn;
	TWeakObjectPtr<APawn> TargetPawn;
	TWeakObjectPtr<APlayerState> TargetPawnState;
	TWeakObjectPtr<AController> PreviousTargetController;
	TWeakObjectPtr<APlayerState> PreviousTargetState;
	FPolicySnapshot OldPolicy;
	FPolicySnapshot TargetPolicy;
	TWeakObjectPtr<AAIController> ReturnController;
	TWeakObjectPtr<APlayerState> ReturnState;
	TWeakObjectPtr<AAIController> SpawnedReturnController;
	TArray<TWeakObjectPtr<const AActor>> Participants;
	bool bNeedsReturnController = false;
};
} // namespace Nelaric::Control
