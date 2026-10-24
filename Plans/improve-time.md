# Improve time plan

Design for Fling's timing API and tick loop: a fixed-tick, deterministic
simulation that runs headless, and an API shape that stays the same when the
game, physics, and render work later move to separate threads. Each game tick
has a unique tick number. Input, rollback, resim, the network, and the
renderer use that number.

This is a plan, not an established standard. When a phase is done, move the
rules that it establishes into [`Skills/`](../Skills/).

Related plans: [`improve-ai-iteration.md`](improve-ai-iteration.md) (headless
deterministic runs, input record and replay) and [`ecs-plan.md`](ecs-plan.md)
(`World`, `Scheduler::RunFrame`, phases, resim).

## Problem

- `Timing` (`FlingEngine/Core/inc/Timing.h`) is a `Singleton<Timing>`. It
  reads `high_resolution_clock` and cuts the result to whole milliseconds. At
  60 Hz each delta is 16 or 17 ms, which causes visible judder.
- `Engine::Tick` (`FlingEngine/Application/src/Engine.cpp`) gives one variable
  `dt` per frame to `World::Update`. Gameplay results depend on the frame rate.
- A `dt` of 1 second or more becomes 1/60. This hides debugger pauses, but
  hitches from 0.2 to 0.9 seconds still go into gameplay as one large step.
- Game time and real time are not separate (there is a `#TODO` for this).
  Pause and time scale have no defined place.
- The FPS counter is part of `Timing`. `GetFrameTime()` returns `1000 / fps`,
  not the time of a frame.
- The loop runs only while a window is open. `VkApp.Update(dt, g_Registry)`
  reads the registry directly, so the renderer and the simulation share state.
- `Random::Init()` seeds from `std::random_device`. Gameplay, the editor, and
  cosmetic code share one global stream.

Only `Engine.cpp` uses `Timing` today, so it is cheap to replace now. Do not
patch the millisecond cut or the 1-second rule. Delete them.

## Goals

1. The simulation advances in fixed ticks. Gameplay never sees a variable
   delta.
2. The same binary, seed, and input give the same state hash
   (see `improve-ai-iteration.md`).
3. Headless runs step as fast as the CPU allows, with no clock.
4. One tick number identifies sim state everywhere: input, rollback history,
   network messages, audio events, and snapshots for the renderer.
5. The renderer interpolates between published game states, so a render rate
   higher than the tick rate does not stutter.
6. Moving game, physics, and render work to separate threads later does not
   change this API. Today everything runs on one thread.

## Non-goals

- Threads. This plan only makes sure that the API does not block them.
- Bit-exact results across compilers or platforms.
- Rollback netcode itself. This plan supplies the tick number, the history
  shape, and the clock hooks that it needs.

## How other engines do it

| | Unreal 5 | Unity 6 | Godot 4 |
|---|---|---|---|
| Platform clock | `FPlatformTime::Seconds()` / `Cycles64()` | Internal; `Time.realtimeSinceStartup` | `Time.get_ticks_usec()` (integer microseconds) |
| Main gameplay tick | Variable `Tick(DeltaSeconds)`, ordered by tick groups (`TG_PrePhysics`, `TG_DuringPhysics`, `TG_PostPhysics`, ...) | Variable `Update()` and fixed `FixedUpdate()` | Variable `_process(delta)` and fixed `_physics_process(delta)` |
| Fixed tick | No general fixed gameplay tick. Async Chaos physics, Network Prediction, and Mover add fixed ticks | `Time.fixedDeltaTime` (default 0.02). An accumulator runs `FixedUpdate` 0..N times per frame | `physics_ticks_per_second` (default 60), accumulator in `MainTimerSync` |
| Long-frame guard | Max delta settings, physics substep limits | `Time.maximumDeltaTime` (default 1/3 s) | `max_physics_steps_per_frame` (default 8) |
| Game time and real time | `UWorld`: `GetTimeSeconds`, `GetRealTimeSeconds`, `GetUnpausedTimeSeconds`, `GetAudioTimeSeconds` | `Time.time` / `fixedTime` (scaled) and `unscaledTime`. `timeAsDouble` was added because float `time` loses precision | `Engine.time_scale`; separate process and physics frame counters |
| Time scale | Global and per-actor dilation | `Time.timeScale` | `Engine.time_scale` |
| Render interpolation | Per system (physics smoothing) | `Rigidbody.interpolation` | `get_physics_interpolation_fraction()`, built-in physics interpolation |
| Fixed frame clock for offline runs | `-UseFixedTimeStep -FPS=60` | `Time.captureDeltaTime` | `--fixed-fps <n>` |

