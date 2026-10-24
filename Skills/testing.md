# Testing

Fling Engine uses [Catch2 3](https://github.com/catchorg/Catch2) for first-party
tests. Each module that owns tests has its own executable. That executable links
Catch2 and only that module (`Fling::Core`, `Fling::Resources`, and so on).
Public dependencies of the module still come along transitively, so changing
Graphics relinks `ApplicationTests` and leaves `CoreTests` alone.

| Executable | Links | What it covers |
| --- | --- | --- |
| `CoreTests` | `Fling::Core` | `FlingTests/Core/` — string utils, command line, timing, random, logger, allocators, circular buffer |
| `ResourcesTests` | `Fling::Resources` | `FlingTests/Resources/` — `ResourceManager` / `FlingConfig`, `Json`, `JsonArchive` |
| `EngineTests` | `Fling::Engine` | `FlingTests/Engine/` — `ComponentTypeRegistry`, world save/load, `Transform` / entt views |
| `PhysicsTests` | `Fling::Physics` | `FlingTests/Physics/` — `Physics::Step` |
| `ApplicationTests` | `Fling::Application` | `FlingTests/Application/` — `Fling::Engine` smoke test (`Engine.h` lives in Application) |

Graphics and Editor have no test executable. Add one only when that module has a real test: a `.cpp` under `FlingTests/<Module>/` is enough, once `fling_add_module_tests` is called for that module.

## Running tests

These executables are only built when the `FLING_BUILD_TESTS` CMake option is ON. It is
ON by default, except for shipping builds (see [`building.md`](building.md)).

Build them as part of a normal build, then run everything with CTest. That runs
every Catch2 `TEST_CASE` from every executable, plus the module isolation tests:

```bash
ctest --test-dir build                         # Linux / single-config generators
ctest --test-dir build -C <Debug|Release>      # Visual Studio
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
```

On Windows the config directory is appended, for example
`build\FlingTests\bin\<Debug|Release>\CoreTests.exe`.

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
  link (`Core`, `Resources`, `Engine`, `Physics`, or `Application`).
  `fling_add_module_tests` globs `FlingTests/<Module>/*.cpp` with
  `CONFIGURE_DEPENDS`, so a new file is picked up on the next build — no CMake
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

## CI

Every PR runs the tests with CTest on Linux (GCC + Clang) and Windows (MSVC
Debug/Release + MSYS2 UCRT64) via `.github/workflows/build.yml`. A Linux shipping
job also builds with tests OFF and without Catch2. A test that only passes on one platform
is not done — check for platform-specific assumptions (path separators, endianness,
float precision) before considering a new test finished.
