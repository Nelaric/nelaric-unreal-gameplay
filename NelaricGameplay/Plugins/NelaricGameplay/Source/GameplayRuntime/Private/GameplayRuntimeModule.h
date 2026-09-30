// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Modules/ModuleManager.h"

class FGameplayRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
