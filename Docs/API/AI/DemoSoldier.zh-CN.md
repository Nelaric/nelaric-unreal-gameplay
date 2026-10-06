<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](DemoSoldier.md) | 简体中文

# Demo 单兵 AI

单兵实现位于 DemoGame，直接使用 ADemoCharacter，复用其生命值、装备和控制权生命周期。
GameplayRuntime 保持通用 GameAI 契约。单兵决策和状态修改只在权威端游戏线程执行；
客户端沿用现有移动、GAS 和装备同步。本实现不复制战斗记忆和命令。

## 配置

继续使用现有继承 ADemoCharacter 的角色蓝图，无需新建角色类或修改父类。
通过角色的 UPawnInitializationConfig DA 装配 UDemoSoldierComponent。组件接入现有
InitState 契约；ADemoCharacter 不在代码中创建 Soldier 或兜底 PawnControl，也不
覆盖配置中的交还控制器。GetSoldierComponent 查询当前已装配的组件，创建前或移除后
返回空引用。

创建继承 UDemoSoldierComponent 的组件蓝图，在其默认值中配置 Settings，
然后在 DA 中选择这个类。也可以直接选择原生组件类，使用默认参数。最小权威端配置为：

| 条目字段 | 值 |
| --- | --- |
| ComponentId | Soldier，或其他稳定且唯一的 ID |
| ComponentClass | UDemoSoldierComponent 或其组件蓝图 |
| bCreateOnAuthority | true |
| bCreateOnClient | false |
| bReplicateComponent | false |
| bRequiredForPawnReady | true |
| DependencyIds | 同一 DA 中所需装备、控制条目的实际 ID |

依赖只能填写 DA 中实际存在的 ID；客户端创建的条目不能依赖这个仅权威端创建的条目。
PawnControl 继续放在同一初始化 DA 中，其组件默认值中的 ReturnControllerClass
设为 ADemoSoldierController 或对应控制器蓝图。角色蓝图的 AIControllerClass 设为
同一控制器，AutoPossessAI 设为 PlacedInWorldOrSpawned，并允许蹲伏。保留现有主动画
和初始装备，并在具有已构建 NavMesh 的关卡中使用。控制器内部的原生 Brain 和
Perception 属于 ADemoSoldierController 实现，不放入角色初始化 DA。

PawnControl 在其 Ready 组提交后尝试启动 Brain，Soldier 还会在完整 Pawn Ready
通知后通过一次性回调重试这个策略。对象池预热禁用 AutoPossessAI；激活后，Soldier
会为尚无控制器的活动角色创建蓝图 AIControllerClass 指定的控制器，并调用现有
ControlSwitchSubsystem 完成初次接管及 GAS 状态迁移。遵循 PawnControl 的接管资格
和 GameMode 控制策略；不能对已经初始化的角色直接调用 Possess。
接管引发的初始化变更完成后，再通过完整 Pawn Ready 通知启动 Brain。此流程不抢占玩家
控制，不在预热期间创建控制器，也不按固定间隔重试。执行要求整个 Pawn、自身和 GAS 就绪；Pawn 就绪被撤销时立即
停止执行。玩家接管停止执行，交还 AI 后恢复同一角色保存的命令。原生执行无需 StateTree
资产。初始化后获取 Soldier 并检查有效性再下命令；以上配置下客户端没有这个组件。

团队 ID 由外部注入，保存在 ADemoCharacter 上并通过 Actor 属性复制。Soldier 组件和
DA 不配置 TeamId；组件的 GetTeamId 读取所属角色的当前 ID。相同 ID 为友方，
不同 ID 为敌方，255 为中立；未注入的角色默认为 255。

第一处注入入口是 ADemoInitialCharacterSpawnPoint 的 TeamId。在关卡中分别选择
初始角色生成点，在 Details 的 Demo / Spawning 下设置 Team Id，例如 0 和 1。
生成点将其 ID 传给 Pool->TryAcquire(Transform, TeamId)，池策略在激活碰撞、
移动和已有 Brain 前调用 Character->SetTeamId(TeamId)。所有团队可以共用
BP_DemoCharacter、同一 Soldier 组件蓝图和初始化 DA；生成点默认注入 0。
对象池归还先停止角色，再清除其 ID；每次借出都重新注入，省略参数则生成中立角色。

