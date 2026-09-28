<!-- Copyright (c) 2026 Nelaric -->

# 玩法模板模块

项目构建四个独立的 Runtime 模块。每个模块提供可通过 Blueprint 继承的
GameMode、PlayerController 和 PlayerState，可按地图选用；GameMode 默认
使用本模块的控制器和玩家状态。四个模板互不引用、互不依赖。

它们依赖 Unreal 的 Core、CoreUObject、Engine 模块和共用的
NelaricFoundation 模块，并继承后者的 GameMode 和玩家控制器基础类，
以保留会话切换配置。

| 模块 | GameMode | PlayerController | PlayerState |
| --- | --- | --- | --- |
| `NelaricOpenWorldTemplate` | `ANelaricOpenWorldGameMode` | `ANelaricOpenWorldPlayerController` | `ANelaricOpenWorldPlayerState` |
| `NelaricBattleRoyaleTemplate` | `ANelaricBattleRoyaleGameMode` | `ANelaricBattleRoyalePlayerController` | `ANelaricBattleRoyalePlayerState` |
| `NelaricMobaTemplate` | `ANelaricMobaGameMode` | `ANelaricMobaPlayerController` | `ANelaricMobaPlayerState` |
| `NelaricSandboxTemplate` | `ANelaricSandboxGameMode` | `ANelaricSandboxPlayerController` | `ANelaricSandboxPlayerState` |

这些类是项目具体规则和玩家数据的起点。创建 Blueprint 或 C++ 派生类，
并配置到对应地图。权威判定在服务端执行，客户端可见状态通过 Unreal 的
GameState、PlayerState 和 Actor 复制。
