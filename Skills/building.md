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
   `find_package(Catch2 3 REQUIRED)` in `FlingTests/CMakeLists.txt` needs this).
3. Configures CMake into `build/` with default settings.

Linux also needs system packages before this will configure cleanly (GLFW, Vulkan
headers, X11/Wayland dev libs) — see the root [README.md](../README.md) for the
`apt-get` list, or `.github/workflows/build.yml` for the CI package list.

## Day-to-day builds

Three convenience scripts reconfigure CMake for a given mode (they do **not**
build — build separately):

```bash
./build-debug.sh      # -DCMAKE_BUILD_TYPE=Debug
./build-release.sh    # -DCMAKE_BUILD_TYPE=Release
./build-shipping.sh   # -DCMAKE_BUILD_TYPE=Release -DDEFINE_SHIPPING=ON
```

Then build with your generator, e.g.:

```bash
cmake --build build --parallel
```

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

## CI reference

`.github/workflows/build.yml` is the ground truth for exactly what gets built and
with which flags across Linux GCC, Linux Clang, Windows MSVC (Debug + Release), and
Windows MinGW64 — check it if a build works locally but fails in CI, or vice versa.