其他生成流程在权威端游戏线程调用 DemoCharacter 的 SetTeamId；它不要求 Soldier
组件已经创建。通过 GetTeamId 查询当前值。角色实现 UE GenericTeamAgentInterface，
没有 Soldier 组件时仍能被识别为所属团队。玩家接管或 AI 交还不会改变角色的 ID。
运行中重新注入会同步 AIController、通知士兵刷新敌我判断，并通知正在看见该角色的
士兵；这些路径使用事件回调，不增加 Tick。其他阵营规则仍可重写 IsHostile。

## 命令与中断

```cpp
FDemoSoldierOrder Order;
Order.Type = EDemoSoldierOrderType::Move;
Order.Location = Destination;
Order.AcceptanceRadius = 100.0f;
Soldier->IssueOrder(Order);
```

IssueOrder 验证参数、补充分配命令身份并替换原有意图。None 取消命令。
接受后通过 GetOrder 和 GetOrderStatus 查询。Move 到达后完成；Hold 与 Defend 持续
执行，追击、调查和找掩体的目标位置都受 HoldRadius 约束。Follow 要求有效角色，
并刷新跟随位置。没有 Actor 的 Attack 表示向位置推进，到达后完成；带 Actor 的
Attack 先前往命令提供的初始位置，在观察到目标后攻击。确认看到目标死亡则完成，
未观察到的目标身份销毁则失败。命令目标与当前战斗目标独立。

战斗、调查和避险保留当前命令，结束后从当前位置继续执行。手雷避险可以临时越过
守点边界，到达安全点后等待报告过期或撤销，再返回守点范围。移动具有超时；
命令移动失败时返回 Failed，避免无限等待。

## 战斗与认知

视觉维护数量受限的弱引用目标记录；听觉和伤害不会设置视觉可见。可见目标可以
刷新位置，失去视线后保留最后观察位置并按时间遗忘。目标评分使用可见性、近期
伤害、距离和指定攻击目标偏好，并通过锁定时间和评分差距限制切换。

原生执行器按反应时间、有限点射和观察间隔调度行动。射击间隔、命中 Trace、弹药
及换弹继续由 DemoWeaponInstance 维护。每次请求射击前额外检查遮挡。换弹使用已有
身份与取消结果。低弹药在安全窗口换弹；空弹匣且有备用弹药时换弹；无备用弹药时
进入明确的 OutOfAmmo 行动。手雷可以打断所有动作，掩体移动和换弹完成后再考虑
普通战斗决策。

DemoWeaponInstance 接受的射击会报告 Gunshot 听觉刺激；实际提交的 GAS 生命值
负效果会报告伤害及方向提示。也支持外部 UE Damage Sense 刺激，同一次伤害不要
同时走这两条报告路径。其他声音可使用 UE ReportNoiseEvent 或直接调用 ReportSound。

ReportNearMiss 增加会衰减的压制。ReportGrenade 接受观察到的位置、半径和剩余
持续时间，不查询手雷 Actor 的实时 Transform。ClearGrenade 撤销已拆除或取消的
危险报告。报告数量、目标记录和重试均有上限。逃跑点与掩体查询在动作开始或失败
后执行。当前掩体通过 ADemoSoldierCoverPoint、导航可达性和蹲伏视线遮挡验证。
将标记放在障碍后的地面高度；当前没有动态掩体生成。

## 正式 StateTree 配置

以下结构使用 StateTree 选择动作，原生组件只执行选中的动作并维护认知。已有单节点
原生规划器接入仍可通过 Run Native Planner 开启；正式树关闭这个选项。一名士兵
只允许一个执行驱动，Global Task 只放一个 Run Demo Soldier，每个叶状态只放一个
Demo Soldier Action。

