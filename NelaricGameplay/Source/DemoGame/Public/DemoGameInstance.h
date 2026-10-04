// Copyright (c) 2026 Nelaric Contributors

/** @file DemoGameInstance.h Declares demo mode and script startup. */
#pragma once
#include "Engine/GameInstance.h"
#include "DemoGameInstance.generated.h"

/** @brief Selects demo GAS mode and starts local presentation scripts.
 * @details Explicit travel options retain engine mode selection.
 * @par Lifetime
 * Rendering game instances share
 * scripts until the last instance exits.
 */
UCLASS(MinimalAPI)
class UDemoGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
public:
	DEMOGAME_API virtual void Init() override;
	DEMOGAME_API virtual void Shutdown() override;
	DEMOGAME_API virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
	                                                                      const FString& MapName,
	                                                                      const FString& Options,
	                                                                      const FString& Portal) const override;

private:
	bool bScriptRuntimeActive = false;
};
