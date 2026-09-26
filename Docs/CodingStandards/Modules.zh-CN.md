<!-- Copyright (c) 2026 Nelaric -->

[English](Modules.md) | 简体中文

# 模块与依赖边界

插件由多个模块组成，每个模块包含 `Public` 和 `Private` 目录。公开头文件定义契约；私有实现细节不得泄漏到公开头文件中。

- Foundation 模块不得依赖厂商 SDK、在线后端、内容分发 Provider 或可选玩法集成。可选模块可以依赖 Foundation。禁止模块之间循环依赖。
- `.Build.cs` 中的公开依赖必须对应公开头文件真正需要的类型。实现依赖应放入私有依赖。不要仅为修复另一模块的缺失包含而把依赖改为公开。
- 非反射的玩法扩展契约应使用 `Nelaric::` 命名空间下的 C++ 接口。在玩法作者需要时提供 Blueprint 入口；不要强制每个实现都使用反射。
- Foundation 必须与具体玩法类型无关。Foundation 承载通用的规则、状态、玩家生命周期与活动组合契约；对局流程、目标和计分由可选模块按需提供，并非每种模式的前置条件。具体战斗、背包、任务和角色规则属于接入游戏或可选扩展。
- 玩法状态复制使用 UE 原生网络机制。Foundation 不实现第二套传输系统，也不要求单机玩法接入后端。
- 内容版本和更新交付机制与对局规则分离。Foundation 可以使用经校验的内容版本，但不依赖特定补丁服务。

引入新模块时，应在模块文档中说明其职责和直接依赖。新增跨越上述边界的依赖时，必须在 PR 中解释并交由维护者审查。

## 内部集成约定

Foundation 公开头文件中供模块内部调用的 C++ 集成方法可以接收 `const Nelaric::FFoundationInternalAccessKey&`。Key 和 `FFoundationInternalAccess::Key()` 位于 `NelaricFoundation/Private/Internal/`，仅供该模块的实现使用。此类方法放在框架集成用的第二个 `public:` 区域，以名称标明内部用途；Key 不参与反射，因此不要把它作为 `UFUNCTION` 参数。不要只为取得 Key 而引入继承。

Key 用于标记 Foundation 模块内的实现调用，不是授权边界。公开头文件可在 C++ 集成方法签名中前置声明 `Nelaric::FFoundationInternalAccessKey`，但不得包含私有定义，也不得向玩法模块提供取得 Key 的入口。调用方应位于所属模块的 `Private` 目录。框架使用者应使用受支持的玩法公开 API。未来若需跨模块集成，应单独设计并记录公开契约及其模块依赖。
