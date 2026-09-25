// Copyright (c) 2026 Nelaric

#pragma once

#include "Modules/ModuleManager.h"

class FNelaricFoundationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
