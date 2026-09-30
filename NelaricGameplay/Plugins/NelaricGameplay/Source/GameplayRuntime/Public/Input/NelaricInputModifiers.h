// Copyright (c) 2026 Nelaric
/** @file NelaricInputModifiers.h
 * Declares settings-driven input modifiers.
 */
#pragma once
#include "InputModifiers.h"
#include "NelaricInputModifiers.generated.h"

/** @brief Scales mouse axes using the owning local player's preferences.
 * @details The action or mapping owns this modifier. Runs on the game
 * thread. Boolean values and missing settings pass through unchanged.
 */
UCLASS(MinimalAPI, NotBlueprintable, meta = (DisplayName = "Nelaric Mouse Sensitivity"))
class UNelaricInputModifierMouseSensitivity : public UInputModifier
{
	GENERATED_BODY()
public:
public:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
	                                                   FInputActionValue CurrentValue, float DeltaTime) override;
};

/** @brief Applies a gamepad sensitivity multiplier from local preferences.
 * @details The action or mapping owns this modifier. Runs on the game
 * thread. Boolean values and missing settings pass through unchanged.
 */
UCLASS(MinimalAPI, NotBlueprintable, meta = (DisplayName = "Nelaric Gamepad Sensitivity"))
class UNelaricInputModifierGamepadSensitivity : public UInputModifier
{
	GENERATED_BODY()
public:
	/// Selects targeting sensitivity; otherwise uses normal look sensitivity.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Settings")
	bool bUseTargetingSensitivity = false;

public:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
	                                                   FInputActionValue CurrentValue, float DeltaTime) override;
};

/** @brief Removes a settings-driven movement or look stick dead zone.
 * @details The action or mapping owns this modifier. Runs on the game
 * thread. Boolean values and missing settings pass through unchanged.
 * An upper threshold at or below the lower threshold produces zero.
 */
UCLASS(MinimalAPI, NotBlueprintable, meta = (DisplayName = "Nelaric Settings Dead Zone"))
class UNelaricInputModifierDeadZone : public UInputModifier
{
	GENERATED_BODY()
public:
	/// Radial uses vector magnitude; axial remaps each component separately.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Settings")
	EDeadZoneType Type = EDeadZoneType::Radial;
	/// Selects the look stick preference; otherwise uses movement dead zone.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Settings")
	bool bUseLookStick = false;
	/// Magnitude reaching full output, clamped to [0, 1] during processing.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float UpperThreshold = 1.0f;

public:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
	                                                   FInputActionValue CurrentValue, float DeltaTime) override;
};

/** @brief Inverts look axes according to local player preferences.
 * @details The action or mapping owns this modifier. Runs on the game
 * thread. Boolean values and missing settings pass through unchanged.
 */
UCLASS(MinimalAPI, NotBlueprintable, meta = (DisplayName = "Nelaric Aim Inversion"))
class UNelaricInputModifierAimInversion : public UInputModifier
{
	GENERATED_BODY()
public:
public:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
	                                                   FInputActionValue CurrentValue, float DeltaTime) override;
};
