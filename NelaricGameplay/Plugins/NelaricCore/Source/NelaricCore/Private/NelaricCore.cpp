// Copyright (c) 2026 Nelaric

#include "Internal/InternalAccess.h"
#include "Modules/ModuleManager.h"

const Nelaric::FInternalAccessKey& Nelaric::FInternalAccess::Key()
{
	static const FInternalAccessKey Instance;
	return Instance;
}

IMPLEMENT_MODULE(FDefaultModuleImpl, NelaricCore)
