# Architecture / where things live

This is orientation for today's layout, not a design doc. The engine is
mid-way through an in-progress module split (Core/Resources/Engine/Graphics/
Application/Editor as separate CMake targets) — the rules below hold regardless of how far that
split has progressed. Read [`adding-modules.md`](adding-modules.md) before
touching CMake targets or cross-folder includes. The phases of the split are in
[`Plans/module-split-plan.md`](../Plans/module-split-plan.md).

## Today

- `FlingEngine/Core` — the `Core` module (`Fling::Core`, built with
  `fling_add_module()`): platform macros, logging, math, memory, timing,
  `FlingPaths`, and the platform-independent `Input` class. Core includes and
  links no other engine module, and its own include path enforces that.
  `WITH_IMGUI`, `WITH_EDITOR`, and `FLING_SHIPPING` are PUBLIC definitions on
  Core, so every target sees them.
- `FlingEngine/Resources` — the `Resources` module (depends on Core): config
  files, JSON, file loading, `ResourceManager`. GPU resources (`Texture`,
  `HDRImage`) live in Graphics.
- `FlingEngine/Engine` — the `Engine` module (depends on Core and Resources):
  the foundational world layer. `World`, `Level`, the `Game` base class,
  cameras, `Transform`, and the component type registry. It cannot include
  Graphics, Editor, or Application headers. Actual gameplay systems (AI,
  abilities, ...) do not go here: they get their own modules that depend on
  Engine. The name `Gameplay` is reserved for those.
- The rest of `FlingEngine/` (`Application`, `Graphics`, `Editor`) is still one
  CMake library (`FlingEngine`) built by glob. It links the modules above and
  exposes its folders' `inc/` dirs as `PUBLIC` include paths, so those three
  folders are not isolated from each other yet.
  `Application/` is the top-level startup layer: `Engine.h`/`Engine.cpp`
  (`Engine::Run`, the main loop) and `FlingEngine.h`. It boots a game client,
  server, test, or editor application. The GLFW input
  backends (`LinuxInput`, `WindowsInput`, keycodes) live in `Graphics/`, next to
  the window.
- `Sandbox/` — the sample game + editor, one executable today (editor support is
  toggled by a project-wide `WITH_EDITOR` define; a planned follow-up splits
  this into two separate executables).
- `FlingTests/` — Catch2 tests, links the full `FlingEngine` library. See
  [`testing.md`](testing.md).
- `external/` — vendored third-party dependencies (submodules). Never modified.
- `scripts/` — repo tooling (`check_comment_style.py`, the one-time
  `migrate_doxygen_comments.py` migration).
- `CMake/` — CMake helper modules: `FlingModule.cmake` (`fling_add_module`),
  `FlingTargets.cmake` (`fling_configure_target` and friends, shared settings
  for every first-party target), `FlingExternals.cmake` (`FlingExt::*` targets
  for header-only third-party libraries — link these instead of adding
  `include_directories()`).

## Rules that hold regardless of the module-split's progress

- The Engine module (`Transform`, `Camera`, `World`, `Game`) must not include
  Graphics headers (`MeshRenderer.h`, Vulkan-facing types). Graphics may include
  Engine. Gameplay modules (AI, abilities, ...) depend on Engine, never on Graphics.
- Editor is a leaf: Application and Graphics must not include `BaseEditor` or any
  Editor header. If you find yourself adding such an include, that's a bug —
  look for the debug-UI hook pattern instead of a direct include.
- Don't add a new module folder or CMake target on a whim — per-module PCH and
  git submodules per engine system have already been considered and rejected
  for this project. The how-to (including a UI module example) is
  [`adding-modules.md`](adding-modules.md).
