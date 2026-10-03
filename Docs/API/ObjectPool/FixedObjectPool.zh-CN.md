<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](FixedObjectPool.md) | 简体中文

# 固定容量原生对象池

GameplayRuntime 模块在 `Nelaric::ObjectPool` 中提供普通 C++ 对象池。自定义 UObject 策略包含 `ObjectPool/FixedUObjectPool.h`，可复用的临时 UObject 包含 `ObjectPool/PlainUObjectPoolPolicy.h`，权威端原生角色池包含 `ObjectPool/CharacterPool.h`。接入模块声明 GameplayRuntime 依赖。

池自身没有反射宏。Capacity 是模板参数，槽位代数、索引空闲链和对象引用使用内联定长数组。预热成功时一次创建全部对象；借还不会扩容，也不会补建丢失的对象。管理存储固定不代表动画、物理、碰撞回调和业务逻辑不产生内存分配。

## 所有权与生命周期

世界级普通 C++ 管理器或 Subsystem 保持池地址稳定。所有操作、读取、构造、析构和策略回调均在 Game Thread 执行。World 的 Actor 结束玩法或引擎资源清理前调用 Shutdown。池禁止复制和移动；句柄必须早于池实例销毁而作废，不能跨同地址的池实例重建保留。

每个实例只允许一次预热尝试。创建失败会回滚已经创建的对象，不允许再次尝试。Shutdown 先使全部未归还租约失效，再销毁 Actor 或解除普通 UObject 引用。重复 Shutdown 成功，但不会允许重新初始化。FLease 不自动归还：复制它不会产生新的租约，Release 也不会清空调用方保存的裸指针副本。

| 引用模式 | 契约 |
| --- | --- |
| Collector | 普通 UObject 默认模式；由非反射 FGCObject 向 GC 报告固定 TObjectPtr 引用表。 |
| WorldWeak | Actor 默认模式；World 持有对象，弱引用检测失效。借还发现对象丢失后禁止后续借出，其他存活租约仍可归还。 |
| WorldRaw | 显式优化模式；禁止外部 Destroy、会到期的 LifeSpan，以及池使用期间的 World/Level 卸载。必须先 Shutdown。 |

弱引用检查对象生命期，generation 检查逻辑租约复用，二者都不延长池实例生命期。Get 在转换中、句柄失效或对象失效时返回空，不改变对象丢失标记。空闲链头部的 generation 饱和后停止借出，禁止计数回绕。

`GetByIndexUnchecked(Index)` 仅供 WorldRaw 池使用，强制内联后直接读取固定指针数组。不执行下标、Game Thread、World、就绪状态、转换状态、generation 或对象生命期检查，没有分配、查找或弱引用解析；地址计算和指针读取本身仍有成本。调用方保证 `Index < CapacityValue`、预热成功、池与 Actor 存活、在 Game Thread 调用且没有正在执行的生命周期转换。其他引用模式调用该接口时会触发编译期断言。

接口可以返回空闲或已借出槽位中的对象，不会借出该槽位。Lease.Handle.Index 标识物理槽位，但仅凭下标无法识别槽位复用前后的逻辑租约；需要 generation 校验时仍使用 Get(Handle) 和 Release(Handle)。World teardown 或 Shutdown 开始前必须结束直接访问，WorldRaw 模式禁止外部 Destroy 和会到期的 LifeSpan。

## 角色接口与共享 Helper

任意 `ACharacter` 继承链都可以实现 `ObjectPool/PoolableCharacter.h` 中的非反射接口 `Nelaric::ObjectPool::IPoolableCharacter`。接口提供 PrepareForPool、ActivateFromPool、DeactivateToPool 和 IsPoolActive。角色 Policy 在编译期检查契约，通过普通原生虚函数调用，不使用 UInterface、ProcessEvent 或运行时类型注册表。

角色持有一个 FCharacterPoolState 成员，将通用转换委托给 `ObjectPool/CharacterPoolHelper.h` 中的 FCharacterPoolHelper。Helper 处理原生移动、骨骼动画暂停、显示、碰撞和 Tick；各角色实现负责在这些调用前后完成业务重置和取消。Helper 不依赖 GAS，也不要求继承特定的项目角色基类。

