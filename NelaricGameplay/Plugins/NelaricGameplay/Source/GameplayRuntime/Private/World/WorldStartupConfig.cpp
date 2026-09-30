// Copyright (c) 2026 Nelaric Contributors

#include "World/WorldStartupConfig.h"

bool UWorldStartupConfig::HasValidPlayerLimits() const
{
	return MinPlayersToActivate >= 0 && MaxPlayers >= 0 && (MaxPlayers == 0 || MinPlayersToActivate <= MaxPlayers);
}

bool UWorldStartupConfig::HasValidActivityParticipantLimits() const
{
	return MinParticipantsToStart >= 0 && MaxParticipants >= 0 &&
	       (MaxParticipants == 0 || MinParticipantsToStart <= MaxParticipants);
}

bool UWorldStartupConfig::HasValidStartupConfig() const
{
	return !WorldMap.IsNull() && HasValidPlayerLimits() && HasValidActivityParticipantLimits() &&
	       (StartPolicy == EActivityStartPolicy::Manual || StartPolicy == EActivityStartPolicy::WhenMinimumReached) &&
	       (JoinInProgressPolicy == EActivityJoinInProgressPolicy::Reject ||
	        JoinInProgressPolicy == EActivityJoinInProgressPolicy::Participate);
}
