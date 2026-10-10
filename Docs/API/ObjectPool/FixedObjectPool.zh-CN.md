<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](FixedObjectPool.md) | 简体中文

# 固定容量原生对象池

GameplayRuntime 模块在 `Nelaric::ObjectPool` 中提供普通 C++ 对象池。自定义 UObject 策略包含 `ObjectPool/FixedUObjectPool.h`，可复用的临时 UObject 包含 `ObjectPool/PlainUObjectPoolPolicy.h`，权威端原生角色池包含 `ObjectPool/CharacterPool.h`。接入模块声明 GameplayRuntime 依赖。

池自身没有反射宏。Capacity 是模板参数，槽位代数、索引空闲链和对象引用使用内联定长数组。预热成功时一次创建全部对象；借还不会扩容，也不会补建丢失的对象。管理存储固定不代表动画、物理、碰撞回调和业务逻辑不产生内存分配。

C++ 概念在模板声明处检查容量、引用与联网模式，以及策略的工厂和生命周期调用。`CPoolPolicy` 要求创建和借出参数类型、可转换为池对象指针的工厂返回值、可作布尔判断的激活结果，以及可调用的归还和销毁方法。联网策略还必须提供 World 访问；可选的 `NetworkName` 必须支持池所使用的 `FName` 操作。角色策略要求公开原生实现 `IPoolableCharacter`；普通 UObject 策略要求可调用的 `OnPoolAcquire` 和 `OnPoolReturn` 方法。

## 所有权与生命周期

世界级普通 C++ 管理器或 Subsystem 保持池地址稳定。所有操作、读取、构造、析构和策略回调均在 Game Thread 执行。World 的 Actor 结束玩法或引擎资源清理前调用 Shutdown。池禁止复制和移动；句柄必须早于池实例销毁而作废，不能跨同地址的池实例重建保留。

每个实例只允许一次预热尝试。创建失败会回滚已经创建的对象，不允许再次尝试。Shutdown 先使全部未归还租约失效，再销毁 Actor 或解除普通 UObject 引用。重复 Shutdown 成功，但不会允许重新初始化。FLease 不自动归还：复制它不会产生新的租约，Release 也不会清空调用方保存的裸指针副本。

| 引用模式 | 契约 |
| --- | --- |
| Collector | 普通 UObject 默认模式；由非反射 FGCObject 向 GC 报告固定 TObjectPtr 引用表。 |
| WorldWeak | Actor 默认模式；World 持有对象，弱引用检测失效。借还发现对象丢失后禁止后续借出，其他存活租约仍可归还。 |
| WorldRaw | 显式优化模式；禁止外部 Destroy、会到期的 LifeSpan，以及池使用期间的 World/Level 卸载。必须先 Shutdown。 |

弱引用检查对象生命期，generation 检查逻辑租约复用，二者都不延长池实例生命期。Get 在转换中、句柄失效或对象失效时返回空，不改变对象丢失标记。空闲链头部的 generation 饱和后停止借出，禁止计数回绕。

`Pool[Index]` 仅供 WorldRaw 池使用，强制内联后直接读取固定指针数组。不执行下标、Game Thread、World、就绪状态、转换状态、generation 或对象生命期检查，没有分配、查找或弱引用解析；地址计算和指针读取本身仍有成本。调用方保证 `Index < CapacityValue`、预热成功、池与 Actor 存活、在 Game Thread 调用且没有正在执行的生命周期转换。`requires` 约束使该接口在其他引用模式下不可用。联网池的客户端读取改为解析弱引用，未收到角色或关闭时可返回空。`At(Index)` 为全部引用模式提供带下标检查的访问。

接口可以返回空闲或已借出槽位中的对象，不会借出该槽位。Lease.Handle.Index 标识物理槽位，但仅凭下标无法识别槽位复用前后的逻辑租约；需要 generation 校验时仍使用 Get(Handle) 和 Release(Handle)。World teardown 或 Shutdown 开始前必须结束直接访问，WorldRaw 模式禁止外部 Destroy 和会到期的 LifeSpan。

