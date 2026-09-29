<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# PuerTS 开发环境配置

先安装带 npm 的 Node.js，关闭 Unreal Editor，然后在仓库根目录运行对应平台的 Setup。

## 使用 Rider

使用 Rider 时，在 Setup 命令后加上 `--Rider`：

| 平台 | 命令 |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1 --Rider` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh --Rider` |
| macOS x86_64 或 arm64 | `sh ./Setup/macOS/Setup.sh --Rider` |

Setup 会自动安装 PuerTS 和项目的 TypeScript 依赖，准备类型检查、编译、source map、脚本入口模板、Rider 附加调试配置及 JavaScript 打包目录，并保留已有脚本和显式配置。

完成后，在 Unreal Editor 中生成 UE 类型声明，在 Rider 中启用 JavaScript and TypeScript、JavaScript Debugger 和 Node.js 插件。接入下文的玩法执行入口后，使用生成的 **Puerts Attach** 配置调试脚本，默认端口为 `8080`。

## 不使用 Rider

不使用 Rider 时，运行不带参数的 Setup：

| 平台 | 命令 |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh` |
| macOS x86_64 或 arm64 | `sh ./Setup/macOS/Setup.sh` |

Setup 会检查后端、安装 PuerTS 编辑器依赖、启用插件，并准备 `TypeScript` 目录和基础 `tsconfig.json`。之后按以下步骤配置自己的开发工具：

1. 打开 Unreal 项目，使用 PuerTS 声明生成工具生成 `Typing/ue`。修改暴露给 TS 的 C++ 反射 API 后，重新生成声明。
2. 在 `NelaricGameplay` 目录安装项目自己的 TypeScript。首次创建 npm 项目时执行 `npm init -y`，然后执行 `npm install --save-dev --save-exact typescript@4.7.4`。已有 npm 项目沿用其依赖配置。
3. 在 `tsconfig.json` 中确认 `module` 为 `commonjs`、`typeRoots` 包含 `Typing`、`outDir` 为 `Content/JavaScript`。启用 `strict` 做严格类型检查，启用 `sourceMap` 生成调试映射。
4. 在 `TypeScript` 目录编写代码。在项目目录执行 `npx tsc --noEmit` 检查类型，执行 `npx tsc` 编译。手动启动脚本环境时，可执行 `npx tsc --watch` 持续编译；自动绑定模式使用 PuerTS 的编辑器编译与热重载流程。
5. 按下文接入玩法执行入口。需要调试时，使用 V8 或 Node.js 后端，开启实际脚本环境的调试端口，在所用编辑器的 JavaScript 调试器中附加到该端口，并保留 `.js.map` 文件。插件管理的环境可在 Project Settings > Plugins > Puerts 中开启调试；手动构造的 `FJsEnv` 需要显式传入调试端口。QuickJS 不支持 Inspector 调试。
6. 打包前，在 Project Settings > Packaging > Additional Non-Asset Directories to Package 中加入 `JavaScript`，并验证打包后的脚本入口与依赖文件。

## 接入玩法执行入口

根据玩法代码选择一种执行方式：

- **自动绑定 UE 类：** 编写继承受支持 UE 类的 TS 类，保持文件名、类名和默认导出一致，再将生成的蓝图类用于关卡或游戏设置。Setup 创建 Puerts 配置时会启用自动环境；已有配置需要确认 `AutoModeEnable=True`。
- **手动启动脚本：** 在游戏自己的生命周期对象中创建并持有 `FJsEnv`，例如 `DemoGame` 的 GameInstance，启动编译后的入口模块，并在退出时释放环境。为游戏模块加入所需的 Unreal 与 `JsEnv` 依赖。调试端口需要在这个环境上显式设置。

脚本入口文件需要通过上述方式执行。`DefaultPuerts.ini` 配置的是插件管理的环境；多个环境同时运行时应使用不同调试端口。

参考：[PuerTS 开发环境](https://puerts.github.io/en/docs/puerts/unreal/dev_environment/)、[执行模式](https://puerts.github.io/en/docs/puerts/unreal/getting_started/)和[调试说明](https://puerts.github.io/docs/puerts/unreal/vscode_debug/)。
