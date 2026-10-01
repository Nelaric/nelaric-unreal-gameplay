<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](ControlSwitching.md) | 简体中文

# 权威端验证的角色控制权切换

`ANelaricPlayerController` 经所属连接提交控制意图。权威端验证请求，由 `ANelaricGameModeBase` 执行玩法资格判断，再通过 `UControlSwitchSubsystem` 执行 Possession，最后向请求玩家返回结果。客户端调用不会在本地直接接管或解除角色。

## 接入

使用 `ANelaricGameModeBase` 及其玩家控制器，或相应派生类。为每个允许接管的 Pawn 添加一个 `UPawnControlComponent`。该组件继承 `UPawnInitStateComponent`；原生组件、蓝图组件和 `UPawnInitializationConfig` 创建的组件均参与既有初始化图。原生基类已有该组件时，不要再添加第二个。

权威端的组件必须已注册并进入 Ready。存在本机 Controller 时，其有效 PlayerState 必须与 Pawn 的 PlayerState 一致。未控制的 Pawn 和远端 AI 副本不要求存在本机 Controller。非框架 Pawn 需要将 Controller、PlayerState 变化接入既有初始化协调器，使上下文替换后参与者失效并重新初始化。

`ADemoCharacter` 创建原生控制组件，并选择 `ANelaricBotController` 为 AI 控制器类。已有蓝图派生类继承该组件；没有修改二进制资产。框架通用 Pawn、Character 基类不强制安装控制策略。

| 设置 | 含义 |
| --- | --- |
| `bAllowPlayerControl` | 允许请求接管该 Pawn。 |
| `bAllowReturnControl` | 允许当前参与者释放或切换离开该 Pawn。 |
| `bReturnToBot` | 释放时必须交给预先准备的 Bot；关闭后留下未控制的 Pawn。 |
| `ReturnControllerClass` | 原 Bot 不可用或正在控制其他 Pawn 时，创建的替代 Bot 类。 |
| `bStartBotLogicOnReady` | AI 控制上下文进入 Ready 时，启动 Pawn 和 Controller 已注册的 Brain 组件。 |

默认替代类为 `ANelaricBotController`，它请求 GameMode 配置类型的 PlayerState。自定义替代类也必须提供有效 PlayerState。使用控制组件协调就绪时，应关闭 Brain 自动启动，并配置其行为资产。具体 Bot 行为由游戏定义。

## 玩家入口

在所属本地玩家控制器调用 `RequestTakeControl(TargetPawn)`；已有角色时，它同时协调旧角色的释放。调用 `RequestReturnControl()` 释放权威端当前角色。发送成功返回正数请求 ID，本地输入无效时返回零。释放请求不能指定其他参与者或角色。

发送前订阅原生 `OnControlSwitchDecision()` 委托，尤其是单机和监听服务器主机，结果可能同步到达。结果包含请求 ID、动作、目标和权威决定。该传输入口不提供取消；结果交付要求连接仍然有效。成功表示权威 Possession 已完成，不表示客户端已收到全部复制状态。

服务器拒绝重复或倒序请求 ID，并比较客户端提交的预期当前 Pawn 与权威状态。排队请求基于旧控制关系创建时，返回 `StaleRequest`。失败请求也消耗其 ID；所属客户端的 Pawn 状态同步后可重新请求。

## 权威验证与玩法策略

协调器检查权威、World 退出、Controller 与 PlayerState 生命周期、包含 Pawn PlayerState 的双向 Possession 一致性、观战身份、目标有效性、联机当前与目标 Pawn 的复制设置、组件 Ready、接管与释放开关和占用情况。各 Controller 的 PlayerState 必须在同一个权威 World 中，且由该 Controller 拥有。相关 Pawn 必须恰好有一个控制组件，策略歧义会被拒绝。其他玩家正在控制的目标返回 `TargetOccupied`。Bot 必须有 PlayerState；不会替换任意非 AI Controller。

在权威 GameMode 覆写 `CanChangePawnControl(Requester, Action, TargetPawn)`，增加阵营、距离、生命状态或权限规则。该入口是同步谓词，不应在其中接管、解除、销毁 Actor 或发起竞争切换。释放请求的 `TargetPawn` 为权威当前 Pawn。原生默认允许通过结构与组件检查的请求；策略回调与替代 Controller 创建完成后，协调器再次检查结构。

请求者身份来自接收所属连接 RPC 的 Controller，不接受客户端指定请求者。Bot 和权威玩法代码可在游戏线程使用已知 Controller 调用 `UControlSwitchSubsystem::ExecuteControlSwitch`，经过同样的检查和玩法规则。不是 Nelaric GameMode 时返回 `NotHandled`。

验证阶段生成固定计划，以弱引用记录请求者、当前与目标 Pawn、相关 PlayerState、目标 Controller、权威 GameMode 和可用的交还 Bot。同时捕获控制组件身份、初始化代际、全部控制设置及 GameMode 的 PlayerState 类。需要创建替代 Bot 时，先检查其类是否具体且可用。回调替换对象或改变捕获的配置、代际后，计划以 `StaleRequest` 失效，不会把原批准自动应用到新上下文。适用性谓词必须只读。

