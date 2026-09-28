<!-- Copyright (c) 2026 Nelaric -->

[English](FoundationArchitectureConstraints.md) | 简体中文

# 基础架构约束

本文说明 Nelaric Unreal Gameplay 已实现的职责与集成边界。

## 定位与职责边界

Nelaric 为 Unreal Engine 5.6 及以上版本提供可复用的玩法基础。`NelaricFoundation` 包含 Pawn 与 Character 基础类、Pawn 上下文查询、组件初始化契约、世界启动配置和客户端换服流程。四个独立 Runtime 模板模块提供可通过 Blueprint 继承的 GameMode、PlayerController 和 PlayerState 类，供游戏扩展。

Foundation 基于 Unreal Engine 的 Gameplay Framework 和原生网络系统，公开依赖 `Core`、`CoreUObject`、`Engine` 和 `OnlineSubsystemUtils`。项目启用 Foundation 与 PuerTS 插件；具体玩法规则和内容由接入游戏提供。

## 玩法权威与状态

`ANelaricGameModeBase` 是项目配置的默认 GameMode，选用 `ANelaricPlayerController`。它在监听服务器和独立服务器上启动转换审批 Beacon；玩家控制器在当前服务器与所属客户端之间传输离开审批，目标端通过 Online Beacon 和目标 GameMode 处理审批。

Pawn 初始化状态、代次和受管理的动态组件实例保留在各端本地。玩法数据使用独立的 Unreal 复制路径；数据到达事件可请求本地初始化刷新。Pawn 组件查询当前本地上下文，不缓存控制器或玩家状态指针。

## 玩法生命周期

`ANelaricPawn` 与 `ANelaricCharacter` 各自拥有初始化组件。`UNelaricPawnInitializationConfig` 资产指定参与组件类、创建端、必需组件和本地 Ready 依赖。参与组件拥有 Registered、DataAvailable、DataInitialized 和 Ready 有序状态；World 子系统协调推进，循环依赖组先统一提交，再通知成员。

Pawn 就绪由必需参与组件的状态和 Pawn 自身上下文检查共同决定。替换配置会撤销就绪并替换受管理实例；参与组件的代次失效检查拒绝过期异步结果。EndPlay 和 World 销毁会停止初始化并清理受管理实例。详见 [Pawn 组件初始化](API/Pawn/PawnInitialization.zh-CN.md)。

## 运行拓扑

| 上下文 | 已实现行为 |
| --- | --- |
| 单机 | Pawn 配置组件使用权威端创建标记。 |
| 监听服务器 | 服务端 Pawn 使用权威端条目，远程客户端创建本地客户端条目；GameMode 启动审批 Beacon。 |
| 独立服务器 | 服务端 Pawn 使用权威端条目；GameMode 无需本地玩家即可启动审批 Beacon。 |
| 客户端 | Pawn 配置组件使用客户端创建标记；会话子系统可请求转移至另一远程服务器。 |

这些上下文使用 Unreal 的网络角色。初始化管理组件依据所属 Pawn 的 `HasAuthority()` 结果选择条目。会话切换要求起点和目标均为客户端，再验证新的客户端 World 和目标连接。详见[网络会话与权威转换](API/Core/NetWork/NetworkSessionTransitions.zh-CN.md)。

## 玩法组合

`UNelaricPawnComponent` 提供 Pawn 上下文查询，`UNelaricGameplayComponent` 将其提供为可在 Blueprint 中添加的组件。初始化参与组件直接实现 `INelaricInitStateParticipantInterface`，或继承 `UNelaricPawnInitStateComponent`。声明的依赖控制进入 Ready 的时机，各组件自行提供数据准备和玩法行为。

`UNelaricWorldStartupConfig` 保存地图软引用、玩家人数策略和活动参与者默认策略；校验函数检查配置中的人数范围、地图引用和策略枚举值。该资产保存配置数据，不是复制的运行状态。

## 内容与开发工具

项目包含 PuerTS，支持选择 V8、QuickJS 或 Node.js 后端。Setup 脚本准备 TypeScript 编辑器工具；编辑器集成支持 TypeScript 编译和脚本热重载。共享框架资产放在 `NelaricGameplay/Content`，具体游戏的地图、角色和规则属于接入游戏或可选功能。

## 验证

Foundation 模块包含 Pawn 上下文查询、初始化生命周期检查、依赖图、代次失效、本地服务端与客户端初始化，以及客户端换服审批协调的自动化测试。框架测试使用 `Nelaric.*` 层级；Editor CI Job 在编译后运行兼容测试，Game、Editor 和 Server Target 分别使用 UE 5.6.1 在 Linux 上构建。

本文与[模块边界](CodingStandards/Modules.zh-CN.md)及[运行时规范](CodingStandards/Runtime.zh-CN.md)共同适用。
