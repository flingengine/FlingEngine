# AGENTS.md

Guidance for AI coding assistants working in this repository. 
This file is the entry point — read it first. Topic-specific
detail lives under [`Skills/`](Skills/) and is linked from the relevant section below.

## What this project is

Fling Engine is a cross-platform Vulkan game engine in C++, currently built as one
CMake project with a `FlingEngine` library, a `Sandbox` game/editor, and a `FlingTests`
Catch2 test target. See the root [README.md](README.md) for the project pitch and
platform setup (Vulkan SDK, GLFW, etc.).

## Build

- First time in a fresh clone: `./Init.sh` (Linux) / `Init.bat` (Windows). This pulls
  git submodules, builds/installs Catch2 locally, and configures CMake into `build/`.
- Day to day: reconfigure `build/` for the mode you want, e.g.
  `cmake -B build -DCMAKE_BUILD_TYPE=Debug`, then build with your generator
  (e.g. `cmake --build build --parallel`).
- `-DDEFINE_SHIPPING=ON` strips dev-only code gated behind `#ifdef FLING_SHIPPING`.
- Full detail: [`Skills/building.md`](Skills/building.md).

## Tests

- Tests use Catch2 3, target `FlingTests`, source under `FlingTests/src`.
- Run the built binary directly, e.g. `./build/FlingTests/bin/FlingTests` on Linux
  (see `.github/workflows/build.yml` for the exact per-platform paths).
- Full detail, including how to add a new test file: [`Skills/testing.md`](Skills/testing.md).

## Coding style

- Canonical style doc: [`docs/CodingStyle.md`](docs/CodingStyle.md) — documentation
  comment conventions (no `@brief`, no `/*!`), enforced by
  `python3 scripts/check_comment_style.py`.
- Layout/formatting is enforced by `.clang-format` at the repo root (run
  `clang-format`, not manual formatting judgment).
- Applies to first-party code only (`FlingEngine/`, `Sandbox/`, `FlingTests/`).
  **Never edit code under `external/`** — it's vendored third-party code.
- AI-assistant-focused summary and common pitfalls: [`Skills/coding-style.md`](Skills/coding-style.md).

## Architecture / module layout

- Current folder layout and the in-progress module split (Core, Graphics, Gameplay,
  Resources, Editor) are documented in [`docs/BuildModules.md`](docs/BuildModules.md) —
  read it before restructuring includes or CMake targets, it records locked decisions.
- Orientation for where things live today: [`Skills/architecture.md`](Skills/architecture.md).
- Adding a new CMake module (folder + `fling_add_module`, with a UI example):
  [`Skills/adding-modules.md`](Skills/adding-modules.md).

## Contribution conventions

- Branching: feature branches off `main`, PR back into `main` when done and tested.
- CI (`.github/workflows/build.yml`) builds on Linux (GCC/Clang) and Windows
  (MSVC/MinGW) and runs `FlingTests` plus the comment-style check on every PR —
  make sure changes pass locally first.

## For Claude Code specifically

`CLAUDE.md` at the repo root imports this file directly (`@AGENTS.md`), so there is
nothing Claude-specific to duplicate here.
