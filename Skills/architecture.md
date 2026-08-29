# Architecture / where things live

This is orientation for today's layout, not a design doc. For the in-progress
module split (Core/Resources/Gameplay/Graphics/Editor as separate CMake targets)
and its locked decisions, read [`docs/BuildModules.md`](../docs/BuildModules.md)
before touching CMake targets, `FLING_ENGINE_INC()`, or cross-folder includes —
several things that look like cleanups are explicitly deferred or ruled out there.

## Today

- `FlingEngine/` — the engine, currently one CMake library (`FlingEngine`) built
  from almost every source file under this tree via glob. Subfolders are
  `Core` (includes former Utils + Platform), `Graphics`, `Gameplay`,
  `Resources`, and `Editor`. CMake/includes still treat it as one target —
  `FLING_ENGINE_INC()` adds every engine `inc/` dir to consumers, so folder
  location doesn't currently enforce isolation. `fling_add_module()` lives in
  `CMake/FlingModule.cmake` but is not yet used for a real shared module.
- `Sandbox/` — the sample game + editor, one executable today (editor support is
  toggled by a project-wide `WITH_EDITOR` define, which `docs/BuildModules.md`
  plans to remove in favor of two separate executables).
- `FlingTests/` — Catch2 tests, links the full `FlingEngine` library. See
  [`testing.md`](testing.md).
- `external/` — vendored third-party dependencies (submodules). Never modified.
- `docs/` — human-facing docs: `docs/CodingStyle.md`, `docs/BuildModules.md`, and
  the `Fling-Engine-logo/` image used by the root README.
- `scripts/` — repo tooling (`check_comment_style.py`, the one-time
  `migrate_doxygen_comments.py` migration).
- `CMake/` — CMake helper modules (e.g. `FlingEngineInc.cmake`).

## Rules that hold regardless of the module-split's progress

- Gameplay code (`Transform`, `Camera`, `World`, `Game`) must not include Graphics
  headers (`MeshRenderer.h`, Vulkan-facing types). Graphics may include Gameplay.
- Editor is a leaf: `Engine` and `Graphics` must not include `BaseEditor` or any
  Editor header. If you find yourself adding such an include, that's the bug
  `docs/BuildModules.md` calls out explicitly — look for the debug-UI hook pattern
  it describes instead.
- Don't add a new module folder or CMake target without reading the "Locked
  decisions" section of `docs/BuildModules.md` first — several plausible-looking
  approaches (per-module PCH, git submodules per system) are explicitly rejected there.
  The how-to (including a UI module example) is [`adding-modules.md`](adding-modules.md).
