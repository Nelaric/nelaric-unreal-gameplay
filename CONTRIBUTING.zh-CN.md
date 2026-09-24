<!-- Copyright (c) 2026 Nelaric -->

[English](CONTRIBUTING.md) | 简体中文

# 参与 Nelaric Unreal Gameplay 开发

感谢你参与 Nelaric Unreal Gameplay。我们欢迎使用英语或中文提交问题报告、设计反馈、文档改进和代码贡献。

`Development/` 下的本地验证工程不纳入 Git。提出新能力时，请说明开发者遇到的问题和预期的玩法制作流程。

## 创建 Issue 前

先搜索[现有 Issue](https://github.com/Nelaric/nelaric-unreal-gameplay/issues)，并阅读[项目概览](README.zh-CN.md)和[编码规范](Docs/CodingStandards/README.zh-CN.md)。通过 [Issue 选择页](https://github.com/Nelaric/nelaric-unreal-gameplay/issues/new/choose)选择合适的表单：

- **缺陷报告**：说明复现步骤、预期和实际行为，以及相关环境信息。涉及 Unreal Engine 时，请提供引擎版本、操作系统，以及问题发生在单机、监听服务器、独立服务器还是客户端。
- **能力提案**：说明开发者遇到的问题和使用场景、期望行为，以及能力应由哪一层负责。可以附上公开 API 草案，但不是必需的。
- **文档问题**：链接到对应页面或章节，并说明内容为何不清晰、不正确或缺失。

不属于上述类别的问题可以使用空白 Issue。每个 Issue 应聚焦一个问题或提案，不要在公开报告中包含私人信息。

## 项目范围

框架面向 Unreal Engine 5.6 及以上版本，在单机、监听服务器和独立服务器拓扑下提供可复用的对局流程、目标、计分、玩家生命周期和玩法组合能力。Core 定义通用机制；游戏提供具体规则与内容。玩法状态复制继续使用 Unreal Engine 的网络系统。已 Cook 内容的版本化交付与玩法执行分属不同能力。

提出改动时，请说明它属于 Core、可选集成、内容更新工具、开发工具还是游戏本身。具体玩法类型的规则与表现属于游戏或可选模块。Core 的新增能力应说明如何适用于不止一种玩法模式。

## 创建 Pull Request 前

贡献代码前，建议先创建 Issue，讨论改动是否必要、是否符合项目定位以及预期范围。提前达成共识有助于后续审查，也能提高 PR 被接受的可能性。小型修复和文档更正仍可直接提交 PR。

如果你不是本仓库成员，请先 Fork 仓库，在自己的 Fork 中创建分支，再从该分支向 `Nelaric/nelaric-unreal-gameplay:main` 发起 PR。

保持 PR 的主题集中。使用 [PR 模板](.github/pull_request_template.md)说明问题、方案、公开 API 或集成影响，以及验证方式。有相关 Issue 或能力提案时请关联；仅当 PR 完全解决该问题时才使用自动关闭关键词。纯文档改动的设计和兼容性影响、没有风险或规范例外的情况，以及没有特别审查提示的情况，可在对应栏目填写 `None`。如有审查重点，请指出相关文件、设计决定、疑问或取舍。申请项目规则例外时，请说明规则、影响范围、原因及考虑过的替代方案。

所有代码贡献都必须遵守 [Epic Games 的 Unreal Engine C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)和[项目编码规范](Docs/CodingStandards/README.zh-CN.md)。项目规范还规定了文本编码、本地检查，以及允许使用的 PR 标题、分支名和提交信息前缀。例如，分支名可用 `docs/clarify-contribution-guide`，PR 标题或提交标题可用 `docs: clarify contribution guide`。

按照[构建脚本、工具与审查规范](Docs/CodingStandards/BuildAndReview.zh-CN.md)运行与改动相关的检查。在 PR 中写明已运行的检查，以及因工具或环境限制未能运行的检查。

GitHub Actions 会检查命名、格式和 API 文档。无论 PR 来自 Fork 还是本仓库分支，CircleCI 都会在合并前用 Unreal Engine 5.6.1 在 Linux 上编译提交的插件代码。你不需要 CircleCI 账号，也无需额外配置。如果 Fork PR 需要修改 CI 工作流、自动化脚本、Unreal 构建脚本或插件描述文件，请先与维护者讨论，并通过源仓库分支提交。检查顺序和编译范围见[开发与 CI 流程](Docs/DevelopmentWorkflow.zh-CN.md)。CI 不强制测试覆盖率门槛。

PR 的目标分支是 `main`。合并前需满足仓库保护规则中的检查与审查要求。维护者会审查正确性、模块边界、API 契约、性能、安全性，以及与 Unreal Engine 5.6 及以上版本的兼容性。
