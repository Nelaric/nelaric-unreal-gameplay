<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](ControlState.md) | 简体中文

# 控制切换中的 GAS 状态

`GameplayAbilitiesIntegration` 是 NelaricGameplay 内的可选 Runtime 模块，公开依赖 Core、CoreUObject、Engine、GameplayRuntime、GameplayAbilities、GameplayTags 和 GameplayTasks，私有依赖 NetCore 用于活动效果复制。GameplayRuntime 不反向依赖该模块。具体战斗属性和能力留在接入游戏。

## 参与者与存储

使用或派生 `ANelaricGasGameMode`、`ANelaricGasPlayerState` 和 `ANelaricGasPlayerController`。人类与 `ANelaricBotController` 都获取 GameMode 配置的同一种 PlayerState。通过 `AttributeSetClasses` 配置角色所有属性集合；`ParticipantProfile` 只初始化一次参与者默认属性与授予。Avatar 变化不改变 PlayerState 或 ASC 对象身份。

`ANelaricGasCharacter` 提供 PawnGasBindingComponent 和 IAbilitySystemInterface。通过 Pawn 初始化配置或蓝图显式添加一个 PawnControlComponent，启用控制切换；同一个 Pawn 只能有一个控制策略组件。其他 Pawn 可以加入这些组件，把能力系统接口转发到绑定组件。在初始化前设置 `StateProfile`；角色生命周期内配置保持不变。同一属性同时被参与者配置与角色配置声明为不同所有权时，拒绝绑定。

无人控制的 Pawn 把状态保留在权威创建的托管 PlayerState ASC 内。该 Actor 由 Pawn 拥有，加入切换预留，随 Pawn 销毁。名单逻辑用 `IsStateCustodian()` 区分托管对象和真实参与者。此时原生 Pawn.PlayerState 可以为空，GAS 绑定仍指向托管 ASC。托管不获取控制加成，没有 Controller 时不能提交输入。

## 逻辑所有权

| 状态 | 所有者 | 控制变化行为 |
| --- | --- | --- |
| 配置中 Participant 属性，或未声明的属性 | 参与者 | 保留在该参与者 ASC。 |
| 配置中 Pawn 属性 | 角色 | 导出 Base，清理旧角色槽，在目标 ASC 导入。 |
| Pawn 配置授予的能力 | 角色 | 保留类、等级、动作 Tag、Input ID 和 SetByCaller，在目标重新授予。 |
| 未标记活动效果 | 参与者 | 保留在原 ASC。 |
| 通过 `TrackEffect` 标记为 Pawn 的活动效果 | 角色 | 恢复 Spec、层数、抑制状态、剩余持续时间及周期截止时间。 |
| 标记为 Control 的活动效果 | 控制关系 | 释放时移除；根据目标配置重新建立加成。 |

属性全部位于 PlayerState ASC 的 AttributeSet，Pawn 不持有另一份永久血量。按游戏选择血量归属。可通过 `ControlEffects`、`PlayerControlEffects` 和 `BotControlEffects` 配置可撤销的操作加成，效果必须非 Instant，且叠层策略为 None，避免 Handle 合并进角色或参与者状态。Current 由导入的 Base 与活动修饰重新计算；禁止把原始属性声明为 Control，应由效果寿命表达可撤销加成。

权威施加角色效果后立即用活动 Handle、Pawn 和所有权调用 `TrackEffect`。已跟踪 Handle 不允许改归其他 Pawn 或寿命。避免引擎把参与者效果与角色效果叠入同一 Handle；只读导出发现目标所有权叠层冲突就拒绝，避免错误合并。共用能力基类自动按 `CooldownOwnership` 标记冷却效果，默认随 Pawn 迁移。Instant 效果没有活动 Handle，其结果已经包含在属性 Base 中。Loose Tag、独立 Cue 及具体执行状态通过领域适配器迁移；标准活动效果内的授予 Tag 和 Cue 由 GAS 管理。

## 统一输入与就绪

玩家输入和 AI Task 都对绑定 ASC 调用 `SubmitAction(ActionTag, bPressed)`，通过 Ability Spec 的 Dynamic Source Tag 匹配动作。`GetInputSourceTag()` 返回 `Input.Source.Player` 或 `Input.Source.AI`，绑定 ASC 同时携带该 Loose Tag。导航仍可以使用 Controller 的 MoveTo。

