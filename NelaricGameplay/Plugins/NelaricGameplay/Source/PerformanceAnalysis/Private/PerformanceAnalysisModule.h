// Copyright (c) 2026 Nelaric

#pragma once

#include "Modules/ModuleManager.h"

namespace Nelaric
{
class FPerformanceAnalysisModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
} // namespace Nelaric
