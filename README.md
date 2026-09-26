<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# Nelaric Unreal Gameplay

An Unreal Engine gameplay framework for persistent worlds and bounded activities across standalone play, listen servers, and dedicated servers, with versioned gameplay content delivery.

## Project layout

The [NelaricGameplay/](NelaricGameplay/) directory contains the framework's Unreal project. Open its [NelaricGameplay.uproject](NelaricGameplay/NelaricGameplay.uproject) with Unreal Engine 5.6 or later. The project's `Source/` contains the `NelaricGameplayCore` runtime module and Game, Editor, and Server targets.

Linux CI builds this project for Game, Editor, and Server, including the enabled Core, Foundation, and PuerTS plugins.

## PuerTS setup

The repository includes the PuerTS Unreal plugin source. After cloning, run **one** Setup script for your operating system from the repository root:

| System | Command |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh` |
| macOS x86_64 or arm64 | `sh ./Setup/macOS/Setup.sh` |

Install Node.js with npm first. Setup also needs network access, `curl`, and `tar`; Linux needs `sha256sum`, and macOS needs `shasum`. No separate npm or PuerTS setup command is needed. The V8 9.4 backend and its [license](NelaricGameplay/Plugins/Puerts/ThirdParty/v8_9.4.146.24/LICENSE) are included in the repository. Setup checks the bundled backend, prepares the TypeScript editor tooling, and confirms that PuerTS is enabled in the shared `.uproject`. If the backend directory is missing, Setup downloads it from the official [PuerTS Unreal 1.0.9 release](https://github.com/Tencent/puerts/releases/tag/Unreal_v1.0.9) and verifies its SHA-256 digest. You can rerun Setup after an interruption.

The largest Windows static library is stored in Git LFS; the other backend files are stored directly in Git. Install Git LFS before cloning on Windows so the full library is checked out. If the checkout contains an LFS pointer, Windows Setup retrieves the library. The download cache stays local. PuerTS is already enabled and its V8 backend is already present for a normal clone; run Setup once per clone when you need the editor TypeScript tooling. The Linux CI builds compile PuerTS with the bundled backend without running Setup. PuerTS's own [license](NelaricGameplay/Plugins/Puerts/LICENSE) is included with the plugin source. This setup supports editor TypeScript compilation and script hot reload; production content delivery and version activation require separate implementation.

See the [NelaricGameplayCore](Docs/Modules/Source/NelaricGameplayCore.md), [NelaricCore](Docs/Modules/Plugins/NelaricCore/NelaricCore.md), and [NelaricFoundation](Docs/Modules/Plugins/NelaricGameplayFoundation/NelaricFoundation.md) module descriptions for their responsibilities and dependencies. The [foundation architecture constraints](Docs/FoundationArchitectureConstraints.md) describe the gameplay model, network topologies, and content-update boundaries.

## Contributing

All developers contributing to this project must follow both [Epic Games' Unreal Engine Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) and the [project coding standards](Docs/CodingStandards/). The project standards define our module boundaries, API contracts, review rules, and required automated checks.

See the [contribution guide](CONTRIBUTING.md) for issue and pull request guidance, and the [development and CI workflow](Docs/DevelopmentWorkflow.md) for automated checks and the Linux project build.

## API Documentation

The [API documentation site](https://nelaric.github.io/nelaric-unreal-gameplay/) publishes the coding standards now and will include the public C++ API when public headers are added.

## Star History

<a href="https://www.star-history.com/?repos=Nelaric%2Fnelaric-unreal-gameplay&type=date&legend=top-left">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-gameplay&type=date&theme=dark&legend=top-left" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-gameplay&type=date&legend=top-left" />
    <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-gameplay&type=date&legend=top-left" />
  </picture>
</a>

## Thanks

Thanks to everyone contributing to Nelaric Unreal Gameplay, to Epic Games for Unreal Engine, and to CircleCI for the Linux project build service.
