<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricGameplayCore.md) | 简体中文

# NelaricGameplayCore

`NelaricGameplayCore` 是 `NelaricGameplay/NelaricGameplay.uproject` 项目的运行时模块，Game、Editor 和 Server Target 都包含它。该模块提供项目共用的玩法 World Settings 基类；可复用玩法契约仍位于独立的 [NelaricFoundation](../Plugins/NelaricGameplayFoundation/NelaricFoundation.zh-CN.md) 插件模块中。

本项目的玩法 World Settings 类必须继承 `ANelaricWorldSettings`。项目配置将该类设为默认 World Settings 类；玩法地图需要附加设置时，应使用它的子类，以保留共用基类。本要求适用于 `NelaricGameplay` 项目；独立接入 Foundation 插件的其他项目可自行选择 World Settings 类。

`ANelaricWorldSettings` 提供 `WorldConfig` 资产引用。每个玩法地图可在 World Settings 中选择 Foundation 插件的 `UNelaricWorldConfig`。地图会在世界生命周期内保留该资产；服务端玩法代码负责读取并执行其中的策略。

`ANelaricWorldSettings` 在公开头文件 `NelaricWorldSettings.h` 中声明。由于公开类型继承 `AWorldSettings`，模块公开依赖 Unreal 的 `Core`、`CoreUObject` 和 `Engine` 模块；由于引用配置资产，也公开依赖 `NelaricFoundation`。