## 角色接口与共享 Helper

任意 `ACharacter` 继承链都可以实现 `ObjectPool/PoolableCharacter.h` 中的非反射接口 `Nelaric::ObjectPool::IPoolableCharacter`。接口提供 PrepareForPool、ActivateFromPool、DeactivateToPool 和 IsPoolActive。角色 Policy 在编译期检查契约，通过普通原生虚函数调用，不使用 UInterface、ProcessEvent 或运行时类型注册表。

角色持有一个 FCharacterPoolState 成员，将通用转换委托给 `ObjectPool/CharacterPoolHelper.h` 中的 FCharacterPoolHelper。Helper 处理原生移动、骨骼动画暂停、显示、碰撞和 Tick；各角色实现负责在这些调用前后完成业务重置和取消。Helper 不依赖 GAS，也不要求继承特定的项目角色基类。

Policy 使用延迟 Spawn，在 FinishSpawning 前调用 PrepareForPool，使自动控制器与原生玩法在 BeginPlay 前就已停用。构造和初始化过程必须保持停用状态。各角色在现有基类上实现接口，将通用转换委托给 Helper。

Demo 角色适配、出生点、死亡回收和网络表现见 Content 中的[角色池与出生文档](https://github.com/liu-kaizhi/nelaric-content/blob/main/Docs/Spawning/DemoCharacterPool.zh-CN.md)。

## 角色接入

插件通过 `ObjectPool/FixedObjectPoolWorldSubsystem.h` 提供抽象的 World 生命周期管理基类。具体子系统以普通 C++ 成员持有类型化对象池，实现 PrewarmPools 和 ShutdownPools。基类在 OnWorldBeginPlay 中预热一次，此时 GameMode 尚未向 Actor 分派 BeginPlay；通过 FWorldDelegates::OnWorldBeginTearDown 在世界内 Actor 的 EndPlay 前关闭池，并在 Deinitialize 中做幂等兜底。所有回调必须保持 World 和子系统存活，不能在池转换中结束世界。

Prewarm、Release、Shutdown 返回包含 EPoolError 的 FPoolResult，并支持布尔转换。TryAcquire 的失败原因使用 FLease::Result 中的同一结果模型。原来的布尔判断方式仍可使用。错误区分未就绪、重复预热、重入、创建失败、激活失败、满池、generation 耗尽、对象丢失和无效句柄。

共享 Helper 将已准备的角色隐藏、关闭碰撞和受伤、禁止自动 Controller。激活要求已经 BeginPlay，在无 sweep 状态下设置 Transform，恢复站姿和基础行走模拟，开启原生 Tick 与显示，最后打开碰撞。归还清理移动、跳跃、输入、累计力和待处理 Launch，关闭 Actor、移动组件和骨骼组件 Tick。

角色策略接受单机、监听服务器和独立服务器 World，生成在 PersistentLevel。客户端不能创建或借出角色，即使本地 Actor 的 Role 恰好是 ROLE_Authority。AI、GAS 重置、Montage、RootMotionSource、Cloth、Ragdoll、自定义组件、定时器和异步任务取消仍需要类型专属 Policy。普通 UObject 也需要合法的工厂、Outer 生命周期和原生重置契约；不能仅凭继承 UObject 就将资源或组件视为可复用对象。

## 网络生命周期

最后一个模板参数 `ENetworkMode` 指定联网能力。默认 `Disabled` 不保存池联网绑定；`Replicated` 支持 Actor 派生类型，通过共享 `UPoolNetworkChannel` 发布槽位归属和活动状态，自动维护客户端非拥有视图。调用方无需附加复制组件、编写 `PostNetInit` 绑定或复制回调，也无需增加联网子系统；现有原生池拥有者继续管理普通 World 生命周期。

```cpp
using FNetworkPool = Nelaric::ObjectPool::TCharacterPool<
    AMyCharacter, 100, Nelaric::ObjectPool::EReferenceMode::WorldWeak,
    Nelaric::ObjectPool::ENetworkMode::Replicated>;
```

自定义联网策略需要提供 `FCreateArgs::World`。可选 `FCreateArgs::NetworkName` 指定 World 内的池标识，默认使用原生类型名称；同一 World 中相同原生类型的多个权威池须使用不同名称。客户端绑定使用相同名称和容量。当前传输支持标准 NetGUID 复制驱动，每个 World 最多 128 个具名池，每池最多 4096 个槽位。Iris 等不支持的驱动、无效 World、名称冲突或超出限制会返回 `NetworkingUnavailable`。Actor 创建、移动、RPC 和业务状态继续使用 UE 现有联网能力。

| 网络模式 | `Replicated` 契约 |
| --- | --- |
| NM_Standalone | 本地权威端预热，使用同步租约。 |
| NM_ListenServer | 服务器预热和借还，主机使用权威角色。 |
| NM_DedicatedServer | 无视口也能在服务器预热和借还。 |
| NM_Client | Prewarm 绑定视图；本地借还返回 NotReady。 |

权威端预热创建固定数量的 Actor，开启 Actor 和移动复制，保持 AlwaysRelevant 和 Awake。模块在各 NetDriver 上自动注册通道，每个完成初始化的连接开启一个共享通道。可靠且有界的消息传递池标识、槽位索引、NetGUID、修订号和活动状态。归还保留客户端代理；晚加入客户端会收到当前池快照和后续变化。

池公告与 Actor 创建可以按任意顺序到达。运行时通过 UE 缓存解析 NetGUID，等待 Actor BeginPlay，再应用通用角色移动、骨骼暂停、碰撞、显示和 Tick 状态。`IsReady` 表示池公告已经到达，部分 `At(Index)` 仍可能因角色尚未到达而返回空；此状态也不保证 GAS 就绪。运行时可以先接收视图，再绑定本地原生池。客户端 Shutdown 只解绑本地视图，不销毁服务器代理；权威池关闭会使对应客户端视图失效。

每次借出和归还都会推进槽位修订号，即使两次更新之间归还后再次借出，最终活动位仍为 true，也会重置客户端预测。更新可以合并中间租约，修订号标识最新状态，不是事件流。通用活动状态通过 `FCharacterPoolHelper::IsActive(Character, PoolState)` 查询，准备状态通过 `IsPrepared` 查询；保留原生状态用于权威端和普通出生角色。各类型继续实现普通池生命周期和业务状态复制。

句柄包含本地池地址，不能跨网络复制；客户端 TryAcquire 无法同步返回服务器租约，请求继续通过游戏的权威请求路径提交。自定义 Replication Graph 和相关性覆盖逻辑须保留池 Actor。复用前完成控制权归还，清空移动预测不能撤销旧连接所有权。

## 扩展与重入

自定义 Policy 提供 FCreateArgs、FAcquireArgs，以及静态 Create、OnAcquire、OnReturn、Destroy。OnAcquire 返回 false 后，对象必须仍可交给不可失败的 OnReturn。原生回调及其触发的引擎回调必须保持池和当前转换对象存活，不能强制 GC。Destroy 只用于关闭池时的销毁路径。

同一池内嵌套修改返回 InTransition，转换中的 Get 返回空。停用完成后槽位才进入空闲链。Overlap 回调需要借出、归还或关闭池时，使用有界延迟命令，在当前转换结束后执行，并明确处理失败的归还结果。

现有角色保留自己的基类并实现 IPoolableCharacter，定义空闲状态如何撤销 Ready 绑定、取消初始化任务，以及再次借出时如何恢复；默认创建或转换契约不足时使用自定义 Policy。池租约 generation 与 Pawn 初始化 generation 保护不同契约，不会自动使彼此失效。具体血量、目标、AI、GAS 和网络语义属于接入游戏或可选集成。

## 验证

编译实际接入的模板实例，在引擎内验证满池、反复复用、旧句柄、激活失败、GC 保活、意外 Destroy、Overlap 重入与世界关闭。槽位管理、生命周期 Policy 和引擎工作分别计时。Development 构建可使用 `-trace=default,memory`，通过 Memory Insights 检查分配调用栈。
