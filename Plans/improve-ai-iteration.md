# Improve AI iteration plan

Design for how AI agents (and humans with scripts) build, run, test, and edit a
Fling project quickly: headless deterministic runs, input as data, scenario
tests, content validation, and one command layer that the editor, a CLI, a
Python library, and an optional MCP server all use.

This is a plan, not an established standard. When a phase is done, move the
rules that it establishes into [`Skills/`](../Skills/).

Related plans: [`project-model.md`](project-model.md) (headless flags, targets,
`scripts/fling.py`) and [`ecs-plan.md`](ecs-plan.md) (`World`, singleton
components, fixed step, reflection).

## Problem

- The main loop needs a window. `Engine::Tick`
  (`FlingEngine/Application/src/Engine.cpp`) loops on
  `VkApp.GetCurrentWindow()->ShouldClose()` and calls `VkApp.Update` every
  frame. You cannot run the simulation without Vulkan and GLFW.
- Time is wall-clock time. `Timing` is a singleton that reads real elapsed
  time, so two runs of the same level do not give the same result.
- Input comes only from the platform. `Input` is a static, string-keyed
  virtual singleton. Its only implementations are in `Graphics/src/Input/`.
  There is no way to record, replay, or inject input.
- Saved levels lose entity IDs and references between entities (see
  `ecs-plan.md`). A save does not give back the file that was loaded, so diffs
  of agent edits are noisy.
- An agent cannot know the fields of a component without reading C++ source.
  There is no schema and no level validator.
- The editor changes the registry directly from ImGui code. A script cannot do
  what a human does in the editor.
- On Windows, an assert or `abort()` can open a modal dialog. An unattended
  agent run then stops until a human closes it.

## Goals

1. Run any level headless, with a fixed step, as fast as the CPU allows, and
   exit with a meaningful exit code.
2. The same binary, seed, and input give the same result (same state hash).
3. Input is data. You can record it, replay it, and inject it.
4. Gameplay tests run as scenario files in CTest, with pass or fail and no
   human review.
5. An agent can write level JSON and validate it in milliseconds, before it
   starts the game.
6. Everything a human can do in the editor, a script can also do, through the
   same commands.
7. Low token cost for agents: text output first, short summaries, no
   always-loaded tool schemas that are not necessary.
8. Every tool is useful to a human too. Nothing is agent-only.

## Non-goals

- Bit-exact determinism across compilers, platforms, or CPU types. The target
  is the same binary on the same platform. Cross-platform lockstep is a
  separate project.
- A full scripting language for gameplay. Gameplay stays in C++.

## Design

### 1. Deterministic headless simulation

New launch flags on the same binary (these extend the flags in
`project-model.md`):

| Flag | What it does |
|------|--------------|
| `--null-render` | No window, no swapchain, no GPU work |
| `--unattended` | No modal dialogs. Asserts and crashes write to the log, print a stack trace, and exit with a non-zero code |
| `--frames N` | Run N fixed ticks, then exit |
| `--seed S` | Seed the world RNG |
| `--dump-state <file>` | At exit, write all serialized components as JSON |
| `--log-format json` | Write JSON lines logs to `Saved/Logs/` |

- The simulation advances by a fixed step from `SimTime` (see
  `ecs-plan.md`). In `--null-render` mode it does not sleep or wait for vsync.
- One RNG per world, seeded from `--seed`. Gameplay code does not use a global
  RNG.
- At exit, the engine prints a state hash: a hash of all serialized components
  in a stable order. A regression test can then check that the same seed and
  the same input give the same hash.
- `--unattended` must set the Windows CRT report mode so that asserts do not
  open a dialog. CI and agent runs always use it.
- Exit codes: `0` success, `1` scenario or assertion failure, `2` crash or
  fatal error, `3` bad arguments or bad content.

### 2. Input as data

Each fixed tick reads one `TickInput` from `InputState`. Devices only
write into it. Gameplay never reads GLFW or platform state directly.

```
GLFW device  ─┐
Replay file  ─┼─► TickInput (one per tick) ─► InputState ─► gameplay systems
Script / RPC ─┘
```

- `TickInput` holds the **raw** device state (keys, mouse, stick x/y,
  triggers) and the **resolved** actions and axes. Recording only the resolved
  actions would make dead-zone and response-curve code impossible to replay or
  tune.
