<!-- Copyright (c) 2026 Nelaric -->

[English](NelaricCore.md) | 简体中文

# NelaricCore

`NelaricCore` 是 `NelaricGameplay/Plugins/NelaricCore` 插件中的运行时模块。项目在 Game、Editor 和 Server Target 中启用该插件。模块提供供多个框架模块按约定使用的统一 C++ Passkey，直接依赖只有 Unreal 的 `Core` 模块。

在公开头文件中包含 `Internal/InternalAccessKey.h`，并让普通 C++ 框架集成方法接收 `const UE::Nelaric::FInternalAccessKey&`。任何依赖 `NelaricCore` 的模块都可以包含 `Internal/InternalAccess.h`，调用时传入 `UE::Nelaric::FInternalAccess::Key()`。不要将带有此参数的方法标记为 `UFUNCTION`：Key 不是可反射的 `USTRUCT`。Blueprint 入口应单独设计。

```cpp
// 公开头文件中，框架类的第二个 public 区域。
void InternalInitialize(const UE::Nelaric::FInternalAccessKey& Access);
```

```cpp
// 依赖 NelaricCore 的模块的实现文件。
#include "Internal/InternalAccess.h"

Object.InternalInitialize(UE::Nelaric::FInternalAccess::Key());
```

调用方不能自行构造、复制、移动或析构 Key；公开的 Access Controller 提供进程生命周期内有效的 Key。`Internal` 路径、方法名和 Key 参数在调用处表明预期用途。玩法 C++ 也可以有意取得 Key，所以这是使用约定，不是访问限制。调用者无需继承辅助类。

框架使用者通常不得通过此 Key 调用内部方法，应优先使用受支持的玩法公开 API。只有公开 API 确实无法满足需求时，才将直接访问内部方法作为万不得已的办法；内部方法可能随框架实现变化。

如果模块公开头文件使用 `FInternalAccessKey`，应在 `PublicDependencyModuleNames` 中列出 `NelaricCore`；只有实现文件使用 Access Controller 时，私有依赖即可。另一个插件依赖 `NelaricCore` 时，还须在插件描述文件中声明该插件依赖。保持模块依赖方向，避免循环。
