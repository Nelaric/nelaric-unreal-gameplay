<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricFoundation.md) | 简体中文

# NelaricFoundation

`NelaricFoundation` 是 `NelaricGameplay/Plugins/NelaricGameplayFoundation` 插件中的运行时模块，承载可复用玩法 Actor、Pawn 组件初始化、世界启动配置和会话传输。`NelaricGameplay/NelaricGameplay.uproject` 项目启用该插件；其 `Source/` 目录包含四个独立玩法模板模块。

模块基于 Unreal Engine 的玩法与网络系统，具体玩法规则由接入游戏提供。`ANelaricPawn` 和 `ANelaricCharacter` 保留 Unreal 基础类行为，并各自创建初始化组件。`UNelaricPawnComponent` 读取当前 Pawn、控制器和玩家状态上下文，不缓存查询结果；`UNelaricGameplayComponent` 是可在 Blueprint 中添加的派生组件。

`UNelaricPawnInitializationConfig` 定义本地参与组件类、稳定组件 ID、权威端与客户端创建标记、必需参与组件和 Ready 依赖。参与组件实现 `INelaricInitStateParticipantInterface`，或继承 `UNelaricPawnInitStateComponent`。`UNelaricInitStateWorldSubsystem` 协调 Registered、DataAvailable、DataInitialized 和 Ready 状态；循环依赖组统一提交 Ready 后再通知。Pawn 就绪结合必需参与组件状态和上下文检查，并可撤销。状态、代次和受管理的动态实例保留在本地。配置、刷新和清理方式见 [Pawn 组件初始化](../../../API/Pawn/PawnInitialization.zh-CN.md)。

`UNelaricWorldStartupConfig` 是声明在 `Public/World/NelaricWorldStartupConfig.h` 中的 `UPrimaryDataAsset`。`WorldMap` 保存地图软引用；`MinPlayersToActivate` 保存启用世界玩法的玩家人数门槛，`0` 表示没有门槛。`MaxPlayers` 保存已接入人数上限，`0` 表示框架不额外设限。`bInitiallyAcceptingPlayers` 和 `bAllowJoinAfterActivation` 分别保存初始和玩法启用后的准入策略。`HasValidPlayerLimits()` 会拒绝负数和小于最少人数的正数上限。

同一资产还保存活动参与者默认策略。`MinParticipantsToStart` 和 `MaxParticipants` 各自以 `0` 表示没有相应门槛或上限。`StartPolicy` 保存 `Manual` 或 `WhenMinimumReached`，`JoinInProgressPolicy` 保存 `Reject` 或 `Participate`。`HasValidActivityParticipantLimits()` 校验人数范围；`HasValidStartupConfig()` 还检查地图引用和枚举值。这些函数校验配置数据，玩法代码在游戏线程读取并应用其中的策略。

`ANelaricGameModeBase` 通过 `DefaultEngine.ini` 成为项目默认 GameMode，并为已连接玩家选用 `ANelaricPlayerController`。地图可以覆盖 GameMode，派生 GameMode 也可以选用更具体的控制器。控制器向当前服务器请求离开，并把旧服直接批准的结果返回所属客户端。目标审批 Beacon 使用调用方提供的地址和端口，在旅行前联系目标服务器。目标 GameMode 的 `TransitionBeaconListenPort` 默认是 15000，可配置为与该端点一致。转换子系统接受客户端换服请求：等待两端回复、开始旅行，并在 30 秒截止时间内验证新的客户端 World 及主机、端口与目标匹配的已打开连接。目标 GameMode 的 `CanAcceptTransition()` 当前直接返回 `true`；`TODO(NELARIC-TRANSITION-CAPACITY-INTEGRATION)` 标出了读取当前生效的 `WorldStartupConfig` 并执行 `MaxPlayers` 限制 的位置。请求与取消行为见[网络会话与权威转换](../../../API/Core/NetWork/NetworkSessionTransitions.zh-CN.md)。

项目配置将 `ANelaricWorldSettings` 设为默认 World Settings 类。该类继承 `AWorldSettings`，可通过 Blueprint 继承；玩法地图需要附加设置时可使用子类。地图引用保存在 `UNelaricWorldStartupConfig` 中。

模块公开依赖 Unreal 的 `Core`、`CoreUObject`、`Engine` 和 `OnlineSubsystemUtils`。模块的内部集成 Passkey 位于 `Private/Internal/`。公开头文件仅在 C++ 内部集成方法签名需要时前置声明 Key；玩法模块无法通过插件公开 API 包含或构造它。项目配置使用 `/Script/NelaricFoundation` 类路径，并为原 `/Script/NelaricGameplayCore` 类名配置重定向，以便已有资产引用仍可解析。
