# Testing

Fling Engine uses [Catch2 3](https://github.com/catchorg/Catch2) for first-party
tests. The test target is `FlingTests`, built from `FlingTests/src`, and links the
full `FlingEngine` library, so tests can exercise real engine
code, not just isolated units.

## Running tests

Build the `FlingTests` target as part of a normal build, then run the resulting
binary directly:

```bash
# Linux
./build/FlingTests/bin/FlingTests

# Windows (path includes the build config)
build\FlingTests\bin\<Debug|Release>\FlingTests.exe
```

Catch2's CLI flags work as usual, e.g. `./build/FlingTests/bin/FlingTests "[tag]"`
to filter, or `--list-tests` to see everything.

`Logs/` should exist before running (CI does `mkdir -p Logs` first) — the engine
writes runtime logs there.

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

## CI

Every PR runs `FlingTests` on Linux (GCC + Clang) and Windows (MSVC Debug/Release +
MinGW64) via `.github/workflows/build.yml`. A test that only passes on one platform
is not done — check for platform-specific assumptions (path separators, endianness,
float precision) before considering a new test finished.
