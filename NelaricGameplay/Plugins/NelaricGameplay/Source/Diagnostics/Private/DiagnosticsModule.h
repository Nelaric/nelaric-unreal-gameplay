// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Modules/ModuleManager.h"

#if !UE_SERVER
#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#endif

class IConsoleVariable;
class UGameViewportClient;
class SWidget;
#if WITH_EDITOR
class IAssetViewport;
#endif

namespace Nelaric
{
class FDiagnosticsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	IConsoleVariable* PerfHUDVariable = nullptr;

#if !UE_SERVER
	bool UpdateHUD(float DeltaTime);
	void RemoveGameHUDs();

	FTSTicker::FDelegateHandle HUDTickerHandle;
	TMap<TWeakObjectPtr<UGameViewportClient>, TSharedPtr<SWidget>> GameHUDs;

#if WITH_EDITOR
	void UpdateEditorHUD(bool bVisible);
	void RemoveEditorHUD();

	TWeakPtr<IAssetViewport> EditorViewport;
	TSharedPtr<SWidget> EditorHUD;
#endif
#endif
};
} // namespace Nelaric
