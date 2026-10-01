// Copyright (c) 2026 Nelaric Contributors

/** @file DemoGameInstance.h Declares the demo game-mode selection. */
#pragma once
#include "Engine/GameInstance.h"
#include "DemoGameInstance.generated.h"

/** @brief Selects GAS mode for the demo's legacy map override.
 * @details Explicit travel mode choices retain engine selection. This
 * bridge keeps existing map and character assets usable from source.
 */
UCLASS(MinimalAPI)
class UDemoGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
public:
	DEMOGAME_API virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
	                                                                      const FString& MapName,
	                                                                      const FString& Options,
	                                                                      const FString& Portal) const override;
};
