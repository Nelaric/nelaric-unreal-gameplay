<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricCore.md) | 简体中文

# NelaricCore

`NelaricCore` 是 `NelaricGameplay/Plugins/NelaricGameplayCore` 插件中的运行时模块。该插件由正式的 `NelaricGameplay/NelaricGameplay.uproject` 项目启用，也可以独立供其他项目使用。

该模块承载对局流程、目标、计分和玩家生命周期的通用玩法契约。它基于 Unreal Engine 的玩法与网络系统，不依赖特定玩法类型、后端、技能系统或内容分发 Provider。可选集成模块可以依赖 Core；Core 不依赖这些模块。

模块当前唯一的直接 Unreal 模块依赖是 `Core`，在 `NelaricCore.Build.cs` 中声明为私有依赖。模块没有公开依赖。
