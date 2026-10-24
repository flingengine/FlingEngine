# Project model plan

Design for how a game project uses Fling Engine: how you create, rename, and
move a project, how game and editor code build into separate targets, and how
game clients, servers, and editors run headless or side by side.

This is a plan, not an established standard. When a phase is done, move the
rules that it establishes into [`Skills/`](../Skills/).

## Problem

- `Sandbox/main.cpp` and `Sandbox/Editor/src/SandboxEditor.cpp` use
  `#if WITH_EDITOR`. `Engine::Run<Game, Editor>` changes its template signature
  on `WITH_EDITOR` (`FlingEngine/Engine/inc/Engine.h`). Every new project must
  copy these checks into its own code.
- A project is one CMake target. Game code and editor code build into the same
  executable.
- `FlingEngine/Resources/src/FlingAssetPaths.cpp.in` puts absolute paths
  (`@FLING_ROOT_DIR@/Assets`, `/Config`, `/Logs`) into the binary. If you move
  or rename the folder, the binary breaks until you rebuild. Engine assets and
  project assets are in the same folder.
- There is no way to make a new project. The root `CMakeLists.txt` hard-codes
  `add_subdirectory( "Sandbox" )`.

## Goals

1. User game code has no `WITH_EDITOR` checks.
2. Separate Game, Editor, and dedicated Server targets from the same game code.
3. Fast editor iteration. Gameplay code changes show as soon as possible.
4. The editor can start N game clients and a dedicated server as preview
   instances.
5. Every mode can run headless, so AI agents can iterate without taking window
   focus from the user.
6. Creating, renaming, and moving a project has low friction.

## How other engines do it

| | Godot 4 | Unreal 5 | Unity 6 | O3DE | Bevy |
|---|---|---|---|---|---|
| Project identity | `project.godot` (fixed filename; the name is a field in it) | `<Name>.uproject` JSON with `EngineAssociation` | Folder + `ProjectSettings/ProjectVersion.txt` | `project.json` + user manifest `~/.o3de/o3de_manifest.json` | Cargo crate |
| Create | Project Manager UI | Launcher, templates | Unity Hub | `o3de create-project` CLI + templates | `cargo new`, Bevy CLI |
| Rename / move | Easy: edit `config/name`. All paths are `res://` relative | Hard: module names, class redirects, `.Target.cs` names | Easy: move the folder | Update the registry | Easy |
| Editor vs game code | One editor binary. Native code is a GDExtension shared library with `editor` and `template_release` builds | Target types Game, Editor, Client, Server, Program. Modules have `Type: Runtime` or `Editor`; game targets do not build editor modules | `Editor/` folders, asmdef platform filters | Gems with `.Editor` variants | None (feature flags) |
| Iteration | GDScript is instant. GDExtension hot reload (4.2+) | Live Coding patches the DLL in process | Domain reload | Rebuild | `dynamic_linking` feature |
| Multi-instance preview | "Customize Run Instances" (4.3+), separate processes | PIE: N clients + listen or dedicated server, in process or separate processes | Multiplayer Play Mode: virtual players in separate processes | None | None |
| Headless | `--headless` (dummy display and audio) | `-nullrhi`, `-RenderOffscreen`, `-unattended`, Server target | `-batchmode -nographics` | `-NullRenderer` | No window plugin |

What Fling takes from this:

- **A fixed descriptor filename and relative paths** (Godot) make rename and
  move free. Do not put the project name in filenames or identifiers that the
  user must keep in sync. This is the main pain point in Unreal.
- **Target types and module filtering** (Unreal) remove `WITH_EDITOR` from game
  code. Editor code is in a different module that only the editor target links.
- **Separate processes for preview instances** (Godot, Unity Multiplayer Play
  Mode). Fling has many singletons (`VulkanApp`, `ResourceManager`, `Logger`).
  In-process multi-world play would need all of them removed first. Processes
  also isolate crashes.
