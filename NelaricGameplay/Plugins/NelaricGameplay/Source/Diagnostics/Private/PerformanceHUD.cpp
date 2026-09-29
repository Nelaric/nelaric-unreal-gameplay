// Copyright (c) 2026 Nelaric

#include "PerformanceHUD.h"

#if !UE_SERVER

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "HAL/PlatformTime.h"
#include "RenderTimer.h"
#include "RHIGlobals.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// UnrealEngine.cpp exports these smoothed, engine-wide frame measurements.
extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;

namespace Nelaric
{
void SPerformanceHUD::Construct(const FArguments& InArgs)
{
	const FString GPULabel =
	    GRHIAdapterName.IsEmpty() ? TEXT("GPU (Unknown):") : FString::Printf(TEXT("GPU (%s):"), *GRHIAdapterName);
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox) +
	                                SVerticalBox::Slot().AutoHeight()[MakeMetricRow(TEXT("FPS:"), FPSValue)] +
	                                SVerticalBox::Slot().AutoHeight()[MakeMetricRow(TEXT("Frame:"), FrameValue)] +
	                                SVerticalBox::Slot().AutoHeight()[MakeMetricRow(TEXT("GT:"), GTValue)] +
	                                SVerticalBox::Slot().AutoHeight()[MakeMetricRow(TEXT("RT:"), RTValue)] +
	                                SVerticalBox::Slot().AutoHeight()[MakeMetricRow(*GPULabel, GPUValue)];

	ChildSlot.HAlign(HAlign_Right)
	    .VAlign(VAlign_Top)
	    .Padding(FMargin(0.0f, 104.0f, 32.0f, 0.0f))[SNew(SBox).MinDesiredWidth(144.0f)[Rows]];
	RefreshMetrics();
}

TSharedRef<SWidget> SPerformanceHUD::MakeMetricRow(const TCHAR* Label, TSharedPtr<STextBlock>& Value)
{
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	TSharedRef<STextBlock> LabelText = SNew(STextBlock)
	                                       .Text(FText::FromString(Label))
	                                       .Font(Font)
	                                       .ColorAndOpacity(FLinearColor::White)
	                                       .ShadowOffset(FVector2D(1.0f, 1.0f))
	                                       .ShadowColorAndOpacity(FLinearColor::Black)
	                                       .Justification(ETextJustify::Right);

	return SNew(SBox).HeightOverride(
	    16.0f)[SNew(SHorizontalBox) +
	           SHorizontalBox::Slot().FillWidth(1.0f).VAlign(
	               VAlign_Center)[SNew(SBox).MinDesiredWidth(60.0f)[LabelText]] +
	           SHorizontalBox::Slot()
	               .AutoWidth()
	               .VAlign(VAlign_Center)
	               .Padding(8.0f, 0.0f, 0.0f,
	                        0.0f)[SNew(SBox).MinDesiredWidth(76.0f)[SAssignNew(Value, STextBlock)
	                                                                    .Font(Font)
	                                                                    .ColorAndOpacity(FLinearColor::White)
	                                                                    .ShadowOffset(FVector2D(1.0f, 1.0f))
	                                                                    .ShadowColorAndOpacity(FLinearColor::Black)]]];
}

void SPerformanceHUD::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (InCurrentTime >= NextRefreshTime)
	{
		RefreshMetrics();
		NextRefreshTime = InCurrentTime + 0.25;
	}
}

void SPerformanceHUD::RefreshMetrics()
{
	const auto TimingColor = [](double TimeMS)
	{
		return TimeMS > 0.0 && GEngine ? FLinearColor(GEngine->GetFrameTimeDisplayColor(static_cast<float>(TimeMS)))
		                               : FLinearColor::White;
	};
	const auto UpdateTiming = [&TimingColor](const TSharedPtr<STextBlock>& Value, double TimeMS)
	{
		Value->SetText(FText::FromString(TimeMS > 0.0 ? FString::Printf(TEXT("%.2f ms"), TimeMS) : TEXT("N/A")));
		Value->SetColorAndOpacity(TimingColor(TimeMS));
	};

	FPSValue->SetText(FText::FromString(GAverageFPS > 0.0f ? FString::Printf(TEXT("%.1f"), GAverageFPS) : TEXT("N/A")));
	FPSValue->SetColorAndOpacity(TimingColor(GAverageMS));
	UpdateTiming(FrameValue, GAverageMS);
	UpdateTiming(GTValue, FPlatformTime::ToMilliseconds(GGameThreadTime));
	UpdateTiming(RTValue, FPlatformTime::ToMilliseconds(GRenderThreadTime));
	UpdateTiming(GPUValue, FPlatformTime::ToMilliseconds(GDynamicRHI ? RHIGetGPUFrameCycles() : 0));
}
} // namespace Nelaric

#endif
