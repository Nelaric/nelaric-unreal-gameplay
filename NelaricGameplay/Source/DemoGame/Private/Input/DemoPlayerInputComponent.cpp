// Copyright (c) 2026 Nelaric Contributors

#include "Input/DemoPlayerInputComponent.h"

#include "Character/DemoCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "InputActionValue.h"
#include "Input/NelaricInputConfig.h"
#include "NativeGameplayTags.h"
#include "NelaricAbilitySystemComponent.h"
#include "Player/DemoOverviewPawn.h"
#include "Player/DemoPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoInput, Log, All);

namespace Nelaric::DemoInputTags
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Character_Move, "InputTag.Character.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Character_Look_Mouse, "InputTag.Character.Look.Mouse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Character_Look_Stick, "InputTag.Character.Look.Stick");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Character_Jump, "InputTag.Character.Jump");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Character_ReturnOverview, "InputTag.Character.ReturnOverview");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Overview_Move, "InputTag.Overview.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Overview_Drag, "InputTag.Overview.Drag");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Overview_Click, "InputTag.Overview.Click");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputMapping_Character, "InputMapping.Character");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputMapping_Overview, "InputMapping.Overview");
} // namespace Nelaric::DemoInputTags

namespace Nelaric::DemoInput
{
constexpr float LookYawRate = 300.0f;
constexpr float LookPitchRate = 165.0f;
} // namespace Nelaric::DemoInput

UDemoPlayerInputComponent::UDemoPlayerInputComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	SelectionObjectType = UEngineTypes::ConvertToObjectType(ECC_Pawn);
}

void UDemoPlayerInputComponent::BindInputActions()
{
	Super::BindInputActions();
	using namespace Nelaric::DemoInputTags;
	UNelaricInputComponent* Component = GetBoundInputComponent();
	APlayerController* Controller = GetPlayerController();
	const bool bOverview = GetPawn<ADemoOverviewPawn>() != nullptr;
	const FGameplayTag MappingTag = bOverview ? InputMapping_Overview : InputMapping_Character;
	const FGameplayTag OtherMappingTag = bOverview ? InputMapping_Character : InputMapping_Overview;
	if (!Component || !Controller || !Controller->IsLocalPlayerController() || !InputConfig ||
	    (!bOverview && !GetPawn<ADemoCharacter>()))
	{
		UE_LOG(LogDemoInput, Error,
		       TEXT("Cannot bind Demo input: pawn=%s component=%s controller=%s config=%s; check local controller and "
		            "pawn type."),
		       *GetNameSafe(GetPawn()), *GetNameSafe(Component), *GetNameSafe(Controller), *GetNameSafe(InputConfig));
		return;
	}
	if (!InputConfig->FindInputMappingForTag(MappingTag))
	{
		UE_LOG(LogDemoInput, Error, TEXT("Input config for %s has no mapping for %s."), *GetNameSafe(GetPawn()),
		       *MappingTag.ToString());
		return;
	}
	Component->RemoveInputMappingByTag(OtherMappingTag);
	if (!Component->AddInputMappingByTag(MappingTag))
	{
		UE_LOG(LogDemoInput, Error, TEXT("Cannot activate Demo input mapping for %s (tag=%s)."),
		       *GetNameSafe(GetPawn()), *MappingTag.ToString());
		return;
	}
	ActiveMappingTag = MappingTag;
	UE_LOG(LogDemoInput, Verbose, TEXT("Active input mapping for %s: %s."), *GetNameSafe(GetPawn()),
	       *ActiveMappingTag.ToString());

	Controller->bShowMouseCursor = bOverview;
	if (bOverview)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(InputMode);
		BindNativeAction(InputTag_Overview_Move, ETriggerEvent::Triggered, this, &ThisClass::InputOverviewMove);
		BindNativeAction(InputTag_Overview_Drag, ETriggerEvent::Triggered, this, &ThisClass::InputOverviewDrag);
		BindNativeAction(InputTag_Overview_Drag, ETriggerEvent::Completed, this, &ThisClass::InputOverviewDragStopped);
		BindNativeAction(InputTag_Overview_Drag, ETriggerEvent::Canceled, this, &ThisClass::InputOverviewDragStopped);
		BindNativeAction(InputTag_Overview_Click, ETriggerEvent::Started, this, &ThisClass::InputOverviewClick);
		return;
	}
	Controller->SetInputMode(FInputModeGameOnly());
	BindNativeAction(InputTag_Character_Move, ETriggerEvent::Triggered, this, &ThisClass::InputMove);
	BindNativeAction(InputTag_Character_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::InputLookMouse);
	// Keyboard and mouse configurations do not need a gamepad look action.
	if (InputConfig->FindNativeInputActionForTag(InputTag_Character_Look_Stick))
	{
		BindNativeAction(InputTag_Character_Look_Stick, ETriggerEvent::Triggered, this, &ThisClass::InputLookStick);
	}
	BindNativeAction(InputTag_Character_Jump, ETriggerEvent::Started, this, &ThisClass::InputJumpStarted);
	BindNativeAction(InputTag_Character_Jump, ETriggerEvent::Completed, this, &ThisClass::InputJumpStopped);
	BindNativeAction(InputTag_Character_Jump, ETriggerEvent::Canceled, this, &ThisClass::InputJumpStopped);
	BindNativeAction(InputTag_Character_ReturnOverview, ETriggerEvent::Started, this, &ThisClass::InputReturnOverview);
}

