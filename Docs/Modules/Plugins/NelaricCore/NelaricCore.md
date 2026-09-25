<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NelaricCore.zh-CN.md)

# NelaricCore

`NelaricCore` is the runtime module in the `NelaricGameplay/Plugins/NelaricCore` plugin. The project enables it for Game, Editor, and Server targets. It provides a shared C++ passkey for conventional internal calls across framework modules. Its only direct dependency is Unreal's `Core` module.

Include `Internal/InternalAccessKey.h` in a public header and accept `const UE::Nelaric::FInternalAccessKey&` in a plain C++ integration method. A caller in any module depending on `NelaricCore` includes `Internal/InternalAccess.h` and passes `UE::Nelaric::FInternalAccess::Key()`. Do not mark a method with this parameter as `UFUNCTION`: the key is not a reflected `USTRUCT`. Keep Blueprint entry points separate.

```cpp
// Public header, in a framework class's second public section.
void InternalInitialize(const UE::Nelaric::FInternalAccessKey& Access);
```

```cpp
// Implementation file in a module depending on NelaricCore.
#include "Internal/InternalAccess.h"

Object.InternalInitialize(UE::Nelaric::FInternalAccess::Key());
```

The key cannot be constructed, copied, moved, or destroyed by consuming code; the public access controller supplies a process-lifetime key. The `Internal` path, method name, and key parameter make the intended audience visible at call sites. Gameplay C++ can deliberately obtain the key, so this is a usage convention rather than an access restriction. No inheritance is needed by callers.

Framework consumers should not use this key to call internal methods under normal circumstances. Use supported gameplay APIs first. Treat direct internal access as a last resort when no suitable public API can meet the need; internal methods may change with framework implementation details.

When a module's public headers mention `FInternalAccessKey`, list `NelaricCore` in `PublicDependencyModuleNames`. If only implementation files use the access controller, a private dependency is sufficient. A separate plugin that depends on `NelaricCore` must also declare the `NelaricCore` plugin dependency in its descriptor. Keep normal module dependency direction and avoid cycles.
