<!-- Copyright (c) 2026 Nelaric -->

# 玩法模板模块

项目按照[基础架构约束](../../Docs/FoundationArchitectureConstraints.zh-CN.md)
中的四个验收样例构建四个独立的 Runtime 模块。每个模块提供可继承的
GameMode、PlayerController 和 PlayerState，可按地图选用；GameMode 默认
使用本模块的控制器和玩家状态。四个模板互不引用、互不依赖；它们只依赖
Unreal 的 Core、CoreUObject、Engine 模块和共用的 NelaricFoundation 插件，
并继承后者的 GameMode 和玩家控制器基础类，以保留会话切换配置。

| 模块 | GameMode | 扩展方向 |
| --- | --- | --- |
| `NelaricOpenWorldTemplate` | `ANelaricOpenWorldGameMode` | 世界持续运行，区域活动独立启停；通过可选持久化适配保存玩家与任务进度。 |
| `NelaricBattleRoyaleTemplate` | `ANelaricBattleRoyaleGameMode` | 以活动管理参赛者、阶段、淘汰和结算；活动结束不代表整个 World 结束。 |
| `NelaricMobaTemplate` | `ANelaricMobaGameMode` | 以活动管理阵营、并行目标、可选计分或资源以及胜负规则。 |
| `NelaricSandboxTemplate` | `ANelaricSandboxGameMode` | 运行玩家状态、世界事件和可选规则，不要求全局目标、计分或终点。 |

这些类是项目具体规则和玩家数据的起点。玩家状态类尚无自定义复制字段，
也尚未实现战斗、背包、任务、持久化、目标或计分。
创建 Blueprint 或 C++ 派生类并配置到对应地图。权威判定在服务端执行；
客户端可见状态通过 Unreal 的 GameState、PlayerState 和 Actor 复制。
同一套规则应适用于单机、监听服务器和独立服务器。
