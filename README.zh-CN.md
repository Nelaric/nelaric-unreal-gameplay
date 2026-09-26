<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# Nelaric Unreal Gameplay

[![版本](https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fapi.github.com%2Frepos%2FNelaric%2Fnelaric-unreal-gameplay%2Fproperties%2Fvalues&query=%24%5B0%5D.value&label=version&color=blue)](https://github.com/Nelaric/nelaric-unreal-gameplay/custom-properties)
[![UE 5.6+](https://img.shields.io/badge/UE-5.6%2B-313131?logo=unrealengine&logoColor=white)](NelaricGameplay/NelaricGameplay.uproject)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.zh-CN.md)
[![Unreal CI](https://github.com/Nelaric/nelaric-unreal-gameplay/actions/workflows/fork-pr-linux.yml/badge.svg)](https://github.com/Nelaric/nelaric-unreal-gameplay/actions/workflows/fork-pr-linux.yml)

面向 Unreal Engine 的玩法框架，支持在单机、监听服务器和独立服务器下构建持续世界与有限时长活动，并支持版本化玩法内容交付。

## 项目结构

[NelaricGameplay/](NelaricGameplay/) 目录包含框架的 Unreal 项目。使用 Unreal Engine 5.6 或更新版本打开其中的 [NelaricGameplay.uproject](NelaricGameplay/NelaricGameplay.uproject)。项目的 [Source/](NelaricGameplay/Source/README.zh-CN.md) 包含四个互不依赖的玩法模板模块及 Game、Editor、Server Target；可复用运行时代码位于 `NelaricGameplayFoundation` 插件的 `NelaricFoundation` 模块中。

Linux CI 会构建该项目的 Game、Editor 和 Server Target，包括已启用的 Core、Foundation 和 PuerTS 插件。

## PuerTS 配置

仓库已包含来自 [Tencent/puerts](https://github.com/Tencent/puerts) 的 PuerTS Unreal 插件源码，版本为 [Unreal_v1.0.9](https://github.com/Tencent/puerts/releases/tag/Unreal_v1.0.9)。以下后端均已纳入仓库，适用于 Windows x64、Linux x86_64 和 macOS x64/arm64。每次构建只链接一种后端。

| 后端 | 选择值 | 说明 |
| --- | --- | --- |
| V8 9.4.146.24 | `v8`（默认） | 已有后端，支持 V8 Inspector。 |
| QuickJS | `quickjs` | 运行时较小；上游构建禁用 V8 Inspector。 |
| Node.js 16.16.0 | `nodejs` | 提供 Node.js API；二进制文件通过 Git LFS 存储。 |

启动 Unreal Editor 或 UnrealBuildTool **之前**，在环境变量 `PUERTS_BACKEND` 中设置 `quickjs` 或 `nodejs`；不设置或设置为 `v8` 即使用默认后端。例如，在 PowerShell 中执行 `$env:PUERTS_BACKEND = 'quickjs'`，或在 Unix shell 中执行 `export PUERTS_BACKEND=quickjs`，再从同一个 shell 启动编辑器或构建。切换后端后，重新编译项目并重启编辑器。目标平台必须有相应后端文件。

克隆后，在仓库根目录按操作系统运行**一个** Setup 脚本，准备 TypeScript 编辑器工具：

| 系统 | 命令 |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh` |
| macOS x86_64 或 arm64 | `sh ./Setup/macOS/Setup.sh` |

请先安装带 npm 的 Node.js。Setup 还需要网络连接、`curl` 和 `tar`；Linux 需要 `sha256sum`，macOS 需要 `shasum`。无需再单独运行 npm 或 PuerTS 配置命令。仓库已包含三种后端及其许可证。Setup 会检查仓库内默认使用的 V8 后端、准备 TypeScript 编辑器工具，并确认共享的 `.uproject` 已启用 PuerTS。若 V8 后端目录缺失，Setup 会从官方 PuerTS 发布页下载并校验 SHA-256。安装中断后可以重新运行。

体积最大的 V8 Windows 静态库和 Node.js 运行时二进制文件通过 Git LFS 存储，其余后端文件直接纳入 Git。克隆前请安装 Git LFS，以便检出这些二进制文件。如果检出的是 V8 Windows 库的 LFS 指针，Windows Setup 会获取实际文件。下载缓存仅保留在本机。PuerTS 已启用；需要编辑器 TypeScript 工具时，每次克隆运行一次 Setup。Linux CI 无需运行 Setup，就会使用仓库内默认的 V8 后端编译 PuerTS。仓库保留了 [PuerTS](NelaricGameplay/Plugins/Puerts/LICENSE)、[V8](NelaricGameplay/Plugins/Puerts/ThirdParty/v8_9.4.146.24/LICENSE)、[QuickJS](NelaricGameplay/Plugins/Puerts/ThirdParty/quickjs/LICENSE) 和 [Node.js](NelaricGameplay/Plugins/Puerts/ThirdParty/nodejs_16/LICENSE) 的许可证。该配置支持编辑器内的 TypeScript 编译和脚本热重载；正式环境的内容交付与版本激活需另行实现。

模块职责和依赖见 [NelaricFoundation](Docs/Modules/Plugins/NelaricGameplayFoundation/NelaricFoundation.zh-CN.md) 模块说明；玩法模型、网络拓扑和内容更新边界见[基础架构约束](Docs/FoundationArchitectureConstraints.zh-CN.md)。

## 参与贡献

所有参与本项目的开发者都必须遵守 [Epic Games 的 Unreal Engine C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) 和[项目编码规范](Docs/CodingStandards/README.zh-CN.md)。项目规范定义了模块边界、API 契约、审查规则和必要的自动检查。

关于 Issue 和 Pull Request 的提交方式，请阅读[贡献指南](CONTRIBUTING.zh-CN.md)；自动检查和 Linux 项目编译见[开发与 CI 流程](Docs/DevelopmentWorkflow.zh-CN.md)。

## API 文档

[API 文档网站](https://nelaric.github.io/nelaric-unreal-gameplay/)目前发布编码规范；加入公开头文件后，也会收录公开 C++ API。

## Star 历史

<a href="https://www.star-history.com/?repos=Nelaric%2Fnelaric-unreal-gameplay&type=date&legend=top-left">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-gameplay&type=date&theme=dark&legend=top-left" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-gameplay&type=date&legend=top-left" />
    <img alt="Star 历史图表" src="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-gameplay&type=date&legend=top-left" />
  </picture>
</a>

## 致谢

感谢所有参与 Nelaric Unreal Gameplay 的贡献者，感谢 Epic Games 提供 Unreal Engine，也感谢 CircleCI 提供 Linux 项目编译服务。
