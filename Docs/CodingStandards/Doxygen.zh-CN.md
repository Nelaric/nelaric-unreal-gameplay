<!-- Copyright (c) 2026 Nelaric -->

[English](Doxygen.md) | 简体中文

# Doxygen 注释规范

本规范适用于项目自有的 C++ 声明及其 API 文档，并与 [Epic C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)和[公开 API 契约规范](Api.zh-CN.md)共同使用。公开 API 的代码注释使用英文，向 Blueprint 暴露的声明也一样。

## 目的与覆盖范围

注释解释意图和可观察行为，不复述声明文字或实现步骤。命名应清晰；契约变化时同步更新注释。

- **必须**为所有公开类型、第一个 `public:` 区域中面向玩法的方法、枚举及枚举值、常量、玩法扩展契约和 Blueprint 入口编写文档注释。公开属性与字段（包括反射属性）也必须有注释。派生类型依赖的受保护声明应按其契约编写注释。第二个 `public:` 区域中供框架集成或 Unreal 生命周期调用的方法不要求 Doxygen 注释，两段之间不得出现其他访问说明符。
- **应该**只在意图或不变量无法从代码中看清时，为私有声明编写 Doxygen 注释。局部实现细节如需解释，使用普通 `//` 注释；它们不属于 API 文档。
- **必须**每行只声明一个变量，以便分别说明其含义。除下述变量和枚举值的短尾随注释外，Doxygen 注释应紧挨声明并放在声明前。公开声明的契约只写一次，不在定义处重复。
- **必须**在每个公开头文件的版权声明后放置 Doxygen `@file` 注释，使文件级声明进入生成文档。

## 格式与单行长度

按所需说明长度选择形式：

| 形式 | 规则 |
| --- | --- |
| 变量或枚举值后的 `///<` | 单行正文最多 25 个字符。同一枚举内所有 `///<` 的首个 `/` 必须在同一视觉列。若任一枚举值的说明超过 25 个字符，该枚举的所有值都改为前置注释。 |
| 声明前一行的 `///` | 单行正文最多 75 个字符；可以不使用 `@` 标签。 |
| 声明前的 `/** ... */` | 无法在 75 字符的一行内说明时使用。每段都以 Doxygen `@` 标签开头：首段通常用 `@brief`，后续使用 `@details`、`@par` 或专门的标签；不得用多行无标签的 `///` 代替。 |

所有形式均将空格与 Doxygen 标签计入正文长度，缩进、注释标记及其分隔空格不计入。每个物理行的正文最多 75 个字符；`///<` 另有更严格的 25 字符上限。对齐枚举注释时，制表符按四列制表位计算。注释中的空行分隔段落；同一段内的换行无需重复标签。每个标签单独占一行，命令统一使用 `@`，例如 `@file`、`@param` 和 `@return`。紧挨声明的注释不需要 `@class`、`@fn` 或 `@var`。[CI 风格检查](BuildAndReview.zh-CN.md#本地检查)会强制执行这些机械规则。

下例中的注释正文恰好是 75 个字符：

```cpp
/// The world keeps the settings chosen by its level until the map is unloaded.
```

下面这句话恰好有 80 个字符，写在单行会超出上限：

```text
The map retains the world settings chosen by its level until this world unloads.
```

在单词边界处换行，保留原句含义：

```cpp
/** @brief The map retains the world settings chosen by its level
 * until this world unloads.
 */
```

## 注释应说明的内容

| 声明 | 适用时应说明的内容 |
| --- | --- |
| 类或结构体 | 解决的问题、职责，以及调用方如何使用。 |
| 方法或函数 | 用途、允许调用的线程、回调线程、前置条件、可观察的副作用与失败行为。 |
| 参数 | 含义、单位、有效范围、特殊值，以及输入或输出方向。 |
| 返回值 | 结果和状态值的含义。简明结果已在用途说明中解释时，可以省略 `@return`。 |
| 属性、字段或常量 | 含义、单位、范围、特殊值、所有权或生命周期。 |
| 枚举及枚举值 | 所表示的状态或结果，以及每个值的含义。 |
| 异步操作 | 取消句柄、终态回调、超时和失败语义，以及取消发生得太晚时的行为。 |

## 常用 Doxygen 标签

下列标签仅在能够补充契约信息时使用，不要求每条注释都包含所有标签。

| 标签 | 用途 |
| --- | --- |
| `@file FileName.h` | 标识公开头文件，使文件级声明进入 API 文档。 |
| `@brief Description` | 给出多行注释的简短摘要。 |
| `@details Description` | 在简述后开启详细说明段落。 |
| `@par [Title]` | 另起说明段落，可附标题；正文写在下一行。 |
| `@param Name Description` | 说明输入参数；名称必须与声明一致。 |
| `@param[out] Name Description` | 说明输出参数。 |
| `@param[in,out] Name Description` | 说明函数既读取又修改的参数。 |
| `@tparam Name Description` | 说明作用或约束不明显的模板参数。 |
| `@return Description` | 用途说明尚未讲清结果含义时说明返回值；`void` 函数不使用。 |
| `@pre Description` / `@post Description` | 说明调用前提或调用后的可观察保证。 |
| `@note Description` | 补充主要描述之外、对使用者有帮助的细节。 |
| `@warning Description` | 提醒可能导致错误使用或数据丢失的情况。 |
| `@see Reference` | 链接直接相关的类型或操作。 |
| `@deprecated Description` | 说明接口已弃用，并指明替代接口或迁移方式。 |

每个标签单独占一个注释行。不要添加空标签，也不要用名称本身充当说明。标签说明较长时，在后续行接着写，使每行注释正文仍符合 75 字符上限。

```cpp
/** @brief Returns whether an activity may start with this player count.
 *
 * @details Call on the game thread. A count below the minimum fails
 * the readiness check; a minimum of zero disables that check.
 *
 * @param PlayerCount Number of admitted players; must be non-negative.
 */
bool CanStartActivity(int32 PlayerCount) const;

/// Maximum admitted players; zero leaves the maximum unrestricted.
int32 MaxPlayers = 0;

enum class ETaskResult
{
    Success,          ///< Task completed.
    RetryableFailure, ///< Retry later.
};
```

只要有一个枚举值的说明超过 25 个字符，就为所有枚举值使用前置注释。单个枚举值需要超过 75 个字符时，该值依照通用规则使用带标签的 `/** ... */` 文档块。

```cpp
enum class ETaskResult
{
    /// Task completed.
    Success,

    /// Task failed but can be retried after a short delay.
    RetryableFailure,
};
```

## 审查

CI 检查公开头文件的 `@file` 注释、Doxygen 警告和机械格式。审查者需检查两段式布局，并判断英文注释是否准确描述玩法契约，包括相关的所有权与运行时行为。运行改动所需的[文档与文本检查](BuildAndReview.zh-CN.md#本地检查)。Doxygen 生成过程没有警告，不能代替内容审查。