Policy 使用延迟 Spawn，在 FinishSpawning 前调用 PrepareForPool，使自动控制器与原生玩法在 BeginPlay 前就已停用。构造和初始化过程必须保持停用状态。各角色在现有基类上实现接口，将通用转换委托给 Helper。

ADemoCharacter 保留 ANelaricGasCharacter 基类并实现同一接口，ADemoPlayerCharacter 继续继承 ADemoCharacter。普通出生的 Demo 保持激活状态及原有控制器、复制配置；池创建的实例从停用状态开始，由权威端复制。Demo 在激活前后都检查 GAS Binding 已提交、可接收操作，且 ASC 的 Avatar 是当前角色；检查失败返回 ActivationFailed 并恢复空闲槽位。已有属性、效果、技能与控制权继续遵循原有契约，池不会按租约重建 GAS 状态。

池化 Demo 在初始化时检查构造阶段是否已经提供控制策略；只有构造阶段及本机初始化配置都不提供策略时才增加临时 UPawnControlComponent；配置将在 BeginPlay 创建策略时，角色订阅 Pawn Ready 并等待该组件，避免生成重复策略。角色先保存现有策略的四个控制开关，或兜底组件的原生默认值，再在空闲期间关闭玩家接管、归还和 Bot 自动启动。激活时，在开启碰撞前恢复这些值，并显式启动已经绑定且 Ready 的 Bot，因为空闲期间完成的 Ready 通知不会重新触发。归还时再次关闭开关，停止 AI 移动以及角色、AIController 的 Brain，保留快照供下一次借出恢复。Controller 关联和 GAS 所有权继续由玩法管理，归还租约前应完成玩家控制归还和控制转换。蓝图配置为 false 的开关在激活时仍为 false。World 的 GameMode 仍需提供 Demo GAS PlayerState 属性布局，ADemoGameMode 已配置该布局。Helper 分别记录已准备的池角色和普通出生角色；DemoGame 在权威端和客户端的原生显示转换中读取逻辑池状态，使客户端选择逻辑也能看到恢复后的本地控制设置。单独修改显示状态不会改变控制状态。这些控制设置保留在 DemoGame，通用 Helper 和池生命周期管理基类不依赖 Pawn 控制或 GAS。

## 角色接入

插件通过 `ObjectPool/FixedObjectPoolWorldSubsystem.h` 提供抽象的 World 生命周期管理基类。具体子系统以普通 C++ 成员持有类型化对象池，实现 PrewarmPools 和 ShutdownPools。基类在 OnWorldBeginPlay 中预热一次，此时 GameMode 尚未向 Actor 分派 BeginPlay；通过 FWorldDelegates::OnWorldBeginTearDown 在世界内 Actor 的 EndPlay 前关闭池，并在 Deinitialize 中做幂等兜底。所有回调必须保持 World 和子系统存活，不能在池转换中结束世界。

DemoGame 在 `ObjectPool/DemoCharacterPoolSubsystem.h` 提供 UDemoCharacterPoolSubsystem，由引擎在所有网络模式的 Game 和 PIE World 中自动创建。子系统直接持有 `TCharacterPool<ADemoCharacter, 200, EReferenceMode::WorldRaw>`。构造函数将 `/Game/Demo/Demo1_GrandWarfront/Characters/BP_DemoCharacter` 加载为 ADemoCharacter 子类，并通过反射类引用供 GC 和打包流程识别。权威端将该类传入现有 FCreateArgs::Class，在 PersistentLevel 中以 Identity Transform 预热 200 个停用的蓝图角色；类加载失败返回 CreationFailed，不回退到原生角色。客户端完成启动，但不创建本地权威池。子系统独占权威角色的销毁控制，在世界内 Actor teardown 前关闭池。容量、引用模式和原生基类是编译期选择，创建参数中的类决定全部槽位实际使用的子类；其他游戏通过自己的具体 World 子系统持有对应的池，GameplayRuntime 无需依赖游戏模块或 GAS。通用角色池的默认模式仍为 WorldWeak。

权威端玩法调用从 World 已经 BeginPlay 后开始。此前、客户端以及 teardown 开始后，借出和归还返回 NotReady，Get 返回空，NumFree 返回零；GetByIndexUnchecked 绕过这些检查，调用方必须保证权威池预热成功且生命期有效。GetPrewarmResult 保留启动结果；客户端启动成功不表示拥有本地角色槽位。IsReady 检查权威 World 与池存储是否可用，不表示全部角色已经完成 GAS 初始化；GAS 尚未提交时借出仍可能返回 ActivationFailed。预热失败会记录错误并回滚已创建角色，同一实例不会重试。调用方无需手动预热或关闭 Demo 池。

