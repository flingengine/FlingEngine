# Engine Build Modules

Plan for [issue #169](https://github.com/flingengine/FlingEngine/issues/169): split the
monolithic `FlingEngine` library into Unreal-style **build modules** so a system
(for example Physics) can depend on math and gameplay types without pulling the
Vulkan pipeline.

This document is the working design for branch `feature/169-engine-modules`.
Implementation has not started; this file is the spec.

## Locked decisions

These are settled. Do not re-open them while implementing.

1. **Fold Foundation into Core.** Delete `FlingEngine/Foundation/` (the
   `FoundationClass` stub, `FOUNDATION_API`, and its CMake target). Remove the
   `#include "Foundation.h"` call from `Engine.cpp`. Do not keep a second
   “core” DLL.
2. **Physically move `Utils/` and `Platform/` into `FlingEngine/Core/`.**
   Keep the existing flat include names (`Logger.h`, `Platform.h`). After the
   move, Core’s public include dir is `FlingEngine/Core/inc`. Delete the empty
   `Utils/` and `Platform/` folders. Do not merge other modules into Core.
3. **Generate per-module `*API.h` from CMake** (e.g. `GraphicsAPI.h` defining
   `GRAPHICS_API`). One template, configured per module into that module’s
   binary dir. Do not check these files into git.
4. **Sandbox is two executables in the same CMake build:** `SandboxGame` and
   `SandboxEditor`. `SandboxGame` must not compile or link any Editor module
   (not `FlingEngine/Editor`, not `Sandbox/Editor`). A global `WITH_EDITOR`
   `#define` cannot express this — both targets exist in one configure.
   Consequence: **Engine and Graphics must not link or include the Editor
   module.** Editor is a leaf linked only by editor executables.

## Current state

- Root `CMakeLists.txt` configures options, third-party deps, then adds
  `FlingEngine`, `FlingTests`, and `Sandbox`.
- `FlingEngine/CMakeLists.txt` glob-recurses almost every engine source file
  into **one** `add_library(FlingEngine ...)` (static `.a` / `.lib`).
- `CMake/FlingEngineInc.cmake` adds **every** engine `inc/` folder to the
  include path of anything that uses Fling. Consumers cannot opt out of Graphics.
- There is already a prototype: `FlingEngine/Foundation` is a **SHARED** library
  with its own `FOUNDATION_API` macro. It is a stub (`FoundationClass`) and is
  still linked into the monolith.
- DLL export today is a single `FLING_API`, toggled by `FLING_EXPORT` inside
  `Utils/inc/pch.h`. That cannot scale to per-module exports.
- `pch.h` is **not actually a precompiled header**. `CMake/MSVC_PCH.cmake` is
  included but never called, and nothing uses `target_precompile_headers`.
  It is a kitchen-sink include parsed from scratch in every TU. It also
  `#define`s `FLING_EXPORT` and `F_ENABLE_LOGGING`. Several **public headers**
  include it (`Timing.h`, `Version.h`, `FlingWindow.h`, `UniformBufferObject.h`),
  which is the opposite of how PCH is supposed to work. Decision: **delete it**.

Folder layout already hints at modules (`Core`, `Graphics`, `Gameplay`,
`Resources`, `Utils`, `Platform`, `Editor`, `Foundation`) but CMake and
includes treat them as one target.

## Goal

Match the *idea* of Unreal `*.Build.cs` files, implemented in CMake:

1. Each module is its own CMake target (shared library by default).
2. Each module has its own `MODULE_API` export (`CORE_API`, `GRAPHICS_API`, …).
3. A module declares **public** and **private** dependencies. Public include
   dirs and link libs propagate; private ones do not.
4. Adding a new module is a small CMake call plus a folder, not a root-file edit.
5. A game or tool links **only** the modules it needs. Physics can use
   `FlingMath` / `Transform` without seeing Vulkan headers or linking
   `spirv-cross`.

Git submodules per engine system (mentioned in #169) are **out of scope**.
Separate repos only make sense after module boundaries are stable.

**Do not refactor, reformat, or edit anything under `external/`.** Third-party
code stays as-is. First-party CMake may change *which module links* glfw,
SPIRV-Cross, ImGui, etc.; it must not change those projects' sources or their
own `CMakeLists.txt` files.

## CMake analogue of Build.cs

Unreal:

```csharp
public class Physics : ModuleRules
{
    public Physics(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
        PrivateDependencyModuleNames.AddRange(new[] { "Gameplay" });
    }
}
```

Fling (proposed):

```cmake
fling_add_module(Physics
    PUBLIC_DEPS  Core
    PRIVATE_DEPS Gameplay
)
```

Implemented as `CMake/FlingModule.cmake`. The helper should:

- `add_library(${NAME} SHARED)` (optional `STATIC` for tools/tests if needed).
- Glob `${module}/src` and `${module}/inc` with `CONFIGURE_DEPENDS` (`.cpp`,
  `.h`, `.hpp`, `.inl`). Do not list every file by hand.
- `target_include_directories(PUBLIC inc PRIVATE src)`.
- `target_link_libraries(PUBLIC ${PUBLIC_DEPS} PRIVATE ${PRIVATE_DEPS})`.
- Define `FLING_<UPPER_NAME>_BUILD` as a **PRIVATE** compile definition on that
  target only (`FLING_GRAPHICS_BUILD`, `FLING_CORE_BUILD`, …).
- Configure `CMake/FlingModuleAPI.h.in` into
  `${CMAKE_CURRENT_BINARY_DIR}/Generated/<Name>API.h` and add that directory as
  a **PUBLIC** include of the module (so consumers get `GraphicsAPI.h` only by
  linking Graphics).
- Alias `Fling::<Name>` to the target.
- Place the target in a Visual Studio folder `Fling/Modules`.
- Fail CMake configure if a dependency cycle is introduced.

Consumers never call `include_directories()` for engine headers. Include paths
come only from `target_link_libraries`. That is what makes isolation real.

`FLING_ENGINE_INC()` goes away once Sandbox and FlingTests link modules.

Two INTERFACE umbrellas, because game and editor pull different modules:

```cmake
add_library(Fling::GameStack INTERFACE)
target_link_libraries(Fling::GameStack INTERFACE
    Fling::Core Fling::Resources Fling::Gameplay Fling::Graphics Fling::Engine
)

add_library(Fling::EditorStack INTERFACE)
target_link_libraries(Fling::EditorStack INTERFACE
    Fling::GameStack Fling::Editor
)
```

`Fling::Engine` is the composition root (`Engine::Run`), not the old monolith.
It does **not** depend on `Fling::Editor`.

## Proposed module graph

Dependencies must be a **DAG**. Shared libraries cannot have link cycles
(especially on Windows).

```
Core  →  Resources  →  Gameplay  →  Graphics  →  Engine
                           │
                           ├── Physics (later; no Graphics)
                           └── Editor (leaf; editor executables only)
```

Editor is **not** under Engine. SandboxGame links through Engine and never
sees Editor. SandboxEditor links `Fling::EditorStack`.

| Module | Folder after moves | Public deps | Third-party |
|--------|--------------------|-------------|-------------|
| **Core** | `FlingEngine/Core` (includes former Utils + Platform) | none | glm, spdlog, entt (public INTERFACE) |
| **Resources** | `FlingEngine/Resources` | Core | nlohmann json, inih, cereal, stb as needed (public/private per header use) |
| **Gameplay** | `FlingEngine/Gameplay` | Core, Resources | entt, cereal |
| **Graphics** | `FlingEngine/Graphics` | Core, Resources, Gameplay | Vulkan, glfw, SPIRV-Cross, tinyobjloader (**private**) |
| **Engine** | `FlingEngine/Engine` once split out of Core sources | Gameplay, Graphics | — |
| **Editor** | `FlingEngine/Editor` | Gameplay, Graphics | ImGui (**private**) |
| **Physics** (later) | `FlingEngine/Physics` | Core, Gameplay | none of Graphics |

**Input** stays in Core. Window creation (`DesktopWindow`) belongs in Graphics,
not in `PlatformLinux.h`.

### Core folder layout after the move

```
FlingEngine/Core/
  inc/          Platform.h, FlingTypes.h, Logger.h, FlingMath.h, Input/, …
  src/          Logger.cpp, Timing.cpp, Engine.cpp (until Engine module split), …
FlingEngine/Resources/
FlingEngine/Gameplay/
FlingEngine/Graphics/
FlingEngine/Editor/
FlingEngine/Engine/     # created in Phase 3 when Engine.cpp leaves Core
```

Keep `#include "Logger.h"` style. Do not introduce `Core/Logger.h` prefixes.

`Platform/inc/ImGuiInputBinding.hpp` currently lives with Platform and is
included from `LinuxInput.cpp` / `WindowsInput.cpp`. Moving Platform into
Core would put an ImGui/GLFW-native header in Core. **Do not leave it there
as a Core public header.** During the move, place it under Graphics or Editor
(ImGui is a Graphics/Editor concern). Input `.cpp` files that need it will
have to stop living as Core-only, or the ImGui path becomes a hook Graphics
registers. Resolve this in Phase 0/1; do not give Core a hard ImGui include.

Until Phase 3, `Engine.h` / `Engine.cpp` / `FlingEngine.h` may still live under
`Core/` but they are **not** part of the Core module’s sources once Engine is
its own target. Phase 3 moves them to `FlingEngine/Engine/`.

### Why Gameplay sits *under* Graphics

The Physics example only works if Gameplay does **not** depend on Graphics.

- `Transform`, `Camera`, `World`, `Game` live in Gameplay.
- `MeshRenderer`, lights, Vulkan live in Graphics.
- Graphics may include Gameplay headers (`Transform`, `Camera`).
- Gameplay must **not** include `MeshRenderer.h` or Vulkan.

Today `World.inl` does the opposite: it `#include`s `MeshRenderer` and light
types so cereal can snapshot `WORLD_COMPONENTS`. That include is the main
Gameplay → Graphics cycle and has to move (see Phase 2).

## Per-module API macros

Replace one `FLING_API` with one generated header per module.

`CMake/FlingModuleAPI.h.in`:

```cpp
#pragma once
#include "FlingLibExports.h"
#if defined(FLING_@MODULE_UPPER@_BUILD)
#  define @MODULE_UPPER@_API FLING_LIB_EXPORT
#else
#  define @MODULE_UPPER@_API FLING_LIB_IMPORT
#endif
```

`fling_add_module(Graphics)` configures that to
`${CMAKE_CURRENT_BINARY_DIR}/Generated/GraphicsAPI.h` with `GRAPHICS_API`.
Same pattern for `CORE_API`, `RESOURCES_API`, `GAMEPLAY_API`, `ENGINE_API`,
`EDITOR_API`, `PHYSICS_API`.

`FlingLibExports.h` lives in Core (`FLING_LIB_EXPORT` / `FLING_LIB_IMPORT` for
MSVC `__declspec` and GCC/Clang visibility). Public classes/functions that
cross a DLL boundary are marked with that module’s `_API`. Header-only
templates stay unmarked.

CMake sets `FLING_GRAPHICS_BUILD` as a **PRIVATE** compile definition on the
Graphics target only. Consumers never get that define, so they `dllimport`.

## Include rules (no PCH)

- Public headers: `Module/inc/...`. Other modules may include these **only** if
  they declared that module as a public or private dep.
- Private headers/sources: `Module/src/...`. Never on a consumer include path.
- No project-wide `include_directories()`.
- **Do not use precompiled headers.** This tree is small (~50 engine `.cpp`
  files). PCH is not wired up today, and a shared PCH is hostile to module
  isolation (every TU would still see Core+Input+Paths+Math). Per-module PCH
  is not worth the CMake and include-order cost unless compile times become a
  real problem later.
- Delete `pch.h` / `pch.cpp` / `MSVC_PCH.cmake`. Move `F_ENABLE_LOGGING` and
  window-size defaults to a Core config header. Export macros come from CMake
  per target, not from a catch-all include.
- `.cpp` files include only the headers they use. Public headers include what
  they need and nothing else — never `pch.h`.
- `FlingEngine.h` becomes an optional convenience header in the Engine module
  for games that want the full stack. Physics must not include it.

Keep existing `#include "VulkanApp.h"` style (flat names) to avoid a mass
include rewrite. Isolation comes from include **paths**, not from renaming
headers to `Graphics/VulkanApp.h`. A later cleanup can namespace includes.

## SandboxGame vs SandboxEditor

Today one `Sandbox` exe is compiled with or without editor via a **global**
`WITH_EDITOR` flag. That goes away.

Same CMake configure produces:

| Target | Sources | Links |
|--------|---------|-------|
| **SandboxGame** | `Sandbox/Gameplay/*` + a game `main.cpp` | `Fling::GameStack` only |
| **SandboxEditor** | `Sandbox/Gameplay/*` + `Sandbox/Editor/*` + an editor `main.cpp` | `Fling::EditorStack` |

Shared gameplay code should be an OBJECT or STATIC lib (`SandboxRuntime`) so
it is not compiled from a glob that also picks up `Editor/`. Do **not** glob
the whole `Sandbox/` tree into one target.

`SandboxGame` must not have `FlingEngine/Editor/inc` or `Sandbox/Editor/inc`
on its include path. Including `BaseEditor.h` or `SandboxEditor.h` from the
game target is a bug.

`WITH_IMGUI` can still exist as a Graphics feature (in-game debug HUD). It is
not the Editor module. `SandboxEditor` requires ImGui; `SandboxGame` does not
require the Editor module even if ImGui is on.

`Engine::Run` takes only `T_GameType`. Editor entry:

```cpp
// Sandbox/main_game.cpp
Engine.Run<Sandbox::Game>(argc, argv);

// Sandbox/main_editor.cpp
Engine.Run<Sandbox::Game>(argc, argv);
// then register Sandbox::SandboxEditor with the debug-UI hook
```

Remove `WITH_EDITOR_FLAG` as a project-wide option that changes Engine’s
signature. If we keep a CMake option, it only controls whether the
`SandboxEditor` **target** is added, default ON. Both targets in one build is
the normal case.

## Coupling that must be broken

These will fail as soon as Graphics is no longer on Gameplay’s include path:

1. **`World.inl` → MeshRenderer / lights**  
   World serializes a **caller-provided** component list. Gameplay must not
   include `MeshRenderer.h` or light headers. Sandbox (game and editor) and/or
   Engine pass graphics types into `LoadLevelFile` / `OutputLevelFile`.

2. **`PlatformLinux.h` / `PlatformWindows.h` → `DesktopWindow.h`**  
   Platform headers must not pull the windowing backend. Window types stay in
   Graphics; platform headers keep macros, `FORCEINLINE`, and export helpers.

3. **`Engine.h` / Graphics → `BaseEditor`**  
   Engine and Graphics must not include Editor headers. Today `Engine::Run`
   is templated on `T_EditorType`, `VulkanApp::Init` takes
   `shared_ptr<BaseEditor>`, and `ImGuiSubpass` stores one. Replace that with
   an editor-free hook (optional `std::function` / thin `IDebugUI` in Core or
   Graphics). `SandboxEditor` constructs `SandboxEditor` and registers the
   draw callback. `SandboxGame` never sees the type.
   `PRIVATE_WITH_EDITOR` / `PROTECTED_WITH_EDITOR` in `FlingTypes.h` cannot
   stay as `#if WITH_EDITOR` if Gameplay is compiled once. Keep
   `friend class BaseEditor;` always (elaborated friend; does not require
   linking Editor).

4. **`pch.h` `#define FLING_EXPORT`**  
   Export is a per-target CMake definition, not a header that every TU includes.

5. **Graphics ↔ Gameplay in render code**  
   `GeometrySubpass`, `ShaderProgramReflections`, `ImGuiSubpass` including
   `Transform` / `Camera` / `FirstPersonCamera` is allowed **if** Graphics
   publicly or privately depends on Gameplay. Do not invert that.

6. **Header-only cycles**  
   Forward-declare across modules. `Game.h` already forward-declares `World`;
   keep doing that.

## Implementation phases

Work stays on `feature/169-engine-modules`. Each phase should leave
`SandboxGame` (and `SandboxEditor` once it exists) plus FlingTests building.

### Phase 0 — Infrastructure + Core tree

- Add `CMake/FlingModule.cmake` with `fling_add_module()` and
  `CMake/FlingModuleAPI.h.in`.
- Add `FlingLibExports.h` in Core.
- Physically move `FlingEngine/Utils/**` and `FlingEngine/Platform/**` into
  `FlingEngine/Core/inc` and `FlingEngine/Core/src`. Update CMake include
  paths. Delete empty Utils/Platform dirs.
- Delete Foundation (sources, CMake target, `Engine.cpp` include).
- Stop globbing the whole engine tree in a way that still picks up moved
  files twice.
- Document how to add a module (this file + a short comment on the macro).

### Phase 1 — Core as a real shared library

- Core module sources: everything now under `FlingEngine/Core/` **except**
  `Engine.h` / `Engine.cpp` / `FlingEngine.h` (those stay in the monolith
  until Phase 3).
- Remaining engine sources still one lib that **links Core**.
- Replace `FLING_ENGINE_INC` shotgun includes for Core headers with
  `target_link_libraries(... Fling::Core)`.
- Remove `pch.h` includes; give each file the headers it actually uses.
- Prove `CORE_API` exports on Windows and Linux.

### Phase 2 — Resources, then Gameplay

- Resources module; monolith links it.
- Gameplay module; **break `World.inl` graphics includes** before splitting.
- Confirm a throwaway target that links only Core+Gameplay cannot `#include`
  `VulkanApp.h` (compile-fail test is the isolation test).

### Phase 3 — Graphics, Editor, Engine, two Sandbox exes

- Graphics module: Vulkan, glfw, SPIRV-Cross stay **private** link deps.
- Decouple Engine/Graphics from `BaseEditor` (debug-UI hook).
- Editor module always built as its own target; only editor exes link it.
- Move `Engine.h` / `Engine.cpp` / `FlingEngine.h` to `FlingEngine/Engine/`.
- Split Sandbox into `SandboxRuntime` + `SandboxGame` + `SandboxEditor`.
- Remove project-wide `WITH_EDITOR` from Engine’s API.
- FlingTests link only the modules they exercise (`Fling::GameStack` unless a
  test needs Editor).

### Phase 4 — Prove the Physics story

- Add a stub `Physics` module that includes `FlingMath.h` and `Transform.h`
  and **does not** link Graphics.
- Optional: a small CMake test or `try_compile` that Graphics headers are
  unreachable from Physics.

## Success criteria

- `ninja` / VS build produces `libCore.so`, `libGraphics.so`, … (or `.dll` +
  import libs) plus **both** `SandboxGame` and `SandboxEditor`.
- `SandboxGame` does not link Editor and does not compile `Sandbox/Editor`
  or `FlingEngine/Editor` sources.
- `SandboxEditor` still runs the deferred demo with the existing editor UI.
- `SandboxGame` still runs the deferred demo without the editor UI.
- FlingTests pass.
- A new module is added with one `fling_add_module()` and a folder; no edits to
  a monolith glob in `FlingEngine/CMakeLists.txt`.
- Physics (stub) compiles against Core+Gameplay without Vulkan on its link line
  or include path.

## Non-goals for this branch

- **Any edits under `external/`** (sources, headers, or third-party CMake).
  Vendored libraries are consumed, not modified.
- Splitting modules into git submodules / separate repos.
- Rewriting the renderer or ECS.
- Changing asset pipelines.
- Mass-renaming includes to `Module/Header.h` (nice follow-up).
- Moving Graphics/Gameplay/Resources/Editor folders except as listed in
  phases (Utils + Platform → Core, and Engine sources → `FlingEngine/Engine/`
  in Phase 3).
- Forcing every type to be `MODULE_API`; start with classes that are used
  across DLL boundaries (singletons, Engine, World, VulkanApp, etc.). Templates
  and header-only types do not need export.

## Risks

- **Windows DLL + templates + singleton**: `Logger`, `VulkanApp`,
  `ResourceManager` must live in one module and be exported, or you get duplicate
  singletons. One instance per process, defined in that module’s `.cpp`.
- **MSVC needs at least one exported symbol** or the import `.lib` is not
  produced (already noted in Foundation’s CMakeLists).
- **Glob + new files**: `CONFIGURE_DEPENDS` on module globs so adding a `.cpp`
  re-runs CMake.
- **Runtime search path**: Linux `RPATH` / Windows DLL copy next to **both**
  Sandbox executables so they find module `.so`/`.dll` files.
- **Compile times**: more DLLs can mean more relinks but better incrementality
  within a module. Do not add PCH to “fix” that unless measurements say so.
