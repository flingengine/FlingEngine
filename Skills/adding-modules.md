# Adding an engine module

This file is the how-to for the in-progress module split described in
[`architecture.md`](architecture.md). Do not invent a second pattern (extra
globs in the root CMakeLists, a hand-rolled `*_API` header, or edits under
`external/`).

`fling_add_module()` lives in [`CMake/FlingModule.cmake`](../CMake/FlingModule.cmake).
Core, Resources, and Engine are modules built this way. Graphics, Editor, and
Application are still compiled into the `FlingEngine` library by a glob. That
glob skips every folder under `FlingEngine/` that has its own `CMakeLists.txt`,
so a new module's sources are never built twice.

## What you add

A module is:

1. A folder under `FlingEngine/` with `inc/` (public headers) and `src/` (sources).
2. A `CMakeLists.txt` that calls `fling_add_module`.
3. `add_subdirectory(...)` in `FlingEngine/CMakeLists.txt` **after** that module's
   dependencies exist.
4. `MODULE_API` on types that cross the DLL boundary.
5. `target_link_libraries(Consumer PRIVATE Fling::ModuleName)` on anything that
   should see that module's headers.

You do not list `.cpp` files by hand. You do not edit the old engine-wide glob:
adding the folder's `CMakeLists.txt` removes it from that glob.

Also add isolation tests for the new module (see
[`testing.md`](testing.md#module-isolation-tests)): one that its headers compile
with only the module linked, and one that it cannot see a module it must not
depend on.

## Example: a `UI` module

In-game HUD / widgets. Publicly needs Core and Engine (types, `Transform`).
Privately needs Graphics and ImGui to draw. A physics-only tool would not link
`Fling::UI` and would not see `UISystem.h`.

### 1. Folder

```
FlingEngine/UI/
  CMakeLists.txt
  inc/UISystem.h
  src/UISystem.cpp
```

Keep `#include "UISystem.h"` (flat names). Isolation comes from include **paths**
via `target_link_libraries`, not from renaming headers to `UI/UISystem.h`.

### 2. `FlingEngine/UI/CMakeLists.txt`

```cmake
fling_add_module(UI
    PUBLIC_DEPS  Core Engine
    PRIVATE_DEPS Graphics
    PRIVATE_LIBS ImGui
)
```

| Argument | Meaning |
|----------|---------|
| `PUBLIC_DEPS` | Fling modules whose headers consumers of UI may also include |
| `PRIVATE_DEPS` | Fling modules UI uses internally; not pushed onto UI's consumers' include path |
| `PUBLIC_LIBS` / `PRIVATE_LIBS` | Third-party CMake targets (glfw, ImGui, Vulkan::Vulkan, …) |
| `SHARED` | Optional. Default is a static library; shared modules wait on `Singleton<T>::Get()` moving out of its header (each shared library would otherwise get its own singleton instances) |

CMake generates `UIAPI.h` (macro `UI_API`) into the UI build dir and puts that
dir on UI's **public** include path. Do not check `UIAPI.h` into git.

### 3. Register it bottom-up

In `FlingEngine/CMakeLists.txt`, add UI **after** Core, Engine, and Graphics.
A dependency cycle is a configure error.

```cmake
add_subdirectory(Core)
add_subdirectory(Resources)
add_subdirectory(Engine)
add_subdirectory(Graphics)
add_subdirectory(UI)
```

If Sandbox or Application should use UI, link the alias — do not `include_directories`
the UI folder:

```cmake
target_link_libraries(SandboxEditor PRIVATE Fling::UI)
```

### 4. Export the DLL surface

```cpp
#pragma once

#include "UIAPI.h"
#include "FlingTypes.h"

namespace Fling
{
    class UI_API UISystem
    {
    public:
        void Init();
        void Draw(float deltaTime);
    };
}
```

```cpp
#include "UISystem.h"

namespace Fling
{
    void UISystem::Init() {}
    void UISystem::Draw(float deltaTime) { (void)deltaTime; }
}
```

- Mark classes/functions that are **defined in this module's `.cpp`** and called
  from another module with `UI_API`.
- Header-only templates stay unmarked.
- Do not use `FLING_LIB_EXPORT` / `FLING_LIB_IMPORT` on engine types; those are
  only for the generated `*API.h` headers.

### 5. Consume it

```cpp
#include "UISystem.h"

Fling::UISystem hud;
hud.Init();
```

If a target does not link `Fling::UI`, `#include "UISystem.h"` must fail to
compile. That is the isolation test.

## Checklist for any new module

- [ ] Folder is `FlingEngine/<Name>/{inc,src}` plus `CMakeLists.txt`.
- [ ] `fling_add_module(<Name> ...)` only; no extra `add_library` / globs.
- [ ] `add_subdirectory(<Name>)` after every module in `PUBLIC_DEPS` / `PRIVATE_DEPS`.
- [ ] Public headers include `<Name>API.h` and use `<NAME>_API` on exported types.
- [ ] Consumers `target_link_libraries(... Fling::<Name>)`. No new
      `include_directories()` for engine headers.
- [ ] Engine still does not include Graphics headers. Editor stays a leaf
      (Application/Graphics do not include Editor). See [`architecture.md`](architecture.md).
- [ ] Nothing under `external/` is modified.
