#!/bin/sh
# Fling Engine first-time setup. All of the logic lives in scripts/init.cmake
# (shared with Init.bat); any args are passed through to it, e.g.
#   ./Init.sh -G Ninja -DCMAKE_BUILD_TYPE=Debug

if ! command -v cmake >/dev/null 2>&1; then
	echo "cmake was not found on PATH. Install CMake 3.21 or newer and run this again." >&2
	exit 1
fi

cd "$(dirname "$0")" || exit 1
exec cmake -P scripts/init.cmake -- "$@"
