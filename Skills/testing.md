# Testing

Fling Engine uses [Catch2 3](https://github.com/catchorg/Catch2) for first-party
tests. The test target is `FlingTests`, built from `FlingTests/src`, and links the
whole engine except the editor (`Fling::GameStack`, plus `Fling::Physics`), so tests can exercise real engine
code, not just isolated units.

## Running tests

`FlingTests` is only built when the `FLING_BUILD_TESTS` CMake option is ON. It is
ON by default, except for shipping builds (see [`building.md`](building.md)).

Build the `FlingTests` target as part of a normal build, then run all tests
with CTest:

```bash
ctest --test-dir build                         # Linux / single-config generators
ctest --test-dir build -C <Debug|Release>      # Visual Studio
```

Each Catch2 `TEST_CASE` is a separate CTest test, and they run from the repo
root. You can also run the binary directly:

```bash
# Linux
./build/FlingTests/bin/FlingTests

# Windows (path includes the build config)
build\FlingTests\bin\<Debug|Release>\FlingTests.exe
```

Catch2's CLI flags work as usual, e.g. `./build/FlingTests/bin/FlingTests "[tag]"`
to filter, or `--list-tests` to see everything.

The engine writes runtime logs to `Logs/` and creates the folder if it is
missing.

## Adding a new test

- Add a new `.cpp` under `FlingTests/src` (see existing files like
  `ResourceTests.cpp`, `UtilsTests.cpp`, `RendererTests.cpp` for structure/naming).
  `FlingTests/CMakeLists.txt` globs sources, so a new file is picked up automatically —
  no CMake edit needed for a new test file, just re-run CMake configure if it doesn't
  show up.
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
`ctest --test-dir build`.

## CI

Every PR runs the tests with CTest on Linux (GCC + Clang) and Windows (MSVC
Debug/Release + MSYS2 UCRT64) via `.github/workflows/build.yml`. A Linux shipping
job also builds with tests OFF and without Catch2. A test that only passes on one platform
is not done — check for platform-specific assumptions (path separators, endianness,
float precision) before considering a new test finished.
