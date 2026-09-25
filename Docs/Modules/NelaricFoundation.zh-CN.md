<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricFoundation.md) | 简体中文

# NelaricFoundation

`NelaricFoundation` 是 `NelaricGameplay/Plugins/NelaricGameplayFoundation` 插件中的运行时模块。该插件由正式的 `NelaricGameplay/NelaricGameplay.uproject` 项目启用，也可以独立供其他项目使用。

该模块承载规则判定、玩法状态、玩家生命周期和活动组合的通用契约。对局流程、目标和计分由可选模块提供；持续世界无需全局对局或结算也可运行。模块基于 Unreal Engine 的玩法与网络系统，不依赖特定玩法类型、后端、技能系统或内容分发 Provider。可选模块可以依赖 Foundation；Foundation 不依赖这些模块。

模块当前唯一的直接 Unreal 模块依赖是 `Core`，在 `NelaricFoundation.Build.cs` 中声明为私有依赖。模块没有公开依赖。
