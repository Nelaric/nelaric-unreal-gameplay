<!-- Copyright (c) 2026 Nelaric -->

[English](Cpp.md) | 简体中文

# C++ 风格与头文件

必须同时遵守 [Epic C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)和下文的项目规则。若发现冲突，应创建 Issue 报告，不得自行选择其中一条。项目使用 clang-format 处理代码布局；提交 PR 前请运行锁定版本的格式化工具。

C++ 和 UE C# 构建脚本的块缩进使用制表符，制表符宽度为四个字符。非制表符字符后可以用空格对齐文本。这遵循 Epic 的缩进规则，由格式化工具配置负责执行。

## 命名与类型

- 遵循所有适用的 Unreal 类型前缀及命名规则。新的非反射公开 API 放在 `UE::Nelaric::` 命名空间下，必要时可增加领域子命名空间，例如 `UE::Nelaric::FSessionId` 和 `UE::Nelaric::ISessionProvider`。不要仅因类型位于该命名空间中，就在类型名里重复添加 `Nelaric`。Unreal Header Tool 不支持将反射类型放入命名空间，因此项目自有的全局反射类型应在 UE 前缀后包含 `Nelaric`，例如 `UNelaricServerSubsystem`。Unreal 要求的符号（例如模块入口）不受此限制。
- 模块和日志类别的命名应围绕 `Nelaric` 及所属领域保持一致。优先使用描述机制或职责的名称，而非某个游戏的具体规则。
- 遵循 Epic 对标准库的指导。优先使用 UE 容器和字符串；除互操作代码外，避免使用标准库容器和字符串。其他标准库设施可在 Epic 允许且效果更好时使用。稳定的跨模块公开 API 使用 UE 类型；不要在同一个 API 中混用 UE 与标准库约定。
- 优先使用有类型的常量和 `constexpr`，避免新增宏。按常规使用 UE 所要求的宏。新增项目宏必须说明原因，并遵循 Epic 的全大写 `UE_` 命名规则；跨模块功能开关必须集中定义。

## 头文件与依赖

- 公开头文件必须自包含：使用者不应依赖偶然的包含顺序才能单独包含它。包含声明真正需要的头文件，足够时使用前向声明。
- 仅供实现使用的包含和声明应放在 `Private`。Core 的公开头文件不得包含具体 Provider 或厂商 SDK 的头文件。
- 尽量减少头文件依赖。不要仅为获取可前向声明的类型而包含宽泛的头文件。
- 反射声明必须遵守 Unreal Header Tool 的要求，包括生成头文件的位置。这些要求优先于机械式的包含排序。

## 源文件文本格式

代码和人工编写的文档使用带 BOM 的 UTF-8 与 CRLF。为保证互操作性，工具配置和可执行脚本可按要求使用不带 BOM 的 UTF-8 与 LF；具体例外见[构建脚本与工具规范](BuildAndReview.zh-CN.md)。同一文件中不得混用换行风格。
