// Copyright (c) 2026 Nelaric Contributors

#include "Input/DemoPlayerInputComponent.h"

#include "Character/DemoPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "InputActionValue.h"
#include "NativeGameplayTags.h"
#include "NelaricAbilitySystemComponent.h"

namespace Nelaric::DemoInputTags
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Move, "InputTag.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Look_Mouse, "InputTag.Look.Mouse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Look_Stick, "InputTag.Look.Stick");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Jump, "InputTag.Jump");
} // namespace Nelaric::DemoInputTags

namespace Nelaric::DemoInput
{
constexpr float LookYawRate = 300.0f;
constexpr float LookPitchRate = 165.0f;
} // namespace Nelaric::DemoInput

UDemoPlayerInputComponent::UDemoPlayerInputComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UDemoPlayerInputComponent::BindInputActions()
{
	Super::BindInputActions();
	BindNativeAction(Nelaric::DemoInputTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::InputMove);
	BindNativeAction(Nelaric::DemoInputTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this,
	                 &ThisClass::InputLookMouse);
	BindNativeAction(Nelaric::DemoInputTags::InputTag_Look_Stick, ETriggerEvent::Triggered, this,
	                 &ThisClass::InputLookStick);
	BindNativeAction(Nelaric::DemoInputTags::InputTag_Jump, ETriggerEvent::Started, this, &ThisClass::InputJumpStarted);
	BindNativeAction(Nelaric::DemoInputTags::InputTag_Jump, ETriggerEvent::Completed, this,
	                 &ThisClass::InputJumpStopped);
	BindNativeAction(Nelaric::DemoInputTags::InputTag_Jump, ETriggerEvent::Canceled, this,
	                 &ThisClass::InputJumpStopped);
}

void UDemoPlayerInputComponent::UnbindInputActions()
{
	InputJumpStopped();
	Super::UnbindInputActions();
}

void UDemoPlayerInputComponent::InputMove(const FInputActionValue& Value)
{
	ADemoPlayerCharacter* Character = GetPawn<ADemoPlayerCharacter>();
	const AController* Controller = Character ? Character->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	const FVector2D Movement = Value.Get<FVector2D>();
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	if (Movement.X != 0.0f)
	{
		Character->AddMovementInput(YawRotation.RotateVector(FVector::RightVector), Movement.X);
	}
	if (Movement.Y != 0.0f)
	{
		Character->AddMovementInput(YawRotation.RotateVector(FVector::ForwardVector), Movement.Y);
	}
}

void UDemoPlayerInputComponent::InputLookMouse(const FInputActionValue& Value)
{
	if (ADemoPlayerCharacter* Character = GetPawn<ADemoPlayerCharacter>())
	{
		const FVector2D Look = Value.Get<FVector2D>();
		Character->AddControllerYawInput(Look.X);
		Character->AddControllerPitchInput(Look.Y);
	}
}

void UDemoPlayerInputComponent::InputLookStick(const FInputActionValue& Value)
{
	ADemoPlayerCharacter* Character = GetPawn<ADemoPlayerCharacter>();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	const FVector2D Look = Value.Get<FVector2D>();
	const float DeltaSeconds = World->GetDeltaSeconds();
	Character->AddControllerYawInput(Look.X * Nelaric::DemoInput::LookYawRate * DeltaSeconds);
	Character->AddControllerPitchInput(Look.Y * Nelaric::DemoInput::LookPitchRate * DeltaSeconds);
}

void UDemoPlayerInputComponent::InputJumpStarted()
{
	if (ADemoPlayerCharacter* Character = GetPawn<ADemoPlayerCharacter>())
	{
		if (auto* ASC = Cast<UNelaricAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
		{
			ASC->SubmitAction(FGameplayTag::RequestGameplayTag(TEXT("Action.Jump")), true);
		}
	}
}

void UDemoPlayerInputComponent::InputJumpStopped()
{
	if (ADemoPlayerCharacter* Character = GetPawn<ADemoPlayerCharacter>())
	{
		if (auto* ASC = Cast<UNelaricAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
		{
			ASC->SubmitAction(FGameplayTag::RequestGameplayTag(TEXT("Action.Jump")), false);
		}
	}
}
