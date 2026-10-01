<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](README.md) | 简体中文

# Nelaric Unreal Gameplay API

Nelaric Unreal Gameplay 是面向 Unreal Engine 5.6 及以上版本的玩法框架。公开契约支持在单机、监听服务器和独立服务器拓扑下开发玩法。

公开 API 遵守[项目编码规范](../CodingStandards/README.zh-CN.md)，包括玩法扩展契约、所有权、错误处理和文档要求。

## 设计

- [网络会话与权威转换](Core/NetWork/NetworkSessionTransitions.zh-CN.md)
- [原生输入](Input/NativeInput.zh-CN.md)

## GameAI StateTree

接入模块使用现有 GameplayRuntime 依赖，头文件位于其 AI/ 目录。创建 StateTree 资产时选择 Game AI Schema（UGameAIStateTreeSchema），在 Pawn 或 Controller 上添加 UGameAIStateTreeComponent 执行。继承的 Context Actor Class 应设置为组件实际 Owner 的类型。必需上下文如下：

| 条目 | 类型 | 来源 |
| --- | --- | --- |
| OwnerActor | AActor 或配置的子类 | 组件实际 Owner |
| Pawn | APawn | Owner Pawn 或 Controller 当前控制的 Pawn |
| Controller | AController | Owner Controller 或 Pawn 当前 Controller |
| GameContext | UGameAIContextSubsystem 或所选 C++ 子类 | 执行组件所在 World 的 Subsystem 集合 |
| StateTreeComponent | UStateTreeComponent | 当前执行组件本身 |

C++ Task 和 Evaluator 分别继承 FGameAIStateTreeTaskBase、FGameAIStateTreeEvaluatorBase。默认实例数据为 FGameAIStateTreeContext；在节点回调中调用 Context.GetInstanceData，随后直接使用 .Pawn、.Controller、.GameContext。需要添加实例字段时，从上下文结构派生反射结构，将其命名为节点的 FInstanceDataType，并重写 GetInstanceDataType() 返回该结构的 StaticStruct()。

Blueprint 节点继承 UGameAIStateTreeTaskBlueprintBase 或 UGameAIStateTreeEvaluatorBlueprintBase，直接读取继承的上下文字段。StateTree 编译器按兼容类型和属性名自动绑定 Context Category 字段。Schema 负责描述数据和节点准入，不向 FStateTreeExecutionContext 本身增加字段。

Schema 允许上述 Task/Evaluator 家族、通用 C++ Condition、Consideration、Property Function 和 Blueprint Condition。其他 Task/Evaluator 家族与任意外部数据链接被拒绝；上下文注入使用具名属性。既有通用或 AI Task 需要通过继承 GameAI Task 基类的适配节点接入。

五项上下文都必须在执行前可用。若 Possess 晚于 BeginPlay，关闭自动启动，完成 Possess 后调用 StartLogic()。UnPossess 或 Owner 生命周期变化使上下文失效前先停止逻辑，就绪后再启动。上下文是本机 Game Thread 数据；在回调外保留的引用需要重新验证。可用 C++ Subsystem 子类添加游戏服务，并在 Schema 中选择其类型。基础 Subsystem 是 World 管理的空扩展点，可与 C++ 子类实例共存。
