<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# 项目编码规范

本规范适用于 Nelaric Unreal Gameplay 的所有贡献。贡献者必须同时遵守 [Epic Games 的 Unreal Engine C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)和本项目编码规范。如果两者的要求发生冲突，应先创建 Issue 报告具体冲突，再继续相关工作；任何一方都不会自动优先。只有在输出同时符合两套规范时，才以锁定版本的格式化工具决定空白字符和布局。

最低支持版本为 Unreal Engine 5.6。项目是支持单机、监听服务器和独立服务器玩法的多模块框架。Foundation 负责通用玩法契约；具体机制由可选模块提供；玩法网络通信仍由 Unreal Engine 的 NetDriver、复制系统、Iris 和 RPC 处理。

## 规范目录

- [C++ 风格与头文件](Cpp.zh-CN.md)
- [模块与依赖边界](Modules.zh-CN.md)
- [公开 API、错误与文档](Api.zh-CN.md)
- [运行时、并发、性能与安全](Runtime.zh-CN.md)
- [构建脚本、工具与审查](BuildAndReview.zh-CN.md)

[版权声明规则](BuildAndReview.zh-CN.md)适用于所有项目自有文件，但需遵守文件格式规定的例外。

<strong>Must（必须）</strong> 表示强制要求。<strong>Should（应该）</strong> 是默认做法，除非 PR 给出合理的例外说明。<strong>May（可以）</strong> 是可选项。“公开 API”指供所属模块之外使用的声明，包括玩法扩展契约和向 Blueprint 暴露的入口。

## 规范遵循与冲突

1. 首先满足正确性和 Unreal Engine 5.6 及以上版本的要求。
2. 同时遵守 Epic 与项目规范。如果发现两者不一致，应创建 Issue 指明具体规则，并等待规范得到协调，而不是自行选择其中一条。
3. 只有在同时满足两套规范时，才以锁定版本的格式化工具输出决定空白字符和布局。若格式化结果与规范冲突，应创建 Issue 报告。
4. PR 申请项目指导性规则的例外时，必须指出规则、原因、影响范围及考虑过的替代方案，并获得维护者批准。这种例外不能覆盖 Epic 的要求，也不能单独解决两套规范之间的冲突。

本规范不统一规定项目必须使用纯 C++、UObject 还是反射类型。各模块应选择合适形式，并说明其生命周期和线程行为。
