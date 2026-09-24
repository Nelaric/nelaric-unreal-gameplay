<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](Runtime.zh-CN.md)

# Runtime, Performance, and Security

## Concurrency and reliability

- A service must state its lifetime and shutdown behavior. Avoid callbacks into a module or world after that owner has begun teardown.
- Public asynchronous callbacks default to Game Thread delivery. Work dispatched to other threads must explicitly marshal results back before touching UE game-thread-only objects.
- Every external request must have an explicit timeout and bounded retries and queue growth. Retry only where the operation is safe to repeat, with cancellation propagated to downstream work where supported. Avoid indefinite waits and unbounded background work.
- Treat remote payloads, player-supplied metadata, and configuration as untrusted input. Validate at the boundary and return ordinary errors for invalid data.

## Hot paths

- Tick, network callbacks, and other high-frequency paths must not perform blocking file, database, or network I/O.
- Avoid unbounded allocation, unbounded loops, and per-frame verbose logging in those paths. Move blocking or lower-frequency work to an appropriate asynchronous path and keep the UE replication path intact.
- A justified hot-path exception must explain its cost and bounds in the PR.

## Logs, configuration, and secrets

- Never log access tokens, server credentials, secrets, or player personal information. Log only the structured diagnostic fields needed to investigate an issue.
- Read and manage configuration and secrets through a central service. Give each optional integration only the values it requires and avoid copying secrets into general runtime metadata or error messages.
- Administrative and security-sensitive operations must check authorization and leave an appropriate audit trail without recording secret material.
