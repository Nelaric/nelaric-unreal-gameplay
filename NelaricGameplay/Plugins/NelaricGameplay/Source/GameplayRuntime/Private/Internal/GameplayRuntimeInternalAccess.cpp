// Copyright (c) 2026 Nelaric Contributors

#include "Internal/GameplayRuntimeInternalAccess.h"

const Nelaric::FGameplayRuntimeInternalAccessKey& Nelaric::FGameplayRuntimeInternalAccess::Key()
{
	static const FGameplayRuntimeInternalAccessKey Instance;
	return Instance;
}