- `--record-input <file>` writes the `TickInput` stream. `--replay-input
  <file>` reads it back. The file has a version field and the tick rate.
- A `VirtualInputDevice` takes frames from a replay file or from the command
  layer. It lives in Core or Engine, not in Graphics, so it works with
  `--null-render`.
- Input processing (dead zones, response curves, action mapping) is pure
  functions: raw values in, processed values out. Catch2 tests cover them
  directly, with no `World` and no engine.
- Aim assist and similar features are tested with scenarios (section 3): a
  level with targets, a scripted stick sweep, and assertions on the result.

### 3. Scenario tests

A scenario is a JSON file that the headless binary runs:

```json
{
  "level": "project://Levels/AimRange.json",
  "seed": 1,
  "frames": 300,
  "input": "project://Tests/Input/sweep_left.input",
  "expect": [
    { "entity": "Target_01", "component": "Health", "field": "value", "op": "<", "value": 100 }
  ]
}
```

- `<Name>Game --null-render --unattended --scenario <file>` runs it, prints a
  JSON result, and exits with `0` or `1`.
- `expect` can also check a state hash (`"stateHash": "..."`) for golden
  regression tests.
- Input can be a recorded file, or inline steps (for example "hold left stick
  at (-1, 0) for frames 0-60").
- A CMake helper registers each scenario file as a CTest test, so scenarios
  run in CI next to the Catch2 tests.
- Tests that need finer control construct a `World` in Catch2 directly (see
  `ecs-plan.md`).

### 4. Content authoring and validation

The JSON level format stays. These additions make agent edits safe:

- **Schema dump.** `--dump-schema <file>` writes a JSON Schema generated from
  the type registry: component names, field names, types, defaults, and
  ranges. An agent reads this instead of the C++ headers.
- **Validator.** `fling validate <files...>` checks levels against the schema
  and checks that referenced assets exist. Each error gives a JSON path, for
  example `entities[2].MeshRenderer.mesh: file not found`. It does not start
  the renderer.
- **Saves that round-trip.** Stable entity IDs, sorted keys, and default values
  left out. Loading and saving a level with no edits gives the same bytes, so a
  diff shows only real changes.
- **Prefabs** come later, after stable IDs.
- Asset references use the `project://` and `engine://` roots from
  `project-model.md`.

### 5. Command layer

The engine has one set of commands. All clients use it:

| Command | What it does |
|---------|--------------|
| `spawn`, `destroy` | Create or remove an entity, optionally with components |
| `get`, `set` | Read or write a component field by path, for example `Sphere.Transform.position` |
| `query` | List entities that match a set of components, with compact output |
| `step`, `pause`, `resume` | Control the simulation |
| `load`, `save` | Load or save a level |
| `input.push` | Push `TickInput`s to the virtual device |
| `screenshot` | Write the current frame to a file (needs a renderer) |
| `undo`, `redo` | Undo or redo editor commands |

- Commands are generic over the type registry and reflection. There is no
  hand-written command per component.
- **The editor UI calls the same commands.** This gives undo and redo, and it
  makes sure that a script can do anything a human does in the editor.
  Editor panels stop changing the registry directly.
- Transport: JSON-RPC 2.0 over a local socket (`--rpc-port <port>`, bound to
  localhost only), and over stdin and stdout for one-shot runs.
- Commands that change the world run at one point in the frame (a command
  queue drained by one system), the same as other deferred side effects in
  `ecs-plan.md`.

### 6. Clients

```
          ┌──────────────┐
          │ Engine/Editor│  command layer + JSON-RPC
          └──────┬───────┘
     ┌───────────┼───────────────┬──────────────┐
  Editor UI   Python lib      fling CLI      MCP server
              (fling pkg)   (wraps lib)    (optional, wraps lib)
```

- **Python package `fling`.** Standard library only. It grows from
  `scripts/fling.py` in `project-model.md`. It is a library
  (`w = fling.connect(); w.spawn("Enemy", Transform={...})`) and a CLI
  (`fling rpc set Sphere.Transform.position [0,2,0]`). Humans use it for batch
  edits and tooling.
- **CLI first, MCP last.** An MCP server's tool definitions are loaded into an
  agent's context in every session, even if they are not used. A CLI plus a
  short skill document costs nothing until it is used, and agents can already
  run it from a shell. The engine process holds the session state, so the CLI
  can connect again on each call and lose nothing.
- **MCP server (optional).** A thin wrapper over the Python package, for agents
  or tools that cannot run a shell. It adds no logic of its own.

### 7. Output for agents

- Text first. A state dump or a `query` result is cheaper to read and easier to
  check than a screenshot. Use screenshots and `--capture` (P4 in
  `project-model.md`) only for rendering work.
- Short, structured output: JSON lines logs, a JSON result for scenarios, and
  a one-line summary at exit.
- `fling check` builds, runs CTest and the comment-style check, and prints a
  short summary. It prints full output only for failures.

### 8. Agent documentation

- `Skills/agent-workflow.md`: how to run headless, write a scenario, validate a
  level, and use the CLI. Write it when phase C is done.
- The command reference and the schema are generated from the code, so they
  stay correct.

## Phases

Each phase must leave the engine, Sandbox, and FlingTests building and passing
on every CI platform.

| Phase | Work | Depends on |
|-------|------|------------|
| **A** | `--null-render`, `--unattended`, `--frames`, `--seed`, exit codes, JSON logs. Separate the main loop from `VkApp` | Overlaps P4 in `project-model.md` |
| **B** | `TickInput`, `VirtualInputDevice`, record and replay. Pure-function dead-zone and curve code with Catch2 tests | A. Lines up with `InputState` in `ecs-plan.md` |
| **C** | Scenario runner, state hash, CTest registration. `Skills/agent-workflow.md` | A, B |
| **D** | `--dump-schema`, `fling validate`, saves that round-trip with stable entity IDs | Reflection and stable IDs from `ecs-plan.md` |
| **E** | Command layer, JSON-RPC, Python package and CLI. Editor panels use commands, with undo and redo | D |
| **F** | Offscreen capture, optional MCP wrapper, hot reload (P6 in `project-model.md`) | E, P4, P6 |

Phases A to C do not need the EnTT replacement. They give agents crash
reproduction and gameplay tests without a window. Phases D and E work best on
top of the reflection in `ecs-plan.md`.

### Phase A status

Done. The rules that it establishes are in
[`Skills/testing.md`](../Skills/testing.md#headless-runs).

- `--null-render`, `--unattended`, `--frames N`, `--seed S`, and
  `--log-format json` are parsed by `LaunchOptions` (Core). `ExitCode` (Core)
  has the exit codes. `CrashHandler` (Core) has the unattended crash handling. `WITH_CRASH_HANDLER_FLAG=OFF` compiles it out.
- `Engine::Tick` no longer needs `VulkanApp`. Under `--null-render`,
  `VulkanApp::Init` is not called, ticks use a fixed 1/60 s delta, and the loop
  does not wait on real time. `--null-render` skips Graphics at runtime only.
  Graphics is still linked. A `<Name>Server` target that does not link Graphics
  is P5 in `project-model.md`.
- Graphics resources keep CPU data only when `VulkanApp::HasDevice()` is false.
- On MSVC, `vulkan-1.dll` is delay-loaded, so a `--null-render` run starts on a
  machine with no Vulkan runtime. Linux and MinGW still need the Vulkan loader
  installed. [`vulkan-loading.md`](vulkan-loading.md) fixes this on every
  platform.
- With no developer UI (a game build, or `--null-render`), the game starts at
  once. Before this change, only the editor's Play button started it.
- `--seed` seeds the global `Random`. The per-world RNG comes with T3 in
  `improve-time.md`. The tick clocks (`FixedTickClock`) come with T1 and T2.
- JSON logs go to the existing `Logs/` folder (`fling_log.jsonl`), not to
  `Saved/Logs/`. Move them when `project-model.md` adds `Saved/`.
- `--dump-state` is not done. It needs the state hash from phase C.
- CTest runs the Sandbox headless and runs `CrashProbe` to check that each kind
  of crash exits with code 2, with a stack trace and no dialog.

## Open questions

- Where does `TickInput` live: Core (with `Input`) or Engine (with
  `InputState`)?
- Format of input recordings: JSON lines (easy to read and edit) or binary
  (small)? JSON lines with optional compression is the current preference.
- Which fields go into the state hash? Some components (render handles, caches)
  must be left out.
- Does `--null-render` skip the Graphics module at link time (the `<Name>Server`
  target), or only at runtime?
- Socket transport: plain TCP with JSON-RPC, or WebSocket so a browser tool can
  connect too?
- How does the command layer handle permissions, if an RPC port is ever open to
  more than localhost?
