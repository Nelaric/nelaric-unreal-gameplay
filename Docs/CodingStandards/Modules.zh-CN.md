<!-- Copyright (c) 2026 Nelaric -->

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

框架使用一个项目自有插件 `NelaricGameplay`，位于 `NelaricGameplay/Plugins/NelaricGameplay/`。插件中的四个模块分别对应 Runtime、Diagnostics、Benchmark 和 Editor 职责。新增框架能力以该插件内的模块组织，并明确记录职责和直接依赖。PuerTS 等第三方插件单独保留，沿用其自身名称和版权归属。

| 职责 | 插件 `Source/` 下的模块目录 | 模块类型 |
| --- | --- | --- |
| Runtime | `GameplayRuntime/` | `Runtime` |
| Diagnostics（性能分析） | `Diagnostics/` | `Runtime` |
| Benchmark | `Benchmark/` | `DeveloperTool` |
| Editor | `Editor/` | `Editor` |

`GameplayRuntime` 提供共用的玩法运行时，包括可复用框架 Actor、初始化与生命周期协调、世界启动配置和会话切换。`Diagnostics` 负责性能分析支持；`Benchmark` 负责可复现的性能场景和结果比较；`Editor` 负责仅在编辑器使用的制作工具。模块命名遵循 [C++ 命名与类型](Cpp.zh-CN.md#命名与类型)中的可选项目前缀规则。

`Diagnostics`、`Benchmark` 和 `Editor` 当前仅包含模块生命周期骨架，分别私有依赖 `Core`，尚未实现性能采集、基准执行或编辑器界面。实现需要跨模块调用时再声明相应依赖；GameplayRuntime 必须保持独立，不依赖这些可选工具。`DeveloperTool` 模块仅在目标构建开发工具时可用；`Editor` 模块仅在编辑器目标中可用。

## 内部集成约定

GameplayRuntime 公开头文件中供模块内部调用的 C++ 集成方法可以接收 `const Nelaric::FGameplayRuntimeInternalAccessKey&`。Key 和 `FGameplayRuntimeInternalAccess::Key()` 位于 `GameplayRuntime/Private/Internal/`，仅供该模块的实现使用。此类方法放在框架集成用的第二个 `public:` 区域，以名称标明内部用途；Key 不参与反射，因此不要把它作为 `UFUNCTION` 参数。不要只为取得 Key 而引入继承。

Key 用于标记 GameplayRuntime 模块内的实现调用，不是授权边界。公开头文件可在 C++ 集成方法签名中前置声明 `Nelaric::FGameplayRuntimeInternalAccessKey`，但不得包含私有定义，也不得向玩法模块提供取得 Key 的入口。调用方应位于所属模块的 `Private` 目录。框架使用者应使用受支持的玩法公开 API。未来若需跨模块集成，应单独设计并记录公开契约及其模块依赖。
