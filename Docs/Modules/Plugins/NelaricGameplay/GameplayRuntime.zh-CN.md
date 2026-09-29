<!-- Copyright (c) 2026 Nelaric -->

[English](GameplayRuntime.md) | 简体中文

# GameplayRuntime

`GameplayRuntime` 是 `NelaricGameplay/Plugins/NelaricGameplay` 插件中的运行时模块，承载玩法契约、可复用项目侧 Actor 和会话传输。`NelaricGameplay/NelaricGameplay.uproject` 项目启用该插件；

该模块承载规则判定、玩法状态、玩家生命周期和活动组合的通用契约。对局流程、目标和计分由可选模块提供；持续世界无需全局对局或结算也可运行。模块基于 Unreal Engine 的玩法与网络系统，不依赖特定玩法类型、后端、技能系统或内容分发 Provider。可选模块可以依赖 GameplayRuntime；GameplayRuntime 不依赖这些模块。

`UWorldStartupConfig` 是声明在 `Public/World/WorldStartupConfig.h` 中的 `UPrimaryDataAsset`，通过 `WorldMap` 软引用选择地图，同时保存世界玩家策略和局部活动的默认策略。启动方应在加载地图前选定并持有该资产。`MinPlayersToActivate` 表示启用世界玩法规则所需的最少已接入玩家数：`0` 表示没有门槛，`1` 表示等待第一名玩家。人数达标前仍可加载世界、接入玩家；人数后来降到门槛以下也不会自动停用玩法。`MaxPlayers` 限制已接入人数，`0` 表示框架不额外设限。`bInitiallyAcceptingPlayers` 指定初始是否接收玩家，`bAllowJoinAfterActivation` 指定玩法启用后是否继续接收玩家。`HasValidPlayerLimits()` 会拒绝负数和小于最少人数的正数上限。

同一启动配置中的 `MinParticipantsToStart` 和 `MaxParticipants` 各自以 `0` 表示没有相应活动门槛或上限。`StartPolicy` 选择显式启动或达到最少人数后自动启动；最少人数为 `0` 时，自动启动条件立即满足。`JoinInProgressPolicy` 决定活动开始后拒绝还是接收新参与者。`HasValidActivityParticipantLimits()` 校验人数范围。`HasValidStartupConfig()` 还会检查地图引用和枚举值，但不加载地图或校验服务器容量。这些值不规定参与者离开后是否结束活动，也不能突破世界或服务器容量。资产本身不执行策略，也不复制运行状态；服务端玩法代码应在游戏线程读取并应用它。

`ANelaricGameModeBase` 通过 `DefaultEngine.ini` 成为项目默认 GameMode，并为已连接玩家选用 `ANelaricPlayerController`。地图可以覆盖 GameMode，派生 GameMode 也可以选用更具体的控制器。控制器向当前服务器请求离开，并把旧服直接批准的结果返回所属客户端。目标审批 Beacon 使用调用方提供的地址和端口，在旅行前联系目标服务器。目标 GameMode 的 `TransitionBeaconListenPort` 默认是 15000，可配置为与该端点一致。转换子系统目前支持客户端换服：等待两端回复、开始旅行并验证新的客户端连接。目标 GameMode 的 `CanAcceptTransition()` 当前直接返回 `true`；`TODO(NELARIC-TRANSITION-CAPACITY-INTEGRATION)` 标出了未来执行活动 `WorldStartupConfig.MaxPlayers` 的位置。在该接入完成前，目标审批不检查容量，Beacon 不预留名额，也不授权 `PreLogin`。

本项目的玩法 World Settings 类必须继承 `ANelaricWorldSettings`。项目配置将这一空基类设为默认 World Settings 类；玩法地图需要附加设置时，应使用它的子类。世界启动配置通过软引用在地图加载前选择地图，地图不通过 World Settings 选择启动策略。独立接入插件的其他项目可以自行选择 World Settings 类。

模块公开依赖 Unreal 的 `Core`、`CoreUObject`、`Engine` 和 `OnlineSubsystemUtils`。模块的内部集成 Passkey 位于 `Private/Internal/`。公开头文件仅在 C++ 内部集成方法签名需要时前置声明 Key；玩法模块无法通过插件公开 API 包含或构造它。项目配置直接使用 `/Script/GameplayRuntime` 类路径。

模块、文件、全局作用域类型和日志类别不强制使用 `Nelaric` 项目前缀，也不禁止使用；名称须避免与已有名称冲突。`Nelaric::` 及其子命名空间内的类型不得带此前缀，保留 Unreal 必需的类型前缀。`ANelaricCharacter`、`ANelaricPawn`、`ANelaricGameModeBase`、`ANelaricPlayerController`、`ANelaricWorldSettings` 和 `UNelaricPawnComponent` 保留前缀，因为 Unreal 已定义对应的无前缀类型和头文件。模块入口文件使用 `GameplayRuntimeModule.h` 和 `GameplayRuntimeModule.cpp`。`Nelaric::` C++ 命名空间和 `Nelaric.*` 自动化测试分组保持稳定。
