<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# Demo 游戏模块

项目的 `Source/` 目录包含 `DemoGame` Runtime 模块和 Game、Editor、Server
三个 Target。三个 Target 都构建 `DemoGame`，由它注册项目的主游戏模块。

`DemoGame` 用于编写项目具体的演示玩法。`ADemoCharacter` 继承框架中的
`ANelaricCharacter`，`ADemoPlayerCharacter` 继承 `ADemoCharacter`，用于玩家
控制的角色变体。`UDemoPlayerInputComponent` 继承框架中的原生玩家输入组件，
可通过玩家角色的 Pawn 初始化配置创建。这些类可在 C++ 和蓝图中派生。
为组件指定 `UNelaricInputConfig`，配置 `InputTag.Move`（Axis2D）、
`InputTag.Look.Mouse`（Axis2D）、`InputTag.Look.Stick`（Axis2D）和
`InputTag.Jump`（Boolean）动作及映射上下文。组件使用原生输入回调处理
移动、视角和跳跃，不依赖 GAS。由于公开头文件使用了框架和引擎类型，
模块公开依赖 `Core`、`CoreUObject`、`Engine`、`GameplayRuntime`、
`EnhancedInput` 和 `GameplayTags`。后续玩法类放在 `Public/`
目录，实现放在 `Private/` 目录，并按实际需要声明其他模块依赖。

可复用玩法契约和框架 Actor 位于 `NelaricGameplay` 插件的 `GameplayRuntime`
模块中。项目配置选择插件提供的默认玩法类。模块职责与集成契约见
[GameplayRuntime 模块文档](../../Docs/Modules/Plugins/NelaricGameplay/GameplayRuntime.zh-CN.md)。
