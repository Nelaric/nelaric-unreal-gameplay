// Copyright (c) 2026 Nelaric

#pragma once

#if !UE_SERVER

#include "Widgets/SCompoundWidget.h"

class STextBlock;

namespace Nelaric
{
class SPerformanceHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPerformanceHUD)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	TSharedRef<SWidget> MakeMetricRow(const TCHAR* Label, TSharedPtr<STextBlock>& Value);
	void RefreshMetrics();

	double NextRefreshTime = 0.0;
	TSharedPtr<STextBlock> FPSValue;
	TSharedPtr<STextBlock> FrameValue;
	TSharedPtr<STextBlock> GTValue;
	TSharedPtr<STextBlock> RTValue;
	TSharedPtr<STextBlock> GPUValue;
};
} // namespace Nelaric

#endif
