<!-- Copyright (c) 2026 Nelaric -->

[English](DevelopmentWorkflow.md) | 简体中文

# 开发与 CI 流程

本文说明 PR 上会出现哪些检查。准备贡献时，请先阅读[贡献指南](../CONTRIBUTING.zh-CN.md)。

## 从改动到 `main`

1. 在分支或 Fork 中完成聚焦的改动，运行相关的[本地检查](CodingStandards/BuildAndReview.zh-CN.md#本地检查)。
2. 向 `main` 发起 PR。创建或更新 PR 后，自动检查会开始运行。
3. 如果检查失败，修复问题后等待检查通过和维护者审查，再合并 PR。
4. 合并后，项目会从 `main` 发布 API 文档网站。验证和插件编译已在 PR 上完成。

## PR 会运行哪些检查

GitHub Actions 会检查 PR 和提交命名、格式与文本规范，以及 API 文档。无论 PR 来自 Fork 还是本仓库，CircleCI 都会用 Unreal Engine 5.6.1 在 Linux 上编译准确的 PR 提交。结果会以 `ci/fork-pr-linux-build` 状态显示在 PR 上。你不需要 CircleCI 账号，也无需额外配置。

这四项检查状态都是合并到 `main` 的必需条件，均在合并前的 PR 上运行。

对于来自 Fork 的 PR，自动编译不会接受对 CI 工作流、自动化脚本、Unreal 构建脚本或插件描述文件的改动。如果需要修改这些文件，请先与维护者讨论，并在源仓库的分支中提交。来自源仓库分支的 PR 即使修改这些文件，也会运行 Linux 编译。

## Linux 编译的范围

CircleCI 在 Linux 上使用 Unreal Engine 5.6.1 编译并打包 `Unreal-Plugins/NelaricServer/NelaricServer.uplugin`，以验证仅含源码的插件。该作业不构建游戏或 Dedicated Server 目标，也不发布可下载的打包产物。按当前设计，作业每次运行都会重新下载引擎镜像，且不使用编译缓存。

项目支持 UE 5.6 及以上版本，但 CI 目前仅验证 Linux 上的 UE 5.6.1。对最新支持版本的验证仍是[构建覆盖缺口](CodingStandards/BuildAndReview.zh-CN.md#unreal-构建脚本)。现阶段不设自动化测试覆盖率要求。

## 检查失败时

从 PR 页面打开失败的检查。Linux 编译状态会链接到对应的 [CircleCI 流水线](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server)，可以查看失败的步骤。如果某项检查没有出现，或你无法解决失败问题，请在 PR 中附上链接和已经尝试过的排查步骤。

工作流配置分别位于 [GitHub Actions Quality](../.github/workflows/quality.yml)、[Fork PR Linux build](../.github/workflows/fork-pr-linux.yml)、[API Pages](../.github/workflows/pages.yml) 和 [CircleCI](../.circleci/config.yml)。
