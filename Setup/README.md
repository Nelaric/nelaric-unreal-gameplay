<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# PuerTS development setup

Install Node.js with npm, close Unreal Editor, and run your platform's Setup script from the repository root.

## Using Rider

When using Rider, append `--Rider` to the Setup command:

| Platform | Command |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1 --Rider` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh --Rider` |
| macOS x86_64 or arm64 | `sh ./Setup/macOS/Setup.sh --Rider` |

Setup automatically installs PuerTS and project TypeScript dependencies and prepares type checking, compilation, source maps, an entry template, a Rider attach configuration, and the JavaScript packaging directory. Existing scripts and explicit settings are retained.

After setup, generate UE declarations in Unreal Editor and enable the JavaScript and TypeScript, JavaScript Debugger, and Node.js plugins in Rider. Connect the gameplay entry described below, then debug scripts using the generated **Puerts Attach** configuration. The default port is `8080`.

## Using another editor

Run Setup without parameters when using another editor:

| Platform | Command |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh` |
| macOS x86_64 or arm64 | `sh ./Setup/macOS/Setup.sh` |

Setup validates the backend, installs PuerTS editor dependencies, enables the plugin, and prepares the `TypeScript` directory and a basic `tsconfig.json`. Configure your development tools as follows:

1. Open the Unreal project and use the PuerTS declaration generator to produce `Typing/ue`. Regenerate after changing reflected C++ APIs exposed to TS.
2. Install project TypeScript in `NelaricGameplay`. For a new npm project, run `npm init -y`, then `npm install --save-dev --save-exact typescript@4.7.4`. Retain dependency settings in an existing npm project.
3. In `tsconfig.json`, verify `module` is `commonjs`, `typeRoots` includes `Typing`, and `outDir` is `Content/JavaScript`. Enable `strict` for type checking and `sourceMap` for debugging mappings.
4. Write code in `TypeScript`. Run `npx tsc --noEmit` to check types and `npx tsc` to compile from the project directory. Use `npx tsc --watch` for continuous compilation with a manually started environment. Automatic binding uses PuerTS editor compilation and hot reload.
5. Connect gameplay execution as described below. For debugging, use V8 or Node.js, enable the actual script environment's debug port, attach your editor's JavaScript debugger to that port, and retain `.js.map` files. Configure the plugin-managed environment in Project Settings > Plugins > Puerts; pass the debug port explicitly when constructing a separate `FJsEnv`. QuickJS does not support inspector debugging.
6. Before packaging, add `JavaScript` under Project Settings > Packaging > Additional Non-Asset Directories to Package, then verify the packaged script entry and dependencies.

## Connect gameplay execution

Choose an execution mode for your gameplay code:

- **Automatic UE class binding:** write a TS class extending a supported UE class, keep the filename, class name, and default export consistent, and use the generated blueprint class in a level or game settings. Setup enables the automatic environment when creating Puerts configuration; verify `AutoModeEnable=True` in existing settings.
- **Manual script startup:** create and retain `FJsEnv` in a game-owned lifecycle object, such as a GameInstance in `DemoGame`; start the compiled entry module and release the environment on shutdown. Add the required Unreal and `JsEnv` dependencies to the game module. Set the debug port explicitly for this environment.

An entry file must be executed through one of these modes. `DefaultPuerts.ini` configures the plugin-managed environment; use different debug ports when multiple environments run simultaneously.

References: [PuerTS environment](https://puerts.github.io/en/docs/puerts/unreal/dev_environment/), [execution modes](https://puerts.github.io/en/docs/puerts/unreal/getting_started/), and [debugging](https://puerts.github.io/docs/puerts/unreal/vscode_debug/).
