<!-- Copyright (c) 2026 Nelaric -->

[English](https://github.com/Nelaric/nelaric-unreal-server/blob/main/Docs/CodingStandards/README.md) | 简体中文

# 项目编码规范

本规范适用于 Nelaric Unreal Server 的所有贡献。默认遵守 [Epic Games 的 Unreal Engine C++ 编码规范](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)。只有本项目规则明确与 Epic 规则不同时，才以项目规则为准，并且必须说明例外原因。空白字符和布局以锁定版本的格式化工具输出为准，不依赖对文字说明的主观理解。

最低支持版本为 Unreal Engine 5.6。项目是与玩法和服务提供商无关的多模块独立服务器插件。Core 负责通用的服务器控制面机制；高频玩法网络通信仍由 Unreal Engine 的 NetDriver、复制系统、Iris 和 RPC 处理。

## 规范目录

- [C++ 风格与头文件](Cpp.zh-CN.md)
- [模块与依赖边界](Modules.zh-CN.md)
- [公开 API、错误与文档](Api.zh-CN.md)
- [运行时、并发、性能与安全](Runtime.zh-CN.md)
- [构建脚本、工具与审查](BuildAndReview.zh-CN.md)

[版权声明规则](BuildAndReview.zh-CN.md)适用于所有项目自有文件，但需遵守文件格式规定的例外。

**Must（必须）**表示强制要求。**Should（应该）**是默认做法，除非 PR 给出合理的例外说明。**May（可以）**是可选项。“公开 API”指供所属模块之外使用的声明，包括 Provider 契约和向 Blueprint 暴露的入口。

## 优先级与例外

1. 首先满足正确性和 Unreal Engine 5.6 及以上版本的要求。
2. 明确写出的项目规则优先于与之冲突的 Epic 规则；其余 Epic 规则仍然适用。
3. 对空白字符和布局，以锁定版本的格式化工具输出为准。
4. PR 申请例外时，必须指出规则、原因、影响范围及考虑过的替代方案，并获得维护者批准。单次例外不会自动改变其他代码适用的规范。

本规范不统一规定项目必须使用纯 C++、UObject 还是反射类型。各模块应选择合适形式，并说明其生命周期和线程行为。
