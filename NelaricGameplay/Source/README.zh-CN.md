<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](README.md) | 简体中文

# Demo 游戏模块

项目的 `Source/` 目录包含 `DemoGame` Runtime 模块和 Game、Editor、Server
三个 Target。三个 Target 都构建 `DemoGame`，由它注册项目的主游戏模块。

`DemoGame` 用于编写项目具体的演示玩法。`ADemoCharacter` 继承框架中的
`ANelaricCharacter`，`ADemoPlayerCharacter` 继承 `ADemoCharacter`，用于玩家
控制的角色变体。`UDemoPlayerInputComponent` 继承框架中的原生玩家输入组件，
可通过玩家角色的 Pawn 初始化配置创建。这些类可在 C++ 和蓝图中派生。
为组件指定 `UNelaricInputConfig`，配置 `InputTag.Move`（Axis2D）、
`InputTag.Look.Mouse`（Axis2D）、`InputTag.Look.Stick`（Axis2D）和
`InputTag.Jump`（Boolean）动作及映射上下文。组件使用原生输入回调处理
移动、视角和跳跃，不依赖 GAS。由于公开头文件使用了框架和引擎类型，
模块公开依赖 `Core`、`CoreUObject`、`Engine`、`GameplayRuntime`、
`EnhancedInput` 和 `GameplayTags`。后续玩法类放在 `Public/`
目录，实现放在 `Private/` 目录，并按实际需要声明其他模块依赖。

可复用玩法契约和框架 Actor 位于 `NelaricGameplay` 插件的 `GameplayRuntime`
模块中。项目配置选择插件提供的默认玩法类。模块职责与集成契约见
[GameplayRuntime 模块文档](../../Docs/Modules/Plugins/NelaricGameplay/GameplayRuntime.zh-CN.md)。

## 装备与武器演示

装备实现位于 `DemoGame/Public/Equipment` 和 `Private/Equipment`。
`UDemoWeaponDefinition` 继承 `UDemoEquipmentDefinition`，
`UDemoWeaponInstance` 继承 `UDemoEquipmentInstance`。定义保存共享配置，
实例保存本地身份与生命周期。管理组件复制装备 ID、定义和唯一的当前激活
装备 ID，客户端根据快照重建实例及表现 Actor。控制者变化时，装备仍归属于
原 Pawn，弹药也保留在原武器实例上。DemoGame 已实现射线射击、有限弹药、
换弹与中断清理；库存和按装备动态授予能力属于后续独立功能。

创建 `UDemoEquipmentManagerComponent` 和 `UDemoPawnAnimationLayerComponent`
的蓝图子类，将两个类添加到已有的 `UPawnInitializationConfig`，
无需在角色构造函数中创建组件。

| 条目 | Authority | Client | Replicate Component | Required |
| --- | --- | --- | --- | --- |
| Equipment | true | true | true | true |
| WeaponAnimation | false | true | false | false |

Equipment 不应依赖 WeaponAnimation，保证独立服务器可以不创建动画组件。
两个组件都实现现有初始化参与者契约，每个 Pawn 各配置一个。联网时，
Pawn 和其初始化协调器需要启用复制；输入组件继续关闭组件复制。

在动画组件蓝图默认值中配置 `MeshComponentName`，填写 Pawn 上组件的准确
对象名称；继承的 Character Mesh 默认为 `CharacterMesh0`。
可用 `DefaultAnimationLayer` 配置徒手层。主动画蓝图需要通过武器动画层接口
放置 Linked Anim Layer 节点，每个武器层实现同一个接口，并使用目标 Mesh
的同一 Skeleton。当前版本提供一个武器动画通道和一个目标 Mesh，
第一人称和双持配置属于后续扩展。动画链接在各端游戏线程本地执行。

创建 `UDemoWeaponDefinition` 类型的 DA，填写互不重复且非空的 `Slot`，
设置 `ActiveAnimationLayer`。默认实例类型是 `UDemoWeaponInstance`，
可以改为其蓝图子类实现生命周期回调。可选的 `Visuals` 填写不复制的表现
Actor 蓝图、目标 Mesh 对象名、Socket 和相对变换。表现 Actor 需要有根组件；
这些 Actor 的碰撞会被关闭。启用 `bActiveOnly` 后，装备未激活时隐藏表现。
Mesh 或 Socket 尚未存在时保留待创建状态，独立服务器跳过模型和动画表现。

创建 `UDemoEquipmentLoadout` 类型的 DA，配置装备定义列表和
`InitialActiveSlot`，在管理组件蓝图默认值中将其赋给 `InitialLoadout`。
权威端在 BeginPlay 且组件 Ready 后应用一次。初始配置的无效条目记录警告并
跳过。打包时应包含定义、Loadout、动画层和表现 Actor 使用的资产。