在每个初始角色位置放置 `Spawning/DemoInitialCharacterSpawnPoint.h` 中的 ADemoInitialCharacterSpawnPoint，或其蓝图子类。该类继承 ATargetPoint，使用生成点的世界 Transform，等待预热角色完成 BeginPlay、Pawn 初始化和 GAS 就绪提交后，再尝试借出一次。启动就绪状态与池存储就绪状态分别判断，启动完成后加载的生成点仍使用正常的单次租约检查。单机、监听服务器和独立服务器的权威端执行借出，客户端生成点不创建本地角色。每个成功的生成点激活一个已有 BP_DemoCharacter 实例。GetSpawnedCharacter 通过 generation 句柄解析当前租约，GetSpawnedHandle 提供供玩法归还使用的原生句柄。激活租约归 World 对象池所有；生成点结束时取消尚未执行的启动回调和就绪订阅，World 关闭时统一关闭租约。

UDemoCharacterPoolSubsystem 在世界启动时统计已经加载的初始生成点，数量超过 Capacity 时记录警告。任何生成点借不到槽位时也会记录警告并保持空置，包括后来加载的关卡生成点。等待阶段订阅首个未就绪角色的现有 Pawn Ready 通知，并以 0.05 秒间隔检查作为 GAS 在该就绪组外推进时的兜底；实际借出延迟到初始化回调之外执行，始终只尝试一次。等待最多持续十秒 World 时间，超时警告会给出待就绪角色的 Pawn 和 GAS 状态。实际激活失败时，角色先记录被拒绝的 GAS 或原生转换状态，生成点随后报告池错误码。流程不扩容、不补 Spawn、不反复尝试借出，也不启用 Actor Tick。生成点必须提供合法位置，激活流程不会搜索无阻挡的出生位置。

`Spawning/DemoRuntimeCharacterSpawnPoint.h` 中的 ADemoRuntimeCharacterSpawnPoint 是第二个可放置的 ATargetPoint 子类，当前只提供运行时生成扩展用的空壳，不借出角色，也不占用池容量。

以下示例在 DemoGame 的 World BeginPlay 后使用：

```cpp
#include "ObjectPool/DemoCharacterPoolSubsystem.h"

UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
if (!Pool || !Pool->IsReady())
{
    return;
}

auto Lease = Pool->TryAcquire(SpawnTransform);
if (!Lease)
{
    // Lease.Result.Error 区分满池、重入、对象丢失和激活失败。
    return;
}

const UDemoCharacterPoolSubsystem::FHandle Handle = Lease.Handle;
ADemoCharacter* Character = Lease.Object;
// Character 用于即时访问；跨帧通过 Pool->Get(Handle) 解析。
ADemoCharacter* SameCharacter = Pool->GetByIndexUnchecked(Handle.Index);
// 直接访问要求生命期有效，不会验证当前租约。
const auto ReturnResult = Pool->Release(Handle);
// 子系统在所属 World 开始 teardown 时自动关闭池。
```

Prewarm、Release、Shutdown 返回包含 EPoolError 的 FPoolResult，并支持布尔转换。TryAcquire 的失败原因使用 FLease::Result 中的同一结果模型。原来的布尔判断方式仍可使用。错误区分未就绪、重复预热、重入、创建失败、激活失败、满池、generation 耗尽、对象丢失和无效句柄。

共享 Helper 将已准备的角色隐藏、关闭碰撞和受伤、禁止自动 Controller。激活要求已经 BeginPlay，在无 sweep 状态下设置 Transform，恢复站姿和基础行走模拟，开启原生 Tick 与显示，最后打开碰撞。归还清理移动、跳跃、输入、累计力和待处理 Launch，关闭 Actor、移动组件和骨骼组件 Tick。

角色策略接受单机、监听服务器和独立服务器 World，生成在 PersistentLevel。客户端不能创建或借出角色，即使本地 Actor 的 Role 恰好是 ROLE_Authority。AI、GAS 重置、Montage、RootMotionSource、Cloth、Ragdoll、自定义组件、定时器和异步任务取消仍需要类型专属 Policy。普通 UObject 也需要合法的工厂、Outer 生命周期和原生重置契约；不能仅凭继承 UObject 就将资源或组件视为可复用对象。