What Fling takes from this:

- **An accumulator with a clamp** (all three), with a maximum number of ticks
  per frame (Godot).
- **Separate counters** for sim ticks and rendered frames (Godot).
- **Sim time separate from real time** (Unreal). Pause and time scale change
  how many ticks run, not what a tick is.
- **Render interpolation** (Godot), calculated on the render side.
- **A fixed clock for offline runs** (all three). Fling uses it for headless
  runs and tests.

What Fling does not take:

- **Unity's `Time.deltaTime` that depends on context.** It returns the fixed
  delta inside `FixedUpdate` and the frame delta elsewhere. This hides which
  clock code depends on. Fling uses different types, and the scheduler
  enforces which systems can read them.
- **Float time.** Fling stores an integer tick and calculates seconds from it.
  Game Engine Architecture (Gregory) gives the same advice: 64-bit integer
  clocks, and floats only for small deltas.
- **Variable gameplay tick as the default** (Unreal). Unreal is adding fixed
  ticks piece by piece for netcode. Fling starts with them.

## Design

### Naming

| Name | What it is |
|------|------------|
| `SimTick` | The tick number |
| `TickRate` | Ticks per second |
| `TickTimeline` | Converts a tick to sim nanoseconds, and handles rate changes |
| `TickClock` | Decides how many ticks run now. Owns pause, time scale, step requests, and the presentation mapping |
| `PresentationClock` | Maps real time to sim time, for the renderer and audio |
| `SimTime` | World data: the current tick and timeline |
| `RealFrameTime` | World data: rendered-frame counter and real delta, for presentation and the editor |
| `TickInput` | The input for one tick (was `InputFrame`) |
| `TickCommands` | The commands for one tick that a resim replays (was `CommandFrame` in `ecs-plan.md`) |
| `RenderSnapshot` | The render extract for one tick |
| `SnapshotRing` | The ring of `RenderSnapshot`s that the game side writes and the render side reads |

- Use "tick" for sim steps everywhere. "Frame" means a rendered frame only.
- Type names do not have a `Singleton` prefix. The access call already says
  what the type is: `world.Singleton<SimTime>()`. The type is marked as a
  singleton when it is registered (`modules.Singleton<T>(...)` and
  `T::Kind`, see `ecs-plan.md`), so a `Query` of it does not compile.

### The tick number

`SimTick` is a strong type, not a bare `uint64`. `SimTick` n means "n ticks
are done". The state at tick n is the state at sim time
`Timeline.TickNs(n)`.

Everything that crosses a thread or network boundary has the tick that it
belongs to:

| Data | Tagged with |
|------|-------------|
| Input for a tick | `TickInput{ Tick }` |
| Commands that a resim replays | `TickCommands{ Tick }` |
| Data for the renderer | `RenderSnapshot{ Tick }` |
| Rollback history | `SimHistory[Tick]` |
| Network messages | Acks and snapshots keyed by tick |
| Audio events | `AudioEvent{ Tick, Entity, Event }` |

Because every message has its tick, the thread that made it does not change
the result. The same code works on one thread today.

**On the wire**, a full `uint64` is not necessary. Send 16 or 32 bits and
compare with wraparound (serial-number arithmetic, RFC 1982).

### Core types

Core has no game concepts.

```cpp
namespace Fling::PlatformTime
{
    /** Monotonic time from steady_clock, in nanoseconds. */
    int64 NowNanoseconds();
}

struct SimTick
{
    uint64 Value = 0;
    // ++, comparisons, and difference (int64). No arithmetic with floats.
};

struct TickRate
{
    uint32 TicksPerSecond = 60;
};

/**
 * Converts ticks to sim nanoseconds. A rate change starts a new segment at a
 * tick boundary, so the times of earlier ticks do not move.
 */
struct TickTimeline
{
    SimTick BaseTick;
    int64 BaseNs = 0;
    TickRate Rate;

    /** BaseNs + ((t - BaseTick) * 1'000'000'000) / TicksPerSecond */
    int64 TickNs(SimTick t) const;
};
```

- Calculate each tick time from the segment base. 1/60 s is not a whole number
  of nanoseconds, so adding a rounded tick length each step makes the error
  grow.