1. 创建控制器蓝图 BP_DemoSoldierController，父类为 ADemoSoldierController。
   在 Class Defaults 关闭 Use Native Brain。添加 Game AI State Tree Component，
   关闭 Start Logic Automatically。由现有 Pawn Ready 流程启动 Brain。
2. 创建 StateTree 资产 ST_DemoSoldier，Schema 选择 Game AI。
   该 Schema 对应原生类型 UGameAIStateTreeSchema；不要选择通用 StateTree AI Schema。
   将资产配置到上述控制器的 Game AI State Tree Component。
3. 沿用 BP_DemoCharacter 与初始化 DA 中的 Soldier、装备和 PawnControl 条目。
   PawnControl 的 ReturnControllerClass 和角色的 AIControllerClass 均选择该控制器
   蓝图；AutoPossessAI 为 PlacedInWorldOrSpawned。不要在角色蓝图再添加 Soldier。
4. 在资产的 Global Tasks 添加 Run Demo Soldier：Run Native Planner=false，
   Initial Order=Hold，Initial Hold Radius=500。首次启动在出生位置生成 Hold 命令，
   无需关卡蓝图下初始命令。Initial Order=None 则没有初始任务，进入 Idle。
   初始命令只应用一次，不覆盖已有、完成或取消的命令；交还控制权不会重设守点。
5. 可以在 Evaluators 添加 Demo Soldier Snapshot，用于查看 Memory、Order、
   OrderStatus 和 Behavior。这是调试输出；下面的条件直接读取已提交状态，不需要
   手动绑定这些输出。Pawn、Controller、StateTreeComponent 等 Context 字段自动绑定。

### 状态层级

按以下顺序排列子状态；带子状态的节点选择 Try Select Children In Order，
叶状态选择 Try Enter。容器不放动作 Task。可将节点名按表命名，以便配置转换。

```text
Root
├─ Dead
├─ Emergency
│  └─ AvoidGrenade
├─ Combat
│  ├─ Reload
│  ├─ TakeCover
│  ├─ OutOfAmmo
│  ├─ TargetVisible
│  │  ├─ ApproachTarget
│  │  ├─ Aim
│  │  ├─ FireBurst
│  │  └─ Reevaluate
│  └─ TargetLost
│     ├─ MoveLastKnownPosition
│     ├─ Search
│     └─ ForgetTarget
├─ Alert
│  ├─ InvestigateDamage
│  └─ InvestigateSound
├─ ExecuteOrder
│  ├─ ReturnToArea
│  ├─ Move
│  ├─ Hold
│  ├─ Attack
│  ├─ Follow
│  └─ Defend
└─ Idle
```

### Enter Conditions 与 Tasks

表中的 Test 都是 Demo Soldier Condition 的 Test 参数；只有注明反转的条件开启
Invert。空白表示无需条件。Action 都是 Demo Soldier Action 的 Action 参数。

| 状态 | Enter Condition：Test | 叶状态 Task：Action |
| --- | --- | --- |
| Root | 无 | 无 |
| Dead | Alive，Invert=true | Dead |
| Emergency | Grenade | 无 |
| AvoidGrenade | 无 | AvoidGrenade |
| Combat | Combat | 无 |
| Reload | NeedsReload | Reload |
| TakeCover | NeedsCover | TakeCover |
| OutOfAmmo | OutOfAmmo | OutOfAmmo |
| TargetVisible | TargetVisible | 无 |
| ApproachTarget | CanApproach | ApproachTarget |
| Aim | 无 | Aim |
| FireBurst | 无 | FireBurst |
| Reevaluate | 无 | Observe |
| TargetLost | TargetVisible，Invert=true | 无 |
| MoveLastKnownPosition | CanSearchMove | MoveToMemory |
| Search | 无 | Search |
| ForgetTarget | 无 | ForgetTarget |
| Alert | Alert | 无 |
| InvestigateDamage | DamageCue | InvestigateDamage |
| InvestigateSound | 无 | InvestigateSound |
| ExecuteOrder | HasOrder | 无 |
| ReturnToArea | ReturnToArea | ExecuteOrder |
| Move | MoveOrder | ExecuteOrder |
| Hold | HoldOrder | ExecuteOrder |
| Attack | AttackOrder | ExecuteOrder |
| Follow | FollowOrder | ExecuteOrder |
| Defend | DefendOrder | ExecuteOrder |
| Idle | 无 | Idle |

