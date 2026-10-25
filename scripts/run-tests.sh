#!/bin/sh
# Compile and run every unit test. Use this before opening a pull request.
#
#   ./scripts/run-tests.sh
#
# Uses the existing build/ directory (run ./Init.sh once if it is missing).
# BUILD_DIR picks another directory. On a Visual Studio build, CONFIG picks
# Debug or Release (default Debug).
#
# Builds each module executable that FlingTests/CMakeLists.txt registers and
# that has sources (CoreTests, ResourcesTests, and so on). CTest then runs
# those cases and the isolation tests. Same flags as CI: fail when nothing is
# registered, and stop a hung test after 300 seconds.

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_dir=${BUILD_DIR:-"$root/build"}
list="$root/FlingTests/CMakeLists.txt"

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
	echo "No CMake build at $build_dir. Run ./Init.sh first." >&2
	exit 1
fi

if ! grep -q '^FLING_BUILD_TESTS:BOOL=ON$' "$build_dir/CMakeCache.txt"; then
	echo "FLING_BUILD_TESTS is OFF in $build_dir." >&2
	echo "Reconfigure with: cmake -B \"$build_dir\" -DFLING_BUILD_TESTS=ON" >&2
	exit 1
fi

if [ ! -f "$list" ]; then
	echo "Missing $list" >&2
	exit 1
fi

if command -v nproc >/dev/null 2>&1; then
	jobs=$(nproc)
else
	jobs=1
fi

# Multi-config generators (Visual Studio) need --config / ctest -C.
# Single-config generators ignore a mismatched -C, so only pass it here.
config=""
if grep -q '^CMAKE_CONFIGURATION_TYPES:' "$build_dir/CMakeCache.txt"; then
	config=${CONFIG:-Debug}
fi

targets=""
modules=$(sed -n 's/^[[:space:]]*fling_add_module_tests([[:space:]]*\([A-Za-z0-9_]*\)[[:space:]]*).*/\1/p' "$list")
for module in $modules; do
	if find "$root/FlingTests/$module" -maxdepth 1 -name '*.cpp' 2>/dev/null | grep -q .; then
		targets="$targets ${module}Tests"
	fi
done

if [ -z "$targets" ]; then
	echo "No module test sources under $root/FlingTests." >&2
	exit 1
fi

# Intentional split: target names come from CMakeLists and contain no spaces.
# shellcheck disable=SC2086
set -- $targets

cd "$root"

echo "Building: $*"
if [ -n "$config" ]; then
	cmake --build "$build_dir" --parallel "$jobs" --config "$config" --target "$@"
else
	cmake --build "$build_dir" --parallel "$jobs" --target "$@"
fi

echo "Running tests"
if [ -n "$config" ]; then
	ctest --test-dir "$build_dir" -C "$config" --output-on-failure --no-tests=error --timeout 300 -j "$jobs"
else
	ctest --test-dir "$build_dir" --output-on-failure --no-tests=error --timeout 300 -j "$jobs"
fi
