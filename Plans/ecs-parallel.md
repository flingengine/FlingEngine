# ECS parallel execution and parallel loading

Status: proposal for review. Nothing here is implemented yet.

This plan builds on [`ecs-systems.md`](ecs-systems.md) (access lists,
phases, and stages) and [`ecs-features.md`](ecs-features.md) (worlds and
streaming). It covers:

- running independent systems at the same time (between systems),
- splitting one system's work across workers (inside a system),
- what makes this safe, and how we test it,
- decoding level data in parallel for large and streamed worlds.

The thread rules in [`Skills/threading.md`](../Skills/threading.md) still
apply. The job system here is the separate type that rule 6 reserves.

## Source

Timothy Ford, *'Overwatch' Gameplay Architecture and Netcode*, GDC 2017
(https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and):

- **43:27:** explicit tuples give knowledge ahead of time of which systems
  touch which components. Systems with no shared writes run in parallel.
  Read-only access lets many readers run together (`Transform` is read by
  many systems and written by few).
- **20:35:** with deferment, all contact requests go to one queue and one
  resolver. That one call site is then easy to fork and join across workers.
  Ford also uses fork/join under the hood for AI path data, a specialist
  system behind a proxy.

These are two kinds of parallelism. We use both.

---

## 1. Between systems: stages

`BuildSchedule` already puts systems in stages. Systems in one stage have no
edge between them, so none writes what another reads or writes. The
executor forks a stage across workers and joins at its end:

```
Phase Simulation
  stage 0:  [Movement.Mover]  [AI.Perception]  [Abilities.Cooldowns]    fork: 3 jobs
            ------------------------- join -------------------------
  stage 1:  [Abilities.Cast]  [AI.Decide]                               fork: 2 jobs
            ------------------------- join -------------------------
  flush commands (one thread, batched)
```

- **Version 1: one barrier per stage.** It is simple. A stage waits for its
  slowest system.
- **Version 2: a dependency graph.** Each system starts when its own
  predecessors are done. It uses the same edges and needs no new
  declarations, only a counter per system.
- **The main thread helps.** While it waits at a join, it runs jobs from the
  queue. It is never idle.
- **Main-thread systems.** A system that touches GLFW or builds the ImGui
  frame sets `SystemInfo.MainThreadOnly = true`. The executor runs it on
  the main thread, in its stage.
- **Small systems.** A job has a cost. The executor runs a system inline
  when its last measured time is under a threshold, and groups small
  systems into one job.
- **Phase ends and flushes are joins.** Structural changes (deferred
  destroy, add, remove) apply on one thread, in schedule order.

## 2. Inside one system: chunked fork/join

This usually helps more than stage parallelism, because a few heavy systems
take most of the frame.

```cpp
void BoidsSystem::FixedUpdate(Context& ctx) const
{
    const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;

    ctx.ParallelForEach<Fling::LocalTransform, const Velocity>(
        [dt](auto& chunk) {                      // One job per storage chunk
            for (auto [xf, vel] : chunk)
            {
                xf.Position += vel.Value * dt;
            }
        });
}
```

The lambda gets a **chunk context**, not the system context:

- It can write only the current entity's components (the ones in the
  query).
- It can read other entities by handle (`TryGet<const T>`). It cannot write
  them. Cross-entity writes in a parallel loop are the common race, so that
  function does not exist there.
- It cannot spawn, destroy, or append to a queue directly. It writes
  per-chunk output (see §3.5), which the system combines after the join.

With EnTT, a "chunk" is a slice of the view's entity range. With native
storage, it is a real 16 KiB chunk.

---

## 3. Is it safe?

### 3.1 What the design guarantees

In most engines, access declarations are documentation, and they can be
wrong. Ours cannot: `ctx.Query<T>()` does not compile unless the system
declares `T` (`ecs-systems.md` §3.6). So for every component and singleton
read or written through the context, the stage grouping proves that no two
systems in a stage race.

| Shared thing | Why it is already safe |
|---|---|
| Deferred commands | One buffer per system. Merged in schedule order at the flush |
| Queues (`Append`) | One buffer per appender. The consumer sees them in schedule order, not in the order threads finish |
| Singletons | The same read and write rules as components |
| Change versions | Only the writer of a column bumps it |

### 3.2 What it does not cover, and the fix

| Hazard | Fix |
|---|---|
| **Immediate spawn.** `ctx.Spawn()` changes the entity table and the storage layout. Two spawns at once race. Ford says immediate creation made their iterators harder too | Version 1: `ctx.Spawn()` needs `Fling::Spawns` in the access list. The executor runs a system with `Spawns` alone in its stage. Later: each system reserves a block of entity handles and stages its new rows, merged at the join |
| **Globals.** `Input::Get()`, `ResourceManager`, `VulkanApp::Get()`, the global `Random`, `Timing`, and `static` locals in `*Util` functions all go around the access checks | They must leave system code before the executor goes parallel. The debug "in simulation" flag asserts when a worker touches one |
| **Random numbers** | A deterministic stream per system, or per entity, seeded from the tick. Never a shared generator |
| **EnTT pool creation.** In EnTT 3.1, `registry.view<T>()` can make the pool for `T` the first time. That changes the registry | `BuildSchedule` makes every pool named in any access table before anything runs |
| **Order of floating-point sums.** A parallel sum that adds in finish order gives different bits on each run. That breaks rollback | Parallel work writes per entity or per chunk. A combining step goes in chunk order (§3.5) |
| **Logging** | `Logger` must be safe from many threads. Messages from workers may come out in any order. That is allowed, because logs are not sim state |