InvestigateDamage 必须配置 DamageCue 进入条件；否则任何 Alert 都会先选择这个
叶状态，包括只有声音的情况。听觉线索通过下一项 InvestigateSound 处理。
位置观察仅在存在有效水平方向时设置焦点。未知伤害方向、与士兵重合的位置或
纯垂直线索保留当前朝向，避免无方向向量将 Yaw 重置为 0。

Combat 条件包括有效战斗目标和必要的武器/掩体维护。Reload 的条件内部处理空弹匣
强制换弹和低弹药安全窗口；备用弹药为零不会选择换弹。未找到掩体会进入重试冷却，
所以没有 CoverPoint 时可以继续战斗。OutOfAmmo 只在有可见目标且无可用武器弹药时
选择；失去目标后仍进入搜索。手雷避险可以越过守点边界，结束后通过 ReturnToArea
回到原有 Hold/Defend 区域。

### Transitions

Root 配置以下两条转换。不要仅依靠子状态顺序来实现运行中的优先级打断。
Root 的 Enter Conditions 必须为空。ShouldReconsider 放在第一条转换自己的
Conditions 中；它控制是否打断当前动作，不能作为 Root 的进入条件。

| 触发 | Required Event → Tag | 条件 | Target State | Priority |
| --- | --- | --- | --- | --- |
| On Event | AI.Event.DecisionChanged | Demo Soldier Condition：ShouldReconsider | Root | High |
| On State Completed | 无 | 无 | Root | 不适用（编辑器不提供） |

Root 不添加 On Tick 转换。第一条只在事件到来时判断新命令、手雷危险、目标变化和
动作类别。Required Event 的 Payload 为空，沿用默认事件消费设置。事件在组件提交
认知与动作状态后发送；不需要蓝图调用 Send StateTree Event。第一条不启用 Delay。
第二条是动作成功或失败后的兜底重选。UE 5.6 的完成转换从已完成状态向父状态查找，
找到第一条条件通过且可选择目标的转换就停止；叶状态的动作序列因此先于 Root 兜底。
On State Completed、On State Succeeded 和 On State Failed 均不提供可配置的 Priority
或 Delay，无需为这些转换设置优先级。

Demo Soldier Action 关闭 Task Tick，移动、换弹等回调通过有效请求身份调用
StateTree 的 FinishTask。Demo Soldier Snapshot 通过通知回调更新，不实现 Evaluator
Tick。正式路径不按 DecisionInterval 轮询；感知、装备弹药提交、可见角色的位置和
生命值变化、移动结果与角色销毁都会唤醒执行器。自身位置回调只在守点、危险、
射程等相关边界变化时唤醒，旋转回调仅处理等待中的瞄准对齐。

反应时间、点射间隔、观察、搜索、记忆过期、危险过期和超时使用一次性定时回调。
每次安排最近的已知截止时间，处理后再安排下一截止时间；没有固定间隔决策循环，
无事件且无待处理截止时间时不会安排更新。进入状态只生成一次动作参数。

| 源叶状态 | 触发 | Target State |
| --- | --- | --- |
| Aim | On State Succeeded | FireBurst |
| Aim | On State Failed | Reevaluate |
| FireBurst | On State Completed | Reevaluate |
| Reevaluate | On State Completed | Root |
| ApproachTarget | On State Succeeded | Aim |
| ApproachTarget | On State Failed | Reevaluate |
| MoveLastKnownPosition | On State Completed | Search |
| Search | On State Succeeded | ForgetTarget |
| ForgetTarget | On State Completed | Root |
| Reload | On State Completed | Root |
| TakeCover | On State Completed | Root |
| AvoidGrenade | On State Completed | Root |
| InvestigateDamage | On State Completed | Root |
| InvestigateSound | On State Completed | Root |
| ReturnToArea、Move、Hold、Attack、Follow、Defend | On State Completed | Root |