具体能力派生 `UNelaricGameplayAbility`，激活检查已提交绑定与初始化就绪。按下和松开转发活动实例的 Prediction Key；迁移阻止新输入并清理按住输入。属性监听者先检查 `IsTransferringState()`，避免把清理阶段的临时值当成伤害或死亡；输入检查 `IsReadyForActions()`。

服务器复制绑定所有者、版本和提交标记。客户端仅解除自己旧的 Avatar 绑定，等待复制到达的 AttributeSet，刷新 ActorInfo，并在引用未齐时重试初始化。旧 Controller/PlayerState 不匹配时无法通过提交检查。活动效果使用 Full 复制，让 Bot 与玩家角色都可观察；具体 AttributeSet 仍需实现属性复制与 RepNotify。

## 事务与恢复

包括不交给 Bot 的释放在内，都使用[控制协调器](../Player/ControlSwitching.zh-CN.md)。GAS 参与导出、释放、解绑、绑定、导入、验证和提交阶段；两个 Pawn 在任何来源清理前完成导出。只有全部导入、关联和 Ready 回调检查通过后才提交。补偿成功保留原控制关系和角色状态；补偿失败保留 GC 安全的不可变快照与预留，继续阻止动作。

修复所有角色的原生控制关系后，对存活 GAS 绑定逐个调用 `RestoreReservedState()`，再调用协调器的 `ResolveControlTransitionRecovery()`。恢复支持重试，先清理部分角色导入。协调器验证完整保留批次后才提交并释放预留。

已初始化 GAS Pawn 直接调用 Possess/UnPossess 会绕过事务；绑定保留旧状态，在上下文不匹配时拒绝 Ready。普通变化使用协调器；新角色状态尚未安装时允许初次 Possession 建立参与者。首次托管安装等待一个 Tick，让 GameMode 在同帧执行初次 Possession。连接离开时，GAS Controller 在原生清理前尝试正常交还；若游戏策略或状态契约拒绝，沿用 Unreal 默认离开时的 Pawn 清理。

## 领域适配器

控制请求前，以稳定 ID 注册 `Nelaric::GAS::ITransferExtension`；预留期间禁止修改注册。适配器导出带 Schema 和版本的不可变数据，释放自己拥有的来源资源，在选定 ASC 恢复、验证，并接收不得失败的提交通知。适配器及其快照在恢复期间持续持有，UObject 数据应明确保证 GC 安全。

活动能力默认策略为 Cancel。`PreserveExecution`、`DetachExecution`、`FinishExecution` 和不可取消能力，必须有通过 `HandlesAbility` 声明负责的适配器。适配器应在公共清理前结束来源执行，并实现实际玩法语义；不会把原始 UGameplayAbility/UAbilityTask 对象直接移动到另一 ASC。默认取消执行在补偿时不重新启动。参与者授予 Spec 保留，依赖离开 Avatar 的执行结束。

适配器通过 `HandlesEffect` 接管效果，公共序列化器便不导出该效果。标准恢复在活动容器插入 Spec，不重新运行 OnApplied 管线，恢复剩余时长并从原周期截止点调度。公共周期恢复支持 NeverReset 抑制策略；ResetPeriod 和 ExecuteAndResetPeriod 由适配器处理，避免多执行一次。有自定义 OnAdded/OnRemoved 副作用、特殊目标捕获语义、外部 Handle 引用、生成 Actor 或任务的效果也需要适配器，新的效果或 Spec Handle 由它重新绑定。领域契约保证明确的恢复行为，不承诺序列化任意游戏对象。

DemoGame 源码提供复制的 Health、MaxHealth、Attack，共用 PlayerState、原生 GameMode，以及通过 `Action.Jump` 激活的预测跳跃能力。Demo GameInstance 在未显式指定旅行 GameMode 时，把关卡旧模式入口选为原生 GAS 模式；原生模式继续使用现有蓝图角色。其他蓝图 GameMode 覆盖应派生 GAS GameMode，并采用兼容的 PlayerState 与 Pawn 类。