### 3.3 Determinism rules

1. The result of a frame must be the same with 1 worker or N workers, bit
   for bit.
2. Nothing in a system may depend on which thread runs it, or when
   (`Skills/threading.md` rule 4).
3. Every merge (commands, queues, chunk output, staged spawns) runs in a
   fixed order: schedule order, then chunk index.

### 3.4 How we test it

- **Serial vs parallel hash.** Run Sandbox headless for N ticks with 1
  worker and with 8 workers. Hash every component and singleton at the end.
  The hashes must match. This runs in CTest.
- **Shuffle mode.** `--schedule-shuffle <seed>` runs each stage's systems in
  a random order, one at a time. If the hash changes, an edge is missing.
  CI runs a few seeds.
- **ThreadSanitizer.** A CI job builds with TSan and runs a headless
  Sandbox with the parallel executor.

### 3.5 Combining chunk output

When a parallel loop must produce more than its own entity's data (a list of
hits, a sum, spawn requests), it writes to an output slot for its chunk:

```cpp
auto hits = ctx.ParallelForEach<const LocalToWorld, const Sensor>(
    [](auto& chunk, Fling::ChunkOutput<Hit>& out) {
        for (auto [xf, sensor] : chunk)
        {
            if (SensorUtil::Triggered(xf, sensor)) out.Push({ chunk.Entity(), sensor.Id });
        }
    });

// After the join: one list, in chunk order, on the system's thread.
for (const Hit& hit : hits)
{
    ctx.Queue<ContactQueue>().Push(hit);
}
```

---

## 4. Parallel level loading

The level file does not decide the parallelism. The pipeline does.

```
StreamingSystem         decides which cells are wanted, appends to StreamingQueue
ResolveStreaming        the only consumer. Gives each cell to the loader
  IO thread             reads the cell's file
  worker jobs (N)       decode each chunk of the file into a staging buffer     <- parallel
ApplyCompletedLoads     main thread, start of RunFrame, time-budgeted:
                        commits staged entities into the world, in a fixed order
```

### 4.1 Workers never touch the world

Each worker makes a **staging buffer**:

- entity rows grouped by component set, with columns in the same layout the
  storage uses,
- entity references as local indices or `EntityGuid` values, not handles.

### 4.2 Committing is cheap

The commit allocates real handles, copies the columns in, and remaps local
references to the new handles. That remap is the same one `EntityGuid`
loading needs anyway (`ecs-plan.md` §1.4).

- **With native storage**, a worker can build complete chunks, and the
  commit takes ownership of them. There is no per-entity work. This is the
  real gain for large worlds.
- **With EnTT**, the commit inserts one entity at a time. It is slower, but
  the design is the same, so nothing changes for callers when the storage
  changes.

### 4.3 Budget and order

- The commit spends a fixed amount of main-thread time per frame (for
  example 2 ms). A large cell comes in over several frames with no hitch.
- The commit runs in a fixed order: cell id, then chunk index. It never
  runs in the order jobs finish. Handles are given out in commit order, and
  iteration order follows handles, so two machines must commit in the same
  order to simulate the same.

### 4.4 Format

You cannot split one large JSON document across workers without parsing it
first. Parallel-friendly levels need one of these:

- **One file per cell or chunk.** This also helps source control: two people
  can edit two cells with no merge conflict.
- **Cooked binary cells,** which `ecs-plan.md` already plans. JSON stays the
  authoring format.

### 4.5 Entities are not the slow part

Meshes and textures usually are. They load through the `ResourceManager`
async path. A committed entity gets its GPU proxy and physics body from the
normal extract and resolver systems when its assets are ready.

---

## 5. Where the code lives

| Code | Module | Why |
|---|---|---|
| `JobSystem`: worker pool, fork/join, `ParallelFor`, the main thread helps at joins | Core (`Core/inc/Threading/JobSystem.h`) | It is a threading primitive with no ECS knowledge |
| `ParallelExecutor`: runs stages or the dependency graph on the `JobSystem` | Engine (`Engine/inc/Ecs/`) | It reads the `Schedule` |
| `ParallelForEach`, `ChunkContext`, `ChunkOutput` | Engine (`Engine/inc/Ecs/`) | They wrap storage |
| Staging buffers, commit | Engine | They write storage |

## 6. Work items

1. **Already in `ecs-systems.md`:** per-system command and queue buffers,
   stages in the schedule, and pools made when the schedule is built.
2. **Remove globals from system code.** Add the end-of-run world hash and
   its CTest, still on one thread.
3. **`JobSystem` in Core.** Tests for fork/join, nested `ParallelFor`, and
   the main thread helping at a join.
4. **`ParallelForEach`** with `ChunkContext` and `ChunkOutput`. Use it in
   the heaviest Sandbox system first. Add the serial vs parallel hash test.
5. **Stage-parallel executor**, `MainThreadOnly`, `Spawns`, `--workers N`,
   and `--schedule-shuffle`. Add the TSan CI job.
6. **Dependency-graph executor** (no barrier per stage). Compare frame times
   with version 1 in the PR.
7. **Parallel cell decode** into staging buffers, with the budgeted,
   fixed-order commit. Works with EnTT. Gets fast with native chunks
   (`ecs-plan.md` Phase F).

## 7. Questions for you

1. **Worker count.** Default to `hardware_concurrency - 2` (keep one core
   for the render thread and one for input and audio), with `--workers N`
   to override?
2. **Spawning in parallel stages.** Version 1 runs spawning systems alone.
   Is that enough for now, or do you expect many spawning systems early?