Idle、OutOfAmmo 和 Dead 不添加完成转换。Idle、Hold、Follow、Defend 和 OutOfAmmo
可以持续 Running，由 Root 的重选转换打断。Aim 是可见目标分支的默认首选；
FireBurst 与 Reevaluate 通过明确转换进入，不会按排列顺序自动执行。

### 无动作时的定位

先看权威端控制器和 StateTree 调试实例。初始化与启动成功时，Output Log 中会出现
Soldier execution started，包含角色、控制器、驱动、规划模式和阵营。不能创建控制器、
缺少 Game AI State Tree Component、Global Task 或动作启动失败都有对应日志。
需要详细的就绪前置条件时，在 Output Log 控制台启用 Log LogDemoSoldier Verbose。

初始 Hold 表示守在出生位置，因此没有敌人时站立是正常状态。互为敌人的测试单位需要
在各自初始生成点设置不同 Team Id，例如 0 和 1；两个生成点共用同一角色蓝图和
初始化 DA。检查生成日志 Initial character spawned 中的 team，或角色的 GetTeamId。
StateTree 编译成功后，运行实例应持续停在
Root / ExecuteOrder / Hold；发现敌人后应进入 Combat，再依次进入 Aim、FireBurst、
Reevaluate。Aim 与 ApproachTarget 的成功转换明确选 On State Succeeded，失败转换
明确选 On State Failed；不要保留新转换默认的 On State Completed。Search 成功转换
目标是 ForgetTarget。完成转换无需设置 Priority；叶状态的有效完成转换先执行，
没有可用转换时才向父状态查找 Root 兜底。

不要给每个 AI.Event 无条件添加 Root 转换：普通感知变化不应反复取消尚未完成的
换弹或掩体移动。ShouldReconsider 已统一处理这些中断限制。任务 Exit 只取消对应
请求，不清空长期 Order。点射每次只生成一次发数；移动到最后已知位置失败也继续
短暂观察，之后遗忘目标。死亡由现有角色生命周期先停止 Brain，Dead 是树的终止
防线，死亡表现仍由现有角色处理。

### 验证资产

编译并保存 StateTree 与控制器蓝图。测试关卡需要 NavMesh；放入两个初始角色生成点，
分别设置 Team Id 为 0 和 1，让对象池激活现有 BP_DemoCharacter。没有目标时保持
出生区域；进入彼此视野后
应看到 Aim → FireBurst → Reevaluate 循环；遮挡目标后应搜索最后已知位置，然后
回到原有守点。使用 StateTree Debugger 查看当前叶状态，使用 Snapshot 查看 Order
身份是否在战斗和控制权交还后保持一致。资产搭建后的 PIE 是这些行为的实际验证。

## 生命周期

原生 AI.Event 标签通过修改完成后的一次性延迟回调通知 OnEvent 监听者；StateTree
接入也会将标签转发给执行组件。DecisionChanged 在已提交的认知与动作变化后发送。
事件表示变化通知，应查询当前快照，不作为历史
动作结果使用。通知数量受限，同一标签会合并。移动和换弹回调按请求身份匹配，
旧回调无法完成替换后的动作。叶状态退出时先解绑完成回调，再取消自己的请求；
停止执行会解绑位置、生命值、装备和销毁通知，并取消待处理定时器。

Brain 停止时取消移动、有限点射和自己启动的换弹，解绑动作回调并恢复移动配置，
保留命令用于控制权交还。InitState 上下文失效会取消相同动作与排队事件；移除或
替换 DA 条目会销毁组件及其命令，角色 getter 不缓存旧实例。
死亡和对象池停用沿用已有 Brain 停止路径。
ResetCombatState 同时清空单兵命令和认知，开始新生命。池复用代表不同士兵时应
明确调用 ResetSoldierState 或 ResetCombatState；普通激活继续保留原有状态。
