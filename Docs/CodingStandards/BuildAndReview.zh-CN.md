<!-- Copyright (c) 2026 Nelaric -->

[English](BuildAndReview.md) | 简体中文

# 构建脚本、工具与审查

## 版权声明

只要文件格式允许，所有项目自有文件都必须包含 Nelaric 版权声明。对于支持注释的文本文件，版权声明必须是第一个逻辑行，位于标题、代码、配置项或其他正文之前。UTF-8 BOM 不算一行。使用 `Copyright (c) <year> Nelaric`，其中 `<year>` 为文件首次发布的年份；不要仅因修改文件就更新年份。

使用文件格式原生的注释语法：

| 文件格式 | 首行示例 |
| --- | --- |
| C++、C# 和 JavaScript 源码 | `// Copyright (c) 2026 Nelaric` |
| Markdown 和 HTML | `<!-- Copyright (c) 2026 Nelaric -->` |
| Python、Shell、YAML 和使用 `#` 注释的配置 | `# Copyright (c) 2026 Nelaric` |

若可执行脚本需要 shebang，则将 `#!...` 保留在第一行，版权声明紧接在第二行。若某格式要求其他首行指令，则把声明放在第一个合法的注释位置。不要向严格 JSON 或其他不允许注释的格式添加注释。不要在 `LICENSE` 前添加注释或修改许可文本；其中现有的版权行是权威声明。二进制资源、生成文件和未修改的第三方文件保留各自适用的归属说明，不要求文件内的 Nelaric 声明。对于无法在文件内写入声明的项目自有文件，应通过仓库许可证或邻近的声明文件记录所有权，而不是破坏文件格式。

此规则适用于所有新增的项目自有文件，以及修改时的现有项目自有文件。审查者必须确认版权声明或有文档依据的格式例外。保留现有第三方版权声明；不得对 Nelaric 不拥有的代码或资源主张版权。

## Unreal 构建脚本

- 对 `.Build.cs` 和 `.Target.cs` 运行 CSharpier。它与 C++ 的 clang-format 一样属于强制要求。
- 模块依赖应保持最少，并依据公开头文件的实际需要区分公开与私有依赖。不要在 Foundation 中加入厂商 SDK 依赖。
- 面向 UE 5.6 及以上版本。引擎编译 CI 应同时覆盖最低支持版本和最新支持版本。CircleCI 在 PR 合并前使用 UE 5.6.1 编译 Linux 上的 Game、Editor 和 Server Target；最新支持版本仍是覆盖缺口。详见 `Docs/DevelopmentWorkflow.zh-CN.md`。

## 自动化测试

框架的每项独立功能都必须有自动化测试，验证其对外可观察的行为。新增功能或改变行为时，应同步新增或更新测试；影响契约的失败路径和边界情况也应覆盖。框架自有的 Unreal 自动化测试应使用 `Nelaric.*` 层级命名，以便 CI 选中。按功能将相关场景组织成少量聚焦的测试，不要为每个方法、分支或细微变化单独建一个测试，也不要在同一模块中堆积大量重复用例。不以测试数量或代码覆盖率为目标；审查者应判断关键行为与风险是否得到验证。

## 本地检查

使用 `.github/workflows/quality.yml` 和 `.config/dotnet-tools.json` 中锁定的版本：

1. `python Scripts/check_text.py` 检查文本编码和换行符。
2. `python Scripts/check_format.py` 检查 C++ 和 UE 构建脚本格式。运行前需安装锁定版本的 clang-format，并恢复本地 .NET 工具清单。
3. `python Scripts/check_public_docs.py` 检查每个公开头文件是否有 `@file` 注释，并检查各类第一个 `public:` 区域的方法是否有 Doxygen 注释；若第一段与第二段 `public:` 之间出现其他访问说明符，则检查失败。第二段不检查方法注释。
4. `python Scripts/check_doxygen_style.py` 检查正文 25 与 75 字符上限、单行及多行格式、多行注释每段开头的标签，以及枚举注释格式与对齐的一致性。
5. `python Scripts/run_doxygen.py` 构建 API 网站，并在 Doxygen 警告出现时失败。运行前需安装锁定版本的 Doxygen。

请求审查前，请先运行与改动相关的本地检查。PR 必须在合并前通过格式、API 文档、PR 命名和 Linux 项目编译状态。CircleCI 会编译提交的项目代码，包括来自本仓库分支的 PR。来自 Fork 的 PR 如需修改 CI 工作流、自动化脚本、Unreal 构建脚本或插件描述文件，请由维护者在源仓库分支处理。现阶段不要求 clang-tidy 或数值化的代码覆盖率门槛。

## 提交与分支命名

新提交和工作分支只能使用以下前缀：

| 前缀 | 用途 |
| --- | --- |
| `feat` | 增加能力。 |
| `fix` | 修复缺陷。 |
| `docs` | 仅修改文档。 |
| `style` | 仅调整格式，不改变行为。 |
| `refactor` | 重构代码，不改变行为。 |
| `perf` | 改进性能。 |
| `test` | 新增或修改测试。 |
| `build` | 修改构建配置或依赖。 |
| `ci` | 修改自动化和 CI 工作流。 |
| `chore` | 执行不属于以上类别的仓库维护。 |
| `revert` | 撤销先前的改动。 |

提交标题必须使用 `<prefix>: <short English summary>` 或 `<prefix>(<scope>): <short English summary>`。若包含 scope，必须使用小写；英文摘要应以动词开头。Squash 合并时的提交标题也必须遵守同一规则。例如：`feat(session): add reservation support` 和 `docs: clarify provider boundaries`。

PR 标题必须采用与提交标题相同的格式。CI 会检查 PR 标题、源分支名称以及 PR 中每个提交的标题，验证允许的前缀、格式和以小写英文字母开头的英文摘要；摘要首词是否为动词、能否准确描述改动，由审查者确认。

工作分支必须使用 `<prefix>/<lowercase-kebab-case-description>`，例如 `fix/admission-timeout`。前缀必须来自上表；保留的默认分支 `main` 是唯一例外。若需新增前缀，必须先更新本规范。

## 文本文件例外

人工编写的 Markdown 与 LICENSE、C++ 源码和头文件，以及 UE C# 构建脚本使用带 BOM 的 UTF-8 与 CRLF。为兼容工具，.gitattributes、.editorconfig、.clang-format、.csharpierrc、.gitignore、.json、.yml、.yaml、Doxyfile 和可执行 .py 脚本使用不带 BOM 的 UTF-8 与 LF。检查脚本会强制执行这些规则。

## 审查

审查时应关注正确性、版权声明、模块边界、公开 API 文档的准确性、取消与失败行为、所有权、性能、安全性，以及每项独立框架功能的自动化测试。CI 检查 Doxygen 注释的机械格式；审查者判断注释是否符合实际契约，以及测试是否覆盖关键行为而没有不必要的重复。申请项目指导性规则例外的 PR 必须说明规则、原因、受影响代码和替代方案，并获得维护者批准；例外不得覆盖 Epic 的要求，也不能单独解决两套规范之间的冲突。此类冲突应通过 Issue 报告。当前 CI 通过即可满足自动检查门槛，但不能代替对必需测试的审查。

审查者必须依据 [UCLASS 导出规则](Cpp.zh-CN.md#uclass-导出)，确认所有项目自有 `UCLASS` 均显式使用 `MinimalAPI`，类声明上没有 `*_API` 宏，且仅对跨模块所需的方法单独导出。