- **Headless is a runtime flag on the same binary.** The dedicated server is
  also its own target that does not link Graphics.

## Design

### Project descriptor

Each project has one file at its root with a fixed name: `fling.project`
(JSON). The project name is only in this file.

```json
{
  "name": "MyGame",
  "displayName": "My Game",
  "engine": "fling-main",
  "startLevel": "project://Levels/Start.json",
  "modules": [
    { "name": "Game", "type": "Runtime" },
    { "name": "Editor", "type": "Editor" }
  ]
}
```

- `engine` is an engine registry id or a path (relative or absolute).
- All paths are relative to the project root, or use a virtual root (see
  [Runtime paths](#runtime-paths)).

### Project layout

```
MyGame/
  fling.project
  CMakeLists.txt        # Finds the engine and calls fling_add_game()
  Source/Game/          # Runtime module. No editor code, no WITH_EDITOR.
  Source/Editor/        # Optional editor module (panels, inspectors)
  Assets/
  Config/
  Saved/                # Logs, caches, per-instance data (gitignored)
  build/                # (gitignored)
```

Sandbox moves to this shape (for example `Samples/Sandbox/`). It is the
dogfood project and it uses the engine the same way an external project does.

### Engine discovery

- `Init.sh` / `Init.bat` (or `fling register-engine`) writes a user registry,
  `~/.fling/engines.json`, that maps an engine id to a path.
- A project's CMake uses `find_package(FlingEngine CONFIG)` with a hint from
  `fling.project`.
- Samples in the engine repo use `add_subdirectory` directly.

### Game module API

User code registers its game and editor types with macros. It does not write
`main()` and it does not check `WITH_EDITOR`.

```cpp
// Source/Game/MyGame.cpp
FLING_IMPLEMENT_GAME( MyGame::Game )

// Source/Editor/MyGameEditor.cpp (only editor targets compile this)
FLING_IMPLEMENT_EDITOR( MyGame::Editor )
```

- The macros register factories in a `Fling::GameModuleDesc`. In the static
  build they use a static registrar. In the later shared-library build they
  emit an exported `FlingGetGameModule()`. The user source is the same for both.
- `Engine::Run(const LaunchConfig&)` replaces `Engine::Run<...>`.
  `LaunchConfig` holds the mode (Game, Editor, Server), `headless`, the project
  root, and the net role. The engine gets the game and editor from the
  registered factories.
- `Engine.h` does not include `BaseEditor.h`.
- `PRIVATE_WITH_EDITOR` (`FlingEngine/Core/inc/FlingTypes.h`) must go. Its
  replacement (reflection or an explicit inspector access pattern) is an open
  question.

### Targets

A new `CMake/FlingGame.cmake` adds `fling_add_game(<Name> ...)`. It reads
`name` from `fling.project` at configure time and makes these targets:

| Target | Contents |
|--------|----------|
| `<Name>Runtime` | STATIC library of `Source/Game` |
| `<Name>Game` | Engine `main` + `<Name>Runtime` + `Fling::GameStack` |
| `<Name>Editor` | Engine `main` + `<Name>Runtime` + `Source/Editor` + `Fling::EditorStack`. Only when `FLING_BUILD_EDITOR` is ON (replaces `WITH_EDITOR_FLAG`) |
| `<Name>Server` | Engine `main` + `<Name>Runtime` + a stack without Graphics |

- The engine supplies `main()`. The user does not write it.
- `<Name>Server` needs Gameplay to not depend on Graphics (module-split
  Phase 2 isolation). Until then, use `<Name>Game --null-render --server`.
- To rename the targets, edit `name` and reconfigure.

### Runtime paths

This is what makes a move work.

- Remove the absolute paths from `FlingAssetPaths.cpp.in`.
- At startup, find the project root in this order:
  1. `--project <path>`
  2. `fling.project` next to the executable (shipping or staged builds)
  3. Search up from the current directory
- Split `FlingPaths` into `EngineContentDir()` (engine shaders and default
  assets) and `ProjectAssetsDir()`, `ProjectConfigDir()`, `ProjectSavedDir()`.
- Asset and level references use virtual roots, `project://` and `engine://`
  (like Godot's `res://`), so they stay valid after a move.
- Config layers: engine defaults, then project `Config/`, then the command
  line. This uses the existing `CommandLine::LoadConfigFile`.

### Headless and multiple instances

- `--headless`: no window and no swapchain. Vulkan renders offscreen.
  `--capture <dir>` writes frames to disk so an agent can look at the output.
- `--null-render`: no GPU work at all (CI, servers).
- Both need a window and surface abstraction in Graphics. Today
  `VulkanApp::Init` always makes a GLFW window.
- **Play in editor** starts child processes: one server and N clients
  (`--client --connect=<addr> --instance=<i>`). Each instance writes logs and
  config to `Saved/Instances/<i>/`.
- A client runs in its own window, or `--headless` with frames sent back into
  an editor viewport (later: shared memory or Vulkan external memory).
- Later: an agent control channel (stdin JSON or a local socket) to step
  frames, take screenshots, query entities, and inject input.

### Fast iteration

- **Now:** separate executables, incremental static builds, and fast restart
  of child instances.
- **Later:** after `Singleton<T>::Get()` moves out of headers (module-split
  locked decision 5), `<Name>Runtime` becomes a shared library in editor builds
  only. The editor watches the build output, copies the library to a unique
  name (Windows locks loaded DLLs), unloads the old one, loads the new one, and
  registers components again. ECS state stays in the registry, so
  `ComponentTypeRegistry` must accept registration again. Shipping builds stay
  static.

### Project tool: `scripts/fling.py`

A Python script that uses only the standard library. CI already needs
`python3`.

| Command | What it does |
|---------|--------------|
| `fling new <path> [--name N] [--template blank\|sandbox]` | Copies `Templates/<template>/`, writes `fling.project`, optionally runs `git init` |
| `fling rename <NewName>` | Changes `name` and `displayName`. Optionally does a guided replace of the C++ namespace. Does not rename files that the build needs |
| `fling doctor` | Checks the descriptor, finds the engine, reports absolute paths that are not correct |
| `fling build\|run [--target game\|editor\|server] [--headless] [--instances N]` | Wrappers around `cmake --build` and the launch arguments |
| `fling register-engine` | Writes `~/.fling/engines.json` |

There is no `fling move`, because all paths are relative. Later, the editor's
Project Manager screen calls the same commands.

## Phases

Each phase must leave the engine, Sandbox, and FlingTests building and passing
on every CI platform.

| Phase | Work | Depends on |
|-------|------|------------|
| **P0** | This document | None |
| **P1** | Runtime path resolution and `fling.project` for Sandbox. Remove absolute paths | None |
| **P2** | `LaunchConfig`, `FLING_IMPLEMENT_GAME` / `FLING_IMPLEMENT_EDITOR`, `fling_add_game()`. Remove `WITH_EDITOR` from `Engine.h` and Sandbox | Module-split Phase 3 (replaces its Sandbox split bullet) |
| **P3** | `scripts/fling.py` (`new`, `rename`, `doctor`, `run`), `Templates/Blank`. Move Sandbox to `Samples/` | P1, P2 |
| **P4** | `--headless`, `--null-render`, window abstraction, frame capture | Graphics module |
| **P5** | `<Name>Server` target and multi-instance launch from the editor | Module-split Phase 2, P4 |
| **P6** | Shared-library game module and hot reload | Singletons defined in their owning modules |

## Open questions

- Descriptor format: JSON, or INI to match `EngineConf.ini`?
- Default `engine` value: registry id or path?
- What replaces `PRIVATE_WITH_EDITOR` (reflection)?
- Network transport for client and server preview instances.
- Does `fling.py` stay in Python, or move to C++ when the editor gets a
  Project Manager?
