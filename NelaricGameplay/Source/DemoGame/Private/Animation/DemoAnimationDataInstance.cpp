// Copyright (c) 2026 Nelaric Contributors

#include "Animation/DemoAnimationDataInstance.h"

void UDemoAnimationDataInstance::UpdateAnimationData(float DeltaSeconds)
{
	LowLevelFatalError(TEXT("A native animation subclass must implement UpdateAnimationData."));
}
