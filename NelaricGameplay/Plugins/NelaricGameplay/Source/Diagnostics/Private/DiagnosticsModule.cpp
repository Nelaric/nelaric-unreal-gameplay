// Copyright (c) 2026 Nelaric Contributors

#include "DiagnosticsModule.h"

#include "HAL/IConsoleManager.h"

#if !UE_SERVER
#include "PerformanceHUD.h"
#include "Containers/Set.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "IAssetViewport.h"
#include "LevelEditor.h"
#endif
#endif

namespace Nelaric
{
void FDiagnosticsModule::StartupModule()
{
	PerfHUDVariable = IConsoleManager::Get().RegisterConsoleVariable(
	    TEXT("ng.Perf.HUD"), 0,
	    TEXT("Controls the Slate performance HUD.\n") TEXT("0: hidden (default), non-zero: visible.\n")
	        TEXT("Displays FPS, Frame, GT, RT and GPU timings. Remains enabled after PIE ends."),
	    ECVF_Default);

#if !UE_SERVER
	if (FApp::CanEverRender())
	{
		HUDTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		    FTickerDelegate::CreateRaw(this, &FDiagnosticsModule::UpdateHUD), 0.1f);
	}
#endif
}

void FDiagnosticsModule::ShutdownModule()
{
#if !UE_SERVER
	FTSTicker::GetCoreTicker().RemoveTicker(HUDTickerHandle);
	HUDTickerHandle.Reset();
	RemoveGameHUDs();
#if WITH_EDITOR
	RemoveEditorHUD();
#endif
#endif

	if (PerfHUDVariable)
	{
		IConsoleManager::Get().UnregisterConsoleObject(PerfHUDVariable, false);
		PerfHUDVariable = nullptr;
	}
}

#if !UE_SERVER
bool FDiagnosticsModule::UpdateHUD(float DeltaTime)
{
	const bool bVisible = PerfHUDVariable && PerfHUDVariable->GetInt() != 0 && FSlateApplication::IsInitialized();
#if WITH_EDITOR
	UpdateEditorHUD(bVisible);
#endif

	TSet<UGameViewportClient*> WantedViewports;
	if (bVisible && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			UGameViewportClient* Viewport = World && World->IsGameWorld() ? World->GetGameViewport() : nullptr;
			if (!IsValid(Viewport) || !Viewport->GetGameViewportWidget().IsValid())
			{
				continue;
			}
#if WITH_EDITOR
			// The editor overlay already covers PIE running inside this level viewport.
			const TSharedPtr<IAssetViewport> ActiveEditorViewport = EditorViewport.Pin();
			if (ActiveEditorViewport && ActiveEditorViewport->GetActiveViewport() == Viewport->Viewport)
			{
				continue;
			}
#endif
			WantedViewports.Add(Viewport);
		}
	}

	for (auto It = GameHUDs.CreateIterator(); It; ++It)
	{
		UGameViewportClient* Viewport = It.Key().Get();
		if (!Viewport || !WantedViewports.Contains(Viewport))
		{
			if (Viewport)
			{
				Viewport->RemoveViewportWidgetContent(It.Value().ToSharedRef());
			}
			It.RemoveCurrent();
		}
	}

	for (UGameViewportClient* Viewport : WantedViewports)
	{
		const TWeakObjectPtr<UGameViewportClient> Key(Viewport);
		if (!GameHUDs.Contains(Key))
		{
			TSharedRef<SWidget> HUD = SNew(SPerformanceHUD).Visibility(EVisibility::HitTestInvisible);
			Viewport->AddViewportWidgetContent(HUD, 100);
			GameHUDs.Add(Key, HUD);
		}
	}
	return true;
}

void FDiagnosticsModule::RemoveGameHUDs()
{
	for (const auto& Entry : GameHUDs)
	{
		if (UGameViewportClient* Viewport = Entry.Key.Get())
		{
			Viewport->RemoveViewportWidgetContent(Entry.Value.ToSharedRef());
		}
	}
	GameHUDs.Empty();
}

#if WITH_EDITOR
void FDiagnosticsModule::UpdateEditorHUD(bool bVisible)
{
	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> WantedViewport =
	    bVisible && LevelEditor ? LevelEditor->GetFirstActiveViewport() : nullptr;
	if (!WantedViewport || EditorViewport.Pin() != WantedViewport)
	{
		RemoveEditorHUD();
	}
	if (WantedViewport && !EditorHUD)
	{
		EditorHUD = SNew(SPerformanceHUD).Visibility(EVisibility::HitTestInvisible);
		WantedViewport->AddOverlayWidget(EditorHUD.ToSharedRef(), 100);
		EditorViewport = WantedViewport;
	}
}

void FDiagnosticsModule::RemoveEditorHUD()
{
	if (const TSharedPtr<IAssetViewport> Viewport = EditorViewport.Pin(); Viewport && EditorHUD)
	{
		Viewport->RemoveOverlayWidget(EditorHUD.ToSharedRef());
	}
	EditorHUD.Reset();
	EditorViewport.Reset();
}
#endif
#endif
} // namespace Nelaric

IMPLEMENT_MODULE(Nelaric::FDiagnosticsModule, Diagnostics)
