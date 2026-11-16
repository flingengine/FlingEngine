# Testing

Fling Engine uses [Catch2 3](https://github.com/catchorg/Catch2) for first-party
tests. Each module that owns tests has its own executable. That executable links
Catch2 and only that module (`Fling::Core`, `Fling::Resources`, and so on).
Public dependencies of the module still come along transitively, so changing
Graphics relinks `ApplicationTests` and leaves `CoreTests` alone.

| Executable | Links | What it covers |
| --- | --- | --- |
| `CoreTests` | `Fling::Core` | `FlingTests/Core/`: string utils, command line, launch options, timing, random, logger, allocators, circular buffer |
| `ResourcesTests` | `Fling::Resources` | `FlingTests/Resources/`: `ResourceManager` / `FlingConfig`, `Json`, JSON serialization |
| `EngineTests` | `Fling::Engine` | `FlingTests/Engine/`: reflection, the type registry, world save/load, `Transform`, ECS systems |
| `PhysicsTests` | `Fling::Physics` + the selected provider | `FlingTests/Physics/`: `PhysicsCoreTests.cpp`: the clock, requests, and body bookkeeping, with a recording provider. `PhysicsProviderTests.cpp`: tests every provider must pass, including determinism, save/restore, and a golden hash that must match on every platform |
| `ApplicationTests` | `Fling::Application` | `FlingTests/Application/`: `Fling::Engine` smoke test (`Engine.h` lives in Application) |
| `GraphicsTests` | `Fling::Graphics` | `FlingTests/Graphics/`: interpolation in every exec mode, the present mode choice, render proxies |
| `SandboxExampleTests` | `Fling::Engine` plus four Sandbox system sources | `FlingTests/SandboxExample/`: the Sandbox look camera and stasis (`LookCameraTests.cpp`, `StasisTests.cpp`). It compiles `CameraStatusSystem.cpp`, `LookCameraSystem.cpp`, `LookCameraViewSystem.cpp` and `StasisSystem.cpp` from `Sandbox/Gameplay/src/Systems/`; no engine test target compiles Sandbox code |

Editor has no test executable. Add one only when that module has a real test: a `.cpp` under `FlingTests/<Module>/` is enough, once `fling_add_module_tests` is called for that module. `SandboxExampleTests` is the one test target added by hand (`add_executable` in `FlingTests/CMakeLists.txt`).

## Running tests

These executables are only built when the `FLING_BUILD_TESTS` CMake option is ON. It is
ON by default, except for shipping builds (see [`building.md`](building.md)).

Build them as part of a normal build, then run everything with CTest. That runs
every Catch2 `TEST_CASE` from every executable, plus the module isolation tests:

```bash
ctest --test-dir build                         # Linux / single-config generators
ctest --test-dir build -C <Debug|Release>      # Visual Studio
```

`scripts/run-tests.sh` does both steps: it builds every module test executable
that has sources, then runs CTest (Catch2 cases and isolation tests) with the
same failure and timeout flags as CI. Run it from a configured `build/` before
opening a pull request:

```bash
./scripts/run-tests.sh
```

Each Catch2 `TEST_CASE` is a separate CTest test, and they run from the repo
root.

`fling_set_bin_output` sets `RUNTIME_OUTPUT_DIRECTORY` to
`${CMAKE_CURRENT_BINARY_DIR}/bin`. The helper is called from
`FlingTests/CMakeLists.txt`, so on Linux each binary is:

```bash
./build/FlingTests/bin/CoreTests
./build/FlingTests/bin/ResourcesTests
./build/FlingTests/bin/EngineTests
./build/FlingTests/bin/PhysicsTests
./build/FlingTests/bin/ApplicationTests
./build/FlingTests/bin/GraphicsTests
./build/FlingTests/bin/SandboxExampleTests
```

With a multi-config generator (Visual Studio) the config directory is appended,
for example `build\FlingTests\bin\<Debug|Release>\CoreTests.exe`. With Ninja on
Windows it is `build\FlingTests\bin\CoreTests.exe`.

Catch2's CLI flags work on one executable, e.g.
`./build/FlingTests/bin/CoreTests "[tag]"` to filter, or `--list-tests` to see
that executable's cases. `ctest --test-dir build` runs every executable and the
isolation tests together.

The engine writes runtime logs to `Logs/` and creates the folder if it is
missing.

`ResourcesTests` copies `${FLING_ROOT_DIR}/Config` next to the executable after
the build. Shipping builds resolve `EngineConfigDir()` to a relative `Config`
path, and `ResourceManager::Init()` changes the working directory to the
executable directory.

## Adding a new test

- Add a new `.cpp` under `FlingTests/<Module>/` for the module the test should
  link (`Core`, `Resources`, `Engine`, `Graphics`, `Physics`, or `Application`).
  `fling_add_module_tests` globs `FlingTests/<Module>/*.cpp` with
  `CONFIGURE_DEPENDS`, so a new file is picked up on the next build: no CMake
  source-list edit. Re-run CMake configure if the file does not show up.
- Put the test in the lowest module whose headers the case needs. A file under
  `FlingTests/Core/` includes Core headers (`Logger.h`, `Timing.h`,
  `Misc/CommandLine.h`). World, JSON, and `Fling::Engine` belong in the module
  that owns those headers.
- Include `<catch2/catch_all.hpp>`. The executable's `main` comes from
  `Catch2::Catch2WithMain`, which the helper already links. Test sources leave
  `main` to that library.
- Use Catch2's `TEST_CASE` / `SECTION` macros; follow the coding-style rules in
  [`coding-style.md`](coding-style.md) for any doc comments you add.
- Prefer testing first-party engine code (`FlingEngine/`). Do not add tests that
  exercise `external/` internals directly.

## Module isolation tests

`FlingTests/CMakeLists.txt` also adds CTest tests named `Isolation: ...`
(`fling_add_isolation_test`). Each builds a one-line program that includes one
header and links one module. A `VISIBLE` test passes when it compiles. A
`HIDDEN` test passes only when the compiler reports that the header is not
found. When you split out a new module, add a test that its headers compile
alone and a test that it cannot see a module it must not depend on.

These tests build targets, so run them after a normal build. They are part of
`ctest --test-dir build`. They are separate from the per-module Catch2
executables and are not duplicated there.

## Headless runs

The game binary runs with no window and no GPU. CI and agents use this to run
the game on machines that have no display and no Vulkan device:

```bash
./build/Sandbox/bin/Sandbox --null-render --unattended --frames 120 --seed 1
```

| Flag | What it does |
| --- | --- |
| `--null-render` | No window, no swapchain, no GPU work. Ticks use a fixed delta of one tick length (1/30 s at the default `--sim-hz 30`) and do not wait on real time, and frames have no cap unless `--max-fps` asks for one. There is no developer UI, so the game starts at once |
| `--unattended` | No modal dialogs. A crash, a failed assert, or an uncaught exception prints a stack trace to stderr and exits with code 2 |
| `--frames N` | No tick runs after tick N (with `--frames-per-tick F`, tick `ceil(N / F)`). The run exits at the first frame where at least N main-thread frames have run and that last tick is shown. With `--null-render`, `--exec direct` and `inline` run exactly N frames; `--exec threaded` (the default) runs N ticks but usually more than N frames (467 to 491 frames for `--frames 120` in two runs). A windowed run lasts until that tick is due on the clock. With no `--frames`, a `--null-render` run continues until the game quits |
| `--seed S` | Seed the random number generator. Each run logs its seed, so you can repeat a run |
| `--log-format json` | JSON lines on the console and in `Logs/fling_log.jsonl` |
| `--exec M` | How play runs: `threaded` (the default), `direct` or `inline`. A test that is about one mode says it |
| `--sim-hz N` | The fixed simulation rate, 1 to 1000 (default 30). Recorded tick cuts replay only at their own rate |
| `--max-fps N` | At most N frames a second on the main thread (0: no cap) |
| `--vsync on\|off` | On (the default): FIFO, and a cap at or above the refresh rate is left to the display. Off: IMMEDIATE, else MAILBOX, else FIFO (no wait, may tear); the cap applies. The swap chain logs `Present mode ...`. Does nothing with `--null-render` (a warning says so) |
| `--interpolation on\|off` | Draw between the latest two published ticks (the default) or the latest tick only. Presentation only: checksums and tick dumps do not change |

A flag takes its value as `--frames 120` or `--frames=120`. The flags are parsed
by `LaunchOptions::Parse` (`FlingEngine/Core/inc/LaunchOptions.h`).

`--null-render` does not call `VulkanApp::Init`, so it does not open the Vulkan
loader. The Sandbox binary does not import `libvulkan` or `vulkan-1.dll`.
CTest checks that (`Sandbox: no Vulkan import`) and checks that every
first-party `vk[A-Z]` call is named in `VulkanFunctions.inl`
(`Vulkan calls are listed`).

Exit codes (`Fling::ExitCode` in `ExitCode.h`): `0` success, `1` scenario or
assertion failure, `2` crash or fatal error, `3` bad arguments or bad content.
At exit the engine prints one summary line to stdout, for example
`Fling exit: code=0 ticks=120 seed=1 render=null wall=0.031s` (a JSON object with
`--log-format json`).

Rules for code that can run under `--null-render`:

- Do not call `VulkanApp` from gameplay code. Under `--null-render`,
  `VulkanApp::Init` is not called, so `GetCurrentWindow()` and `GetCamera()` are
  null.
- A Graphics resource that makes GPU objects checks `VulkanApp::Get().HasDevice()`
  first and keeps its CPU data only when there is no device (see `Model` and
  `Texture`).
- Do not depend on a renderer hook to add components. For example, the renderer
  adds a `Transform` to a new `PointLight`, but under `--null-render` there is no
  renderer. Use `get_or_assign`.

`Sandbox/CMakeLists.txt` adds CTest tests (`Sandbox: ...`) that run the game
headless, check the JSON exit summary, and check that a bad argument exits with
code 3.

## Seeing the game without a window

An agent (or a script) can look at a run in two ways, and neither opens a window.
Read the state first: it is cheaper than an image and you can check it exactly. Use a
screenshot only when the question is about what the frame looks like.

### The state as text: `--dump-state`

```bash
./build/Sandbox/bin/Sandbox --null-render --unattended --frames 60 --seed 1 \
    --dump-state Saved/state.json --dump-state-ticks 10,60
# writes Saved/state_tick10.json and Saved/state_tick60.json
```

| Flag | What it does |
| --- | --- |
| `--dump-state F` | Write the simulation state as JSON to `F` (relative to the project folder, or absolute; missing folders are made). Works with `--null-render`: no GPU |
| `--dump-state-ticks L` | The ticks to write, a comma list such as `10,60`. Without it: the last tick of the run (`--frames` is then required) |

Each file is written on the game thread right after its tick, so it is the state the
tick left (`World::SaveStateToJson`). It holds:

- `tick`, `simHz`, `entityCount` and `stateHash` (the same hash as `--dump-ticks`).
- `entities`, in handle order: `entity` (`index:generation`), `name` when it has a
  `NameComponent`, `guid`, then one object for each component, with every reflected field.
- `singletons`: one object for each singleton.

The types are the ones the state hash covers (rollback state, Transient ones too) and
the ones a level saves. A type that is neither (a `RollbackClass::Keep` setting, a
render cache) is left out. The same seed, input and flags write the same bytes in
every exec mode, so two dumps can be diffed. It is not a level file: `--level` cannot
load it.

### The rendered frame: `--offscreen` and `--screenshot`

```bash
./build/Sandbox/bin/Sandbox --offscreen --unattended --frames 30 --seed 1 \
    --resolution 640x360 --screenshot Saved/shot.png --screenshot-ticks 1,30
# writes Saved/shot_tick1.png and Saved/shot_tick30.png
```

| Flag | What it does |
| --- | --- |
| `--offscreen` | Render with Vulkan into images that the renderer owns. No window, no surface, no window system extension, and no GLFW: it runs on a machine with no display, and opens nothing on a machine that has one. The timing is the same as `--null-render` (a virtual clock, no frame cap), so the ticks are the same as a `--null-render` run. There is no developer UI (ImGui needs a window). Cannot be used with `--null-render` |
| `--resolution WxH` | The size of the frames (`--offscreen`) or of the window. Overrides `[Engine] WindowWidth` and `WindowHeight`. A window is still made smaller to fit the screen's work area; offscreen frames are always this size |
| `--screenshot F` | Save the frame as a PNG file `F`. Works with `--offscreen` and with a window (then the editor UI is in the image too). Not with `--null-render` |
| `--screenshot-ticks L` | Save the first frame that shows each of these ticks. Without it: the last tick of the run (`--frames` is then required). The log line `Saved the frame of tick N (it shows tick M, WxH) to F` says which tick the frame shows |

File names: with one tick the path is used as it is. With more than one, `_tick<N>` goes
before the extension. `{tick}` in the path puts the tick where you want it
(`--screenshot frames/{tick}.png`). `CaptureFilePath` in `LaunchOptions.h` has the rule.

A capture that was asked for and not made (the tick never ran, the file cannot be
written) logs an error and the run exits with code 1, so a script never reads an
old file.

`--offscreen` needs a Vulkan driver, but a software one is enough: Mesa's lavapipe
(`llvmpipe`, package `mesa-vulkan-drivers` on Ubuntu) renders the Sandbox at 640x360
in about 5 s for 30 ticks. The same flags give the same image in every exec mode.
`FLING_TEST_OFFSCREEN=ON` adds the CTest `Sandbox: offscreen screenshots`; it is off by
default because CI machines may have no Vulkan driver. `Sandbox: headless state dump`
always runs.

To view a PNG, an agent reads the file (most agent tools show images). Choose a small
`--resolution` to keep the image cheap.

### Screenshots from the editor or from C++

The editor's GPU Options window (menu Windows; open in `--play` too) has a **Take
screenshot** button. It saves the next frame as it is shown, editor UI included, to
`Saved/Screenshots/Screenshot_<local time>_<n>.png`, and shows the path under the button.

Code anywhere in the engine can do the same with `Fling::Screenshot` (`Screenshot.h`, in
Core, so a module needs no Graphics header):

```cpp
#include "Screenshot.h"

// Any thread. Empty path: a new file under Saved/Screenshots
Fling::Screenshot::Request("Saved/door_open.png",
    [](const Fling::ScreenshotResult& t_Result)
    {
        // On the main thread, after the file is written (or not)
        F_LOG_TRACE("{}: {}", t_Result.Path, t_Result.Ok ? "saved" : t_Result.Error);
    });
```

The renderer saves every waiting request at its next frame, before the present, and logs
`Screenshot saved (WxH) to <path>` or the error. The callback is optional. Under
`--null-render` there is no renderer, so a request fails at the next frame with that
reason; a request still waiting when the run ends fails too. `--screenshot` uses this API.
Use it for tools, tests and debugging, not to feed results back into the simulation: the
callback arrives on the main thread, out of step with the ticks.

## Unattended crash tests

`FlingTests/CMakeLists.txt` adds `Unattended crash: <mode>` tests. Each runs
`CrashProbe`, which installs `CrashHandler::InstallUnattended` and then aborts,
fails an assert, writes to a null pointer, or throws. The test passes only if the
process exits with code 2 and prints a stack trace. A modal dialog makes the test
time out. These tests exist only when `WITH_CRASH_HANDLER_FLAG` is ON (see
[`building.md`](building.md#with_crash_handler_flag)).

## CI

Every PR runs the tests with CTest on Linux (GCC + Clang) and Windows (MSVC
Debug/Release + MSYS2 UCRT64) via `.github/workflows/build.yml`. A Linux shipping
job also builds with tests OFF and without Catch2. A test that only passes on one platform
is not done: check for platform-specific assumptions (path separators, endianness,
float precision) before considering a new test finished.
