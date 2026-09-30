// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Modules/ModuleManager.h"

namespace Nelaric
{
class FEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
} // namespace Nelaric
