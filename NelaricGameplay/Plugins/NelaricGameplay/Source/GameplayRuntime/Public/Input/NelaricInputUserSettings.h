// Copyright (c) 2026 Nelaric
/** @file NelaricInputUserSettings.h
 * Defines per-local-player native input preferences.
 */
#pragma once
#include "UserSettings/EnhancedInputUserSettings.h"
#include "NelaricInputUserSettings.generated.h"

/** @brief Persistent native input preferences and engine key remapping.
 * @details Enhanced Input creates and owns one settings object per local
 * player. Change preferences on the game thread; inherited AsyncSaveSettings
 * persists SaveGame fields. Modifiers read current values each input frame.
 */
UCLASS(MinimalAPI, BlueprintType)
class UNelaricInputUserSettings : public UEnhancedInputUserSettings
{
	GENERATED_BODY()
public:
	/// Mouse sensitivity per axis; values are clamped to [0, 10] at use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input",
	          meta = (ClampMin = "0", ClampMax = "10"))
	FVector MouseSensitivity = FVector::OneVector;
	/// Normal gamepad look multiplier, clamped to [0, 10] at use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input",
	          meta = (ClampMin = "0", ClampMax = "10"))
	float GamepadLookSensitivity = 1.0f;
	/// Targeting look multiplier selected by the modifier, clamped at use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input",
	          meta = (ClampMin = "0", ClampMax = "10"))
	float GamepadTargetingSensitivity = 1.0f;
	/// Movement stick lower dead zone, clamped to [0, 1] at use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input",
	          meta = (ClampMin = "0", ClampMax = "1"))
	float GamepadMoveDeadZone = 0.25f;
	/// Look stick lower dead zone, clamped to [0, 1] at use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input",
	          meta = (ClampMin = "0", ClampMax = "1"))
	float GamepadLookDeadZone = 0.25f;
	/// Inverts the X component of look actions using the inversion modifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input")
	bool bInvertHorizontalAxis = false;
	/// Inverts the Y component of look actions using the inversion modifier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Nelaric|Input")
	bool bInvertVerticalAxis = false;
};
