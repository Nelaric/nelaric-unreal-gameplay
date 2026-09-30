// Copyright (c) 2026 Nelaric

#include "Input/PlayerInputComponent.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Input/NelaricInputConfig.h"

UPlayerInputComponent::UPlayerInputComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UPlayerInputComponent::SetInputConfig(UNelaricInputConfig* NewConfig)
{
	if (InputConfig != NewConfig)
	{
		InputConfig = NewConfig;
		InvalidateInitContext();
	}
}

bool UPlayerInputComponent::CanEntryDataAvailable()
{
	if (!InputConfig)
	{
		return true;
	}
	const APlayerController* Controller = GetPlayerController();
	if (!Controller || !Controller->IsLocalController())
	{
		return true;
	}
	return Controller->GetLocalPlayer() &&
	       Cast<UNelaricInputComponent>(GetPawn() ? GetPawn()->InputComponent : nullptr);
}

bool UPlayerInputComponent::CanEntryDataInitialized()
{
	if (!InputConfig)
	{
		return true;
	}
	const APlayerController* Controller = GetPlayerController();
	if (!Controller || !Controller->IsLocalController())
	{
		return true;
	}
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	return LocalPlayer && LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
}

void UPlayerInputComponent::OnInitReady()
{
	Super::OnInitReady();
	if (!InputConfig)
	{
		return;
	}
	APlayerController* Controller = GetPlayerController();
	ULocalPlayer* LocalPlayer = Controller && Controller->IsLocalController() ? Controller->GetLocalPlayer() : nullptr;
	UNelaricInputComponent* Component = GetPawn() ? Cast<UNelaricInputComponent>(GetPawn()->InputComponent) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
	    LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Component || !Subsystem)
	{
		return;
	}
	if (!Component->AddInputMappings(InputConfig, Subsystem))
	{
		return;
	}
	BoundInputComponent = Component;
	BoundSubsystem = Subsystem;
	bLocalInputActive = true;
	BindInputActions();
	if (bLocalInputActive && BoundInputComponent == Component && BoundSubsystem == Subsystem)
	{
		PlayerInputReady.Broadcast(this);
	}
}

void UPlayerInputComponent::OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous)
{
	ReleaseLocalInput();
	Super::OnInitGenerationInvalidated(Previous);
}

void UPlayerInputComponent::ReleaseLocalInput()
{
	if (!bLocalInputActive)
	{
		return;
	}
	bLocalInputActive = false;
	PlayerInputRevoked.Broadcast(this);
	UnbindInputActions();
	if (UNelaricInputComponent* Component = BoundInputComponent.Get())
	{
		Component->RemoveBinds(BindHandles);
		Component->RemoveInputMappings();
	}
	BindHandles.Reset();
	BoundInputComponent.Reset();
	BoundSubsystem.Reset();
}
