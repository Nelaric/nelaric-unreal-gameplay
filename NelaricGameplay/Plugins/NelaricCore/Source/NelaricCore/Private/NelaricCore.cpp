// Copyright (c) 2026 Nelaric

#include "Internal/InternalAccess.h"
#include "Modules/ModuleManager.h"

const UE::Nelaric::FInternalAccessKey& UE::Nelaric::FInternalAccess::Key()
{
	static const FInternalAccessKey Instance;
	return Instance;
}

IMPLEMENT_MODULE(FDefaultModuleImpl, NelaricCore)
