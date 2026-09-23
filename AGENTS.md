# Project Context for Agents

## What this repository is

Nelaric Unreal Server is an Unreal Engine 5.6+ Dedicated Server framework. It aims to provide reusable server infrastructure without prescribing a game's rules or binding the project to one hosting or backend provider. The intended deliverable is a multi-module, UE-native plugin with C++-first public APIs and optional Blueprint adapters.

Core covers lower-frequency control-plane concerns such as server lifecycle, player admission, sessions, persistence primitives, backend communication, configuration, observability, and hosting integration. Gameplay systems such as inventory, combat, character rules, and matchmaking algorithms belong to games or optional extensions. Keep high-frequency gameplay networking on Unreal's NetDriver, replication, Iris, and RPC paths.

## Current state and sources of truth

The repository is in its initial documentation and tooling stage. It does not yet contain implemented UE plugin modules or a UE build target. Do not treat the capability descriptions as existing APIs.

- `README.md` gives the public project overview.
- `Docs/CodingStandards/README.md` is the index for binding project coding rules; read the relevant linked topic before changing code or build scripts.
- [Epic's Unreal Engine C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) applies by default. Explicit project rules take precedence where they conflict.
- `Docs/Api/README.md` is the API documentation entry page. `Scripts/` and `.github/workflows/` contain the current checks and Pages publishing workflow.

## Working in this repository

- Keep Core free of vendor SDK and gameplay dependencies. Provider modules may depend on Core; module dependencies must not form cycles. Use `Public` and `Private` directories as described in the module standard.
- Follow the API standard for ownership, thread affinity, cancellation, errors, and Doxygen comments. Public API documentation is written in English.
- Preserve the repository's text conventions: code and human-written Markdown use UTF-8 BOM and CRLF; tool files have the documented exceptions.
- Run `python Scripts/check_text.py` for text changes. Run `python Scripts/check_format.py` for C++ or Unreal build-script changes, and `python Scripts/check_public_docs.py` plus `python Scripts/run_doxygen.py` when public headers change. The required tool versions are pinned in the repository. State when a relevant check could not run.
- The current CI checks formatting and API documentation. It does not compile against Unreal Engine or impose a separate test-coverage requirement; add focused behavioral tests when a change needs them.

The `main` branch is protected. Check the current repository rules before publishing changes, and use a pull request with the required checks and review. Explain any proposed exception to a coding rule in that PR.