权威端蓝图或 C++ 调用 `Equip`、`Unequip`、`ActivateEquipment` 和
`DeactivateEquipment`，通过 `EDemoEquipmentResult` 获取结果。按槽位查找
装备后，将其装备 ID 传给激活操作。校验失败保留当前选择。客户端可通过
`GetEquipment` 和 `GetActiveEquipment` 查询本地状态；当前演示不提供客户端
装备 RPC，玩家请求应走游戏自身有权威校验的输入或能力流程。
生命周期回调在各端执行，回调内重入管理组件的修改操作返回 Busy。

## 射线战斗与蓝图配置

`ADemoCharacter` 的原生状态配置自动授予 `Action.Fire` 对应的
`UDemoFireAbility` 和 `Action.Reload` 对应的 `UDemoReloadAbility`。
如果覆盖了 StateProfile，需要在自定义配置中加入这两个能力与动作标签。
控制转移沿用现有 GAS 取消流程，武器弹药继续保留在原 Pawn 上。
客户端只预测能力生命周期；服务器决定射击、扣血和弹药变化，开火表现
通过独立的、不可靠的多播事件发送。

编译后先保存资源并重启编辑器，加载新的反射类型，再配置现有 Demo 内容：

1. 创建 Boolean 类型的 `IA_Character_Fire`、`IA_Character_Reload`。
   在 `IMC_Character` 中映射鼠标左键和 R，并在输入配置的 Native Input
   Actions 中分别绑定 `InputTag.Character.Fire`、
   `InputTag.Character.Reload`。开火使用普通 Boolean 动作，不添加只在
   按下瞬间触发的 Pressed Trigger，保证松键时产生 Completed 或 Canceled。
   C++ 已绑定按下、松开和取消，不需要在蓝图里再写输入或连射逻辑。
   现有徒手与主武器输入继续可用。
2. 完成 Rifle 的 `UDemoWeaponDefinition`：Slot 使用 `PrimaryWeapon`，
   指定兼容的 ActiveAnimationLayer 和模型 Visuals，并将其加入管理组件的
   InitialLoadout。需要默认持枪时，将 InitialActiveSlot 设为
   `PrimaryWeapon`。
3. 可保留原生参数：MagazineCapacity 为 30，InitialReserveAmmo 为 90，
   FireInterval 为 0.1 秒，Range 为 10000 厘米，DamagePerShot 为 10，
   ReloadDuration 为 2 秒，Automatic 开启。DamageEffect 原生默认值已使用
   负的 `Data.Weapon.Damage` 数值扣除 Health，无需额外创建伤害蓝图。
   如需替换效果，必须使用 Instant 类型并消费同一 SetByCaller 标签。
4. 让掩体和目标碰撞阻挡武器的 TraceChannel，默认是 Visibility。
   首版伤害目标为存活、处于活动状态且 GAS 绑定已提交的 `ADemoCharacter`。
   服务器射线从 Pawn 的视点出发，沿基础瞄准方向发射，不依赖表现模型。
   第三人称相机到枪口的瞄准修正和延迟补偿不属于当前演示。
5. 可选配置 FireMontage、ReloadMontage 和 FireSound。角色蒙太奇应使用
   兼容骨架，并在主动画图中接入对应 Slot。换弹蒙太奇的播放速度和起始位置
   跟随服务器时间。MuzzleSocketName 对应表现武器骨骼模型的枪口 Socket。
   FireGameplayCue 默认使用 `GameplayCue.Weapon.Rifle.Fire`，
   ImpactGameplayCue 默认使用 `GameplayCue.Weapon.Rifle.Impact`；空标签禁用
   对应 Cue。在 `/Game/Demo` 下创建匹配的 GameplayCueNotify_Static 蓝图，
   此目录已配置为 GameplayCueNotifyPaths 扫描根。TS 通过 mixin 接管
   OnExecute，参数约定见下文。OnWeaponShot 可用于命中标记等反馈；同一特效只保留
   一个播放入口，避免同时在此事件与 Cue 中生成。
6. 弹药 UI 查询 GetActiveWeapon → GetWeaponState，换弹倒计时查询
   GetReloadRemainingTime。需要事件驱动更新时，创建 `UDemoWeaponInstance`
   蓝图子类，赋给武器定义的 InstanceClass，实现 OnWeaponStateChanged。
   新建 Widget 时先查询当前值；快照可能合并多次状态变化。角色蓝图可使用
   GetHealth、GetMaxHealth、OnHealthChanged 和 OnDeath。死亡时 C++ 已取消
   战斗、禁用移动并停止 Bot 逻辑；死亡动画、UI 和返回概览行为由蓝图配置。

TryFire 对未激活武器、死亡或回池角色、换弹中、空弹匣和射速限制内的请求
返回明确结果，不改变弹药。有效射击先消耗一发弹药，未命中也耗弹。
BeginReload 返回换弹 GUID，CancelReload 仅取消匹配的 GUID。换弹完成时才
按 `min(弹匣容量 - 当前弹药, 备用弹药)` 转移弹药。控制转移、武器停用或
卸下、初始化失效、死亡、回池和退出会取消任务；延迟回调同时核对任务身份、
原控制器和原 ASC，避免旧动作修改新状态。

