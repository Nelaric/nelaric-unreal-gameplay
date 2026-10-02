<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](ControlSwitching.md) | 简体中文

# 权威端验证的角色控制权切换

`ANelaricPlayerController` 经所属连接提交控制意图。权威端验证请求，由 `ANelaricGameModeBase` 执行玩法资格判断，再通过 `UControlSwitchSubsystem` 执行 Possession，最后向请求玩家返回结果。客户端调用不会在本地直接接管或解除角色。

## 接入

使用 `ANelaricGameModeBase` 及其玩家控制器，或相应派生类。为每个允许接管的 Pawn 添加一个 `UPawnControlComponent`。该组件继承 `UPawnInitStateComponent`；原生组件、蓝图组件和 `UPawnInitializationConfig` 创建的组件均参与既有初始化图。原生基类已有该组件时，不要再添加第二个。

权威端的组件必须已注册并进入 Ready。存在本机 Controller 时，其有效 PlayerState 必须与 Pawn 的 PlayerState 一致。未控制的 Pawn 和远端 AI 副本不要求存在本机 Controller。非框架 Pawn 需要将 Controller、PlayerState 变化接入既有初始化协调器，使上下文替换后参与者失效并重新初始化。

`ADemoCharacter` 选择 `ANelaricBotController` 为 AI 控制器类。通过 Pawn 初始化配置或蓝图显式添加控制组件，原生角色构造函数不再创建它。上帝视角摄像机 Pawn 同样需要显式配置控制策略，并关闭 Bot 交还。框架通用 Pawn、Character 基类不强制安装控制策略。

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

## 状态导出与关联切换

可操控 Pawn 通过 `UPawnControlComponent::RegisterStateTransferParticipant(Id, Participant)` 注册可选的原生 `Nelaric::Control::IStateTransferParticipant` 集成。ID 在本 Pawn 内必须非空且唯一。组件共享持有集成对象，通过 `UnregisterStateTransferParticipant(Id)` 移除。在预留期间，包括恢复失败期间，禁止改变注册集合。应在控制操作前安装注册，不要在 Ready 回调中替换。固定计划同时捕获注册版本和初始化代际。

各集成提供以下同步权威游戏线程操作：

| 操作 | 契约 |
| --- | --- |
| `GetReservationActors` | 预留前只读发现额外状态持有者，例如托管 PlayerState。 |
| `ExportState` | 只读导出原状态，返回非空不可变 `FStateSnapshot`；其 `SchemaId` 非空，`SchemaVersion` 为正数。 |
| `ReleaseState` | 在原 ActorInfo 仍可用时移除选定端点的角色状态；支持部分清理后的补偿与重试。 |
| `DetachAssociation` | 只解除选定端点拥有的绑定；允许原绑定已经不存在，不清除其他 Pawn 后来建立的绑定。 |
| `AttachAssociation` | 原生 Possession 改变后、Ready 回调前建立选定绑定；Source 表示恢复，Destination 表示正向切换。 |
| `ImportState` | 在 Ready 前把完整角色快照恢复到已绑定端点；Source 用于补偿，Destination 用于正向导入。 |
| `IsStateValid` | 只读验证导入状态及领域适配器。 |
| `CommitState` | 整批验证后发布已提交状态；通知不得失败或改变 Possession。 |
| `IsAssociationValid` | 在绑定后、Ready 回调后及显式恢复解决期间只读检查关联。 |

`FStateTransferContext` 记录共用转换 ID、Pawn 身份及原始、目标两个端点。端点以非拥有引用记录 Controller 和 PlayerState。显式空引用表示未控制端点；失效引用不能当成有意为空。无法支持拟议空端点的集成必须在导出时拒绝。集成可派生 `FStateSnapshot` 保存有类型的数据。UObject 引用应采用弱引用或明确的 GC 安全所有权；原生共享指针本身不会阻止 UObject 被垃圾回收。

准备完成后，协调器合并 Pawn 上下文变化，撤销 Pawn Ready，并停止旧移动及运行中的 Brain。在任何导出回调前，先复制两个角色的完整注册集合。导出时原 Possession 和 PlayerState 关联仍然存在，每个导出回调后重新验证固定计划。导出失败返回 `StateExportFailed`；上下文被替换时返回相应验证错误。两者都不开始关联切换，也不公布部分导出结果。

所有角色导出成功后，协调器才为各 Pawn 公布完整 `FControlStateExport`，其中包含两个关联端点、导出 World 时间及带注册 ID 的快照。关联回调和恢复失败期间可以通过 `GetExportedControlState(Pawn)` 读取。完整公布前及转换释放后返回空。调用者可以独立保留共享快照，但 Actor 引用仍为弱引用。没有状态集成的 Pawn 仍导出关联元数据，状态列表为空。

切换先释放**全部**来源角色状态，再解除**全部**来源绑定，然后为完整角色集合切换原生 Possession，使用各新 Controller 自己已有的 PlayerState。全部目标绑定建立后，才导入全部目标状态。Ready 回调前后都验证状态及关联，最后整批提交。这样来源角色不会清除新 Avatar，也不会覆盖尚未导出的另一个角色状态。释放先于 ActorInfo 解绑，能力清理仍能访问原 Avatar。

