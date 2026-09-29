<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# Demo 游戏模块

项目的 `Source/` 目录包含 `DemoGame` Runtime 模块和 Game、Editor、Server
三个 Target。三个 Target 都构建 `DemoGame`，由它注册项目的主游戏模块。

`DemoGame` 用于编写项目具体的演示玩法。目前只提供模块骨架，直接依赖为
Unreal 的 `Core` 模块，作为私有依赖用于模块注册。后续将玩法类放在 `Public/`
目录，实现放在 `Private/` 目录，并按实际需要声明其他模块依赖。

可复用玩法契约和框架 Actor 位于 `NelaricGameplay` 插件的 `GameplayRuntime`
模块中。项目配置选择插件提供的默认玩法类。模块职责与集成契约见
[GameplayRuntime 模块文档](../../Docs/Modules/Plugins/NelaricGameplay/GameplayRuntime.zh-CN.md)。
