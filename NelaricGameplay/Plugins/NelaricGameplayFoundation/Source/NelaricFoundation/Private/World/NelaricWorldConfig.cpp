// Copyright (c) 2026 Nelaric

#include "World/NelaricWorldConfig.h"

bool UNelaricWorldConfig::HasValidPlayerLimits() const
{
	return MinPlayersToActivate >= 0 && MaxPlayers >= 0 && (MaxPlayers == 0 || MinPlayersToActivate <= MaxPlayers);
}

bool UNelaricWorldConfig::HasValidActivityParticipantLimits() const
{
	return MinParticipantsToStart >= 0 && MaxParticipants >= 0 &&
	       (MaxParticipants == 0 || MinParticipantsToStart <= MaxParticipants);
}
