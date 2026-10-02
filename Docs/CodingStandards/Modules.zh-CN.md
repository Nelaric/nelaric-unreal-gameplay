<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](Modules.md) | 简体中文

# 模块与依赖边界

插件由多个模块组成，每个模块包含 `Public` 和 `Private` 目录。公开头文件定义契约；私有实现细节不得泄漏到公开头文件中。

- GameplayRuntime 模块不得依赖厂商 SDK、在线后端、内容分发 Provider 或可选玩法集成。可选模块可以依赖 GameplayRuntime。禁止模块之间循环依赖。
- `.Build.cs` 中的公开依赖必须对应公开头文件真正需要的类型。实现依赖应放入私有依赖。不要仅为修复另一模块的缺失包含而把依赖改为公开。
- 非反射的玩法扩展契约应使用 `Nelaric::` 命名空间下的 C++ 接口。在玩法作者需要时提供 Blueprint 入口；不要强制每个实现都使用反射。
- GameplayRuntime 必须与具体玩法类型无关。GameplayRuntime 承载通用的规则、状态、玩家生命周期与活动组合契约；对局流程、目标和计分由可选模块按需提供，并非每种模式的前置条件。具体战斗、背包、任务和角色规则属于接入游戏或可选扩展。
- 玩法状态复制使用 UE 原生网络机制。GameplayRuntime 不实现第二套传输系统，也不要求单机玩法接入后端。
- 内容版本和更新交付机制与对局规则分离。GameplayRuntime 可以使用经校验的内容版本，但不依赖特定补丁服务。

引入新模块时，应在模块文档中说明其职责和直接依赖。新增跨越上述边界的依赖时，必须在 PR 中解释并交由维护者审查。

## 插件与模块结构

框架使用一个项目自有插件 `NelaricGameplay`，位于 `NelaricGameplay/Plugins/NelaricGameplay/`。插件模块承载 Runtime、可选 GAS 集成、Diagnostics、Benchmark 和 Editor 职责。新增框架能力以该插件内的模块组织，并明确记录职责和直接依赖。PuerTS 等第三方插件单独保留，沿用其自身名称和版权归属。

| 职责 | 插件 `Source/` 下的模块目录 | 模块类型 |
| --- | --- | --- |
| Runtime | `GameplayRuntime/` | `Runtime` |
| GAS 集成 | `GameplayAbilitiesIntegration/` | `Runtime` |
| Diagnostics（性能分析） | `Diagnostics/` | `Runtime` |
| Benchmark | `Benchmark/` | `DeveloperTool` |
| Editor | `Editor/` | `Editor` |

`GameplayRuntime` 提供共用的玩法运行时，包括可复用框架 Actor、初始化与生命周期协调、世界启动配置和会话切换。`Diagnostics` 负责性能分析支持；`Benchmark` 负责可复现的性能场景和结果比较；`Editor` 负责仅在编辑器使用的制作工具。模块命名遵循 [C++ 命名与类型](Cpp.zh-CN.md#命名与类型)中的可选项目前缀规则。

`Diagnostics` 在启动时注册 `ng.Perf.HUD`，关闭时注销。默认值为 `0`（隐藏），非零值显示全英文 Slate HUD，包含 FPS、Frame、GT、RT 和 GPU 耗时。HUD 覆盖游戏视口与当前活动的关卡编辑器视口，退出 PIE 后保持开启，且不拦截输入。指标为引擎进程级数据；GPU 耗时使用主渲染 GPU，其显卡名称从 RHI 读取，不可用的指标显示 `N/A`。`UFrameworkPerformanceSubsystem` 提供初始的 Game Instance 子系统类型，暂未定义性能操作；HUD 仍由模块持有，以便编辑器覆盖层在 PIE 结束后继续显示。其公开基类要求所有目标公开依赖 `Core`、`CoreUObject` 和 `Engine`。客户端构建额外私有依赖 `Slate`、`SlateCore`、`RenderCore` 和 `RHI`；编辑器构建额外私有依赖 `LevelEditor` 和 `UnrealEd`。服务器构建不创建 HUD。`Benchmark` 仍仅包含生命周期骨架并私有依赖 `Core`，尚未实现基准执行。`Editor` 还承载 Pawn 初始化的外部消费者和网络事件自动化测试，私有依赖 `Core`、`CoreUObject`、`Engine`、`GameplayRuntime` 和 `UnrealEd`，用于通过运行时 Public 契约进行跨模块编译和真实 PIE 生命周期验证；这些依赖不会反向引入 GameplayRuntime。编辑器制作工具尚未实现。实现需要跨模块调用时再声明相应依赖；GameplayRuntime 必须保持独立，不依赖这些可选工具。`DeveloperTool` 模块仅在目标构建开发工具时可用；`Editor` 模块仅在编辑器目标中可用。

GameplayRuntime 的 AI 目录还提供 Game AI StateTree Schema、执行组件、C++ 与 Blueprint 节点扩展基类和 World 管理的 AI 上下文 Subsystem。公开 StateTree 契约增加 StateTreeModule、GameplayStateTreeModule 与 AIModule 公开依赖。插件声明 GameplayStateTree 依赖，由它传递启用 StateTree。具体行为、感知、导航策略和游戏服务由接入方实现。参见 [API 使用说明](../API/README.zh-CN.md#gameai-statetree)。

`GameplayAbilitiesIntegration` 提供参与者 PlayerState ASC、统一能力输入、角色状态托管和控制状态迁移。公开依赖为 Core、CoreUObject、Engine、GameplayRuntime、GameplayAbilities、GameplayTags 和 GameplayTasks，效果复制私有依赖 NetCore。插件启用 GameplayAbilities。使用方按需选择该模块，GameplayRuntime 不反向依赖；具体 AttributeSet 和游戏能力仍留在游戏模块。参见[控制切换中的 GAS 状态](../API/GAS/ControlState.zh-CN.md)。

## 内部集成约定

GameplayRuntime 公开头文件中供模块内部调用的 C++ 集成方法可以接收 `const Nelaric::FGameplayRuntimeInternalAccessKey&`。Key 和 `FGameplayRuntimeInternalAccess::Key()` 位于 `GameplayRuntime/Private/Internal/`，仅供该模块的实现使用。此类方法放在框架集成用的第二个 `public:` 区域，以名称标明内部用途；Key 不参与反射，因此不要把它作为 `UFUNCTION` 参数。不要只为取得 Key 而引入继承。

Key 用于标记 GameplayRuntime 模块内的实现调用，不是授权边界。公开头文件可在 C++ 集成方法签名中前置声明 `Nelaric::FGameplayRuntimeInternalAccessKey`，但不得包含私有定义，也不得向玩法模块提供取得 Key 的入口。调用方应位于所属模块的 `Private` 目录。框架使用者应使用受支持的玩法公开 API。未来若需跨模块集成，应单独设计并记录公开契约及其模块依赖。