void UDemoPlayerInputComponent::UnbindInputActions()
{
	InputJumpStopped();
	InputOverviewDragStopped();
	ActiveMappingTag = FGameplayTag();
	Super::UnbindInputActions();
}

bool UDemoPlayerInputComponent::HasActiveMapping(const FGameplayTag& MappingTag) const
{
	const APawn* Pawn = GetPawn();
	const APlayerController* Controller = GetPlayerController();
	return ActiveMappingTag == MappingTag && IsValid(Pawn) && !Pawn->IsActorBeingDestroyed() && Controller &&
	       Controller->IsLocalPlayerController() && Controller->GetPawn() == Pawn && GetBoundInputComponent() &&
	       GetBoundInputComponent() == Pawn->InputComponent;
}

ADemoCharacter* UDemoPlayerInputComponent::GetInputCharacter() const
{
	return HasActiveMapping(Nelaric::DemoInputTags::InputMapping_Character) ? GetPawn<ADemoCharacter>() : nullptr;
}

ADemoOverviewPawn* UDemoPlayerInputComponent::GetInputOverview() const
{
	const ADemoPlayerController* Controller = GetPlayerController<ADemoPlayerController>();
	return HasActiveMapping(Nelaric::DemoInputTags::InputMapping_Overview) && Controller &&
	               Controller->GetDemoControlMode() == EDemoControlMode::Overview
	           ? GetPawn<ADemoOverviewPawn>()
	           : nullptr;
}

void UDemoPlayerInputComponent::InputMove(const FInputActionValue& Value)
{
	ADemoCharacter* Character = GetInputCharacter();
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
	if (ADemoCharacter* Character = GetInputCharacter())
	{
		const FVector2D Look = Value.Get<FVector2D>();
		Character->AddControllerYawInput(Look.X);
		Character->AddControllerPitchInput(Look.Y);
	}
}

void UDemoPlayerInputComponent::InputLookStick(const FInputActionValue& Value)
{
	ADemoCharacter* Character = GetInputCharacter();
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
	if (ADemoCharacter* Character = GetInputCharacter())
	{
		if (auto* ASC = Cast<UNelaricAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
		{
			ASC->SubmitAction(FGameplayTag::RequestGameplayTag(TEXT("Action.Jump")), true);
		}
	}
}

void UDemoPlayerInputComponent::InputJumpStopped()
{
	if (ADemoCharacter* Character = GetPawn<ADemoCharacter>())
	{
		if (auto* ASC = Cast<UNelaricAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
		{
			ASC->SubmitAction(FGameplayTag::RequestGameplayTag(TEXT("Action.Jump")), false);
		}
	}
}

void UDemoPlayerInputComponent::InputOverviewMove(const FInputActionValue& Value)
{
	ADemoOverviewPawn* Overview = GetInputOverview();
	const UWorld* World = GetWorld();
	if (!Overview || !World || Value.GetValueType() != EInputActionValueType::Axis2D)
	{
		return;
	}
	const FVector2D Movement = Value.Get<FVector2D>();
	if (FMath::IsFinite(Movement.X) && FMath::IsFinite(Movement.Y))
	{
		Overview->PanOverview(Movement.GetClampedToMaxSize(1.0) * FMath::Max(0.0f, OverviewMoveSpeed) *
		                      World->GetDeltaSeconds());
	}
}

void UDemoPlayerInputComponent::InputOverviewDrag(const FInputActionValue& Value)
{
	ADemoOverviewPawn* Overview = GetInputOverview();
	APlayerController* Controller = GetPlayerController();
	if (!Overview || !Controller || Value.GetValueType() != EInputActionValueType::Boolean || !Value.Get<bool>())
	{
		InputOverviewDragStopped();
		return;
	}
	if (!bOverviewDragging)
	{
		// Discard mouse displacement accumulated before this drag began.
		bOverviewDragging = true;
		return;
	}
	FVector2D MouseDelta;
	Controller->GetInputMouseDelta(MouseDelta.X, MouseDelta.Y);
	if (FMath::IsFinite(MouseDelta.X) && FMath::IsFinite(MouseDelta.Y))
	{
		Overview->PanOverview(-MouseDelta * FMath::Max(0.0f, OverviewDragSensitivity));
	}
}

void UDemoPlayerInputComponent::InputOverviewDragStopped()
{
	bOverviewDragging = false;
}

void UDemoPlayerInputComponent::InputOverviewClick(const FInputActionValue& Value)
{
	ADemoPlayerController* Controller = GetPlayerController<ADemoPlayerController>();
	if (!GetInputOverview() || !Controller || bOverviewDragging ||
	    Value.GetValueType() != EInputActionValueType::Boolean || !Value.Get<bool>())
	{
		return;
	}
	FHitResult Hit;
	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes{SelectionObjectType};
	const bool bHit = Controller->GetHitResultUnderCursorForObjects(ObjectTypes, false, Hit);
	UE_LOG(LogDemoInput, Verbose, TEXT("Overview cursor selection hit: %s."), *GetNameSafe(Hit.GetActor()));
	if (bHit)
	{
		if (APawn* Target = Cast<APawn>(Hit.GetActor()))
		{
			Controller->TakeControlOfBot(Target);
		}
	}
}

void UDemoPlayerInputComponent::InputReturnOverview()
{
	if (GetInputCharacter())
	{
		if (ADemoPlayerController* Controller = GetPlayerController<ADemoPlayerController>())
		{
			Controller->ReturnToOverview();
		}
	}
}
