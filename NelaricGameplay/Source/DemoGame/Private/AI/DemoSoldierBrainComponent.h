// Copyright (c) 2026 Nelaric Contributors

#pragma once
#include "BrainComponent.h"
#include "DemoSoldierBrainComponent.generated.h"

class UDemoSoldierComponent;

/// Uses existing brain start/stop and control-transfer integration.
UCLASS(MinimalAPI)
class UDemoSoldierBrainComponent : public UBrainComponent
{
	GENERATED_BODY()
public:
public:
	UDemoSoldierBrainComponent();
	virtual void StartLogic() override;
	virtual void RestartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	virtual void Cleanup() override;
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
	virtual bool IsRunning() const override;
	virtual bool IsPaused() const override;
	virtual FString GetDebugInfoString() const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<UDemoSoldierComponent> Soldier;
	bool bPaused = false;
};