- Rebasing on a rate change also prevents overflow. `n * 1e9` overflows
  `int64` after about 9.2 × 10^9 ticks: about 4.9 years at 60 Hz, but only
  about 107 days at 1 kHz. Rebase after a long time even without a rate change.
- A rate change happens only at a tick boundary. It is recorded in input
  recordings and in `SimHistory`, so a replay or a resim uses the same rate.

### Tick clocks

```cpp
/** Maps real time to sim time. Changes only at an anchor point. */
struct PresentationClock
{
    int64 AnchorRealNs = 0;
    int64 AnchorSimNs = 0;
    uint32 ScaleQ16 = 1u << 16;   // Time scale in 16.16 fixed point. 0 when paused.

    int64 SimNsAt(int64 realNs) const;
};

class TickClock
{
public:
    virtual ~TickClock() = default;

    /** Number of ticks to run now. */
    virtual uint32 TicksDue(SimTick current) = 0;

    /** Run count more ticks: editor single step, RPC step, --frames N. */
    virtual void RequestSteps(uint32 count) = 0;

    virtual void SetPaused(bool paused) = 0;
    virtual void SetTimeScale(uint32 scaleQ16) = 0;

    /** Small rate change for network clock sync, in parts per million. */
    virtual void SetRateNudge(int32 ppm) = 0;

    virtual const PresentationClock& Presentation() const = 0;
};
```

| Clock | Use | Behavior |
|-------|-----|----------|
| `RealTimeTickClock` | Game and editor | Converts scaled real time to ticks due, in integer nanoseconds. Clamped by `MaxTicksPerFrame` (default 8). Adds any requested steps |
| `FixedTickClock` | Headless, tests, `--frames N` | No real time. Returns the requested ticks that remain, in batches (for example up to 1000), so RPC commands are still handled between batches. Never one tick per frame |

- **Time scale and pause belong to the clock, not to the sim.** Slow motion
  means fewer ticks per real second. A tick always has the same length. The
  sim never reads the time scale, so an editor pause cannot change the state
  hash.
- The accumulator is `int64` nanoseconds. The scale is 16.16 fixed point, and
  the remainder is kept, so there is no drift. The number of ticks per frame
  is never deterministic. Determinism comes from input keyed by tick, so this
  is for smooth pacing, not for the hash.
- **Hitches and breakpoints.** A real gap longer than a hitch threshold
  (default 0.1 s, the value that Game Engine Architecture suggests) is treated
  as a hitch. The clock does not try to catch up on it: it re-anchors the
  `PresentationClock`. Presses and releases in the gap are dropped, but held
  keys stay held (see the tick assignment rules in `improve-input.md`). Under
  the threshold, `MaxTicksPerFrame` clamps, and the time over the clamp is
  dropped. The presses and releases from that time are not dropped: they go
  into the next tick that runs.
- **Networked play cannot drop ticks.** A networked client catches up, or it
  resyncs with the server. `SetRateNudge` is the hook for Overwatch-style
  clock sync: the client runs slightly faster or slower to keep the server's
  input buffer at a target depth. A later `NetworkTickClock` uses it without
  a change to the interface.
- The clock re-anchors `PresentationClock` only on pause, time-scale change,
  rate change, or hitch recovery. Between anchors the mapping is a straight
  line, so presentation time does not jitter with game-thread load.

### World data

The scheduler writes these. Systems only read them.

```cpp
struct SimTime
{
    SimTick Tick;
    TickTimeline Timeline;

    double Seconds() const;      // Timeline.TickNs(Tick) / 1e9. Never a running sum.
    float FixedDelta() const;    // 1 / TicksPerSecond
};

struct RealFrameTime
{
    uint64 Frame;                // Rendered frames. Separate from Tick.
    float RealDelta;             // Real time since the last frame. Presentation and editor only.
};
```

