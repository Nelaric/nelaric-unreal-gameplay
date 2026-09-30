<!-- Copyright (c) 2026 Nelaric -->

[English](NativeInput.md) | 简体中文

# 原生输入基础设施

GameplayRuntime 迁移 Lyra 的原生输入配置、Tag 绑定、映射管理和设置驱动修饰器，使用 Enhanced Input 与 Gameplay Tags，不依赖 GameplayAbilities，也不分发能力输入。

## 职责边界

框架提供动作查找、绑定与解绑、映射激活与释放、输入偏好。具体动作 Tag、值类型、触发事件、输入回调、移动规则、视角控制、蹲伏、跳跃和自动前进由使用框架的游戏定义。框架 Pawn 和 Character 基类没有具体输入处理函数，不自动创建输入玩法组件，也不自动绑定动作。

## 配置与绑定

1. 在游戏中创建 Input Action 和 Input Mapping Context 资产，根据游戏行为选择值类型和触发器。
2. 在游戏中定义动作 Tag，创建 `UNelaricInputConfig` 数据资产并配置 `NativeInputActions` 与 `MappingContexts`。重复动作 Tag 使用首个非空动作，空项跳过；框架不预定义具体动作 Tag。
3. 在游戏的输入初始化代码中获取 Pawn 的 `UNelaricInputComponent` 和所属本地玩家的 `UEnhancedInputLocalPlayerSubsystem`。不要通过全局玩家索引查找。
4. 调用 `AddInputMappings` 添加映射，再对游戏提供的各个回调和触发事件调用 `BindNativeAction`；由游戏侧保存绑定句柄。
5. 输入替换或结束时，在创建绑定的组件上调用 `RemoveBinds`，并调用 `RemoveInputMappings`。上下文就绪后由游戏重新初始化输入；可以使用现有 `UPawnInitializationComponent` 的 Ready 和撤销通知协调这一生命周期。

所有接口与回调在游戏线程运行，配置生命周期和回调行为由游戏管理。配置、动作或回调对象缺失时，`BindNativeAction` 返回 false，不添加句柄；UObject 回调目标采用弱绑定。框架仅绑定游戏显式指定的动作，不调用移动、视角、蹲伏或跳跃接口。

`DefaultInput.ini` 使用 `UEnhancedPlayerInput` 和 `UNelaricInputComponent`，启用 Enhanced Input 用户设置并选择 `UNelaricInputUserSettings`。其他项目使用插件时也需应用这些配置并启用 `NelaricGameplay`，无需专用 LocalPlayer 子类。动作、映射和配置资产由游戏制作，不复制 Lyra 二进制资产。

## 映射生命周期

`AddInputMappings` 替换当前输入组件的映射配置。传入空配置或空子系统时释放旧映射并返回 false。用户设置注册为可选项，与映射激活独立；已注册的重映射条目随本地玩家保留，跨 Pawn 切换继续可用。

已有映射仅借用并保留原优先级；组件只记录自己新激活的上下文，在 `RemoveInputMappings` 或 `OnUnregister` 时释放。重复上下文项使用首次激活的优先级。不调用全局 `ClearAllMappings` 或 `ClearActionBindings`。

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

框架不包含 Lyra 的具体 Hero 输入函数、自动前进 Tick、固定玩法输入 Tag、共享设置 UI、延迟标记、Game Feature 注入、相机模式或能力输入分发。手柄灵敏度使用可编辑倍率，不依赖 Lyra 专用档位枚举与查表资产。
