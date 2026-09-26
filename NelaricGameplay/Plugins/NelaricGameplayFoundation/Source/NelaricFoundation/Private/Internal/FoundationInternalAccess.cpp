// Copyright (c) 2026 Nelaric

#include "Internal/FoundationInternalAccess.h"

const Nelaric::FFoundationInternalAccessKey& Nelaric::FFoundationInternalAccess::Key()
{
	static const FFoundationInternalAccessKey Instance;
	return Instance;
}