失败时补偿所有尝试过的目标导入和绑定，包括返回失败前已部分执行的回调；恢复原生来源关系、来源绑定，再为所有尝试过释放的来源重新导入快照。恢复整批验证后才提交。`StateImportFailed` 表示状态移除或导入失败但恢复成功；`StateAssociationFailed` 表示绑定失败且恢复成功；原生 Possession 失败使用 `ExecutionFailed`。`RecoveryFailed` 保留完整导出、预留和修复所需的新替代 Bot。

玩法修复可以读取保留快照，修复原生关系及集成状态后调用 `ResolveControlTransitionRecovery`。存活的集成 Pawn 必须落在捕获的某个端点，并通过绑定和状态验证。解决器在释放预留前提交恢复状态，但不执行 Possession、不建立关联、不导入数据。World 退出会停止回调并清空保留快照；Actor 引用保持为弱引用。

关联方法只更新上下文、订阅或 ASC ActorInfo；玩法状态变化放在释放与导入阶段。新增状态方法有默认实现，兼容只管理关联的已有集成。可选的 [GAS 集成](../GAS/ControlState.zh-CN.md) 完成属性、效果及能力授予迁移，GameplayRuntime 不增加 GameplayAbilities 依赖。

## 执行与恢复

权威端先为当前 Pawn 准备替代控制器，再修改 Possession。接管时停止旧上下文移动及运行中的 Brain，解除当前 Pawn 与目标 Bot 的控制关系，将目标交给请求者，再将旧 Pawn 交给已准备的 Bot。释放时解除当前控制，并执行该 Pawn 的返回策略。请求者已经控制目标时直接成功，不重启角色。

目标的原 Bot 在整个操作成功后被记住，供以后交还。如果它被销毁或已控制其他 Pawn，则先复用本组件以前创建且空闲的 Bot，再按需创建替代 Bot。Controller 由 World 管理；组件记录自己创建的 Bot，结束生命周期时仅销毁其中空闲且未被预留的对象，不销毁其他 Controller 正在控制的 Pawn，也不销毁其他转换已预留的 Bot。

框架 Pawn 的初始化上下文变化在操作期间合并，旧本地绑定撤销，参与者基于最终 Controller 和 PlayerState 重新初始化。Ready 回调后再次验证最终控制关系；这些回调及恢复过程的回调期间，预留始终有效。游戏回调不应执行竞争性的直接 Possession；涉及已预留对象的嵌套请求返回 `ControlTransitionInProgress`，World 内无关对象的嵌套协调请求仍返回 `Busy`。

执行检查 Controller、Pawn 的双向关系及 PlayerState 一致性。失败时尝试恢复原关系，但不会抢回已经由回调重新分配的无关 Pawn 或 Controller。`ExecutionFailed` 表示执行失败且恢复成功；`RecoveryFailed` 表示销毁或竞争回调阻止恢复。停止的导航不会自动恢复；配置的 Bot Brain 在 Ready 上下文完成提交且预留释放后重新启动。启动延迟到下一 Tick，并复核初始化代际，避免 AI 动作在状态事务中执行。

执行成功、普通拒绝或恢复成功时，作用域结束会释放本次转换的预留与导出。`RecoveryFailed` 保留仍存活对象的预留，阻止后续普通请求。`IsControlTransitionRecoveryRequired(Actor)` 可区分待修复与正在执行。权威玩法修复控制关系或移除相关对象后，以任意仍存活的预留对象调用 `ResolveControlTransitionRecovery(Actor)`；协调器检查所有存活参与者、双向 Possession、有效 PlayerState 及已预留 Pawn 的就绪状态，通过后才释放整个失败转换。该方法不执行 Possession，也不能在正在执行的控制操作内调用。后续操作前清理已销毁参与者，World 退出时清空记录。预留使用弱引用，不延长 Actor 生命周期。

## 结果与范围

普通拒绝不会断开请求者连接。状态阶段新增 `StateExportFailed` 与 `StateAssociationFailed`。除执行、恢复结果外，还包括 `Denied`、`TargetOccupied`、`NoCurrentPawn`、`PlayerStateUnavailable`、`StaleRequest`、`ReplacementUnavailable`、`ControlTransitionInProgress`、`Busy`、`InvalidRequest`、`InvalidTarget` 和 `WorldUnavailable`。

本机制更新控制关系，使用新 Controller 自己已有的 PlayerState，不交换 PlayerState 身份。GameplayRuntime 协调领域状态，不认识 GAS 类型。GameplayAbilitiesIntegration 提供 ASC 存储、Pawn 托管、所有权配置、统一动作输入、状态迁移和自定义恢复契约。已初始化的 GAS Pawn 应通过协调器切换；直接原生 Possession 绕过整批事务后，玩法会保持阻止状态，直到原上下文被修复。