## 网络生命周期

| 网络模式 | 池的归属与访问 |
| --- | --- |
| NM_Standalone | 本地权威端预热，使用同一套同步借还 API。 |
| NM_ListenServer | 服务器预热与借还，主机本地使用权威角色。 |
| NM_DedicatedServer | 无本地玩家和视口也能在服务器预热与借还。 |
| NM_Client | 接收服务器角色；Demo 子系统的本地借还返回 NotReady。 |

句柄包含本地池地址，不能复制到其他 World 或网络端。客户端请求仍通过游戏已有的权威请求路径提交；同步本地 TryAcquire 无法直接返回远程服务器租约。客户端角色是 World 持有的复制代理，不属于另一套本地槽位池。

Helper 在预热阶段只创建一次私有复制组件。Policy 在角色初始化后开启 Actor 与移动复制，将池角色保持为 AlwaysRelevant，并关闭仅 Owner 可见的相关性设置。空闲代理在租约之间继续存在；归还不会关闭复制或主动销毁客户端副本。这样以固定客户端角色数量和复制管理成本换取避免每次租约重新建立通道；默认保持 Awake，不自动对空闲对象启用 Dormancy。

组件复制一个单调递增且包含激活位的转换值。在两次网络更新之间完成归还再借出，即使最终激活位没有变化，转换值仍会变化。客户端在应用新状态前清理原生移动与预测数据，保留 UE 已接收的位置和移动模式。复制可以合并中间租约；转换值标识最新状态，不是逐次事件流。适配器不需要反射接口、GAS 或运行时类型注册表。

使用 Helper 的角色还需要在首次复制完成后绑定原生状态，ADemoCharacter 已接入：

```cpp
void AMyCharacter::PostNetInit()
{
    Nelaric::ObjectPool::FCharacterPoolHelper::PrepareForPool(*this, PoolState);
    Super::PostNetInit();
}
```

权威端的 PrepareForPool 仍用于延迟出生阶段；客户端调用时，将状态绑定到已接收的私有适配器，没有该适配器的普通出生角色保持原有行为。在 Super::PostNetInit 分派 BeginPlay 前完成绑定，使启动回调观察到池化停用状态。Actor BeginPlay 前收到初始状态时先停用，待 BeginPlay 完成后才应用最终激活状态；EndPlay 会取消延迟回调。客户端适配处理通用表现和移动，各类型仍负责自己的业务状态复制。

权威端激活、归还会先唤醒角色，再修改复制属性，完成后强制网络更新。租约期间修改网络所有权仍须经过控制协调器，复用前先完成控制权归还；清空移动预测不能替代撤销旧连接的所有权。自定义 Replication Graph、Iris Filter 或相关性覆盖逻辑应保留池代理的相关性，才能保证客户端实例持续存在。

## 扩展与重入

自定义 Policy 提供 FCreateArgs、FAcquireArgs，以及静态 Create、OnAcquire、OnReturn、Destroy。OnAcquire 返回 false 后，对象必须仍可交给不可失败的 OnReturn。原生回调及其触发的引擎回调必须保持池和当前转换对象存活，不能强制 GC。Destroy 只用于关闭池时的销毁路径。

同一池内嵌套修改返回 InTransition，转换中的 Get 返回空。停用完成后槽位才进入空闲链。Overlap 回调需要借出、归还或关闭池时，使用有界延迟命令，在当前转换结束后执行，并明确处理失败的归还结果。

现有角色保留自己的基类并实现 IPoolableCharacter，定义空闲状态如何撤销 Ready 绑定、取消初始化任务，以及再次借出时如何恢复；默认创建或转换契约不足时使用自定义 Policy。池租约 generation 与 Pawn 初始化 generation 保护不同契约，不会自动使彼此失效。具体血量、目标、AI、GAS 和网络语义属于接入游戏或可选集成。

## 验证

编译实际接入的模板实例，在引擎内验证满池、反复复用、旧句柄、激活失败、GC 保活、意外 Destroy、Overlap 重入与世界关闭。槽位管理、生命周期 Policy 和引擎工作分别计时。Development 构建可使用 `-trace=default,memory`，通过 Memory Insights 检查分配调用栈。
