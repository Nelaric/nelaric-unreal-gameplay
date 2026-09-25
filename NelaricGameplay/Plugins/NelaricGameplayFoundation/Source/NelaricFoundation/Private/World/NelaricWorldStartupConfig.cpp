// Copyright (c) 2026 Nelaric

#include "World/NelaricWorldStartupConfig.h"

bool UNelaricWorldStartupConfig::HasValidPlayerLimits() const
{
	return MinPlayersToActivate >= 0 && MaxPlayers >= 0 && (MaxPlayers == 0 || MinPlayersToActivate <= MaxPlayers);
}

bool UNelaricWorldStartupConfig::HasValidActivityParticipantLimits() const
{
	return MinParticipantsToStart >= 0 && MaxParticipants >= 0 &&
	       (MaxParticipants == 0 || MinParticipantsToStart <= MaxParticipants);
}

bool UNelaricWorldStartupConfig::HasValidStartupConfig() const
{
	return !WorldMap.IsNull() && HasValidPlayerLimits() && HasValidActivityParticipantLimits() &&
	       (StartPolicy == ENelaricActivityStartPolicy::Manual ||
	        StartPolicy == ENelaricActivityStartPolicy::WhenMinimumReached) &&
	       (JoinInProgressPolicy == ENelaricActivityJoinInProgressPolicy::Reject ||
	        JoinInProgressPolicy == ENelaricActivityJoinInProgressPolicy::Participate);
}
