# Module-split plan

Working plan for [issue #169](https://github.com/flingengine/FlingEngine/issues/169):
split the monolithic `FlingEngine` library into build modules, similar to
Unreal's `*.Build.cs` files, so that (for example) a Physics module can use math
and world types (`Transform`) without the Vulkan pipeline on its include path or
link line.

This replaces the earlier `docs/BuildModules.md`, which you can still read with
`git show dfdb2bc^:docs/BuildModules.md`. For where code lives today, see
[`architecture.md`](../Skills/architecture.md). For the `fling_add_module()` recipe, see
[`adding-modules.md`](../Skills/adding-modules.md).

## Locked decisions

Do not re-open these while you implement the plan.

1. **Foundation is folded into Core.** There is no second "core" library.
2. **`Utils/` and `Platform/` live in `FlingEngine/Core/`.** Keep flat include
   names (`#include "Logger.h"`, not `Core/Logger.h`).
3. **CMake generates each module's `<Name>API.h`** (for example `GRAPHICS_API`)
   from `CMake/FlingModuleAPI.h.in`. Do not check these files into git.
4. **Games and editors are separate executables in one configure** (for
   Sandbox: `SandboxGame` and `SandboxEditor`, made by the project model plan's
   `fling_add_game()`). The game does not compile or link any Editor code. Thus
   the Engine module, Graphics, and Application must not include or link Editor.
5. **Modules are static libraries by default.** `SHARED` is opt-in. Shared
   modules wait until `Singleton<T>::Get()` is defined in the module that owns
   each singleton, not in the header. If not, each shared library gets its own
   copy of `Logger`, `ResourceManager`, `VulkanApp`, and so on.
6. **No precompiled headers.** `pch.h` is not a real PCH and is deleted in
   Phase 1.
7. **Tests are optional.** No engine or game target depends on the test
   executables or Catch2, and test-only engine code is compiled out of
   builds that do not need it (see [Tests](#tests)).
8. **Module names.** `Engine` is the foundational world layer: `World`,
   `Level`, the `Game` base class, `Transform`, cameras, and the component type
   registry. `Application` is the top-level startup layer (`Engine::Run`, the
   main loop) that boots a game client, server, test, or editor application.
   `Gameplay` is reserved for actual gameplay systems (AI, abilities, ...),
   which are their own modules on top of Engine and never depend on Graphics.

## Module graph

The dependencies must be a DAG. `fling_add_module()` stops the configure if
there is a cycle.

```
Core  →  Resources  →  Engine  →  Graphics  →  Application
                         │
                         ├── Gameplay modules (later: AI, abilities, ...; no Graphics)
                         ├── Physics (later; no Graphics)
                         └── Editor (leaf; editor applications only)
```

| Module | Public deps | Third-party |
|--------|-------------|-------------|
| **Core** | none | glm, spdlog, entt, cereal |
| **Resources** | Core | inih (public), json (private) |
| **Engine** | Core, Resources | entt |
| **Graphics** | Core, Resources, Engine | Vulkan, glfw, SPIRV-Cross, stb, tinyobjloader (private) |
| **Application** | Engine, Graphics | none |
| **Editor** | Engine, Graphics | ImGui (private) |
| **Gameplay modules** (later) | Engine | none of Graphics |
| **Physics** (later) | Core, Engine | none of Graphics |

There are two INTERFACE umbrella targets: `Fling::GameStack` (Core through
Application) and `Fling::EditorStack` (`GameStack` + Editor).

## Phases

Each phase must leave Sandbox (both executables, after Phase 3) and the
per-module test executables building and passing on every CI platform.

### Phase 0: Infrastructure and Core tree (done)

- `fling_add_module()`, `FlingModuleAPI.h.in`, and `FlingLibExports.h` added.
- `Utils/` and `Platform/` moved into `Core/`. `ImGuiInputBinding.hpp` moved
  to Graphics. The Foundation stub is deleted.

### Phase 0.5: CMake cleanup (done, #206)

- `FLING_ENGINE_INC()` removed. Settings are per target through
  `fling_configure_target()`, with one shared warning set
  (`fling_set_warnings()`).
- Header-only libraries are `FlingExt::*` INTERFACE targets with SYSTEM
  includes. There are no global `include_directories()` or
  `add_definitions()` calls.
- All first-party code builds as C++20. This needs GCC 13+ or Clang 16+, which
  CMake checks: EnTT 3.1 hangs on older compilers in C++20 mode. An EnTT
  upgrade would remove that limit.
- `fling_add_module()` builds STATIC by default (locked decision 5).
- `FLING_BUILD_TESTS` option. It is OFF by default for shipping builds and
  when Fling is not the top-level project, and Catch2 is only needed when it is
  ON. Tests are registered with CTest through `catch_discover_tests()`.
- CI runs the tests with CTest, and a Linux shipping job builds without
  Catch2.

### Phase 1: Core as a real module

- Remove `pch.h`. Each file includes only the headers it uses.
  `F_ENABLE_LOGGING` and the window-size defaults move to `BuildConfig.h`.
  `Platform.h` stops including `DesktopWindow.h` and the input headers.
  (Done on branch `chore/remove-pch`.)
- Core is a `fling_add_module(Core)` target (`FlingEngine/Core/CMakeLists.txt`).
  The rest of the engine stays one library that links `Fling::Core`, with
  `Core/` excluded from its glob. (Done on branch `chore/core-module`.)
  To get there:
  - `Engine.h`, `Engine.cpp`, and `FlingEngine.h` moved to
    `FlingEngine/Engine/` now instead of in Phase 3. (That folder is now
    `FlingEngine/Application/`; see Phase 2.5.)
  - `FlingPaths` (and the generated `FlingAssetPaths.cpp`) moved from Resources
    to Core, because `Logger` needs the log directory.
  - The GLFW input backends (`LinuxInput`, `WindowsInput`, keycodes) moved to
    Graphics, next to `DesktopWindow`. The abstract `Input` class (key maps,
    bindings) stays in Core. This replaces the planned ImGui hook: Core has no
    GLFW, ImGui, or Vulkan code to hook.
  - `GitVersion.h` is generated into Core's build dir.
  - `WITH_IMGUI`, `WITH_EDITOR`, and `FLING_SHIPPING` are PUBLIC definitions on
    Core.
- Core types use `CORE_API` instead of `FLING_API`. `Singleton<T>` is a
  header-only template and is not marked. With static modules (locked decision
  5) `CORE_API` is empty; proving the exports work waits for `SHARED`.

### Phase 2: Resources, then Gameplay (done, #210)

The `Gameplay` module named here is now the `Engine` module (Phase 2.5).

- Resources (`PUBLIC_DEPS Core`) and Gameplay (`PUBLIC_DEPS Core Resources`)
  are `fling_add_module()` targets. The monolith links both and excludes their
  folders from its glob.
- `Texture` and `HDRImage` moved from Resources to Graphics: they are Vulkan
  images, and only Graphics uses them. nlohmann/json is a PRIVATE dependency
  of Resources (`Json` hides it behind a pimpl).
- `FirstPersonCamera` no longer asks `VulkanApp` for the window size.
  `VulkanApp` calls `SetViewportSize()` on it each frame, before `Update()`.
- `World` already did not include `MeshRenderer.h`; Graphics registers its
  types through `RegisterGraphicsComponents()`.
- Isolation tests in `FlingTests/CMakeLists.txt` (`fling_add_isolation_test`):
  each module's own headers compile when only that module is linked, and Core
  cannot see Resources, Resources cannot see Gameplay, Gameplay cannot see
  Graphics. See [Tests](#tests).
- `ComponentTypeRegistry::ResetForTests()` is behind `FLING_WITH_TEST_HOOKS`,
  which Gameplay defines only when `FLING_BUILD_TESTS` is ON.

### Phase 2.5: Rename Gameplay to Engine, and Engine to Application (branch `chore/engine-application-rename`)

- `FlingEngine/Gameplay/` is now `FlingEngine/Engine/`, the `Engine` module
  (`Fling::Engine`, `ENGINE_API`). See locked decision 8.
- The old `FlingEngine/Engine/` (`Engine.h`, `Engine.cpp`, `FlingEngine.h`) is
  now `FlingEngine/Application/`.
- `RegisterGameplayComponents()` is now `RegisterEngineComponents()`.
- The class `Fling::Engine` (`Engine::Run`) keeps its name for now. Renaming it
  changes every game's `main()`, so do it with the project model plan's
  `FLING_IMPLEMENT_GAME` work (phase P2).

### Phase 3: Graphics, Editor, and Application modules (done, branch `chore/finish-module-split`)

- Graphics (`PUBLIC_DEPS Core Resources Engine`), Editor
  (`PUBLIC_DEPS Engine Graphics`), and Application
  (`PUBLIC_DEPS Engine Graphics`) are `fling_add_module()` targets. The old
  `FlingEngine` library is gone: every folder under `FlingEngine/` is a module.
- `Fling::GameStack` (everything except Editor) and `Fling::EditorStack`
  (`GameStack` + Editor) are the INTERFACE targets that Sandbox links. The
  per-module test executables link one module each (see [Tests](#tests)).
- Editor-free debug-UI hook: `IDebugUI` (`Graphics/inc/IDebugUI.h`) has
  `Init(registry, world, game)` and `Draw(registry, dt)`. `VulkanApp::Init` and
  `ImGuiSubpass` take an `IDebugUI`. `BaseEditor` implements it; its `Init`
  replaces the `friend class Engine` access that set the world and game.
- `Engine::Run<Game>()` and `Engine::Run<Game, DebugUI>()` are two overloads.
  `Engine.h` has no `WITH_EDITOR` and does not include `BaseEditor.h`. Only
  Sandbox's `main()` still checks `WITH_EDITOR`, to pick the overload.
- Editor is a leaf. It is added only when `WITH_EDITOR_FLAG` is ON, and only
  editor builds link it. Sandbox compiles its `Editor/` sources only then.
  `WITH_EDITOR_FLAG=OFF` (and `WITH_IMGUI_FLAG=OFF`) build and run again.
- `ImGuiInputBinding.hpp` moved from `Graphics/inc` to `Graphics/src`, so
  ImGui is a PRIVATE dependency of Graphics.
- Isolation tests: Graphics cannot see Application or Editor, Application cannot
  see Editor, and each module's headers compile on their own.

Moved to the project model plan ([`project-model.md`](project-model.md),
phase P2), not done here:

- Separate `SandboxGame` / `SandboxEditor` targets (`fling_add_game()`).
- Removing `WITH_EDITOR` entirely (Sandbox `main()`, and `PRIVATE_WITH_EDITOR` in
  `FlingTypes.h`, which is an always-on `friend class BaseEditor;` today).

Still to do:

- **Vulkan, GLFW, SPIRV-Cross, and stb are PUBLIC dependencies of Graphics.**
  Graphics headers expose their types (`FlingVulkan.h`, `DesktopWindow.h`,
  `Shader.h`, `Texture.h`), so Editor and Application see them. Making them
  private needs those headers to stop exposing the types. The project model
  plan's window abstraction (phase P4) is a natural place to start.

### Phase 4: Prove the Physics story (done)

- `FlingEngine/Physics` (`PUBLIC_DEPS Core Engine`) is a stub module: a
  `RigidBody` component and `Physics::Step()`, which moves each `Transform` by
  its velocity. It is not called from the engine loop yet.
- `FlingTests/Physics/PhysicsTests.cpp` (`PhysicsTests`) tests `Physics::Step()`.
  The isolation test `PhysicsCannotSeeGraphics` proves that Graphics headers
  are not reachable.

## Tests

Goal: the test target is separate from everything else, and builds that do not
need tests contain no test code and do not need Catch2.

Done in Phase 0.5:

- `FLING_BUILD_TESTS` controls whether the test executables are added.
- `ctest --test-dir build` runs each Catch2 `TEST_CASE` as a CTest test.
- Test-only engine code (`ComponentTypeRegistry::ResetForTests()`) is behind
  `FLING_WITH_TEST_HOOKS`, set on the owning module only when
  `FLING_BUILD_TESTS` is ON (Phase 2).
- Module isolation tests (`fling_add_isolation_test` in
  `FlingTests/CMakeLists.txt`) run under CTest (Phase 2). They stay as they
  are: one header, one module, separate from the Catch2 executables.
- CI runs `ctest` (with `--no-tests=error`, so a broken test registration
  fails the job) instead of per-platform binary paths.
- CI has a Linux shipping job that builds with `FLING_BUILD_TESTS` at its
  default (OFF) and with Catch2 disabled, to prove that a shipping build does
  not need it.

Per-module executables (done):

- `CoreTests`, `ResourcesTests`, `EngineTests`, `PhysicsTests`, and
  `ApplicationTests`. Each links Catch2 (`Catch2::Catch2WithMain`) and only
  `Fling::<Module>`. Public dependencies of that module come along
  transitively, so a Graphics change does not relink `CoreTests`.
- Graphics and Editor have no test executable. They have no real tests.
- `ctest --test-dir build` runs every `TEST_CASE` from those executables and
  the isolation tests. Sources live in `FlingTests/<Module>/*.cpp`, picked up
  by `fling_add_module_tests` (`CONFIGURE_DEPENDS`). An empty glob adds no
  target.

## Success criteria

- A new module is one `fling_add_module()` call plus a folder. You do not edit
  a monolith glob.
- A build with the editor off does not compile or link any Editor sources and
  runs the deferred demo without the editor UI. A build with the editor on runs
  it with the editor UI. (Separate `SandboxGame` and `SandboxEditor` targets in
  one configure come with the project model plan.)
- Physics (stub) compiles against Core and Engine without Vulkan on its
  include path or link line.
- The per-module test executables pass. A shipping build with tests OFF
  contains no test hooks and does not need Catch2.

## Non-goals

- Edits under `external/`.
- Git submodules or separate repos per engine system.
- Rewriting the renderer or ECS, or changing asset pipelines.
- Mass-renaming includes to `Module/Header.h`.
- Exporting every type. Start with types used across module boundaries
  (singletons, `Engine`, `World`, `VulkanApp`).

## Risks

- **Singletons across shared libraries.** See locked decision 5.
- **MSVC import libraries.** A shared module with no exported symbols does
  not produce a `.lib`.
- **Runtime search path.** Shared modules need RPATH on Linux, and DLLs copied
  next to each executable on Windows.
- **Globs.** Module globs use `CONFIGURE_DEPENDS` so that new files re-run
  CMake.
