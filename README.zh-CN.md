<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# Nelaric Unreal Server

面向 Unreal Engine 的玩法框架，用于快速构建可在单机、监听服务器和独立服务器下运行的玩法模式，并支持版本化玩法内容交付。

插件位于 `Unreal-Plugins/NelaricServer/`。`Development/` 下的本地验证工程不纳入 Git。

模块当前的职责和依赖见 [NelaricCore 模块说明](Docs/Modules/NelaricCore.zh-CN.md)。

玩法模型、网络拓扑和内容更新边界见[核心架构约束](Docs/CoreArchitectureConstraints.zh-CN.md)。

## 参与贡献

所有参与本项目的开发者都必须遵守 [Epic Games 的 Unreal Engine C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) 和[项目编码规范](Docs/CodingStandards/README.zh-CN.md)。项目规范定义了模块边界、API 契约、审查规则和必要的自动检查。

关于 Issue 和 Pull Request 的提交方式，请阅读[贡献指南](CONTRIBUTING.zh-CN.md)；自动检查和 Linux 插件编译见[开发与 CI 流程](Docs/DevelopmentWorkflow.zh-CN.md)。

## API 文档

[API 文档网站](https://nelaric.github.io/nelaric-unreal-server/)目前发布编码规范；加入公开头文件后，也会收录公开 C++ API。

## Star 历史

<a href="https://www.star-history.com/?repos=Nelaric%2Fnelaric-unreal-server&type=date&legend=top-left">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-server&type=date&theme=dark&legend=top-left" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-server&type=date&legend=top-left" />
    <img alt="Star 历史图表" src="https://api.star-history.com/chart?repos=Nelaric/nelaric-unreal-server&type=date&legend=top-left" />
  </picture>
</a>

## 致谢

感谢所有参与 Nelaric Unreal Server 的贡献者，感谢 Epic Games 提供 Unreal Engine，也感谢 CircleCI 提供 Linux 插件编译服务。