预留阶段先检查去重后的完整参与者集合，再写入任何预留。当前 Pawn、目标 Pawn、相关 Controller 和 PlayerState，以及已选中的现有交还 Bot 和它的 PlayerState，在玩法资格回调前以同一个转换 ID 整体预留。遇到冲突时不留下部分预留。其他请求涉及已预留对象时返回 `ControlTransitionInProgress`，即使角色暂时没有 Controller 或已经重新进入 Ready，也不能重复转换。内部复核使用自己的转换 ID；只有所属转换能够释放这些预留。

准备阶段在预留下执行玩法资格判断，随后复核整个固定计划，包括资格回调拒绝请求的情况。需要新 Bot 时，使用延迟构造，并在生成前初始化回调中预留 Controller，使构造、生成回调和 BeginPlay 都处于预留下。初始化完成的 PlayerState 在修改 Possession 前加入预留。生成回调后、完成构造后、执行前分别复核计划。复用的 Bot 保持空闲；准备不会停止旧控制器的移动或 Brain，不修改 Possession、记忆 Bot，也不销毁旧缓存 Bot。

只有 Possession 完成且最终上下文复核成功后，才更新记忆 Controller。拒绝、准备失败或执行恢复成功时，销毁该请求创建且仍空闲的 Bot；销毁回调期间仍持有所有预留。不会销毁复用 Bot；游戏代码已经将新 Controller 分配给其他 Pawn 时也不强行销毁。World 退出会使后续准备和预留失效。

权威端可通过 `UPawnControlComponent::IsControlTransitionInProgress()` 查询所属角色，或通过 `UControlSwitchSubsystem::IsControlTransitionInProgress(Actor)` 查询 Pawn、Controller、PlayerState。组件在客户端返回 false；这些查询不复制状态。初始化就绪、操控资格与控制转换占用是独立检查。

## 执行与恢复

权威端先为当前 Pawn 准备替代控制器，再修改 Possession。接管时停止旧上下文移动及运行中的 Brain，解除当前 Pawn 与目标 Bot 的控制关系，将目标交给请求者，再将旧 Pawn 交给已准备的 Bot。释放时解除当前控制，并执行该 Pawn 的返回策略。请求者已经控制目标时直接成功，不重启角色。

目标的原 Bot 在整个操作成功后被记住，供以后交还。如果它被销毁或已控制其他 Pawn，则先复用本组件以前创建且空闲的 Bot，再按需创建替代 Bot。Controller 由 World 管理；组件记录自己创建的 Bot，结束生命周期时仅销毁其中空闲且未被预留的对象，不销毁其他 Controller 正在控制的 Pawn，也不销毁其他转换已预留的 Bot。

框架 Pawn 的初始化上下文变化在操作期间合并，旧本地绑定撤销，参与者基于最终 Controller 和 PlayerState 重新初始化。Ready 回调后再次验证最终控制关系；这些回调及恢复过程的回调期间，预留始终有效。游戏回调不应执行竞争性的直接 Possession；涉及已预留对象的嵌套请求返回 `ControlTransitionInProgress`，World 内无关对象的嵌套协调请求仍返回 `Busy`。

执行检查 Controller、Pawn 的双向关系及 PlayerState 一致性。失败时尝试恢复原关系，但不会抢回已经由回调重新分配的无关 Pawn 或 Controller。`ExecutionFailed` 表示执行失败且恢复成功；`RecoveryFailed` 表示销毁或竞争回调阻止恢复。停止的导航不会自动恢复；配置的 Bot Brain 通过 Ready 控制上下文重新启动。

执行成功、普通拒绝或恢复成功时，作用域结束会释放本次转换的预留。`RecoveryFailed` 保留仍存活对象的预留，阻止后续普通请求。`IsControlTransitionRecoveryRequired(Actor)` 可区分待修复与正在执行。权威玩法修复控制关系或移除相关对象后，以任意仍存活的预留对象调用 `ResolveControlTransitionRecovery(Actor)`；协调器检查所有存活参与者、双向 Possession、有效 PlayerState 及已预留 Pawn 的就绪状态，通过后才释放整个失败转换。该方法不执行 Possession，也不能在正在执行的控制操作内调用。后续操作前清理已销毁参与者，World 退出时清空记录。预留使用弱引用，不延长 Actor 生命周期。

## 结果与范围

普通拒绝不会断开请求者连接。除执行、恢复结果外，还包括 `Denied`、`TargetOccupied`、`NoCurrentPawn`、`PlayerStateUnavailable`、`StaleRequest`、`ReplacementUnavailable`、`ControlTransitionInProgress`、`Busy`、`InvalidRequest`、`InvalidTarget` 和 `WorldUnavailable`。

本机制更新控制关系，使用新 Controller 自己已有的 PlayerState，不交换 PlayerState。GameplayRuntime 不依赖 GameplayAbilities，不复制 ASC 属性、效果、冷却或活动能力。GAS 集成需要协调状态交接；Possession 本身不构成状态迁移。要求状态连续的项目应完成该集成后，再为 GAS 角色启用这些请求。
