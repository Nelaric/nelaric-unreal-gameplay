<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# Nelaric Unreal Gameplay

An Unreal Engine gameplay framework for persistent worlds and bounded activities across standalone play, listen servers, and dedicated servers, with versioned gameplay content delivery.

## Project layout

The [NelaricGameplay/](NelaricGameplay/) directory contains the framework's Unreal project. Open its [NelaricGameplay.uproject](NelaricGameplay/NelaricGameplay.uproject) with Unreal Engine 5.6 or later. The project's `Source/` contains the `NelaricGameplayCore` runtime module and Game, Editor, and Server targets.

Linux CI builds this project for Game, Editor, and Server, including the enabled Foundation plugin.

See the [NelaricGameplayCore](Docs/Modules/Source/NelaricGameplayCore.md) and [NelaricFoundation](Docs/Modules/Plugins/NelaricGameplayFoundation/NelaricFoundation.md) module descriptions for their responsibilities and dependencies. The [foundation architecture constraints](Docs/FoundationArchitectureConstraints.md) describe the gameplay model, network topologies, and content-update boundaries.

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
