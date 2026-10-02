<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](NativeInput.md) | 简体中文

# 原生输入基础设施

GameplayRuntime 提供原生输入配置、Tag 绑定、映射管理和设置驱动修饰器，使用 Enhanced Input 与 Gameplay Tags，不依赖 GameplayAbilities，也不分发能力输入。

## 职责边界

框架提供动作查找、绑定与解绑、映射激活与释放、输入偏好。具体动作 Tag、值类型、触发事件、输入回调、移动规则、视角控制、蹲伏、跳跃和自动前进由使用框架的游戏定义。框架 Pawn 和 Character 基类没有具体输入处理函数，也不自动创建或绑定这个可选的输入生命周期组件。

## Pawn 输入生命周期组件

`UPlayerInputComponent` 继承 `UPawnInitStateComponent`，提供类似 Hero 组件的输入生命周期管理，但不包含玩法输入函数。将它配置为 Pawn 的 `UPawnInitializationConfig` 组件条目；若 Pawn 可能在权威端或客户端由本地玩家控制，应在相应侧创建。可在组件上指定 `InputConfig`，运行时通过 `SetInputConfig` 更换。只有当输入可用性应阻止 Pawn 进入 Ready 时，才将它标记为 Pawn Ready 的必需组件。

远端 Pawn 和 AI 可以在不处理本地输入的情况下推进初始化状态。对于拥有配置的本地玩家，该组件等待所属 `ULocalPlayer`、`UNelaricInputComponent` 和 `UEnhancedInputLocalPlayerSubsystem`。初始化组提交 Ready 后，它激活映射并调用 `BindInputActions`。游戏可派生 C++ 子类并覆写该钩子，使用受保护的 `BindNativeAction` 绑定游戏动作；覆写 `UnbindInputActions` 清理游戏状态。控制器变化或组件销毁导致初始化代际失效时，会移除已记录的绑定句柄和本组件拥有的映射。`OnPlayerInputReady()` 与 `OnPlayerInputRevoked()` 提供原生 C++ 多播生命周期通知，不定义玩法回调，也不使用蓝图可分配的动态多播委托。

`InputConfig` 为空时，该参与者仍可进入 Ready，但不安装映射或广播本地输入就绪。`SetInputConfig` 会使当前代际失效，协调器随之重试。使用此组件时，不应在同一个输入组件上另行调用 `AddInputMappings` 管理相同映射，因为该方法会替换此前的映射。具体动作 Tag、回调逻辑和内容资产仍由游戏负责。

## 配置与绑定

1. 在游戏中创建 Input Action 和 Input Mapping Context 资产，根据游戏行为选择值类型和触发器。
2. 在游戏中定义动作 Tag 与 `InputMapping.*` 下的映射 Tag，创建 `UNelaricInputConfig` 数据资产并配置 `NativeInputActions` 与 `MappingContexts`。重复动作 Tag 使用首个非空动作；按 Tag 查找映射时使用首个匹配的非空上下文。空项跳过；框架不预定义具体 Tag。
3. 在游戏的输入初始化代码中获取 Pawn 的 `UNelaricInputComponent` 和所属本地玩家的 `UEnhancedInputLocalPlayerSubsystem`。不要通过全局玩家索引查找。
4. 调用 `AddInputMappings` 添加映射，再对游戏提供的各个回调和触发事件调用 `BindNativeAction`；由游戏侧保存绑定句柄。
5. 不使用 `UPlayerInputComponent` 时，输入替换或结束时在创建绑定的组件上解除绑定，并调用 `RemoveInputMappings`；使用生命周期组件时，由其初始化代际失效自动清理，并在后续重试初始化。

所有接口与回调在游戏线程运行，游戏负责制作配置与实现回调；输入组件持有已安装的配置，直到映射移除或配置替换。配置、动作或回调对象缺失时，`BindNativeAction` 返回 false，不添加句柄；UObject 回调目标采用弱绑定。框架仅绑定游戏显式指定的动作，不调用移动、视角、蹲伏或跳跃接口。

`DefaultInput.ini` 使用 `UEnhancedPlayerInput` 和 `UNelaricInputComponent`，启用 Enhanced Input 用户设置并选择 `UNelaricInputUserSettings`。其他项目使用插件时也需应用这些配置并启用 `NelaricGameplay`，无需专用 LocalPlayer 子类。动作、映射和配置资产由游戏制作。

## 映射生命周期

每个映射条目都有可选的 `MappingTag` 和 `bActivateOnStart`，后者默认为 true，以保持现有资产的行为。需要运行时控制的条目应填写有效映射 Tag。`AddInputMappings` 替换当前输入组件的映射配置，并激活标记为开始时启用的条目。传入空配置或空子系统时释放旧映射并返回 false。用户设置注册为可选项，与激活独立；开始时不激活的映射也可注册。已注册的重映射条目随本地玩家保留，跨 Pawn 切换继续可用。

配置安装后，可调用 `AddInputMappingByTag` 按配置优先级启用 IMC，调用 `RemoveInputMappingByTag` 移除本组件启用的 IMC。两者按完整 Tag 匹配；Tag 无效或不存在时返回 false。重复启用已激活的 IMC 不改变其优先级；借用的 IMC 无法由本组件移除。组件持有当前配置，直到调用 `RemoveInputMappings` 或替换配置。需要独立切换的 Tag 应使用不同的 IMC。

已有映射仅借用并保留原优先级；组件只记录自己新激活的上下文，在 `RemoveInputMappings` 或 `OnUnregister` 时释放。重复上下文项使用首次激活的优先级；重复映射 Tag 在按 Tag 操作时选择首个非空上下文，开始时激活仍逐项处理。没有 Tag 的条目仍可在开始时激活，但不能按 Tag 切换。不调用全局 `ClearAllMappings` 或 `ClearActionBindings`。

独立管理的系统应使用不同映射上下文。其他系统不能同时接管本组件拥有的上下文，因为 Enhanced Input 不提供引用计数式激活所有权。移除映射不会解除游戏回调绑定，解绑由 `RemoveBinds` 单独完成。

## 偏好与修饰器

每个本地玩家由 Enhanced Input 管理一个 `UNelaricInputUserSettings`，使用引擎的按键配置档与重映射接口。将动作或上下文按键标记为 Player Mappable，开启配置中的 `bRegisterWithSettings`，即可通过继承的 `MapPlayerKey`、`ApplySettings` 和 `AsyncSaveSettings` 修改、应用并保存按键。输入偏好也带有 SaveGame 标记，修改后调用 `AsyncSaveSettings` 保存。

| 修饰器 | 设置与行为 |
| --- | --- |
| `UNelaricInputModifierMouseSensitivity` | 鼠标每轴灵敏度，使用时限制为 0–10。 |
| `UNelaricInputModifierGamepadSensitivity` | 修饰器选择普通或瞄准倍率，限制为 0–10。 |
| `UNelaricInputModifierDeadZone` | 移动或视角摇杆下阈值，支持径向、逐轴死区与上阈值。 |
| `UNelaricInputModifierAimInversion` | 水平和垂直轴反转。 |

修饰器应用到哪个动作或映射由游戏决定。死区先于灵敏度，鼠标和手柄倍率分别用于对应设备路径。输出保留原值类型；Boolean 或缺少设置时原值通过；死区上阈值不高于下阈值时输出零，非有限偏好倍率按零处理。修饰器只转换输入值，不执行玩法动作。

手柄灵敏度使用可编辑倍率。
