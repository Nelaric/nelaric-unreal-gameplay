<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# Nelaric Unreal Gameplay

面向 Unreal Engine 的玩法框架，支持在单机、监听服务器和独立服务器下构建持续世界与有限时长活动，并支持版本化玩法内容交付。

## 项目结构

[NelaricGameplay/](NelaricGameplay/) 目录包含框架的 Unreal 项目。使用 Unreal Engine 5.6 或更新版本打开其中的 [NelaricGameplay.uproject](NelaricGameplay/NelaricGameplay.uproject)。项目的 `Source/` 包含 `NelaricGameplayCore` 运行时模块及 Game、Editor、Server Target。

Linux CI 会构建该项目的 Game、Editor 和 Server Target，包括已启用的 Core 和 Foundation 插件。

## PuerTS 配置

仓库已包含 PuerTS Unreal 插件源码。克隆后，在仓库根目录按操作系统运行**一个** Setup 脚本：

| 系统 | 命令 |
| --- | --- |
| Windows x64 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\Setup\Windows\Setup.ps1` |
| Linux x86_64 | `sh ./Setup/Linux/Setup.sh` |
| macOS x86_64 或 arm64 | `sh ./Setup/macOS/Setup.sh` |

请先安装带 npm 的 Node.js。脚本还需要网络连接、`curl` 和 `tar`；Linux 需要 `sha256sum`，macOS 需要 `shasum`。无需再单独运行 npm 或 PuerTS 配置命令。脚本从官方 [PuerTS Unreal 1.0.9 发布页](https://github.com/Tencent/puerts/releases/tag/Unreal_v1.0.9)下载 V8 9.4 后端，校验 SHA-256，安装后端及其[许可证](Setup/V8-LICENSE)，准备 TypeScript 编辑器工具，并在本地 `.uproject` 中启用 PuerTS。安装中断后可以重新运行。

后端二进制文件和下载缓存仅保留在本机，不纳入 Git。Setup 只在本地工作树中修改受 Git 跟踪的 `.uproject`；除非 CI 也配置安装后端，否则共享提交中应保持 `Puerts` 禁用。插件源码随附 PuerTS 的[许可证](NelaricGameplay/Plugins/Puerts/LICENSE)。该配置支持编辑器内的 TypeScript 编译和脚本热重载；正式环境的内容交付与版本激活需另行实现。

模块职责和依赖见 [NelaricGameplayCore](Docs/Modules/Source/NelaricGameplayCore.zh-CN.md)、[NelaricCore](Docs/Modules/Plugins/NelaricCore/NelaricCore.zh-CN.md) 与 [NelaricFoundation](Docs/Modules/Plugins/NelaricGameplayFoundation/NelaricFoundation.zh-CN.md) 模块说明；玩法模型、网络拓扑和内容更新边界见[基础架构约束](Docs/FoundationArchitectureConstraints.zh-CN.md)。

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
