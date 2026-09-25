// Copyright (c) 2026 Nelaric

/** @file NelaricWorldSettings.h
 * Declares the project World Settings base class.
 */

#pragma once

#include "GameFramework/WorldSettings.h"
#include "UObject/ObjectPtr.h"

#include "NelaricWorldSettings.generated.h"

class UNelaricWorldConfig;

/** @brief Base World Settings class for NelaricGameplay maps.
 *
 * @details Gameplay-specific World Settings classes must derive from
 * this class. The owning level retains its instance while loaded.
 * Access it on the game thread like other world actors.
 *
 * @warning If the project's World Settings Class is not this class or
 * a subclass, worlds using that class lack WorldConfig. Gameplay code
 * expecting ANelaricWorldSettings cannot obtain their configuration;
 * unchecked assumptions about the class can cause runtime failures.
 */
UCLASS(Blueprintable)
class NELARICGAMEPLAYCORE_API ANelaricWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	/** @brief Configuration selected by this level, or null if unset.
	 *
	 * @details The reference keeps the asset loaded with the level.
	 * Authority-side gameplay code reads and applies its policy on the
	 * game thread.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nelaric|World")
	TObjectPtr<UNelaricWorldConfig> WorldConfig;
};
