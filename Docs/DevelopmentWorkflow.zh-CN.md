<!-- Copyright (c) 2026 Nelaric -->

[English](DevelopmentWorkflow.md) | 简体中文

# 开发与 CI 流程

本文说明 PR 上会出现哪些检查。准备贡献时，请先阅读[贡献指南](../CONTRIBUTING.zh-CN.md)。

## 从改动到 `main`

1. 在分支或 Fork 中完成聚焦的改动，运行相关的[本地检查](CodingStandards/BuildAndReview.zh-CN.md#本地检查)。
2. 向 `main` 发起 PR。创建或更新 PR 后，自动检查会开始运行。
3. 如果检查失败，修复问题后等待检查通过和维护者审查，再合并 PR。
4. 合并后，项目会从 `main` 发布 API 文档网站。验证和项目 Target 编译已在 PR 上完成。

## PR 会运行哪些检查

GitHub Actions 会检查 PR 和提交命名、格式与文本规范，以及 API 文档。Linux 编译工作流先验证 PR 并触发 CircleCI，然后在 Actions 运行图中并列显示 Game、Editor 和 Server 三个 Job，最后由汇总 Job 整理结果。无论 PR 来自 Fork 还是本仓库，CircleCI 都会用 Unreal Engine 5.6.1 在 Linux 上编译准确的 PR 提交。每个 Actions Job 都在自己的摘要中报告目标结果并链接到相应的 CircleCI 日志。CircleCI Job 结束后，Actions Job 日志还会以可折叠分组显示各步骤结果、PR 校验与编译命令及其控制台输出。完整日志仍可在 CircleCI 查看；认证与环境准备步骤的输出不会复制到 Actions。Game、Editor 和 Server 也分别以 `ci/linux-game-build`、`ci/linux-editor-build` 和 `ci/linux-server-build` 显示为独立检查；原有的 `ci/fork-pr-linux-build` 检查汇总三个 Job 的结果。你不需要 CircleCI 账号，也无需额外配置。

三个 GitHub Actions 检查与 CircleCI 汇总检查仍是合并到 `main` 的必需条件。三个 Target 检查分别显示各自的编译结果。

对于来自 Fork 的 PR，自动编译不会接受对 CI 工作流、自动化脚本、Unreal 构建脚本或插件描述文件的改动。如果需要修改这些文件，请先与维护者讨论，并在源仓库的分支中提交。来自源仓库分支的 PR 即使修改这些文件，也会运行 Linux 编译。

## Linux 编译的范围

一次 CircleCI 流水线会同时启动三个独立的 Linux Job，使用 Unreal Engine 5.6.1 构建 `NelaricGameplay/NelaricGameplay.uproject`，分别以 Development 配置编译 `NelaricGameplay`（Game）、`NelaricGameplayEditor`（Editor）和 `NelaricGameplayServer`（Server）Target。每个 Job 都会检出同一 PR 提交并使用独立工作区，避免 Unreal 构建产物冲突。启用的 `NelaricGameplayCore` 插件会随项目一起编译。只有三个 Job 都成功，工作流才会成功。工作流不发布可下载的打包产物；每个 Job 都会重新下载引擎镜像，且不使用编译缓存。

项目支持 UE 5.6 及以上版本，但 CI 目前仅验证 Linux 上的 UE 5.6.1。对最新支持版本的验证仍是[构建覆盖缺口](CodingStandards/BuildAndReview.zh-CN.md#unreal-构建脚本)。现阶段不设自动化测试覆盖率要求。

## 检查失败时

从 PR 页面打开失败的 GitHub Actions 目标 Job，查看结果摘要并点击对应的 CircleCI Job 日志链接。[CircleCI 流水线](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server)在 GitHub 仓库改名后仍使用原项目 slug。如果某项检查没有出现，或你无法解决失败问题，请在 PR 中附上链接和已经尝试过的排查步骤。

工作流配置分别位于 [GitHub Actions Quality](../.github/workflows/quality.yml)、[Fork PR Linux build](../.github/workflows/fork-pr-linux.yml)、[API Pages](../.github/workflows/pages.yml) 和 [CircleCI](../.circleci/config.yml)。
