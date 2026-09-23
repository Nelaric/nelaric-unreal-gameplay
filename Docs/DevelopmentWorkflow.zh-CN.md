<!-- Copyright (c) 2026 Nelaric -->

[English](DevelopmentWorkflow.md) | 简体中文

# 开发与 CI 流程

## 从改动到 `main`

1. 在分支或 Fork 中完成聚焦的改动，并运行相关的[本地检查](CodingStandards/BuildAndReview.zh-CN.md#本地检查)。
2. 向 `main` 发起 PR。GitHub Actions 的 **Quality** 工作流会检查 PR 与提交命名、格式与文本规范，以及 API 文档。CircleCI 的 GitHub OAuth 流水线会对 Fork PR 运行不使用密钥的 `public_quality` 作业。合并前必须通过这些检查并完成审查。
3. 改动进入 `main` 后，GitHub Actions 再次运行格式与 API 文档检查，**API Pages** 工作流发布文档网站；另一条 CircleCI GitHub App 流水线编译 Linux 插件。Unreal 编译发生在合并后，不是 PR 的必需检查。

分支、PR 和审查要求见[贡献指南](../CONTRIBUTING.zh-CN.md)。工作流配置分别位于 [GitHub Actions Quality](../.github/workflows/quality.yml)、[API Pages](../.github/workflows/pages.yml) 和 [CircleCI](../.circleci/config.yml)。

## CircleCI Fork PR 检查

在 CircleCI 项目的 Advanced 设置中启用 **Build forked pull requests** 后，GitHub OAuth 流水线会构建 Fork PR。`public_quality` 作业使用仅限本仓库的只读 Deploy Key 检出提交，检查文本规范和公开 API 文档；它不拉取 Unreal Engine、不编译插件，也不使用受限 context。**Pass secrets to builds from forked pull requests** 必须保持关闭。贡献者不需要创建 CircleCI 项目或获取项目凭据。

OAuth 流水线会向 GitHub 回报 `ci/circleci: public_quality`。`Protect main` 规则集已将 CircleCI App 上报的这一状态与三项 GitHub Actions 质量检查一同设为必需检查。GitHub App 的 Unreal 编译仍在合并后执行，因为 GitHub App 流水线不会由 Fork PR 触发。如果 OAuth 状态没有出现，先检查 OAuth 触发器、Fork 构建开关、GitHub webhook 和检出权限，再修改分支保护。

## CircleCI Linux 插件编译

CircleCI 的 GitHub App 触发器和工作流都只针对 `main` 推送。`build_plugin_linux` 作业使用 Linux Large 虚拟机，拉取 Epic 的 `ghcr.io/epicgames/unreal-engine:dev-slim-5.6.1` 镜像，并通过 Unreal Automation Tool 的 `BuildPlugin` 编译和打包 `Unreal-Plugins/NelaricServer/NelaricServer.uplugin`，目标平台为 `Linux`。

这会验证仅含源码的插件能否在 UE 5.6.1 下完成 Linux 编译与打包；不会构建游戏或 Dedicated Server 目标。打包结果写入作业内的临时存储，不会作为可下载产物发布。作业未启用 Docker 层缓存，也不保存 CircleCI 缓存，因此每次运行都会重新拉取引擎镜像。

项目支持 UE 5.6 及以上版本，但当前作业只测试 Linux 上的 UE 5.6.1。[构建规范](CodingStandards/BuildAndReview.zh-CN.md#unreal-构建脚本)建议同时编译最低支持版本与最新支持版本；最新版本仍是覆盖缺口。现阶段不要求自动化测试覆盖率。

## 凭据与故障排查

CircleCI 项目通过环境变量保存 `GHCR_USERNAME` 和 `GHCR_TOKEN`。`GHCR_USERNAME` 是有权拉取 Epic 引擎镜像的 GitHub 账号；`GHCR_TOKEN` 是仅授予 `read:packages` 权限的 classic 个人访问令牌。不要把令牌写入仓库、PR 或日志；Fork 构建不得收到这些变量。令牌过期或被撤销后，应在 CircleCI 中替换。

在 [CircleCI 项目页面](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server)查看构建结果。手动运行时，选择 GitHub App 流水线，并将配置来源与代码检出来源都设为 `main`。如果拉取镜像失败，检查 GHCR 访问权限与这两个项目环境变量；如果编译失败，检查 `Compile and package NelaricServer for Linux` 步骤，并使用相同引擎版本复现。
