<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricFoundation.md) | 简体中文

# NelaricFoundation

`NelaricFoundation` 是 `NelaricGameplay/Plugins/NelaricGameplayFoundation` 插件中的运行时模块。该插件由正式的 `NelaricGameplay/NelaricGameplay.uproject` 项目启用，也可以独立供其他项目使用。

该模块承载规则判定、玩法状态、玩家生命周期和活动组合的通用契约。对局流程、目标和计分由可选模块提供；持续世界无需全局对局或结算也可运行。模块基于 Unreal Engine 的玩法与网络系统，不依赖特定玩法类型、后端、技能系统或内容分发 Provider。可选模块可以依赖 Foundation；Foundation 不依赖这些模块。

`UNelaricWorldConfig` 是声明在 `Public/World/NelaricWorldConfig.h` 中的 `UDataAsset`，同时保存世界玩家策略和局部活动的默认策略。`MinPlayersToActivate` 表示启用世界玩法规则所需的最少已接入玩家数：`0` 表示没有门槛，`1` 表示等待第一名玩家。人数达标前仍可加载世界、接入玩家；人数后来降到门槛以下也不会自动停用玩法。`MaxPlayers` 限制已接入人数，`0` 表示框架不额外设限。`bInitiallyAcceptingPlayers` 指定初始是否接收玩家，`bAllowJoinAfterActivation` 指定玩法启用后是否继续接收玩家。`HasValidPlayerLimits()` 会拒绝负数和小于最少人数的正数上限。

同一世界配置中的 `MinParticipantsToStart` 和 `MaxParticipants` 各自以 `0` 表示没有相应活动门槛或上限。`StartPolicy` 选择显式启动或达到最少人数后自动启动；最少人数为 `0` 时，自动启动条件立即满足。`JoinInProgressPolicy` 决定活动开始后拒绝还是接收新参与者。`HasValidActivityParticipantLimits()` 校验人数范围。这些值不规定参与者离开后是否结束活动，也不能突破世界或服务器容量。资产本身不执行策略，也不复制运行状态；服务端玩法代码应在游戏线程读取并应用它。

该公开资产类型使 `NelaricFoundation.Build.cs` 公开依赖 Unreal 的 `Core`、`CoreUObject` 和 `Engine` 模块。
