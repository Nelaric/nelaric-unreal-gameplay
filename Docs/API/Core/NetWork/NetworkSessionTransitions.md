<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NetworkSessionTransitions.zh-CN.md)

# Network Sessions and Authority Transitions

## Purpose and boundaries

`UNelaricSessionTransitionSubsystem` coordinates a client's move from one remote server to another. Unreal creates one instance per GameInstance. Requests, cancellation, and callbacks run on the game thread.

`GetNetMode()` returns the current world's observed network mode, or an empty value when no world exists. During travel, that value may describe the old or an intermediate world. Completion is determined by the destination world and connection checks.

## Transition path

| Source | Destination | Execution |
| --- | --- | --- |
| Client | Client | Request departure approval from the current server and arrival approval from the target server, then call `ClientTravel` after both approve. |

`RequestTransition()` accepts `NM_Client` as both the source and target mode. The caller supplies `Nelaric::FTransitionDestination` containing two `FNetworkEndpoint` values. `GameEndpoint` selects the travel connection; `BeaconEndpoint` selects the target approval channel. Each endpoint requires a nonempty address and a port from 1 to 65535. Both endpoints must form valid URLs with a host.

The request returns a nonzero `FTransitionHandle` when accepted. It returns a zero handle without calling a terminal callback if another request is active, no world is available, the source or target mode is unsuitable, the endpoints are invalid, or the approval transport is not bound. Only one request can be active per subsystem.

## Authority confirmation before execution

The internal transport obtains the first local `ANelaricPlayerController` and sends the departure request through its server RPC. The server approves a nonzero request ID with a nonempty target address. The owning client receives the result; the transport checks the returned target address against the requested address before reporting approval.

The transport also creates an `ANelaricTransitionBeaconClient` to contact the supplied beacon endpoint. On a listen server or dedicated server, `ANelaricGameModeBase::StartPlay()` starts an Online Beacon host when its listen port is valid. `TransitionBeaconListenPort` defaults to 15000. The destination's supplied beacon port must match that server's configuration.

The coordinator accepts replies only for its active request ID before travel starts. A refusal ends the request with `AuthorityRejected`; inability to start approval or reach the target authority reports `AuthorityUnavailable`. After both replies approve, the next coordinator tick checks the deadline and obtains the local player controller before starting absolute `ClientTravel` to the game endpoint.

The target GameMode's `CanAcceptTransition()` returns `true`. `TODO(NELARIC-TRANSITION-CAPACITY-INTEGRATION)` in that method marks retrieval of the active `WorldStartupConfig` and enforcement of `MaxPlayers`, including pending joins.

## Completion and cancellation

An accepted request has a 30-second deadline covering approval and travel. Success requires a world different from the source world, `NM_Client`, and an open server connection whose host matches the game endpoint without case sensitivity and whose port matches exactly. A world or connection that does not meet those checks keeps the request pending until its deadline.

`FTransitionCallbacks` provides optional `OnSucceeded`, `OnCancelled`, `OnTimedOut`, and `OnFailed` delegates. Each accepted request has one terminal outcome, and only its matching bound callback runs. The coordinator clears the active request and releases transport state before invoking the callback. Transport cleanup removes the departure listener and destroys the target beacon.

`CancelTransition()` succeeds only for the active handle before travel starts. It then reports cancellation. After travel starts, the method returns `false`. Clearing the approval transport fails an active request with `AuthorityUnavailable`; coordinator deinitialization cancels an active request.

## World startup configuration

`UNelaricWorldStartupConfig` stores a map soft reference, world player policy, and activity participant defaults. `HasValidPlayerLimits()` and `HasValidActivityParticipantLimits()` check nonnegative counts and that a positive maximum is at least the minimum. `HasValidStartupConfig()` also checks the map reference and policy enum values. The asset's validation functions check authored values; they do not load the map or perform travel.
