# AGENTS.md

Guidance for AI coding assistants working in this repository. 
This file is the entry point — read it first. Topic-specific
detail lives under [`Skills/`](Skills/) and is linked from the relevant section below.

## What this project is

Fling Engine is a cross-platform Vulkan game engine in C++, currently built as one
CMake project with engine modules under `FlingEngine/` (Core, Resources, Engine, Graphics,
Application, Editor, ...), a `Sandbox` game/editor, and per-module Catch2
test executables under `FlingTests/`. See the root [README.md](README.md) for the project pitch and
platform setup (Vulkan SDK, GLFW, etc.).

## Build

- First time in a fresh clone: `./Init.sh` (Linux) / `Init.bat` (Windows). Both are
  thin wrappers around `scripts/init.cmake`, which pulls git submodules, builds/installs
  Catch2 locally, and configures CMake into `build/`. Change setup logic there, not in
  the wrappers.
- Day to day: reconfigure `build/` for the mode you want, e.g.
  `cmake -B build -DCMAKE_BUILD_TYPE=Debug`, then build with your generator
  (e.g. `cmake --build build --parallel`).
- `-DDEFINE_SHIPPING=ON` strips dev-only code gated behind `#ifdef FLING_SHIPPING`.
- The build compiles shaders to `build/Shaders` (`fling_add_shaders`). Do not
  commit `.spv` files.
- Full detail: [`Skills/building.md`](Skills/building.md).

## Tests

- Tests use Catch2 3. Each module that owns tests has an executable
  (`CoreTests`, `ResourcesTests`, `EngineTests`, `PhysicsTests`,
  `ApplicationTests`) built from `FlingTests/<Module>/`. Each links only that module.
- Run them with CTest, e.g. `ctest --test-dir build` (add `-C <Debug|Release>` for
  Visual Studio). That runs every `TEST_CASE` plus the isolation tests. On Linux
  one executable is `./build/FlingTests/bin/<Module>Tests` (for example
  `./build/FlingTests/bin/CoreTests`). The `FLING_BUILD_TESTS` CMake option
  controls whether these executables exist.
- Run the game with no window and no GPU:
  `Sandbox --null-render --unattended --frames 120 --seed 1`. It exits with
  `0` success, `1` failure, `2` crash, or `3` bad arguments. Use `--unattended` for
  every run with no human present, so that a crash cannot open a dialog.
- Drive a headless run with `--replay-input <file>`. Agents can write their own
  `.input.jsonl` files. Format and flags: [`Skills/input-playback.md`](Skills/input-playback.md).
- Full detail, including how to add a new test file: [`Skills/testing.md`](Skills/testing.md).

## Coding style

- Style doc: [`Skills/coding-style.md`](Skills/coding-style.md) — documentation
  comment conventions (no `@brief`, no `/*!`), enforced by
  `python3 scripts/check_comment_style.py`.
- Layout/formatting is enforced by `.clang-format` at the repo root (run
  `clang-format`, not manual formatting judgment).
- Applies to first-party code only (`FlingEngine/`, `Sandbox/`, `FlingTests/`).
  **Never edit code under `external/`** — it's vendored third-party code.

## Architecture / module layout

- Orientation for where things live today, and the in-progress module split
  (Core, Resources, Engine, Graphics, Application, Editor): [`Skills/architecture.md`](Skills/architecture.md) —
  read it before restructuring includes or CMake targets.
- Adding a new CMake module (folder + `fling_add_module`, with a UI example):
  [`Skills/adding-modules.md`](Skills/adding-modules.md).

## Plans

[`Plans/`](Plans/) holds in-progress design docs. [`Skills/`](Skills/) holds only
established standards. When a plan's phase is done, move the rules it establishes
into `Skills/`.

- Phased plan for the module split, including what is done and what is next:
  [`Plans/module-split-plan.md`](Plans/module-split-plan.md).
- How game projects use the engine (project descriptor, Game/Editor/Server
  targets, headless runs, project tooling):
  [`Plans/project-model.md`](Plans/project-model.md).
- How agents and scripts run, test, and edit the engine (headless deterministic
  runs, input record and replay, scenario tests, command layer, CLI/Python/MCP):
  [`Plans/improve-ai-iteration.md`](Plans/improve-ai-iteration.md).
- Timing API and fixed-tick loop (SimTick, tick sources, frame publishing for
  render interpolation and rollback): [`Plans/improve-time.md`](Plans/improve-time.md).
- How Graphics loads Vulkan at runtime with only the official SDK
  (`VK_NO_PROTOTYPES`, a function-pointer table, no import library), replacing
  the MSVC `/DELAYLOAD` fix: [`Plans/vulkan-loading.md`](Plans/vulkan-loading.md).
- Input events, the input thread, and per-tick input (timestamped event ring,
  readers, gamepad polling, input contexts, render-side look):
  [`Plans/improve-input.md`](Plans/improve-input.md).

## Contribution conventions

- Branching: feature branches off `main`, PR back into `main` when done and tested.
- Third-party code or data (a library in `external/`, a data file such as
  `Config/gamecontrollerdb.txt`): add it, with its license, to
  [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- CI (`.github/workflows/build.yml`) builds on Linux (GCC/Clang) and Windows
  (MSVC/MinGW) and runs the per-module Catch2 tests via CTest plus the comment-style check on every PR —
  make sure changes pass locally first.

## For Claude Code specifically

`CLAUDE.md` at the repo root imports this file directly (`@AGENTS.md`), so there is
nothing Claude-specific to duplicate here.

This repo ships a committed `.claude/settings.json` that registers the
[flingengine/skills-marketplace](https://github.com/flingengine/skills-marketplace)
marketplace and enables three of its plugins:

- `fling-engine` — build/test/coding-style/module-architecture skills.
- `gameplay-architecture` — strict (Overwatch-style) ECS design skill.
- `rollback-netcode` — rollback / client-side prediction netcode expert agent.

Opening this repo in Claude Code and trusting the folder will offer to install them
automatically — no manual `/plugin marketplace add` needed.