回池默认保留血量和弹药。需要开始新生命时，在 GAS Ready 后由权威端调用
角色的 ResetCombatState，恢复最大血量、初始弹药和活动角色的移动状态。
玩家归还控制权时不调用此重置。只恢复武器资源可调用 ResetWeaponAmmunition。

验收应覆盖命中与未命中、空弹匣拒绝、部分换弹、中断换弹、按住开火时转移
控制、死亡与回池，再在单机、监听服务器和独立服务器中验证相同内容。
弹药与血量显示应与权威端一致，停用或回池后不应残留射击或换弹完成回调。

## 武器 Gameplay Cue

每次有效射击沿用现有不可靠 Shot 多播；有渲染的世界收到事件后，通过
`ExecuteGameplayCue_NonReplicated` 在本地执行开火与命中 Cue，独立服务器
跳过表现。OnWeaponShot 和 Cue 蓝图无需再发送复制 Cue。扣血、弹药和射击
判定继续由服务器执行，不依赖 Cue。两个 Cue 都使用 Executed，MyTarget
均为开枪 Pawn。任何阻挡命中都会触发 Impact，包括未造成扣血的墙壁命中。

| Cue 参数 | Fire | Impact |
| --- | --- | --- |
| Location | 本地枪口世界位置 | 服务器命中世界位置 |
| Normal | 服务器射击方向的单位向量 | 服务器命中法线 |
| TargetAttachComponent | 包含枪口 Socket 的武器模型组件，或空 | 空 |
| SourceObject | 本次射击的 UDemoWeaponDefinition | 同一个武器定义 |
| Instigator / EffectCauser | 开枪 Pawn | 开枪 Pawn |
| EffectContext 的 Origin | 本地枪口世界位置 | 同一个枪口位置 |
| EffectContext 的 HitResult | 服务器射线结果，可能未命中 | 阻挡命中结果 |
| PhysicalMaterial | 未设置 | 命中的物理材质，如可用 |

C++ 按本次射击的装备 ID 查找模型，不查询可能已切换的当前槽位。
模型或 Socket 不可用时，Fire 位置退回模型原点或服务器 TraceStart，
TargetAttachComponent 为空。本地装备已不存在时，Impact 仍会执行。

现有 GCN_Weapon_Rifle_Fire 与 GCN_Weapon_Rifle_Impact 继续使用
GameplayCueNotify_Static 父类，位于 Demo 扫描目录。
[GCN_Weapon_Rifle_Fire_C.ts](../TypeScript/Demo/GAS/GCN_Weapon_Rifle_Fire_C.ts) 与
[GCN_Weapon_Rifle_Impact_C.ts](../TypeScript/Demo/GAS/GCN_Weapon_Rifle_Impact_C.ts)
分别加载对应蓝图的准确类路径，通过 blueprint.mixin、objectTakeByNative 接管 OnExecute。
无需生成子类或修改父类，也无需再在 OnWeaponShot 或 K2_HandleGameplayCue
中添加第二套 Niagara 播放逻辑。

Fire mixin 使用 SourceObject 武器定义的 MuzzleSocketName，优先将现有枪口
系统附着到 TargetAttachComponent，组件不可用时在 Location 生成。激活前
设置 User.Direction 与 User.Trigger。Impact mixin 用 Location、Normal
分别填写 Niagara Position Array 和 Vector Array，将 User.NumberOfHits
设为 1。命中 Character 或其派生类使用角色火花，其他阻挡命中使用混凝土效果，
额外设置 User.StartOffset 为 0、User.MuzzlePosition 为上下文 Origin。
每个组件均在未激活状态下创建，开启 Auto Destroy，设置参数后激活一次。
UE 世界管理的一次性 Deactivate 计时器将枪口生命周期限制为 0.08 秒，
命中生命周期限制为 1.5 秒，覆盖导入资源的循环配置。计时器弱引用组件，
不持有 JS 回调；Static Cue 不存储某次射击的组件。

[Entry.ts](../TypeScript/Entry.ts) 导入 mixin 模块。DemoGameInstance 的 Init
获取引擎持有的脚本运行环境，在战斗前启动 Entry。Mixin 修改进程共享的
UClass，因此同一进程内的游戏实例共享一个环境；最后一个实例 Shutdown
时释放环境，由 Puerts 恢复原函数。独立服务器跳过表现脚本启动。
Puerts Auto Mode 和编辑器 TS Watcher 与此入口各自独立；重新开始游戏会话
时会加载重新编译后的 JavaScript。

在 Unreal 工程目录执行 npm run typecheck 与 npm run build。需要已经生成
Puerts 声明文件，并准备项目 tsconfig。输出位于 Content/JavaScript；打包
时包含此脚本目录、两个 GC 资产和依赖的 Niagara 资源。

FireSound 和角色蒙太奇仍是可选原生表现。如在 Fire Cue 中播放声音，
将武器定义的 FireSound 留空。中断动作后，已经确认的短时特效可自行播完；
此流程不创建持续的 Add/Remove Cue，也不创建额外开火计时器。
