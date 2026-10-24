# Building Fling Engine

## First-time setup

Run once per fresh clone (or after `external/` submodule updates):

```bash
./Init.sh    # Linux/macOS
Init.bat     # Windows
```

This does three things:

1. `git submodule update --init --recursive` — pulls all vendored deps under `external/`.
2. Builds and installs Catch2 locally to `external/Catch2/install` (no `sudo` needed;
   `find_package(Catch2 3 REQUIRED)` in `FlingTests/CMakeLists.txt` needs this when
   `FLING_BUILD_TESTS` is ON).
3. Configures CMake into `build/` with default settings.

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
cmake -B build -DFLING_BUILD_TESTS=OFF                                 # no FlingTests, no Catch2 needed
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

## `FLING_BUILD_TESTS`

Controls whether the `FlingTests` target exists at all. When OFF, CMake does not
look for Catch2. It defaults to ON, except for shipping builds
(`DEFINE_SHIPPING=ON`) and when Fling is added to another project with
`add_subdirectory()`. Pass `-DFLING_BUILD_TESTS=ON` to test a shipping build.

Like any CMake option, the value is cached in `build/` after the first configure:
switching `DEFINE_SHIPPING` on an existing build dir does not change it.

## CI reference

`.github/workflows/build.yml` is the ground truth for exactly what gets built and
with which flags across Linux GCC, Linux Clang, Windows MSVC (Debug + Release), and
Windows MSYS2 UCRT64 (MinGW-w64 GCC) — check it if a build works locally but fails in CI, or vice versa.
