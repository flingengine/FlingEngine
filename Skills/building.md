# Building Fling Engine

## First-time setup

Run once per fresh clone (or after `external/` submodule updates):

```bash
./Init.sh    # Linux/macOS
Init.bat     # Windows
```

Both are thin wrappers around one platform-agnostic CMake script,
[`scripts/init.cmake`](../scripts/init.cmake) (run with `cmake -P`), so the setup
logic lives in one place. CI runs the same script to install Catch2. It does three
things, stopping at the first failure:

1. `git submodule update --init --recursive` — pulls all vendored deps under `external/`.
2. Builds and installs Catch2 locally to `external/Catch2/install`, in both Debug and
   Release (no `sudo` needed; `find_package(Catch2 3 REQUIRED)` in
   `FlingTests/CMakeLists.txt` needs this when `FLING_BUILD_TESTS` is ON).
3. Configures CMake into `build/` with default settings.

Arguments to `Init.sh` / `Init.bat` are passed through to the `build/` configure
(e.g. `./Init.sh -G Ninja -DCMAKE_BUILD_TYPE=Debug`). Arguments of the form
`-DFLING_INIT_<OPTION>=<value>` are options for the script itself:

| Option | Effect |
| --- | --- |
| `FLING_INIT_CONFIGS` | Catch2 configs to build (default `Debug;Release`) |
| `FLING_INIT_SKIP_SUBMODULES=ON` | Skip step 1 |
| `FLING_INIT_SKIP_TESTS=ON` | Skip step 2 and configure with `FLING_BUILD_TESTS=OFF` |
| `FLING_INIT_SKIP_CONFIGURE=ON` | Skip step 3 |

Change setup steps in `scripts/init.cmake`, not in the wrappers.

### Testing a fresh clone

The `fresh-clone` CI job runs `Init.sh` (in a clean Ubuntu container) and `Init.bat`
on a checkout without submodules, then builds and runs the tests. To run the Linux
one locally with Docker or Podman, from the repo root:

```bash
docker build -t fling-fresh-clone scripts/fresh-clone-test
docker run --rm -v "$PWD:/src:ro" fling-fresh-clone
```

It clones the checked-out branch of your repo, so only committed changes are tested.

Minimum compilers: GCC 13, Clang 16, or Visual Studio 2022. CMake stops with an
error on older GCC or Clang: the vendored EnTT 3.1 hangs at runtime on them in
C++20 mode (its `entt::null` comparison calls itself forever). On Ubuntu 22.04,
install `g++-13` or newer and configure with `CC=gcc-13 CXX=g++-13`.

Linux also needs system packages before this will configure cleanly (GLFW, Vulkan
headers, X11/Wayland dev libs) — see the root [README.md](../README.md) for the
`apt-get` list, or `.github/workflows/build.yml` for the CI package list.

## Day-to-day builds

Reconfigure `build/` for the mode you want (this does **not** build — build
separately), e.g.:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug                                # debug
cmake -B build -DCMAKE_BUILD_TYPE=Release                              # release
cmake -B build -DCMAKE_BUILD_TYPE=Release -DDEFINE_SHIPPING=ON         # shipping
cmake -B build -DFLING_BUILD_TESTS=OFF                                 # no test executables, no Catch2 needed
```

Then build with your generator, e.g.:

```bash
cmake --build build --parallel $(nproc)   # $(nproc) uses all available CPU cores
```

There used to be `build-debug.sh`/`build-release.sh`/`build-shipping.sh` wrapper
scripts for the commands above; they were removed since CI and IDE tooling
(VS Code's cmake-tools extension) never used them and they only added an
indirection layer over a couple of CMake flags.

## `DEFINE_SHIPPING`

`-DDEFINE_SHIPPING=ON` sets `FLING_SHIPPING`, used in first-party code to strip
dev-only paths:

```cpp
#ifdef FLING_SHIPPING
// shipping-only behavior
#else
// dev behavior, verbose logging, etc.
#endif
```

It also switches Asset paths from absolute (convenient for iterating without
reconfiguring) to relative, so a shipped executable is portable.

It also turns off `WITH_INPUT_RECORDING` (a PUBLIC definition on Core, 0 or 1).
Shipping builds then have no input recording or playback: no `InputRecorder`,
`InputPlayback` or recording file code, no "Recording + Playback" editor section,
and `--record-input` / `--replay-input` exit with code 3. A shipped game cannot be
driven by an input file. Guard code that uses them with `#if WITH_INPUT_RECORDING`.
`VirtualInputDeviceInterface` stays, because only code can make one.

## `WITH_CRASH_HANDLER_FLAG`

ON by default. It builds `CrashHandler` (Core), which `--unattended` uses to
stop crash dialogs and to print a stack trace (see
[`testing.md`](testing.md#headless-runs)). It has no cost until a crash: it
installs once at startup, and only with `--unattended`.

Configure with `-DWITH_CRASH_HANDLER_FLAG=OFF` to compile it out, for example
when a shipping title has its own crash reporter that must own the
unhandled-exception filter and signal handlers. Then `CrashHandler` functions do
nothing, Core does not link `dbghelp` on Windows, the `Unattended crash: ...`
tests are not added, and `--unattended` logs a warning that a crash can open a
dialog. Code reads the setting as `WITH_CRASH_HANDLER` (0 or 1), a PUBLIC
definition on Core.

## `FLING_BUILD_TESTS`

Controls whether the per-module Catch2 executables exist (`CoreTests`,
`ResourcesTests`, `EngineTests`, `PhysicsTests`, `ApplicationTests`). When OFF,
CMake does not look for Catch2 and does not add `FlingTests/`. It defaults to ON,
except for shipping builds (`DEFINE_SHIPPING=ON`) and when Fling is added to
another project with `add_subdirectory()`. Pass `-DFLING_BUILD_TESTS=ON` to test
a shipping build. Run them with `ctest --test-dir build` (see
[`testing.md`](testing.md)).

Like any CMake option, the value is cached in `build/` after the first configure:
switching `DEFINE_SHIPPING` on an existing build dir does not change it.

## CI reference

`.github/workflows/build.yml` is the ground truth for exactly what gets built and
with which flags across Linux GCC, Linux Clang, Windows MSVC (Debug + Release), and
Windows MSYS2 UCRT64 (MinGW-w64 GCC) — check it if a build works locally but fails in CI, or vice versa.
