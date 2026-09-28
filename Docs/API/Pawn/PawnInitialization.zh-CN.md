<!-- Copyright (c) 2026 Nelaric -->

[English](PawnInitialization.md) | 简体中文

# Pawn 组件初始化

## Pawn 基础类与上下文

`ANelaricPawn` 保留 Unreal 的 `APawn` 默认行为；`ANelaricCharacter` 保留 `ACharacter` 的胶囊体、网格和移动组件。两者均可通过 Blueprint 继承，并创建名为 `PawnInitializationComponent` 的默认子对象。

`UNelaricPawnComponent` 查询直接所属的 Pawn、当前控制器、玩家控制器和玩家状态。查询返回非拥有指针，读取当前本地状态且不缓存。玩家状态直接从 Pawn 读取，因此可以在没有本地控制器时可用；对象缺失、无效或正在销毁时返回空指针。默认关闭 Tick 和组件复制。`UNelaricGameplayComponent` 将这一基础类提供为可在 Blueprint 中添加的组件。

## 配置资产

`UNelaricPawnInitializationConfig` 是包含 `FNelaricPawnInitializationEntry` 条目的 `UDataAsset`。在 Pawn 或 Character 派生 Blueprint 中，将其赋给初始化组件的 `InitializationConfig` 属性。

| 字段 | 含义 |
| --- | --- |
| `ComponentId` | 稳定且唯一的 ID，用于实例命名和依赖引用。 |
| `ComponentClass` | 实现 `INelaricInitStateParticipantInterface` 的具体 Actor 组件类。 |
| `bCreateOnAuthority` | 在权威端创建，包括单机；默认为 `true`。 |
| `bCreateOnClient` | 在非权威端创建；默认为 `true`。 |
| `bRequiredForPawnReady` | 纳入本地 Pawn 就绪判定；默认为 `true`。 |
| `DependencyIds` | 进入 Ready 所依赖的本地参与组件 ID。 |

创建组件前，管理组件检查 ID、组件类、依赖声明、创建端兼容性，以及与已有实例名或已配置 ID 的冲突。无效配置会阻止 Pawn 就绪。所选实例使用 `NelaricInit_<ComponentId>` 命名；管理组件先创建完整的本地组件集合，解析依赖图并配置 World 子系统，再注册这些实例。

权威端和客户端各自创建所选实例。受管理的动态实例关闭复制，初始化状态和代次均保留在本地。玩法数据通过独立的 Unreal 复制路径传输，所需数据到达后再请求本地刷新。

## 组件状态与依赖

各参与组件拥有自己的状态、代次和终态失败标记。`UNelaricInitStateWorldSubsystem` 保留弱引用注册记录，并在游戏线程协调推进。

| 状态 | 含义 |
| --- | --- |
| `Registered` | 参与组件已注册，可以取得自身数据。 |
| `DataAvailable` | 初始化所需的本地数据已可用。 |
| `DataInitialized` | 本地数据已初始化；进入 Ready 前还需最终准备和依赖检查。 |
| `Ready` | 可执行组件玩法；依赖完整 Pawn 的工作还需等待 Pawn 就绪。 |

`UNelaricPawnInitStateComponent` 是实现参与接口的可选 C++ 基础类。通过覆写 `CanEntryDataAvailable()`、`CanEntryDataInitialized()` 和 `CanEntryReady()` 执行可重试的本地准备；这些检查默认返回 `true`。在 `OnInitReady()` 中启动组件玩法。等待中的检查可在 `RequestInitRefresh()` 后重试，例如 `OnRep` 更新了所需玩法数据之后。

依赖约束进入 Ready 的时机。循环组外的依赖必须已经 Ready；存在循环依赖的参与组件组成就绪组。组内所有成员到达 DataInitialized 并通过本地准备后，协调器先将全组提交为 Ready，再逐一通知。成功的 `CanEntryReady()` 准备结果保留至本代次结束。

## Pawn 就绪与清理

初始化组件在 BeginPlay 尝试就绪。Pawn 就绪要求所属 Pawn 有效、配置有效、本地必需参与组件全部 Ready，且 `CanInitializePawn()` 通过。该 BlueprintNativeEvent 默认接受有效的所属 Pawn。上下文变化后可调用 `TryInitializePawn()`，通过 `IsPawnInitialized()` 查询当前条件。配置为空时不创建组件，使用 Pawn 上下文检查判定就绪。

`OnPawnInitialized` 在每次进入 Ready 时广播；在 BeginPlay 前绑定可观察立即就绪的情况。`OnPawnInitializationRevoked` 在撤销就绪时广播。必需参与组件失效、替换配置和销毁流程都可能撤销 Pawn 就绪。运行期间通过 `SetInitializationConfig()` 换用不同配置，会撤销就绪、销毁受管理实例、创建新集合并重新尝试就绪。EndPlay 和 World 销毁会停止初始化并销毁受管理实例。

`InvalidateInitGeneration()` 先增加代次，再执行失效钩子和工作取消，随后将组件重置为 Registered 并清除终态失败。当 Ready 依赖失效或失败时，协调器使依赖它的已配置组件失效。`MarkTerminalInitFailure()` 在有序状态之外记录失败，并停止该次尝试的推进。

异步结果应保留组件弱引用以及启动时的 World 和代次。回到游戏线程后，先通过 `ResolveInitResult()` 或 `CanApplyInitResult()` 检查再应用结果；无效、未注册、正在离开、已失败、代次不符的组件，以及不同 World 均被拒绝。派生组件通过 `OnInitGenerationInvalidated()` 和 `CancelInitGenerationWork()` 释放监听和进行中的工作。