- `SimTime` has no time scale and no pause flag (see
  [Tick clocks](#tick-clocks)).
- A system in the `Simulation` or `ResolveDeferred` phase that declares
  `Read<RealFrameTime>` is a scheduler build error.
- The interpolation fraction is **not** on the world. The renderer calculates
  it (see [Interpolation](#interpolation)).
- `Timing` stops being a singleton and is then deleted. FPS and frame-time
  stats move to `Stats`, where `Stats::Frames::TickStats` already is.

### The loop

```cpp
void Engine::Tick()
{
    m_World->Init();
    while (m_Runner->ShouldContinue(*m_World))   // window open, or ticks remain, or World::ShouldQuit
    {
        // One thread today. Later the input thread produces events (see improve-input.md).
        m_InputProducer->PollOnCallerThread();

        const uint32 ticks = m_TickClock->TicksDue(m_SimTime.Tick);
        for (uint32 i = 0; i < ticks; ++i)
        {
            // Builds this tick's TickInput from timestamped input events.
            m_World->BindTickInput(m_SimTime.Tick, m_TickClock->Presentation());
            m_World->FixedUpdate(m_SimTime);         // gameplay and physics, fixed delta only
            ++m_SimTime.Tick;
        }

        if (ticks > 0)
        {
            // Only the last live tick of this frame is extracted.
            RenderSnapshot& snapshot = m_Snapshots.BeginWrite();
            ExtractRenderData(*m_World, snapshot);
            m_Snapshots.EndWrite(m_SimTime.Tick, m_SimTime.Timeline, m_TickClock->Presentation());
        }

        if (m_Renderer)                              // null under --null-render
        {
            m_Renderer->Render(m_Snapshots);
        }
    }
}
```

- `Game::Update(reg, DeltaTime)` becomes a fixed-tick update that always gets
  `FixedDelta()`.
- Under `--null-render`, `Engine::Startup` does not call `VulkanApp::Init`,
  and `m_Renderer` is null.
- When the ECS plan lands, `Scheduler::RunFrame` takes over this loop with the
  same steps.

### Input

The full input design is in [`improve-input.md`](improve-input.md). The parts
that touch timing:

- Input events have a real-time timestamp and a sequence number.
- Before each live tick, the engine builds one `TickInput` from the events
  whose timestamps fall inside that tick. The tick's real-time range comes
  from `PresentationClock` and `TickTimeline`.
- `TickInput` has **edge counts** (presses and releases in that tick), so a
  press and release inside one tick is not lost. When the clock drops ticks,
  their events go into the next tick that runs.
- The `Input` phase runs once per live tick (not once per frame) and writes
  `InputState`. A resim uses stored `TickCommands` and reads no events.
- Each `RenderSnapshot` stores the last input sequence its tick used, so the
  render side can apply newer look input without counting it twice.

### Physics

- Physics steps on the same `SimTick` as gameplay, with `FixedDelta()`. It
  never has its own clock.
- Physics runs **inside** the tick with fork and join: gameplay before
  physics, then a physics step that runs as parallel jobs, then a join, then
  gameplay after physics. The order of systems stays the same as today; only
  the work inside the physics step runs in parallel.
- Today the physics step runs on the game thread, with no jobs.
- Do not run physics one tick behind on its own thread. It is also
  deterministic, but it adds one tick of latency to every physical response
  to input. Rollback snapshots would also need the in-flight physics state,
  and a restore would need to cancel jobs in progress. Game Engine
  Architecture compares the two: fork and join keeps the game loop logic
  single-threaded, and one thread per subsystem scales poorly.
- Cosmetic physics that is not in the rollback set (debris, cloth) can run
  behind or on its own thread.

### Frame publishing

```cpp
struct RenderSnapshot
{
    SimTick Tick;
    uint64 LastInputSequence;       // Last input event this tick used (see improve-input.md)
    TickTimeline Timeline;          // Converts Tick to sim ns on the render side
    PresentationClock Presentation; // The mapping in use when this tick was written

    struct Entry
    {
        uint64 EntityId;            // Stable ID, to match an entity across two snapshots
        uint8 Flags;                // Spawned, Despawned, Teleported
        // Transform, mesh and material handles, light data, ...
    };
    std::vector<Entry> Entries;
    // Camera: position, SimView (yaw and pitch), LookScale, LateLook flag (see improve-input.md).
    // Global lights, ...
};

/**
 * Ring of RenderSnapshots, 4 to 6 slots, each with a sequence number.
 * One thread today. Lock-free later.
 */
class SnapshotRing
{
public:
    RenderSnapshot& BeginWrite();                                   // game side
    void EndWrite(SimTick tick, const TickTimeline& timeline, const PresentationClock& clock);

    /** The two snapshots around renderSimNs. False if fewer than two exist. */
    bool AcquirePair(int64 renderSimNs,
                     const RenderSnapshot*& prev,
                     const RenderSnapshot*& next);                  // render side
    void ReleasePair();
};
```

- The renderer reads only `SnapshotRing`. It never reads the `World` or the
  registry. This replaces `VkApp.Update(dt, g_Registry)`.
- The ring needs **at least 4 slots**. The render side holds 2 for a whole
  render frame. The game side needs 1 to write into and 1 for the newest
  snapshot that the render side has not read yet. With 3 slots, the game side
  must wait or overwrite the newest snapshot. Each slot has a sequence number
  (or a reference count), so the render side can choose the pair around its
  time. That pair is not always the newest two.
- Only the **last live tick of each frame** is extracted. When a frame runs 3
  ticks, only the third is extracted. Interpolation uses tick times, so a gap
  from tick n-2 to tick n still interpolates correctly.
- Only `Live` ticks are written. Resim ticks (`SimPass::Resim` in
  `ecs-plan.md`) are not. A resim replays earlier ticks and then the live tick
  is written, so written ticks always increase.

### Interpolation

The render side calculates the fraction. It does not use the time when the
game side wrote a snapshot, because that time jitters with game-thread load.
When one frame runs several ticks, they are written microseconds apart, and
motion would stutter exactly during a hitch.

```cpp
// Once per render frame.
const int64 presentRealNs = PredictedPresentTimeNs();   // or NowNanoseconds()
const PresentationClock& clock = snapshots.LatestPresentation();
const int64 renderSimNs = clock.SimNsAt(presentRealNs) - oneTickNs;   // draw one tick behind

const RenderSnapshot* prev = nullptr;
const RenderSnapshot* next = nullptr;
if (snapshots.AcquirePair(renderSimNs, prev, next))
{
    const int64 prevNs = prev->Timeline.TickNs(prev->Tick);
    const int64 nextNs = next->Timeline.TickNs(next->Tick);
    const float alpha = std::clamp(float(renderSimNs - prevNs) / float(nextNs - prevNs), 0.0f, 1.0f);
    // Per entity: lerp(prev, next, alpha). Snap when there is no prev entry or the entry is Teleported.
    snapshots.ReleasePair();
}
```

- Sample the present time once per render frame. When the
  `VK_EXT_present_timing` or `VK_GOOGLE_display_timing` extension is
  available, use the predicted display time, not the current time.
- The game side's accumulator remainder is the same number, but only while
  rendering runs in step with the game loop (as it does today). The mapping
  keeps working when rendering moves to its own thread.
- **Pause** freezes the mapping (`ScaleQ16` is 0), so the image stays exactly
  where it is.
- If `renderSimNs` is later than the newest snapshot, clamp to the newest
  snapshot. Do not extrapolate. If it falls more than a few ticks behind the
  newest snapshot, the tick clock re-anchors.
- An entity that is in only one of the two snapshots (spawned or despawned)
  does not interpolate. It is drawn at its `next` state, or not drawn.
- The camera position interpolates like any entity. When late look is on, the
  camera **rotation** does not interpolate: it is the latest snapshot's
  `SimView` plus the input the simulation has not used yet (see
  `improve-input.md`).
- After a resim, the renderer blends from the old (wrong) state to the
  corrected state. Smoothing large corrections is renderer work for later.

### Rollback history

`SimHistory` and `SnapshotRing` are separate on purpose:

| | `SimHistory` | `SnapshotRing` |
|---|---|---|
| Purpose | Rollback and resim | Rendering |
| Contents | Full sim state (predicted or replicated components) and the tick rate | Render extract only |
| Depth | Rollback window, for example 8 to 16 ticks | 4 to 6 |
| Threads | Game side only | Game side writes, render side reads |

`SimHistory` is built with the netcode work, not in this plan. This plan only
makes sure that its key is `SimTick`.

### Audio

- Game-to-audio events have a `SimTick`. The audio system schedules each
  event at the presentation time of its tick, so sound matches what is on
  screen.
- A predicted sound is identified by (tick, entity, event). After a resim the
  audio system does not play it again if it already played, and it stops it
  if the corrected sim no longer makes it.

### Random numbers

- The sim uses a counter-based RNG: each value is
  `hash(seed, tick, entityId, stream)`. It has no state, so rollback does not
  need to save it, and the result does not depend on the order in which
  systems or entities update.
- `--seed S` sets the seed. Without it, the engine picks a seed and logs it,
  so any run can be repeated.
- The bounded-integer and float range functions are Fling code.
  `std::uniform_*_distribution` output depends on the standard library, so
  MSVC and libstdc++ give different numbers from the same seed.
- The existing global `Random` stays for cosmetic, editor, and UI code. The
  sim does not use it.

### Enforcing the rules

- `Read<RealFrameTime>` in a sim phase is a scheduler build error.
- In debug builds the scheduler sets a thread-local "in simulation" flag
  while `Simulation` and `ResolveDeferred` run. `PlatformTime::NowNanoseconds`
  and the global `Random` assert when the flag is set.

### Other sources of nondeterminism

| Source | Rule |
|--------|------|
| Float code generation differs between Debug and Release | Store golden state hashes per build configuration, or build sim code with `-ffp-contract=off` and without `/fp:fast`. Never use `-ffast-math` |
| Asset loads that finish asynchronously | Headless runs block until the level is loaded before tick 0. The real-time game applies finished loads only at a tick boundary |
| Unordered containers keyed by pointer | Iterate by entity ID or another stable key |
| Threads (later) | Fixed system order. Merged results must not depend on which job finished first |

## Rules

1. Sim code reads only `SimTime` and `InputState`. It never reads
   `RealFrameTime`, `PlatformTime`, the global `Random`, or a wall clock.
2. Everything that crosses a thread or network boundary has a `SimTick`.
3. The renderer reads only `SnapshotRing`.
4. Sim time is calculated from `SimTick` and `TickTimeline`. It is never
   summed.
5. Time scale and pause change how many ticks run. They never change the
   length of a tick, and the sim cannot read them.
6. Only the last `Live` tick of a frame is written to `SnapshotRing`.

## Tests

- **Same seed, same hash:** run a `World` for 600 ticks twice with seed 1 and
  compare state hashes.
- **Frame rate does not change results:** give `RealTimeTickClock` a fake
  clock with irregular frame times (1/30, 1/144, random long frames) and
  recorded input. Run to the same tick as a `FixedTickClock` run and compare
  hashes.
- **Input is not lost:** a press and a release inside one frame that runs 3
  ticks give one press edge and one release edge on the correct ticks.
- **Tick times do not drift:** `TickNs` at tick 60 × 86 400 (one day at
  60 Hz) is exactly 86 400 × 10^9.
- **Rate change:** after a change from 60 Hz to 30 Hz at tick 600, `TickNs`
  for ticks before 600 is the same as before.
- **Pause does not change the hash:** pausing and resuming in the middle of a
  run with recorded input gives the same hash as a run without a pause.
- **Snapshot ring:** written ticks always increase, `AcquirePair` returns the
  pair around a given time, and the game side never waits with 4 slots.

## Phases

Each phase must leave the engine, Sandbox, and FlingTests building and passing
on every CI platform.

| Phase | Work | Depends on |
|-------|------|------------|
| **T1** | `PlatformTime`, `SimTick`, `TickRate`, `TickTimeline`, `PresentationClock`, `TickClock` with both clocks. Unit tests | None |
| **T2** | `Engine::Tick` uses a tick clock and calls `World::Update(FixedDelta())` per tick. `--frames`, `--seed`, and `--null-render` (skip `VulkanApp`). Delete `Timing`; FPS stats move to `Stats` | T1. Phase A in `improve-ai-iteration.md` |
| **T3** | `SimTime` and `RealFrameTime` on the world. Counter-based sim RNG. Debug "in simulation" asserts | T2 |
| **T4** | `RenderSnapshot` with transforms only, single-thread `SnapshotRing`, render-side interpolation | T2 |
| **T5** | `TickInput` per tick, with edge counts (phases I1 and I2 in `improve-input.md`) | T3 |
| **T6** | Move into `Scheduler::RunFrame`; scheduler rejects `Read<RealFrameTime>` in sim phases | `ecs-plan.md` scheduler |
| **Later** | Render thread with a lock-free `SnapshotRing`, physics step as jobs, `SimHistory` and resim, `NetworkTickClock`, audio scheduling, correction smoothing | Threading, job system, and netcode work |

## Open questions

- Which hitch threshold do editor builds use? A breakpoint is always a hitch,
  but a slow editor frame may not be.
- Does the editor need a "step N ticks then pause" command in addition to
  `RequestSteps`, or is `SetPaused(true)` plus `RequestSteps(N)` enough?
- Where does the audio clock come from: `PresentationClock`, or the audio
  device clock with an offset?
- How many bits does a tick use on the wire: 16 (smaller packets) or 32
  (longer gaps before wraparound)?
