#!/bin/sh
# Runs inside the fresh-clone-test container (see Dockerfile). Any args are
# passed to Init.sh, e.g. -G Ninja.
set -eu

if [ ! -d /src/.git ]; then
	echo "Mount the Fling Engine repo at /src, e.g. docker run --rm -v \"\$PWD:/src:ro\" fling-fresh-clone" >&2
	exit 1
fi

# The mounted repo is owned by another user, which git refuses by default
git config --global --add safe.directory '*'

echo "== Cloning /src ($(git -C /src rev-parse --abbrev-ref HEAD) @ $(git -C /src rev-parse --short HEAD))"
git clone --quiet /src /work/FlingEngine
cd /work/FlingEngine

echo "== ./Init.sh $*"
./Init.sh "$@"

echo "== Building"
cmake --build build --parallel "$(nproc)"

echo "== Testing"
ctest --test-dir build --output-on-failure --no-tests=error --timeout 300

echo "== Fresh clone OK"
