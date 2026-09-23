<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](Modules.zh-CN.md)

# Modules and Dependency Boundaries

The plugin is divided into modules with `Public` and `Private` directories. Public headers are contracts; private implementation details must not leak into them.

- Core modules must not depend on a vendor SDK or a concrete hosting, authentication, storage, or backend Provider. A Provider module may depend on Core. Circular module dependencies are forbidden.
- Public dependencies in `.Build.cs` must reflect types genuinely required by public headers. Put implementation dependencies in private dependencies. Do not make a dependency public merely to fix a missing include in another module.
- Prefer a pure C++ Provider interface at the boundary. Add a separate Blueprint adapter where Blueprint use is needed; do not force reflection onto every Provider implementation.
- Keep Core gameplay-agnostic. Inventory, combat, quests, character rules, team rules, and matchmaking algorithms belong to the consuming game or optional extensions. Core may carry opaque match/session metadata without interpreting gameplay policy.
- Use UE networking for frequent gameplay state replication. Framework services handle lower-frequency control-plane work such as admission, sessions, allocation, storage, configuration, health, and administration.

Every module should state its responsibility and direct dependencies in its module documentation when it is introduced. A new dependency crossing these boundaries requires a PR explanation and maintainer review.
