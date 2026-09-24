<!-- Copyright (c) 2026 Nelaric -->

[English](DevelopmentWorkflow.md) | 简体中文

# 开发与 CI 流程

## 从改动到 `main`

1. 在分支或 Fork 中完成聚焦的改动，并运行相关的[本地检查](CodingStandards/BuildAndReview.zh-CN.md#本地检查)。
2. 向 `main` 发起 PR。GitHub Actions 的 **Quality** 工作流和 CircleCI GitHub OAuth 流水线的不使用密钥的 `public_quality` 作业会执行检查。另一条可信的 GitHub Actions 工作流会核验 PR 目标、准确的提交 SHA 和改动文件；符合条件时启动 CircleCI GitHub App Linux 编译，并将结果回报到该 PR 提交。合并前必须通过必需检查并完成审查。
3. 改动进入 `main` 后，GitHub Actions 再次运行格式与 API 文档检查，**API Pages** 工作流发布文档网站；CircleCI GitHub App 流水线针对 `main` 推送再次编译插件。

分支、PR 和审查要求见[贡献指南](../CONTRIBUTING.zh-CN.md)。工作流配置分别位于 [GitHub Actions Quality](../.github/workflows/quality.yml)、[Fork PR Linux build](../.github/workflows/fork-pr-linux.yml)、[API Pages](../.github/workflows/pages.yml) 和 [CircleCI](../.circleci/config.yml)。

## CircleCI Fork PR 检查

在 CircleCI 项目的 Advanced 设置中启用 **Build forked pull requests** 后，GitHub OAuth 流水线会构建 Fork PR。`public_quality` 作业使用仅限本仓库的只读 Deploy Key 检出提交，检查文本规范和公开 API 文档；它不拉取 Unreal Engine、不编译插件，也不使用受限 context。**Pass secrets to builds from forked pull requests** 必须保持关闭。贡献者不需要创建 CircleCI 项目或获取项目凭据。

OAuth 流水线会向 GitHub 回报 `ci/circleci: public_quality`。`Protect main` 规则集已将 CircleCI App 上报的这一状态与三项 GitHub Actions 质量检查一同设为必需检查。如果 OAuth 状态没有出现，先检查 OAuth 触发器、Fork 构建开关、GitHub webhook 和检出权限，再修改分支保护。

## 合并前的 Linux 编译

`Fork PR Linux build` GitHub Actions 工作流通过 `pull_request_target` 在可信的目标分支版本上运行，绝不在 GitHub Runner 上检出或执行 PR 文件。它确认打开的 PR 指向 `Nelaric/nelaric-unreal-server:main`，固定当前 head SHA，并拒绝修改 `.circleci/`、GitHub Actions 工作流与自动化文件、`Scripts/`、Unreal 构建脚本、插件描述文件、`.gitattributes` 和 `.gitmodules` 的 PR。如需修改这些受保护文件，请交由维护者另行处理；自动路径会回报失败状态。

符合条件时，工作流通过项目专用的 CircleCI Custom Webhook 发送 PR 编号和 SHA。GitHub App 流水线从 `main` 读取配置并初始检出，重新核验 PR 和 SHA，再获取 `refs/pull/<编号>/head` 并检出准确提交。随后只把插件复制到隔离的 UE 容器中编译；该容器没有网络，也拿不到 GHCR 凭据。GitHub 工作流等待 CircleCI 结果，再把 `ci/fork-pr-linux-build` 状态写到 PR head SHA。CircleCI App 自己的状态会附着在初始检出的 `main` 提交上，不能直接作为该 PR 的合并门槛。

将 Custom Webhook URL 存为 GitHub 仓库 Secret `CIRCLECI_PR_BUILD_WEBHOOK_URL`。在现有 GitHub App 流水线中配置 CircleCI Custom Webhook，配置和检出分支均设为 `main`。GitHub 工作流仅需 `contents: read`、`pull-requests: read` 和 `statuses: write`，无需单独的 bot 账号。先用真实 Fork PR 验证，再在 `main` 规则集中将 `ci/fork-pr-linux-build` 设为必需状态，并指定 GitHub Actions 为预期来源。CircleCI 项目的流水线信息公开，bot 通过其 API 查询对应作业的结果。

## CircleCI Linux 插件编译

CircleCI 的 GitHub App 流水线在推送到 `main` 时运行 `build_plugin_linux`，通过可信 Custom Webhook 触发时运行 `build_fork_pr_linux`。两者都使用 Linux Large 虚拟机，拉取 Epic 的 `ghcr.io/epicgames/unreal-engine:dev-slim-5.6.1` 镜像，并通过 Unreal Automation Tool 的 `BuildPlugin` 编译和打包 `Unreal-Plugins/NelaricServer/NelaricServer.uplugin`，目标平台为 `Linux`。

这会验证仅含源码的插件能否在 UE 5.6.1 下完成 Linux 编译与打包；不会构建游戏或 Dedicated Server 目标。打包结果写入作业内的临时存储，不会作为可下载产物发布。作业未启用 Docker 层缓存，也不保存 CircleCI 缓存，因此每次运行都会重新拉取引擎镜像。

项目支持 UE 5.6 及以上版本，但当前作业只测试 Linux 上的 UE 5.6.1。[构建规范](CodingStandards/BuildAndReview.zh-CN.md#unreal-构建脚本)建议同时编译最低支持版本与最新支持版本；最新版本仍是覆盖缺口。现阶段不要求自动化测试覆盖率。

## 凭据与故障排查

CircleCI 项目通过环境变量保存 `GHCR_USERNAME` 和 `GHCR_TOKEN`。`GHCR_USERNAME` 是有权拉取 Epic 引擎镜像的 GitHub 账号；`GHCR_TOKEN` 是仅授予 `read:packages` 权限的 classic 个人访问令牌。不要把令牌写入仓库、PR 或日志。可信的 PR 编译作业只能用这些变量拉取镜像，绝不能把它们传给隔离的插件编译容器。OAuth 流水线的 **Pass secrets to builds from forked pull requests** 必须保持关闭。令牌过期或被撤销后，应在 CircleCI 中替换。

在 [CircleCI 项目页面](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server)查看构建结果。手动运行时，选择 GitHub App 流水线，并将配置来源与代码检出来源都设为 `main`。如果拉取镜像失败，检查 GHCR 访问权限与这两个项目环境变量；如果编译失败，检查 `Compile and package NelaricServer for Linux` 步骤，并使用相同引擎版本复现。
