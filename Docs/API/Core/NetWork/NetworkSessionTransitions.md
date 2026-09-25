<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NetworkSessionTransitions.zh-CN.md)

# Network Sessions and Authority Transitions

## Purpose and boundaries

Unreal Engine's network mode answers, “What role does this world currently play?”

- **Standalone:** Local gameplay authority without remote players.
- **Listen server:** Local gameplay authority that accepts remote players.
- **Client:** Connected to remote authority; it does not decide gameplay outcomes.
- **Dedicated server:** Authority provided by a server process without a local player.

A network mode describes an established world. It cannot say whether a transition is underway or has succeeded. The framework separately tracks the intended role, old and destination authorities, target world, and transition progress. It confirms the result only when the target world and required connection are ready. For example, the front-end world may remain standalone while joining a remote server, but it is not a ready gameplay server.

A transition does not necessarily change maps. World travel, network-role changes, progress synchronization, and online feature availability can occur separately. Joining a remote server usually requires travel; signing in, synchronizing progress, enabling online features, or admitting remote players to an existing local world need not change the map.

## Transition paths

| Path | Meaning and required boundary |
| --- | --- |
| Standalone → Listen server | Open an existing local authority world to remote players when its content and runtime state can support admission. Confirm that listening is established before admitting players. A new map may instead be loaded when the selected startup configuration requires it. |
| Listen server → Standalone | Close admission, handle connected players and session visibility, then stop listening. Local authority may continue in the same world if the gameplay state supports it. Stopping the listener alone does not settle remote-player state. |
| Standalone → Client | Settle local progress, connect to remote authority, and follow the server's world. The local startup map does not dictate the remote map. |
| Client → Standalone | Leave remote authority and establish a local authority world. Returning to a local front end is distinct from continuing the same gameplay content, which requires an authorized snapshot and explicit restoration rules. |
| Client → Listen server | Leaving a remote session and starting an unrelated host composes the ordinary leave and host operations. Taking over the previous session is authority migration with state transfer and reconnection. |
| Listen server → Client | Closing one host and joining another composes the ordinary stop and join operations. Handing the existing session to another host requires authority migration. |
| Client → Client | Changing remote authority is a transition even though the network mode remains client. Track the old connection, destination identity, travel, new connection, and completion or failure. |

A server-led map change within the same authority and a client-initiated move to another server have different ownership of the travel decision. The transition must preserve that distinction. A dedicated server starts and loads its configured local world as a server process; it is not a runtime destination mode for a client or listen-server process.

## Starting and admitting a world

A startup owner selects and retains a world startup configuration before loading its map. The configuration defines the authored map and player policy, not the current network role or replicated runtime state. The owner validates the configuration and target capacity before travel. Once the destination world and its intended role are confirmed, the authority creates its runtime state and applies the startup policy. Admission stays closed until that preparation is complete; the configured initial admission policy then determines whether it opens.

An operation has one terminal outcome: ready, cancelled, timed out, or failed with a reason. World teardown, travel failure, connection loss, and superseded operations must not let an earlier completion mark a later world ready. The session coordinator therefore retains the intended role and transition progress independently of the observed network mode.

## Purpose of transitions and exceptions

The transition mechanism coordinates worlds, connections, and gameplay authority so that an old world or an unfinished connection cannot be mistaken for the destination session. A change of gameplay authority should normally travel to the destination authority's world and resume play only after its connection and runtime state are ready. This is the default path for joining a remote server, moving between servers, and leaving a remote session.

The framework also permits a project with an explicit need to keep the map content while completing a transition that does not depend on a map change. For example, offline progress can be submitted after local play for an online authority to accept and govern thereafter; an existing local world can also admit remote players once ready. Such paths should not be the default. The project defines state transfer, trust, and conflict rules.
