// Copyright (c) 2026 Nelaric

/** @file NelaricNetworkEndpoint.h
 * Declares a reusable network address and port value.
 */

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

namespace UE::Nelaric
{
/** @brief Host address and transport port of one network endpoint.
 *
 * @details Address is a host name or IP address without a port. Callers
 * choose the protocol and interpret the endpoint for their transport.
 */
struct FNetworkEndpoint
{
	/// Host name or IP address without a port.
	FString Address;

	/// Transport port from 1 through 65535.
	int32 Port = 0;

	/** @brief Checks whether an address and usable port are present.
	 * @return True if both fields have usable values.
	 */
	bool IsValid() const
	{
		return !Address.IsEmpty() && Port >= 1 && Port <= 65535;
	}
};
} // namespace UE::Nelaric
