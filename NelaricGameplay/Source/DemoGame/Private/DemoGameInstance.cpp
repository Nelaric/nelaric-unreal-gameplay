// Copyright (c) 2026 Nelaric Contributors

#include "DemoGameInstance.h"
#include "DemoGameMode.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Scripting/DemoScriptSubsystem.h"

void UDemoGameInstance::Init()
{
	Super::Init();
	if (!IsDedicatedServerInstance() && !bScriptRuntimeActive && GEngine)
	{
		if (UDemoScriptSubsystem* Scripts = GEngine->GetEngineSubsystem<UDemoScriptSubsystem>())
		{
			bScriptRuntimeActive = true;
			Scripts->AcquireRuntime();
		}
	}
}

void UDemoGameInstance::Shutdown()
{
	if (bScriptRuntimeActive && GEngine)
	{
		bScriptRuntimeActive = false;
		if (UDemoScriptSubsystem* Scripts = GEngine->GetEngineSubsystem<UDemoScriptSubsystem>())
		{
			Scripts->ReleaseRuntime();
		}
	}
	Super::Shutdown();
}

TSubclassOf<AGameModeBase> UDemoGameInstance::OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
                                                                    const FString& MapName, const FString& Options,
                                                                    const FString& Portal) const
{
	if (!UGameplayStatics::HasOption(Options, TEXT("game")) && GameModeClass &&
	    GameModeClass->GetPathName() == TEXT("/Game/Demo/Game/BP_DemoGameMode.BP_DemoGameMode_C"))
	{
		return ADemoGameMode::StaticClass();
	}
	return Super::OverrideGameModeClass(GameModeClass, MapName, Options, Portal);
}
