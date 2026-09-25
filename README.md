<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# Nelaric Unreal Gameplay

An Unreal Engine gameplay framework for persistent worlds and bounded activities across standalone play, listen servers, and dedicated servers, with versioned gameplay content delivery.

## Project layout

Open [NelaricGameplay/NelaricGameplay.uproject](NelaricGameplay/NelaricGameplay.uproject) with Unreal Engine 5.6 or later. This tracked project is the development entry point. Its `Source/` directory contains only project bootstrap code and Game, Editor, and Server targets; shared gameplay contracts belong in `Plugins/NelaricGameplayFoundation/`. `Config/` contains shared project defaults. Game-specific rules and presentation belong in consuming games or optional features.

The local `Development/` validation project remains excluded from Git. Linux CI builds the tracked project's Game, Editor, and Server targets, including the enabled Foundation plugin.

The Editor and Game targets build with the installed UE 5.6 distribution. Building the Server target requires an engine distribution with server-target support; the installed launcher distribution does not provide it.

See the [NelaricFoundation module description](Docs/Modules/NelaricFoundation.md) for its current responsibility and dependencies.

The [foundation architecture constraints](Docs/FoundationArchitectureConstraints.md) define the gameplay model, network topologies, and content-update boundaries.

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
