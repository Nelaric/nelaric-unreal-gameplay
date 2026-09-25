<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricGameplayCore.md) | 简体中文

# NelaricGameplayCore

`NelaricGameplayCore` 是 `NelaricGameplay/NelaricGameplay.uproject` 项目的运行时模块，Game、Editor 和 Server Target 都包含它。该模块提供项目共用的玩法 World Settings 基类；可复用玩法契约仍位于独立的 [NelaricFoundation](../Plugins/NelaricGameplayFoundation/NelaricFoundation.zh-CN.md) 插件模块中。

本项目的玩法 World Settings 类必须继承 `ANelaricWorldSettings`。项目配置将该类设为默认 World Settings 类；玩法地图需要附加设置时，应使用它的子类，以保留共用基类。本要求适用于 `NelaricGameplay` 项目；独立接入 Foundation 插件的其他项目可自行选择 World Settings 类。

`ANelaricWorldSettings` 是空的基类。世界启动配置通过软引用在地图加载前选择地图；地图不再通过 World Settings 选择启动策略。项目可继承此基类添加地图专属设置。

`ANelaricWorldSettings` 在公开头文件 `NelaricWorldSettings.h` 中声明。由于公开类型继承 `AWorldSettings`，模块公开依赖 Unreal 的 `Core`、`CoreUObject` 和 `Engine` 模块。启动配置资产仍位于独立的 Foundation 插件中；加入运行时启动流程前，Core 不需要依赖它。
