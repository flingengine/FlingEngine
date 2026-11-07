#!/usr/bin/env bash
# Compares Fling's native ECS storage with EnTT 3.1 (the storage it replaced) on the
# same workloads. Linux, Clang. Everything it makes goes under build/ecs-bench.
#
#   scripts/EcsBenchmark/run.sh [entity counts...]     (default: 1000000 10000000)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/ecs-bench"
ENGINE="$OUT/engine"
SRC="$ROOT/scripts/EcsBenchmark/EcsBenchmark.cpp"
# The EnTT commit that external/entt pointed at before the engine removed it
ENTT_COMMIT=b652357a5c8babe16687e0525ca455fdb9d411ca
COUNTS=("${@:-1000000 10000000}")
mkdir -p "$OUT"

if [ ! -d "$OUT/entt" ]; then
    git clone -q https://github.com/flingengine/entt.git "$OUT/entt"
fi
git -C "$OUT/entt" checkout -q "$ENTT_COMMIT"

cmake -B "$ENGINE" -S "$ROOT" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DFLING_BUILD_TESTS=OFF > /dev/null
cmake --build "$ENGINE" --parallel --target Engine > /dev/null

COMMON=(-std=c++20 -O3 -DNDEBUG)
FLING=(-DFLING_CORE_STATIC=1 -DFLING_ENGINE_STATIC=1 -DFLING_RESOURCES_STATIC=1 -DGLM_FORCE_CXX17
    -I"$ROOT/FlingEngine/Engine/inc" -I"$ROOT/FlingEngine/Core/inc" -I"$ROOT/FlingEngine/Resources/inc"
    -I"$ENGINE/FlingEngine/Engine/Generated" -I"$ENGINE/FlingEngine/Core/Generated" -I"$ENGINE/FlingEngine/Resources/Generated"
    -isystem "$ROOT/external/glm" -isystem "$ROOT/external/spdlog/include")
LIBS=("$ENGINE/FlingEngine/Engine/libEngine.a" "$ENGINE/FlingEngine/Resources/libResources.a" "$ENGINE/FlingEngine/Core/libCore.a" -lpthread -ldl)

clang++ "${COMMON[@]}" -DBACKEND_ENTT=1 -I"$OUT/entt/src" "$SRC" -o "$OUT/bench_entt"
clang++ "${COMMON[@]}" "${FLING[@]}" "$SRC" "${LIBS[@]}" -o "$OUT/bench_fling_each"
clang++ "${COMMON[@]}" "${FLING[@]}" -DFLING_RANGE_FOR=1 "$SRC" "${LIBS[@]}" -o "$OUT/bench_fling_rangefor"

# One core, so the numbers do not move between cores. Fewer repeats for big counts.
for n in ${COUNTS[@]}; do
    reps=7; [ "$n" -ge 10000000 ] && reps=3
    for b in bench_entt bench_fling_each bench_fling_rangefor; do
        taskset -c 2 "$OUT/$b" "$n" "$reps"
    done
done
