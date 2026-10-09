// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "JsEnv.h"
#include "Subsystems/EngineSubsystem.h"

#include "DemoScriptSubsystem.generated.h"

class ADemoGrandWarfrontGameMode;
DECLARE_DYNAMIC_DELEGATE_OneParam(FDemoStartBattlefront, ADemoGrandWarfrontGameMode*, Mode);

// Mixin changes UClasses shared by PIE worlds, so its runtime is process-wide.
UCLASS(MinimalAPI)
class UDemoScriptSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	FDemoStartBattlefront StartBattlefrontHandler;
	void AcquireRuntime();
	void ReleaseRuntime();
	virtual void Deinitialize() override;

private:
	TUniquePtr<PUERTS_NAMESPACE::FJsEnv> Environment;
	int32 ActiveInstances = 0;
};
