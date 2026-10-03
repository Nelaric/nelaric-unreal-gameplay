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
原 Pawn。战斗、弹药、库存和装备能力授予属于后续独立功能。

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
