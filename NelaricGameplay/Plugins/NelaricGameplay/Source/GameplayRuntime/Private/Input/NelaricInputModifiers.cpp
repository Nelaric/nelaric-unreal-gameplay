// Copyright (c) 2026 Nelaric
// Copyright Epic Games, Inc. All Rights Reserved.
#include "Input/NelaricInputModifiers.h"
#include "Input/NelaricInputUserSettings.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
namespace Nelaric::Input
{
static const UNelaricInputUserSettings* GetSettings(const UEnhancedPlayerInput* PlayerInput)
{
	const APlayerController* Controller = PlayerInput ? Cast<APlayerController>(PlayerInput->GetOuter()) : nullptr;
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Subsystem =
	    LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return Subsystem ? Cast<UNelaricInputUserSettings>(Subsystem->GetUserSettings()) : nullptr;
}
static float ClampSetting(float Value, float Maximum)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, Maximum) : 0.0f;
}
static FInputActionValue WithVector(FInputActionValue Original, const FVector& Value)
{
	return FInputActionValue(Original.GetValueType(), Value);
}
} // namespace Nelaric::Input
FInputActionValue
UNelaricInputModifierMouseSensitivity::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
                                                                FInputActionValue CurrentValue, float DeltaTime)
{
	const UNelaricInputUserSettings* Settings = Nelaric::Input::GetSettings(PlayerInput);
	if (!Settings || CurrentValue.GetValueType() == EInputActionValueType::Boolean)
	{
		return CurrentValue;
	}
	const FVector Scalar(Nelaric::Input::ClampSetting(Settings->MouseSensitivity.X, 10.0f),
	                     Nelaric::Input::ClampSetting(Settings->MouseSensitivity.Y, 10.0f),
	                     Nelaric::Input::ClampSetting(Settings->MouseSensitivity.Z, 10.0f));
	return Nelaric::Input::WithVector(CurrentValue, CurrentValue.Get<FVector>() * Scalar);
}
FInputActionValue
UNelaricInputModifierGamepadSensitivity::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
                                                                  FInputActionValue CurrentValue, float DeltaTime)
{
	const UNelaricInputUserSettings* Settings = Nelaric::Input::GetSettings(PlayerInput);
	if (!Settings || CurrentValue.GetValueType() == EInputActionValueType::Boolean)
	{
		return CurrentValue;
	}
	const float Scalar =
	    bUseTargetingSensitivity ? Settings->GamepadTargetingSensitivity : Settings->GamepadLookSensitivity;
	return Nelaric::Input::WithVector(CurrentValue,
	                                  CurrentValue.Get<FVector>() * Nelaric::Input::ClampSetting(Scalar, 10.0f));
}
FInputActionValue UNelaricInputModifierDeadZone::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
                                                                          FInputActionValue CurrentValue,
                                                                          float DeltaTime)
{
	const UNelaricInputUserSettings* Settings = Nelaric::Input::GetSettings(PlayerInput);
	const EInputActionValueType ValueType = CurrentValue.GetValueType();
	if (!Settings || ValueType == EInputActionValueType::Boolean)
	{
		return CurrentValue;
	}
	const float Lower = Nelaric::Input::ClampSetting(
	    bUseLookStick ? Settings->GamepadLookDeadZone : Settings->GamepadMoveDeadZone, 1.0f);
	const float Upper = Nelaric::Input::ClampSetting(UpperThreshold, 1.0f);
	if (Upper - Lower <= UE_SMALL_NUMBER)
	{
		return Nelaric::Input::WithVector(CurrentValue, FVector::ZeroVector);
	}
	const auto Remap = [Lower, Upper](double Axis)
	{ return FMath::Clamp((FMath::Abs(Axis) - Lower) / (Upper - Lower), 0.0, 1.0) * FMath::Sign(Axis); };
	FVector Value = CurrentValue.Get<FVector>();
	if (Type == EDeadZoneType::Axial)
	{
		Value = FVector(Remap(Value.X), Remap(Value.Y), Remap(Value.Z));
	}
	else if (ValueType == EInputActionValueType::Axis3D)
	{
		Value = Value.GetSafeNormal() * Remap(Value.Size());
	}
	else if (ValueType == EInputActionValueType::Axis2D)
	{
		Value = Value.GetSafeNormal2D() * Remap(Value.Size2D());
	}
	else
	{
		Value.X = Remap(Value.X);
	}
	return Nelaric::Input::WithVector(CurrentValue, Value);
}
FInputActionValue UNelaricInputModifierAimInversion::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
                                                                              FInputActionValue CurrentValue,
                                                                              float DeltaTime)
{
	const UNelaricInputUserSettings* Settings = Nelaric::Input::GetSettings(PlayerInput);
	if (!Settings || CurrentValue.GetValueType() == EInputActionValueType::Boolean)
	{
		return CurrentValue;
	}
	FVector Value = CurrentValue.Get<FVector>();
	if (Settings->bInvertHorizontalAxis)
	{
		Value.X *= -1.0;
	}
	if (Settings->bInvertVerticalAxis)
	{
		Value.Y *= -1.0;
	}
	return Nelaric::Input::WithVector(CurrentValue, Value);
}
