# Agent guide: how the Fling prototype works

This file is for an AI coding agent with no context on this repository. It describes
the runtime model, the ECS API, time, publish and blend, input, threads, settings,
build and test, recipes, and the rules you must not break. Every type name, path and
snippet here was checked against the code at the time of writing. If the code and this
file differ, the code is correct.

Fling is a C++20 Vulkan engine built with CMake. This prototype is a reference for one
architecture: a deterministic fixed-tick game thread, an ECS with compile-time
declared order and data access, and a variable-rate presentation thread. There is no
networking. Tick numbering, tick stamps, rollback classes and resimulation groups exist
so that rollback and resimulation can be built later. No snapshot or resimulation
runner exists.

Main code locations:

| Area | Path |
| --- | --- |
| ECS core (components, access, systems, schedule, runner) | `FlingEngine/Engine/inc/Ecs/`, `FlingEngine/Engine/src/Ecs/` |
| Publish, mirror, blend | `FlingEngine/Engine/inc/Ecs/Publish/` |
| World, drivers, links, game thread | `FlingEngine/Engine/inc/World.h`, `TickDriver.h`, `PresentationLink.h`, `GameLane.h`, `TickEvents.h` |
| Time, input, threads, settings | `FlingEngine/Core/inc/` (`TickTimeline.h`, `SimRate.h`, `RunSettings.h`, `ExecMode.h`, `Input/`, `Threading/`) |
| Main loop | `FlingEngine/Application/src/Engine.cpp` |
| Example game | `Sandbox/Gameplay/` (look camera, stasis) |
| Tests | `FlingTests/` |

## 1. Mental model

Every system declares three things at compile time: its group, its order against other
work, and its data access. Groups nest under exactly two roots.

| Root | Name | Rate | Update function | Thread in threaded play | May read | May write |
| --- | --- | --- | --- | --- | --- | --- |
| `Order::Tick` | `Engine.Tick` | Fixed, once per tick (30 Hz default) | `FixedUpdate(Context&) const` | Game thread (`GameLane`, OS name "Game Thread") | Simulation components, `SimTime`, `InputState` | Simulation components |
| `Order::Frame` | `Engine.Frame` | Variable, once per rendered frame | `Update(Context&) const` | Main thread | Published copy of Simulation components, `RealFrameTime`, `PendingInput`, Presentation components | Presentation components only |

Engine groups (`FlingEngine/Engine/inc/Ecs/EngineOrder.h`, namespace `Fling::Order`):

| Group | Parent | After | `Resim` | Purpose |
| --- | --- | --- | --- | --- |
| `Input` | Tick | | no | Input preprocessors that read `InputState`; app keys |
| `Simulation` | Tick | `Input` | yes | Gameplay |
| `Physics` | Tick | `Simulation` | yes | `Physics.Step` |
| `ResolveDeferred` | Tick | `Physics` | yes | Queue consumers (spawns, damage, effects) |
| `Finalize` | Tick | `ResolveDeferred` | no | Derived data from the final state (`LocalToWorld`) |
| `Presentation` | Frame | | no | What the player sees and hears |
| `Render` | Frame | `Presentation` | no | `Graphics.PublishSnapshot` fills `RenderSnapshot` |
| `Editor` | Frame | `Render` | no | Editor tools |

The group gives the rate, and the rate gives the thread. No system names a thread.

What lives where:

- The simulation storage (an `EntityStorage`) holds all Simulation components. Only
  fixed systems change it, on the tick thread.
- The frame side reads a mirror storage (in `inline` and `threaded` modes) that holds
  only the published types, plus its own Presentation singletons (`PendingInput`,
  `RealFrameTime`, `CameraView`, `RenderSnapshot`).
- The schedule derives the published set: every Simulation type that a frame system
  declares `Read<T>` for (`Schedule::Published`).

What crosses between the two sides (each has one direction):

| What | Direction | Mechanism |
| --- | --- | --- |
| Published state of the newest tick (tick N, plus tick N - 1 for blended types) | game to main | `TickPacket` in a latest-wins `TripleBuffer` (one atomic exchange per side) |
| Input messages | main to game | `InputMessageQueue`: one writer, seqlock slots, 4096 messages; each tick takes a cut |
| What a tick asks of the application (quit, cursor) | game to main | `ThreadQueue<TickOutboxEntry>`, one entry per tick, stamped with the tick |
| Clock control (Pause, Resume, SetRate) | main to game | `ThreadQueue<ClockCommand>`, applied at the start of a wake, optionally stamped `BeforeTick = N` |
| Ledger rows for tools (`--dump-ticks`, the editor) | game to main | `ThreadQueue<TickRecord>` (`World::TakeNewTickRecords`) |

A few atomics are read across too (the tick count, the current sim rate, the
last-tick flag, the input watermark). None of them carries simulation state.

Nothing crosses from the frame side into the simulation through components. The
compile rules forbid a frame system to write a Simulation component, and a fixed
system to touch a Presentation component.

## 2. Components and singletons

A component is a plain struct with a `static constexpr Fling::ComponentInfo Component`
member (`FlingEngine/Engine/inc/Ecs/Component.h`):

```cpp
struct ComponentInfo
{
    const char* Name = nullptr;                       // saved name; must end with the C++ type name
    ComponentKind Kind = ComponentKind::Table;        // Table, Tag, Singleton, Queue
    ComponentDomain Domain = ComponentDomain::Simulation; // Simulation or Presentation
    bool Transient = false;                           // never saved to a level
    ComponentStorage Storage = ComponentStorage::Table;   // Table, Enableable, Sparse
    bool External = false;                            // singletons/queues made by code outside systems
    std::optional<RollbackClass> Rollback;            // left out: Restore
};
```

Real example, `Sandbox/Gameplay/inc/Stasis.h`:

```cpp
struct Stasis
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "Stasis", .Transient = true, .Rollback = Fling::RollbackClass::Restore};

    /** Simulation time left, in nanoseconds. 0: not in stasis. */
    int64 RemainingNs = 0;
};
```

Compile-time rules on components:

- `.Name` must end with the C++ type name, optionally after a prefix and a dot
  (`"Health"` or `"MyGame.Health"` for `struct Health`). `static_assert` in
  `Detail::CheckedComponentInfo`.
- A `Tag` must be an empty struct, and an empty struct must be a `Tag`.
- A `Transient` Simulation component that is not a queue must set `.Rollback`
  (check `TransientWithoutRollback`).
- `using Requires = Fling::TypeList<Fling::Transform>;` declares required components.
  `storage.Create(...)` and `ctx.Spawn(...)` do not compile without them. Only Table
  and Tag components (not Sparse) can require or be required.
- A component must be nothrow move constructible.

Rollback classes (`RollbackClass`):

| Class | Meaning | Examples |
| --- | --- | --- |
| `Restore` | Game state: saved per tick, restored before a resimulation, hashed | `Transform`, `LookCamera`, `Stasis` |
| `Rebuild` | Made again each tick from other state | `SimTime`, `InputState`, `LocalToWorld`, `Children` |
| `Keep` | A rollback leaves it alone (settings, scratch) | `TransformWork`, `PhysicsSettings`, `EntityGuid` |
| `Subsystem` | A module outside the ECS owns and restores it | `PhysicsWorld`, `PhysicsBody` |

`IsRollbackState(domain, kind, rollback)` is true for `Simulation` + `Restore` + not a
`Queue`. That one rule defines both the future restore set and what the state hash
covers.

What is hashed (`HashSimulationState(const EntityStorage&, const TypeRegistry&)`,
`Ecs/StateHash.h`):

- Every type for which `IsRollbackState` is true, in `TypeHash` order.
- For a Table type, each entity that has it, in entity order (index, then generation).
  For a singleton, the one object. For a Tag, which entities have it.
- Each reflected field by value, `Transient()` fields too. Never padding. A `vec3` is
  hashed as x, y, z. Floats are bit-exact (`-0.0` and `+0.0` differ). A `Name` as its
  hash, a `String` as its text, an `Entity` as index and generation.
- The type hash of each registered `Restore` type is part of the hash, so adding a
  registered `Restore` type changes every state hash, even with no entity having it.
- Not hashed: Presentation, `Rebuild`, `Keep`, `Subsystem` types, and queues.

Register every field of a `Restore` type by hand. `TypeRegistry::Build` reports an
error for a `Restore` type with bytes that no field covers (`UnreflectedBytes` in
`src/Ecs/Reflection.cpp`). It counts a gap as padding when some power-of-two
alignment up to the struct's alignment explains it: a gap before a field that is
smaller than an alignment that puts the field at its offset, or a gap at the end that
is smaller than the struct's alignment. So a missing small member can go unnoticed.
For example, in `{ int64 A; bool B; bool C; }` with `A` and `B` registered, the
missing `C` sits in what looks like tail padding. Real example,
`Sandbox/Gameplay/inc/StasisType.h`:

```cpp
inline void RegisterStasisType(Fling::ModuleRegistration& t_Modules)
{
    t_Modules.Component<Stasis>(
        [](Fling::TypeBuilder<Stasis>& t_Type)
        {
            t_Type.Category("Camera");
            t_Type.Field("remainingNs", &Stasis::RemainingNs).Tooltip("Simulation time left, in nanoseconds");
        });
}
```

A nested field takes two member pointers:
`t_Type.Field("inStasis", &LookCamera::Policy, &LookPolicy::InStasis).Transient();`.
`World::Init` logs an error and returns false when a system's access list names a
`Restore` type that is not registered. A `Restore` type that only game code adds, and
that no system names and nobody registers, is seen by neither check.

Runner-written singletons (`FlingEngine/Engine/inc/Ecs/RunnerWritten.h`, condensed;
the templates are in `namespace Detail`, and `RunnerWritten<R>` names the list):

```cpp
template <UpdateRate R> struct RunnerWrittenOf { using Type = TypeList<SimTime, InputState>; };
template <> struct RunnerWrittenOf<UpdateRate::Variable> { using Type = TypeList<RealFrameTime, PendingInput>; };
```

The runner writes these before the systems of that rate. No system may write them
(`static_assert(!WritesRunnerWritten<A>, ...)` in `Detail::CheckSystem`). Readers need
no edge. The fixed ones appear in `Schedule::ResimInputs` with `FromRunner`.

Singletons are created by the engine. `SystemDesc::Prepare` runs before any `OnCreate`
and makes each singleton that any system names with `Read<T>` or `Write<T>`, and each
queue that a system consumes, with default values, unless it is `.External` or has no
default constructor. A Simulation singleton goes in the simulation storage (the publish
copies it to the mirror if a frame system reads it). A Presentation singleton goes in
the frame storage. So a singleton that systems read and no system writes is valid: it
holds its default values until code outside the systems (for example `Game::Init`) sets
it. The schedule dump lists such types under `Singletons that no system writes`
(`Schedule::ReadOnlySingletons`). Use `setup.AddSingleton<T>(values...)` in `OnCreate`
only for non-default initial values; `SystemSetup` allows it only for a singleton the
system writes or a queue it consumes.

## 3. Systems

A system is a stateless class (`FlingEngine/Engine/inc/Ecs/SystemBase.h`,
`System.h`, `Access.h`, `SystemContext.h`):

```cpp
// Sandbox/Gameplay/inc/Systems/StasisSystem.h (inside namespace Sandbox)
class StasisSystem final : public Fling::System<StasisSystem,
                                                Fling::Access<Fling::Read<Fling::InputState>,
                                                              Fling::Read<Fling::SimTime>,
                                                              Fling::Write<Stasis>,
                                                              Fling::Write<LookCamera>>>
{
public:
    static constexpr Fling::SystemInfo Info{
        .Name = "Sandbox.Stasis", .In = Fling::Order::Simulation, .After = Fling::SystemOf<LookCameraSystem>};

    void FixedUpdate(Context& ctx) const;
};
```

Access items (`Access<...>`, each type at most once):

| Item | Meaning | Order effect |
| --- | --- | --- |
| `Read<T>` | Read `T` (query `const T`) | After an unordered writer of `T` |
| `Write<T>` | Read and write `T`; implies read, do not also list `Read<T>` | Before an unordered reader; two unordered writers are `WriteConflict` |
| `Append<Q>` | Push into queue `Q` (fixed only) | Before the consumer; appenders get no edge between them |
| `Consume<Q>` | Drain queue `Q` (fixed only); one consumer per queue | After every unordered appender |
| `ReadBlend<T>` | Frame systems only, next to `Read<T>`, Simulation Table types only; enables `BlendFrom`/`BlendTo` | Marks `T` as held for the blend |
| `Spawns` | `ctx.Spawn` (fixed only) | none |
| `RawStorage` | `ctx.RawStorage()`, for engine bridges to a subsystem (physics) only; still list every touched type | none |

`SystemInfo` fields, in this order (MSVC rejects out-of-order designators, C7560):

| Field | Type | Meaning |
| --- | --- | --- |
| `Name` | `const char*` | `"Module.Name"`: errors, dump, tie-break, profiling |
| `In` | `GroupRef` | The group; its root decides `FixedUpdate` or `Update` |
| `RunIn` | `WorldRole` | World roles that run it (`Play` default; `Edit`, `Preview`, `All`) |
| `After` | `OrderList` | Groups or systems that run before this one |
| `Before` | `OrderList` | Groups or systems that run after this one |

An `OrderList` holds one `OrderRef` or a braced list of up to 8 (`OrderList::Capacity`).
A reference is a group (`Fling::Order::Physics`, or any `inline constexpr GroupInfo`)
or `Fling::SystemOf<T>`. `T` needs only a forward declaration. The builder finds it by
`SystemTypeId<T>()`, a hash of the type name with class keys removed.

Per-module order headers hold groups and forward declarations of systems that others
may order against, nothing else:

| Module | Header | Declares |
| --- | --- | --- |
| Engine | `FlingEngine/Engine/inc/Ecs/EngineOrder.h` | `Order::Tick`, `Order::Frame`, the eight groups, `Order::All` |
| Physics | `FlingEngine/Physics/inc/PhysicsSystemsFwd.h` | `class PhysicsStepSystem;` |
| Sandbox | `Sandbox/Gameplay/inc/SandboxSystemsFwd.h` | `class LookCameraSystem;`, `class RotatorSystem;` |

A system header includes order headers, never another system's header
(`scripts/check_system_includes.py` checks this in CI). Only the system's `.cpp`, the
module register file and tests include system headers.

`GroupInfo` (`Ecs/Order.h`) fields in order: `Name`, `In`, `After`, `Before`,
`FlushAfter`, `Resim`. Groups are known by name. A group can only name groups defined
above it in the source (write `B.After = A`). Only a direct child of `Order::Tick` may
set `.Resim`. A real nested group from `FlingTests/Physics/PhysicsOrderTests.cpp`:

```cpp
inline constexpr GroupInfo Vehicles{
    .Name = "Game.Vehicles", .In = Order::Physics, .Before = SystemOf<PhysicsStepSystem>};
```

The context API (`SystemContext<Derived, A>`). Every call is checked against the access
list at compile time.

| Call | Needs | Rate | Notes |
| --- | --- | --- | --- |
| `ctx.Query<Ts...>(With<...>{}, Without<...>{})` | `Write<T>` for `T`, `Read<T>` or `Write<T>` for `const T` | both | Table components only; storage order; range-for or `.Each(fn)` |
| `ctx.Get<T>(e)`, `ctx.TryGet<T>(e)` | as Query | both | Table only; `TryGet` returns null if gone or missing |
| `ctx.Has<T>(e)`, `ctx.IsAlive(e)` | `Has`: `Read<T>` or `Write<T>`; `IsAlive`: nothing | both | `Has` works for Table and Tag |
| `ctx.Singleton<T>()` | as Query | both | `Kind == Singleton`; exists from `SystemRunner::Create` |
| `ctx.BlendFrom<T>(e)`, `ctx.BlendTo<T>(e)` | `ReadBlend<T>` | frame | Values at tick N - 1 and tick N; a new entity gives its latest value |
| `ctx.Queue<Q>().Push(r)` | `Append<Q>` | fixed | |
| `ctx.Drain<Q>()` | `Consume<Q>` | fixed | Returns the requests and empties the queue |
| `ctx.Spawn(cs...)` | `Spawns`, `Write<C>` per component | fixed | Immediate; loops already running do not see it |
| `ctx.Add(e, v)`, `ctx.Remove<T>(e)` | `Write<T>` | fixed | Deferred to the end of the tick |
| `ctx.Defer<Ws...>(fn)` | `Write<W>` each | fixed | Escape hatch: `fn` gets the whole storage |
| `ctx.Destroy(e)` | nothing | fixed | Deferred; destroys children too; handle valid for the rest of the tick |
| `ctx.RawStorage()` | `RawStorage` | fixed | Engine bridges only |

Optional hooks: `OnCreate(Setup&)`, `OnDestroy(Setup&)`, `OnPlayBegin(Context&)`,
`OnPlayEnd(Context&)`. The context has no input reader: `ctx.ReadTickInput()` does not
exist, by design.

Compile errors (from `Detail::CheckSystem` and the context; each has a case in
`FlingTests/EcsCompileFail/EcsCompileFail.cpp`):

- A data member in the system (`sizeof(T) == sizeof(ISystem)`), no `.Name`, no `.In`,
  a group not rooted in Tick or Frame, `.Resim` on a group not directly in Tick.
- `Update` in a Tick group, `FixedUpdate` in a Frame group, both, or a non-`const` one.
- Any system writes `SimTime`, `InputState`, `RealFrameTime` or `PendingInput`.
- A fixed system reads `RealFrameTime`, touches any Presentation component (including
  `PendingInput`, `CameraView`), or declares `ReadBlend`.
- A frame system writes, appends to or consumes a Simulation component, lists `Spawns`
  or `RawStorage`, or calls `Add`, `Remove`, `Defer`, `Destroy`, `Queue`, `Drain`,
  `Spawn` or `RawStorage`.
- Any context call on a type the access list does not allow; a const access item; a
  duplicate type; `Read`/`Write` of a queue; `Append`/`Consume` of a non-queue.
- An order list over 8 items or with an empty item; a misspelled group or system.

Schedule errors at startup (`BuildSchedule`, `ScheduleErrorKind` in `Ecs/Schedule.h`).
They are all returned at once in `ScheduleResult::Errors`. `World::Init` runs it, and
the build runs `Sandbox --validate-schedule` after linking (`fling_validate_schedule`,
option `FLING_VALIDATE_SCHEDULE`), so a bad schedule fails `cmake --build`.

| Kind | Cause |
| --- | --- |
| `MissingName`, `DuplicateName`, `AmbiguousSystemType` | Naming problems |
| `MissingGroup`, `DuplicateGroup`, `MissingParent` | Group problems (two different groups with one name; a non-root group with no `.In`) |
| `WriteConflict` | Two writers of one type, no path between them, overlapping `RunIn`. The message names the fix, for example `.After = Fling::SystemOf<Sandbox::LookCameraSystem>` |
| `ConsumeConflict` | Two consumers of one queue |
| `Cycle` | A loop of edges (includes ordering against your own group) |
| `OrderTargetNotAdded`, `EmptyOrderRef`, `OrderListTooLong` | Bad references |
| `OrderAcrossUpdateRates` | An order between fixed and variable work |
| `UpdateRateAccess` | A hand-built `SystemDesc` that breaks a rate rule |
| `FlushNotOrdered`, `FlushInVariableGroup` | `FlushAfter` misuse |
| `ResimPlacement` | `.Resim` on a group not directly in Tick |
| `ResimGap` | A non-resim fixed system not ordered before all or after all resim systems |
| `ResimSkipsState` | A non-resim fixed system writes a `Restore` type, or writes what an earlier resim system reads |
| `ResimAppendsOutput` | A resim system appends to a queue that no resim system consumes (for example `AppRequests`; so app keys live in `Order::Input`) |

How run order is decided (one graph per rate; each group is a begin and an end node):

1. Structure: `In` puts a node between its parent's begin and end. A group's
   `After`/`Before` order the whole group. A child group is not ordered against the
   other members of its parent unless something says so.
2. Stated order: a system's `After`/`Before`, and `SystemRegistry::RunBefore(first, second)`.
   Rules 1 and 2 are edges in one graph; a disagreement is a `Cycle`.
3. Data edges, only for pairs that rules 1 and 2 leave unordered in both directions:
   writer before reader, appender before consumer.
4. Each system gets a segment (count of `FlushAfter` group ends before it) and a stage
   (longest chain of systems before it). Systems run segment by segment, stage by
   stage, and by `Name` inside a stage. Never by registration order, address or thread.

A stated order may put a reader before its writer. That is legal: the reader sees the
previous tick's value, and the schedule records it in `Schedule::Lags`.

Reading the dump (`Sandbox --null-render --unattended --dump-schedule`, or
`Schedule::Dump()`, or editor Windows > World Singletons). Excerpt of the real Sandbox
dump (the group tree and some systems and published types are left out; `...`
shortens long lists):

```text
Fixed (once per tick)
  the runner writes first: SimTime, InputState
  stage 0  Sandbox.AppKeys              [Engine.Input]             writes AppRequests
  stage 1  Sandbox.FirstHit             [Engine.Simulation]        writes SandboxPhysicsState
             one tick late: reads ContactEvents before Physics.Step writes it
  stage 1  Sandbox.LookCamera           [Engine.Simulation]        writes LookCamera
  stage 2  Sandbox.Stasis               [Engine.Simulation]        writes Stasis, LookCamera
             after Sandbox.LookCamera: .After
  stage 3  Sandbox.Mover                [Engine.Simulation]        writes Mover, Transform
             after Sandbox.Rotator: .After
             after Sandbox.Toggles: it writes SandboxToggles
  stage 4  Physics.Step                 [Engine.Physics]           writes Transform, RigidBody, ...
  stage 5  Engine.Transform             [Engine.Finalize]          writes LocalToWorld, TransformWork
  flush: end of tick
  resim runs: Sandbox.BallControl, ..., Physics.Step
  resim reads from outside: InputState (from the runner)
  resim reads from outside: SimTime (from the runner)
Variable (once per frame)
  the runner writes first: RealFrameTime, PendingInput
  stage 0  Sandbox.CameraStatus         [Engine.Presentation]      writes CameraStatus
  stage 0  Sandbox.LookCameraView       [Engine.Presentation]      writes CameraView
  stage 1  Graphics.PublishSnapshot     [Engine.Render]            writes RenderSnapshot
Published each tick, for Order::Frame
  LookCamera (table), read by Sandbox.CameraStatus, Sandbox.LookCameraView
  Transform (table), read by Graphics.PublishSnapshot; held for the blend of Graphics.PublishSnapshot
```

`writes` lists every non-read access (appends too). `after X: .After` is a stated order;
`after X: it writes T` is a data edge (`it appends to T` for queues). `one tick late`
is a lag (`one frame late` at the variable rate). `resim reads from outside` is
`Schedule::ResimInputs`. `Published each tick` is `Schedule::Published`.

The engine plus physics schedule is checked in at
`FlingTests/Physics/Schedules/EnginePhysics.schedule.txt`. On a mismatch the test writes
`EnginePhysics.schedule.actual.txt`; copy it over the checked-in file when the change is
wanted.

## 4. Time

All times are `int64` nanoseconds from one clock, `PlatformTime::NowNanoseconds()`
(steady clock, or a `PlatformTime::VirtualClock` headless and in tests).

Tick length: `SimRate::StepNs(hz)` (`Core/inc/SimRate.h`), whole nanoseconds,
33 333 333 at 30 Hz. Valid rates are 1 to 1000 (`SimRate::IsValidHz`). The only source
of the length is `SystemRunnerConfig::SimHz`.

`TickTimeline` (`Core/inc/TickTimeline.h`) maps tick numbers to windows. Tick `n` covers
`[StartOf(n), StartOf(n) + step)` with `StartOf(n) = BaseNs + (n - BaseTick) * StepNs`.
Windows are computed from the tick number, never summed. A tick is due when
`now >= End(next)`. The first tick is 1. `TickWindow` is `{StartNs, EndNs, Trigger}`
with `TickTrigger::Clock` or `TickTrigger::Request`. The window is wall-clock time: it
is for input assignment, pacing and presentation, never for simulation results.

`SimTime` (`Ecs/SimTime.h`) is what a fixed system may read: `Tick`, `FixedDeltaNs`,
`ElapsedNs`, `DeltaSeconds()`, `ElapsedSeconds()`. The runner writes it before each tick
from the stamp. `ElapsedNs` is computed from the tick number and a rate base, never a
running sum. Read `FixedDeltaNs` every tick and use `ElapsedNs`; never assume a rate or
compute `Tick * FixedDeltaNs`.

`TickStamp` (`Ecs/TickStamp.h`) is the one identity of a tick:

```cpp
struct TickStamp
{
    uint64 Tick = 0;          // first tick is 1; 0 means none
    int64 FixedDeltaNs = 0;   // the tick's length
    int64 ElapsedNs = 0;      // sim time at the tick's end
    TickWindow Window;        // wall clock: input assignment and pacing only
    InputCut Cut;             // input messages the tick consumed
    uint64 StateHash = 0;     // HashSimulationState after the tick (0 if no hasher)
};
```

`SimTimeOf(stamp)` rebuilds the tick's `SimTime`, so a tick can be rerun from its ledger
row alone. Holders: `TickRecord::Stamp` (the ledger, `TickLedger = TickHistory<TickRecord, 256>`),
`TickPacketHeader::Stamp`, `RealFrameTime::Latest`/`BlendFrom`/`BlendTo`. `Tick`,
`FixedDeltaNs`, `ElapsedNs`, `Cut` and `StateHash` are the same for the same input at
any frame rate; `Window` and `TickRecord::RanAtNs` are not.

One tick (`SystemRunner::RunOneTick`, `src/Ecs/SystemRunner.cpp`):

1. `TickTimeline::Advance` gives the window; the tick number goes up by one.
2. The tick input source fills `InputState::Tick` (the cut is recorded in the stamp).
3. The runner writes `SimTime = SimTimeOf(stamp)`.
4. Under a `SimulationCheck::Scope`, the fixed segments run, then `FlushCommands`
   applies deferred adds, removes and destroys (sorted by entity, then call order).
5. The state hash is computed (if enabled), the record goes into the ledger, and the
   tick output moves `AppRequests` into the outbox.

Catch-up budget: `SystemRunnerConfig::MaxCatchUp` is 250 ms. `MaxTicksPerWake()` is that
time in ticks, rounded up (8 at 30 Hz, 250 at 1000 Hz). If a wake ran that many ticks
and time is still due, `TickTimeline::DropBacklog(now)` moves the windows forward by
whole steps. No tick number is skipped; the dropped count goes in
`TickRecord::DroppedSteps`. A clock that only moves with frames (headless) never drops.

Run-time rate change is a tick-stamped command, never shared state.
`World::RequestSimHz(uint32 t_Hz, String& t_OutError, uint64 t_BeforeTick = 0)` sends a
`ClockCommand` of type `ClockCommandType::SetRate`. The tick thread applies it in
`World::ApplyClockCommands`, between two ticks, in every exec mode
(`SystemRunner::SetSimHz`, `TickTimeline::SetStep`). Tick N, the first at the new rate,
starts where tick N - 1 ended; `ElapsedNs` continues from the old rate's base.
`BeforeTick = 0` means the next wake (the GPU Options window). `World::RequestSimHz`
(and `RunControls::RequestSimHz`, which calls it) returns false, with the reason, for
a rate outside 1 to 1000 or while a replay or recording runs; the rule is checked again
when the command is applied. Pause and Resume are
`World::PauseTicks()`/`ResumeTicks()`; `SimTime` never sees a pause.

`--dump-ticks <file>` writes one ledger line per tick (`Engine::DumpNewTicks`):

```text
tick=4 dt=33333333 elapsed=133333332 cut=7 samples=0 keys=2 hash=de17e9c7b3aa3011
```

`dt` and `elapsed` come from the stamp, `cut` is `Stamp.Cut.LastSequence`, `samples` and
`keys` are input counts, `hash` is the state hash. A line ends in ` input-overran` when
the input ring overwrote messages before the tick took them. The `cut` column is a raw
queue sequence: device connect messages take sequences too, so a run and the replay of
its recording can differ there while samples, keys and hash agree. The state hash is on
for `--null-render` and `--dump-ticks`, off in a windowed game.

What makes it deterministic:

- Systems in one stage run by name; queries return storage order; the flush sorts.
- Fixed systems read no clock. `SimTime` has no wall time. `PlatformTime`, `Timing`,
  the global `Random` and `Input` internals call `FLING_SIMULATION_FORBID`, which asserts
  in debug builds while fixed systems run or flush (not in release).
- Input reaches a tick only through its cut. Random numbers come from `SimRandom`
  (`Core/inc/SimRandom.h`), a pure function of seed, stream and draw number.
- The simulation never reads presentation state or frame timing.
- Floating point contraction is off for every first-party target: MSVC `/fp:precise`,
  clang-cl `/fp:precise` plus `/clang:-ffp-contract=off`, GCC and Clang
  `-ffp-contract=off -fno-fast-math` (`fling_set_deterministic_math` in
  `CMake/FlingTargets.cmake`, called by `fling_configure_target`). Simulation code calls
  no `sin`, `cos`, `atan2`, `exp`, `log` or `pow` of the C library, and no glm helper
  built on them: it uses `SimMath` (`Core/inc/SimMath.h`), which uses only operations
  that IEEE 754 defines exactly ([`Skills/ecs.md`](../Skills/ecs.md) rule 6). GLM types
  are packed on every platform (`FlingMath.h`), so components have one layout. Before
  that, the pins were Windows values: MSVC Debug, MSVC RelWithDebInfo and clang-cl Debug
  agreed with each other, and Linux did not.
- The hash reads values in little-endian byte order (every supported platform). Windows
  and Linux must give the same hash for the same ticks, so the pinned dump hashes are one
  value for every platform. They were measured on Linux (GCC and Clang); MSVC and clang-cl
  have not confirmed them yet.

## 5. Publish and blend

After every tick in `inline` and `threaded` modes, the game thread fills a `TickPacket`
(`Ecs/Publish/TickPacket.h`, filled by `TickPacketWriter::Fill`) and publishes it to a
`TripleBuffer<TickPacket>`. The packet is the full published state, not a delta:
`Header` (`PacketNumber`, `Stamp`), `Alive` entities, `Tables`, `Singletons`, and
`Previous`: tick N - 1's stamp and values of the blended types
(`PublishedSet::Blended`, from types that a frame system declares `ReadBlend<T>` for).
The newest packet wins; a reader that skips packets loses nothing.

The main thread applies the newest packet to the mirror (`MirrorApplier::Apply`) at the
start of `RunFrame`. The mirror keeps the simulation's entity handles (index and
generation), so stored `Entity` values are valid on both sides. The frame side cannot
create entities in the mirror. In `direct` mode, `DirectLink` reads the simulation
storage and copies the blended types after each tick instead.

The blend (`FrameBlend`, `Ecs/Publish/FrameBlend.h`) is owned by the world and lives on
the frame side only. It keeps its own drawn point and moves it by frame time:

```text
Drawn      = clamp(DrawnLastFrame + FrameDeltaNs / TickLengthNs, N - 1, N)
BlendAlpha = Drawn - (N - 1)
```

`TickLengthNs` is the length of `BlendTo`'s window from its stamp, so no rate is
assumed. `RunFrameAt` writes `RealFrameTime::BlendFrom` (tick N - 1), `BlendTo` (tick N,
equal to `Latest`) and `BlendAlpha`. Properties:

- Never waits for the game thread. The frame cap is the only thing that paces frames.
- Never extrapolates: with no new tick the point stops at `BlendTo` and frames repeat.
- No added latency or margin: the drawn state is at most one tick behind the latest
  published tick.
- Several new ticks at once move the point up to `BlendFrom` (a skip), never a faster
  blend. The first frame of a link starts at `BlendFrom`. With one state, a tick with a
  `Request` trigger, or interpolation off, the frame draws `BlendTo` with factor 1.
- Presentation only: no tick, cut, hash or checksum depends on it.

A frame system that draws between ticks declares `ReadBlend<T>` next to `Read<T>` and
blends the two ends, not `BlendFrom` toward the query value. The real example is
`FlingEngine/Graphics/inc/Systems/PublishSnapshotSystem.h`:

```cpp
class PublishSnapshotSystem final : public System<PublishSnapshotSystem,
                                                  Access<Read<RealFrameTime>,
                                                         Read<Transform>,
                                                         ReadBlend<Transform>,
                                                         Read<Parent>,
                                                         Read<LocalToWorld>,
                                                         Read<MeshRenderer>,
                                                         Read<PointLight>,
                                                         Read<DirectionalLight>,
                                                         Write<RenderSnapshot>>>
```

It uses `Interpolate::BlendedWorld` (`FlingEngine/Engine/inc/Interpolate.h`, with `Lerp`,
`Slerp`, `Blend`): it blends local transforms up the parent chain, then composes
matrices. `Read<T>` always gives the latest tick, so unblended values, spawns and
removals show up to one tick before the blended transforms reach that tick.

The interpolation toggle (`--interpolation`, `[Engine] Interpolation`, GPU Options,
`World::SetInterpolation`, `FrameBlend::SetInterpolation`) only changes the frame side.
Off means `BlendFrom == BlendTo` and factor 1.

The look camera is not blended. Its view is the latest published `LookCamera` run
through the pending input, which is newer than either tick.

## 6. Input

One stream, written only by the main thread. `InputMessageQueue`
(`Core/inc/Input/InputMessageQueue.h`) is a ring of `DefaultCapacity = 4096` slots.
Each slot is a seqlock: the writer marks the slot busy, writes, then stores the sequence
with release; a reader loads, copies, and loads again, and retries on a mismatch. The
writer never blocks. Each message has a `Sequence`, a `TimeNs` and the UI capture at the
time it was written, so both readers drop the same captured messages.

Two readers build the same type, `InputSlice` (`Core/inc/Input/InputSlice.h`: `Cut`,
`MouseSamples`, `KeyEvents`, `Overran`), with the same `InputSlice::Append`:

| Side | Type | Written by | Read by | Consumes |
| --- | --- | --- | --- | --- |
| Fixed | `InputState` (`Engine/inc/Components/InputState.h`): a Simulation singleton, `Rebuild`, holding `TickInput Tick` (`Slice`, merged `Keys`, `Capture`) plus `IsHeld`, `WasPressed`, `WasReleased`, `GetAxisValue` | The runner, before the tick's systems | Fixed systems with `Read<InputState>` | yes |
| Frame | `PendingInput` (`Engine/inc/Components/PendingInput.h`): a Presentation singleton deriving from `InputSlice` | The runner, before the frame's systems, from scratch each frame | Frame systems with `Read<PendingInput>` | no |

The cut: `InputCut` (`Core/inc/Input/InputCut.h`) is the range
`(AfterSequence, LastSequence]` a tick consumed. `TickInputProvider::Build`
(`Core/inc/Input/TickInputProvider.h`) is the one assignment rule:

1. Replay with cuts: the tick takes exactly the recorded cut
   (`InputPlayback::ClaimTickCut`).
2. Otherwise, a `Clock` tick takes, in sequence order, the untaken messages with
   `TimeNs < Window.EndNs`. A message at exactly `EndNs` belongs to the next tick.
3. A `Request` tick takes everything queued.
4. The cut goes to the recorder (`InputRecorder::NoteTickCut`).

The cut is stored in the tick's `TickStamp`. The packet carries that stamp, so the frame
gets the cut as `RealFrameTime::Latest.Cut`, together with the state of the same tick.
`RunFrameAt` fills `PendingInput` with every message after `Latest.Cut.LastSequence`
(empty before the first tick). State, tick number and cut always come from one packet:
no torn pair is possible.

Each frame polls twice: `Input::Poll` at the start, and `Input::PollLate` right before
the frame systems (`World::SetLateInputPoll`). The late poll makes input that arrived
while the ticks ran visible as pending in the same frame.

Live rule for threaded windowed play: the tick does not wait for the main thread's poll
(`TickInputProviderConfig::WatermarkWaitNs = 0`). Headless and replay runs wait without
limit (`-1`), which makes their cuts equal to `direct` mode. So live threaded play is
reproducible only from its recording.

Record and replay: `--record-input <file>` and `--replay-input <file>` use
`.input.jsonl` files (format in `Skills/input-playback.md`). A recording stores each
tick's cut and its tick rate (`tickCuts`). A replay gives every tick exactly that input
in every exec mode and at any frame rate. A replay at another rate is refused with exit
code 3. A file without cuts replays by time. Replay overrun is an error: when the ring
overwrites messages before a tick takes them, both sides drop the same range
(`TickInputBuilder::Resync` on the tick, `PendingInputReader` on the frame), the tick is
marked `TickRecord::InputOverran`, and a replay stops with exit code 1
(`InputPlayback::NoteTickOverran`). Recording and playback are compiled out in shipping
builds (`WITH_INPUT_RECORDING=0`).

Fold per sample: a rule must apply each mouse sample in sequence order and key edges at
their place in the stream (`ForEachInOrder(samples, keys, onMouse, onKey)`). Then
`Run(Run(c, a), b) == Run(c, a + b)` bit for bit, so the result does not depend on how
input is split into ticks.

## 7. Presentation-side camera with freeze and stasis

Files: `Sandbox/Gameplay/inc/LookCamera.h` (`LookView`, `LookPolicy`, `LookCamera`),
`LookUtil.h` (the pure rule), `Stasis.h` (`Stasis`, `StasisUtil`),
`Systems/LookCameraSystem.h`, `LookCameraViewSystem.h`, `StasisSystem.h`,
`CameraStatusSystem.h`, and `FlingEngine/Engine/inc/Components/CameraView.h`.

`LookCamera` is a Simulation Table component, `Restore`, holding `LookView View`
(`Yaw`, `Pitch`) and `LookPolicy Policy` (`LookScale`, `Frozen`, `InStasis`,
`PitchMin`, `PitchMax`). The policy is data in the sealed component. The frame side
treats it as plain data.

The same pure function runs on both sides:

```cpp
// Sandbox/Gameplay/src/Systems/LookCameraSystem.cpp (fixed, Order::Simulation)
const Fling::InputSlice& input = ctx.Singleton<const Fling::InputState>().Tick.Slice;
for (auto [entity, camera] : ctx.Query<LookCamera>())
{
    camera = LookUtil::Run(camera, input);
}

// Sandbox/Gameplay/src/Systems/LookCameraViewSystem.cpp (frame, Order::Presentation)
const Fling::PendingInput& pending = ctx.Singleton<const Fling::PendingInput>();
Fling::CameraView& view = ctx.Singleton<Fling::CameraView>();
view.Active = false;
for (auto [entity, camera] : ctx.Query<const LookCamera>())
{
    const LookView shown = LookUtil::Run(camera, pending).View; // the same rule the tick runs
    view = Fling::CameraView{.Active = true, .Yaw = shown.Yaw, .Pitch = shown.Pitch};
}
```

Why it works: the frame side keeps no state between frames. Each frame computes
`Run(sealed camera, input after the sealed cut)`. When a tick consumes a message, the
message is in the sealed camera and no longer pending, so each message is applied exactly
once. The view that a frame shows is what the ticks will make from the same input. The
renderer (`VulkanApp::Update`) applies `CameraView` when `Active` is true, else it keeps
its debug camera.

`LookUtil::Detail::Apply` drops mouse input when `Policy.Frozen || Policy.InStasis`.
The two flags differ:

| | `Frozen` (F key) | `InStasis` (G key, `Sandbox.Stasis`) |
| --- | --- | --- |
| Source | Input, handled inside `LookUtil::Run` at its place in the stream | A gameplay decision made only by a fixed system |
| Frame side sees it | At once (the press is in `PendingInput`) | From the first frame that shows the tick that set it |

`StasisSystem::FixedUpdate` reads `InputState::WasPressed(Keys::G)` and
`SimTime::FixedDeltaNs` and calls `StasisUtil::Step`, which counts `RemainingNs` down,
restarts a full stasis on a press (`LengthNs` rounds 2 s to whole ticks), and sets
`Policy.InStasis = RemainingNs > 0`. `Sandbox.Stasis` must run after `Sandbox.LookCamera`
(`.After = Fling::SystemOf<LookCameraSystem>`). Then the tick that takes the press first
turns the camera by all of its own input under the old policy, as the frames before it
showed, and only then sets the flag. Ordered the other way, the tick drops input that the
frames already showed, and the view snaps back. Both systems write `LookCamera`, so
without the order the schedule reports `WriteConflict`. `Sandbox.CameraStatus` (frame)
reads the published `LookCamera` and `Stasis` and writes the Presentation singleton
`CameraStatus` for the UI.

Known threaded take-back limit: in threaded play, a tick is published after it runs. A
frame that runs between a tick's cut and its publish shows the previous tick plus the
input after the new cut under the old policy. If the new tick starts a stasis, that input
turns the view and is taken back when the tick arrives; at the end of a stasis it shows
late. Test "Stasis: with a late publish, ..." in `FlingTests/SandboxExample/StasisTests.cpp`
pins it. No order can fix it, because the frame side cannot know a gameplay decision
before its tick arrives. `Frozen` has no such gap.

Other limits: `CameraView` is one view (with several `LookCamera` entities, the last in
storage order wins); only the camera that `SandboxGame` makes gets a `Stasis`;
`WasPressed` is per tick (two presses in one tick start one stasis).

## 8. Threading

Exec modes (`ExecMode` in `Core/inc/ExecMode.h`; `--exec`, `[Engine] Exec`). The world
picks two policy objects at play start (`World::StartGame`, `World::StartPlayDriver`);
`World::Update` and `World::RunWake` have no mode branch.

| Mode | `TickDriver` (who runs wakes) | `PresentationLink` (what frames read) | Simulation storage owner lane |
| --- | --- | --- | --- |
| Edit (editor, not playing) | `SameThreadDriver` | `DirectLink` | `Lane::None` |
| `direct` | `SameThreadDriver` | `DirectLink` (the simulation storage) | `Lane::None` |
| `inline` | `SameThreadDriver` (wake runs under a `Lane::Game` scope) | `MirrorLink` | `Lane::Game` |
| `threaded` (default) | `GameLane` (game thread) | `MirrorLink` | `Lane::Game` |

Thread ownership:

| Thread | Owns |
| --- | --- |
| Main ("Main Thread", `Lane::Frame`) | Window and GLFW, input polling (sole input writer), ImGui and editor, play requests, outbox apply, frame systems on the mirror, `RenderSnapshot`, Vulkan |
| Game ("Game Thread", `Lane::Game`) | Only `World::RunWake`: clock commands, fixed systems, ledger, outbox entries, publish |
| Physics workers (`JoltJobSystem`) | Jolt jobs inside `Physics::Tick`, fork and join; never systems |

One wake on the tick thread: read the clock, `ApplyClockCommands`, run due ticks
(`RunFixedAt`, at most `MaxTicksPerWake()`), drop backlog after a hitch, publish if any
tick ran, then `GameLane::WaitForNextWake` (`GameLanePacing::RealTime` sleeps until the
next tick is due, at most 100 ms; `AsFastAsPossible` for headless; `Stepped` for tests).

One frame on the main thread (`Engine::RunFrames`): `Input::Poll` (in
`Engine::PumpInput`); `RunControls::OnFrame`; `World::Update` (play requests, the
driver's `OnFrame`, which runs the due ticks in `direct` and `inline`,
`ApplyTickOutbox`, then `RunFrame`: `ShowLatest`, `Input::PollLate`, `RunFrameAt`
runs the frame groups); `Engine::DumpNewTicks`; `Engine::HandleAppRequests`; then the
frame limiter (headless) or `Engine::Present`.

No shared lock in live play. The two threads share only the packet triple buffer (one
atomic exchange per side), the input ring (seqlock slots and a watermark atomic; readers
retry, never wait) and the `ThreadQueue`s (outbox, clock commands, ledger rows for tools).
A `ThreadQueue` mutex is held only for a `push_back` or a vector swap. No other mutex is
taken by both threads in live play. The playback cut lock is taken only while a replay
with cuts plays, and the recorder cut queue only while recording.

What may block, and where:

- The main thread never waits for the game thread in play. At Stop it joins the game
  thread after its current wake.
- In headless runs and in `--replay-input`, a tick waits for the main thread's input
  poll. `TickInputProvider` loops on `InputMessageHandler::WaitForPoll` with a timeout
  of at most 10 ms (`LongestWriterWait`) and checks for a stop between waits. A replay
  with cuts waits for its recorded cut the same way. Live windowed play does not wait.
- The game thread sleeps between ticks (`RealTime` pacing) and on an `Event` while paused.
- The main thread sleeps in the frame limiter, and in `glfwWaitEvents` while minimised
  (which also stalls a threaded replay, since replay ticks wait for polls).

Debug ownership checks: `FLING_LANE_CHECK(owner)` (`Core/inc/Threading/Lane.h`) fires
when a thread tagged with one lane calls an entry point of data that another lane owns.
The checks sit at entry points, not on every access through a pointer or reference
already returned: the `EntityStorage` entry points in `NativeStorage`, the mirror apply,
the input writer calls (`Input::Poll`, `PollLate`, the handler's poll), the tick input
build, and `Timing`, the global `Random`, `Stats` and `ResourceManager` (main thread
only). Only the main thread (`Lane::Frame`, in `Engine::RunFrames`) and the game thread
(`Lane::Game`) are tagged; untagged threads and unowned data pass. Lane checks compile
out under `NDEBUG` or `FLING_SHIPPING`; `SimulationCheck` compiles out under `NDEBUG`.

## 9. Settings and command line

`RunSettings` (`Core/inc/RunSettings.h`) is read once at startup with one precedence:
flag, then `[Engine]` in `Config/EngineConf.ini`, then default. Every key in the file is
checked in every run, even when a flag wins. A bad value exits with code 3 and names the
flag or key. The startup line `Run settings: ...` names each value and its source.

| Setting | Flag | `[Engine]` key | Default | Live change |
| --- | --- | --- | --- | --- |
| Exec mode | `--exec direct\|inline\|threaded` | `Exec` | threaded | no |
| Tick rate | `--sim-hz N` (1 to 1000) | `SimHz` | 30 | GPU Options, at a tick boundary |
| Frame cap | `--max-fps N` (0: none, max 1000) | `MaxFps` | monitor refresh rate, else 60 (`FallbackMaxFps`); none with `--null-render` | GPU Options |
| VSync | `--vsync on\|off` | `VSync` | on | GPU Options (swap chain rebuilt) |
| Interpolation | `--interpolation on\|off` | `Interpolation` | on | GPU Options |
| Thread jitter (tests) | `--thread-jitter S:U` (threaded only; an error otherwise) | none | off | no |

Other flags: `--null-render` (no window or GPU, virtual clock), `--offscreen` (render
with the GPU into images, no window, with the timing of `--null-render`),
`--resolution WxH`, `--screenshot <file.png>` and `--screenshot-ticks L` (PNG of the
frame that shows each tick), `--dump-state <file.json>` and `--dump-state-ticks L` (the
simulation state as JSON after each tick), `--unattended` (no
dialogs; a crash exits 2 with a stack trace), `--frames N` (see below),
`--frames-per-tick N` (headless frames between ticks), `--seed S`, `--level <file>` (relative to `Assets/`),
`--play`, `--dump-schedule`, `--validate-schedule`, `--dump-ticks <file>`,
`--record-input <file>`, `--replay-input <file>`, `--log-format json`. Exit codes
(`ExitCode.h`): 0 success, 1 failure, 2 crash, 3 bad arguments or content.

`--frames N` (`Engine::LastTickOfRun`, `Engine::ShouldTick`): no tick runs after tick
`ceil(N / frames-per-tick)` (`World::SetLastTick`), and the run exits at the first frame
where at least N main-thread frames have run and the frame side shows that tick. So:

| Run | Frames | Ticks |
| --- | --- | --- |
| `--null-render`, `--exec direct` or `inline` | exactly N | `ceil(N / frames-per-tick)` |
| `--null-render`, `--exec threaded` | at least N, usually more (467 and 491 for `--frames 120` in two runs) | `ceil(N / frames-per-tick)` |
| Windowed, any mode | at least N; the run lasts until that tick is due on the clock (4 s for `--frames 120` at 30 Hz) | `ceil(N / frames-per-tick)`, which is N by default |

The comment in `LaunchOptions.h` ("Run N frames, then exit") reads as an exact frame
count; trust the table. A test that needs an exact tick count reads the tick count
(`Fling exit: ... ticks=N`) or the dump, not the frame count.

VSync (`ChoosePresentMode`, `FlingEngine/Graphics/inc/PresentMode.h`): on gives FIFO;
off gives IMMEDIATE, else MAILBOX, else FIFO. `LimiterFps` combines cap and mode: with a
mode that waits for the display (FIFO, FIFO_RELAXED) and a cap at or above the refresh
rate, the limiter does not wait and the display paces frames; below the refresh rate, or
with MAILBOX/IMMEDIATE, the cap applies. In the background the cap is at most
`RunSettings::BackgroundMaxFps` (30). The game thread's ticks are never capped. The
limiter (`Core/inc/Threading/FrameLimiter.h`) sleeps with a high-resolution timer until
1 ms before the frame is due, then spins.

The GPU Options window (editor, menu Windows; also drawn in `--play`, where the Sandbox
opens it at start) changes VSync, frame cap, "Simulation rate (Hz)" and "Render
interpolation" at run time through `RunControls` (`Engine/inc/RunControls.h`). It holds no
rules of its own. No frame setting changes a replay or a headless run.

## 10. Build and test

Generic build (any platform):

```bash
./Init.sh        # or Init.bat: submodules, local Catch2, configure into build/
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build            # add -C <Debug|Release> for Visual Studio
```

Options: `-DDEFINE_SHIPPING=ON` (sets `FLING_SHIPPING`, removes input recording),
`-DFLING_BUILD_TESTS=OFF`, `-DFLING_PHYSICS_PROVIDER=Jolt|Bullet|None`,
`-DFLING_VALIDATE_SCHEDULE=OFF`. Shaders compile to `build/Shaders`; never commit `.spv`.

On Windows, run these from a Developer Command Prompt (MSVC tools and the Vulkan SDK on
the path). With Ninja (`cmake -G Ninja ...`) the Sandbox binary is
`build/Sandbox/bin/Sandbox(.exe)` and the test binaries are in `build/FlingTests/bin/`;
the Visual Studio generator adds a `Debug` or `Release` folder. Start the Sandbox and
the tests from the repository root. clang-cl configure flags are in
`Skills/building.md`.

Headless run: `Sandbox --null-render --unattended --frames 120 --seed 1` (the last log
lines print `Sandbox: 754 transforms, checksum ...` and `Fling exit: code=0 ticks=120 ...`).
To see what a run did without a window, read the state (`--dump-state`), or render
offscreen and look at a PNG (`--offscreen --screenshot`; a software Vulkan driver such
as llvmpipe is enough): `Skills/testing.md`, "Seeing the game without a window".

Test layout (Catch2 3; each executable links only its module):

| Executable or test set | Source | Covers |
| --- | --- | --- |
| `CoreTests` | `FlingTests/Core/` | Timeline, input queue and threads, tick input, run settings, frame limiter, triple buffer |
| `ResourcesTests` | `FlingTests/Resources/` | Config, JSON, run settings from ini |
| `EngineTests` | `FlingTests/Engine/` | ECS, schedule (`EcsScheduleTests.cpp`), publish, blend, threaded world (`ThreadedWorldTests.cpp`), tick time and hash (`TickTimeTests.cpp`), input boundary |
| `GraphicsTests` | `FlingTests/Graphics/` | Interpolation in every mode, present mode |
| `PhysicsTests` | `FlingTests/Physics/` | Provider determinism, order test and checked-in schedule |
| `ApplicationTests` | `FlingTests/Application/` | Engine smoke test |
| `SandboxExampleTests` | `FlingTests/SandboxExample/` | Look camera and stasis (`LookRig.h`, `LookCameraTests.cpp`, `StasisTests.cpp`) |
| `ECS compile check: ...` | `FlingTests/EcsCompileFail/` | Must-not-compile rules |
| `Isolation: ...` | `FlingTests/CMakeLists.txt` | Module header visibility |
| `Sandbox: ...` | `Sandbox/CMakeLists.txt`, `Sandbox/Tests/*.cmake` | Headless runs, tick dumps, replays |

Compile-fail checks: one `#elif defined(FLING_ECS_CASE_<Name>)` block in
`EcsCompileFail.cpp` and one `fling_add_ecs_compile_check(<Name> "<regex>")` line in
`EcsCompileChecks.cmake`. The test passes only if the build fails and its output matches
the regex. Current cases (plus a `Good` control case): `QueryUndeclared`,
`WriteThroughRead`, `HasUndeclared`, `GetUndeclared`, `SingletonUndeclared`,
`BlendToUndeclared`, `QueueUndeclared`, `DrainUndeclared`, `AddUndeclared`,
`RemoveUndeclared`, `DeferUndeclared`, `SpawnComponentUndeclared`, `FilterUndeclared`,
`QuerySingleton`, `SpawnUndeclared`, `RawStorageUndeclared`, `FrameDestroy`,
`CommandsAddUndeclared`, `FrameQueue`, `FrameDrain`, `FrameSpawn`, `FrameRawStorage`,
`FrameListsSpawns`, `FrameListsRawStorage`, `MemberState`, `WrongUpdateRate`,
`NonConstUpdate`, `FixedWithUpdate`, `FrameWithFixedUpdate`, `FrameWithoutUpdate`,
`NoGroup`, `NoName`, `FixedReadsRealTime`, `FixedReadsPresentation`,
`FrameWritesSimulation`, `WritesSimTime`, `WritesInputState`, `WritesRealFrameTime`,
`WritesPendingInput`, `ReadsTickInput`, `ReadsPendingInput`, `SingletonNotOwned`,
`QueueMadeByAppender`, `QueueRead`, `NotAComponent`, `ConstAccessItem`,
`NotAnAccessItem`, `AppendNonQueue`, `AccessTagWithData`, `AccessEmptyTable`,
`NameMismatch`, `DuplicateAccess`, `CreateWithoutRequired`, `RequireEachOther`,
`OrderListTooLong`, `GroupWithoutRoot`, `OrderGroupTypo`, `OrderSystemTypo`,
`FixedReadsPendingInput`, `ResimGroupInFinalize`, `ResimGroupInSimulation`,
`OrderListEmptyItem`, `TransientWithoutRollback`, `BlendInFixedSystem`,
`BlendOfPresentation`, `BlendOfSingleton`, `BlendWithoutRead`, `BlendUndeclared`. Each
new case must produce exactly one compiler error.

Determinism ctests:

- `Sandbox: tick dump exec modes cuts | cuts 60 Hz | time | jitter | stasis`
  (`Sandbox/Tests/CompareExecModes.cmake`): byte-identical `--dump-ticks` against a
  `direct` run, for `inline`, `inline --frames-per-tick 3`, `threaded`, `threaded
  --frames-per-tick 3`; `time` and `stasis` add threaded at `--max-fps` 30, 60, 144 and
  1000; `jitter` is 10 threaded jitter seeds at `--max-fps 1000`. Each fails unless the
  reference dump has mouse samples and more than one hash.
- `Sandbox: tick dump replay by cuts | by time | ...` (`CompareTickDumps.cmake`).
- `Sandbox: tick dump level Default | Level_1 | PhysicsTest`: two direct runs, inline
  and threaded, no input.
- EngineTests `[threads]`: 30 000 ticks of a free game thread against `direct`, jitter
  runs, and live play replayed from its recorded cuts.

Pinned SHA-256 values (Jolt provider only, in `Sandbox/CMakeLists.txt`, passed as
`-DEXPECT_SHA256`): `_LevelDumpSha256_Default`, `_LevelDumpSha256_Level_1`,
`_LevelDumpSha256_PhysicsTest` (300-tick level dumps), and the 600-tick
`exec modes time` replay of `look_camera.input.jsonl`. They must change, deliberately,
when simulated results change: a gameplay or physics change, a new or changed field of a
`Restore` type, or a new registered `Restore` type (its type hash enters every state
hash). The failing test prints the new SHA-256 and the last tick line; update the pin in
the same change and say why. A pin that changes with no simulation change means
something nondeterministic reached the hashed state; find it, do not repin.

Run a determinism set repeatedly:
`ctest --test-dir build -R "Exec modes|tick dump|threaded headless|threaded replay|look camera replay|Input boundary" --repeat until-fail:10`
(from cmd or PowerShell; from Git Bash through `cmd //c` the quoted `|` does not survive).

## 11. Recipes

### Add a component

1. Declare the struct with `static constexpr Fling::ComponentInfo Component{.Name = "Game.Foo"}`.
   Choose `Kind`, `Domain` (Presentation for frame-only data), and `Rollback`. A
   `Transient` Simulation type must set `Rollback`.
2. Register it in the module register file (for the Sandbox,
   `Sandbox/Gameplay/src/RegisterSandbox.cpp`, `Game::Register`):
   `t_Modules.Component<Foo>([](Fling::TypeBuilder<Foo>& t_Type) { t_Type.Field("bar", &Foo::Bar); });`.
   Register every field of a `Restore` type (`Transient()` on fields a level must not
   save). A Presentation or `Keep` transient type may use `t_Modules.Component<Foo>()`.
3. Expect pinned dump hashes to change if it is a registered `Restore` type.

### Add a fixed-tick system

1. Header `inc/Systems/FooSystem.h`: derive from `Fling::System<FooSystem, Fling::Access<...>>`,
   `static constexpr Fling::SystemInfo Info{.Name = "Game.Foo", .In = Fling::Order::Simulation};`,
   declare `void FixedUpdate(Context& ctx) const;`. Include only order headers and
   component headers.
2. Body in `src/Systems/FooSystem.cpp`. Read time from `SimTime`, input from
   `InputState`, randomness from `SimRandom` or component state. No statics, no members,
   no globals. Put shared logic in a `*Util` free function.
3. Register: `t_Modules.System<FooSystem>();`.
4. Check `--dump-schedule`: stage, edges, lags. Add `.After`/`.Before` only where data
   does not already order it or gives the wrong order.
5. Side effects (requests to the application, sounds) go through a queue consumed by a
   resim system, or the system goes in `Order::Input` (`ResimAppendsOutput`).
6. Test with `EntityStorage` + `SystemRegistry` + `SystemRunner` (see
   `FlingTests/Engine/EcsSystemTests.cpp`).

### Add a presentation system

1. `.In = Fling::Order::Presentation` (or `Render`), implement `void Update(Context& ctx) const`.
2. Read Simulation types with `Read<T>`; they enter the published set automatically.
   Write only Presentation components. Read frame time from `RealFrameTime`.
3. To draw between ticks: add `ReadBlend<T>` next to `Read<T>` and blend
   `ctx.BlendFrom<T>(e)` toward `ctx.BlendTo<T>(e)` by `RealFrameTime::BlendAlpha`.
4. To show input before a tick consumes it: `Read<PendingInput>` and run the same pure
   function as the fixed system on the sealed component. Keep no state between frames.
5. No spawning, no queues, no writes to simulation. No `.After` against fixed systems
   (`OrderAcrossUpdateRates`).

### Order two writers of the same component

`BuildSchedule` reports `WriteConflict` with the fix. Add
`.After = Fling::SystemOf<OtherSystem>` to one system. Declare `OtherSystem` in its
module's order header (`*SystemsFwd.h`) if it is not there; never include its header.
Across modules, order against a group, or let the application call
`SystemRegistry::RunBefore(first, second)`. Nested groups do not order a child against
its parent's members.

### Add a gameplay effect that must affect a presentation-side value

Follow the stasis pattern:

1. Put the predicted value and its policy in one Simulation `Restore` component that the
   frame side reads. Register every field.
2. Write one pure function from (state, `InputSlice`) to state. Call it from the fixed
   system on `InputState::Tick.Slice` and from the frame system on `PendingInput`.
3. Apply input-driven policy changes inside that function at their place in the stream.
4. Make each gameplay-driven change a separate fixed system that writes the policy field,
   ordered `.After` the system that folds the input.
5. Test frame by frame: the view never steps back, and each tick makes exactly what the
   frame before it showed. Compare floats bit for bit (`SameBits` in `LookRig.h` tests).
6. Accept the threaded late-publish limit of section 7 for gameplay-driven changes.

## 12. Rules and traps for agents

Invariants you must not break:

- [ ] Systems have no member state and do not call other systems.
- [ ] Fixed systems read no wall clock, no `Timing`, no global `Random`, no `Input`
      singleton, no frame data. Release builds do not catch this; debug builds assert.
- [ ] Order never comes from addresses, hash tables, threads or registration order.
- [ ] Every `Restore` type has every member registered as a field.
- [ ] Systems never write runner-written singletons.
- [ ] A frame system never writes simulation state; a fixed system never reads
      presentation state.
- [ ] No thread waits for the other in live play. The frame side never waits for a
      tick and never extrapolates. Do not add a mutex shared by both threads.
- [ ] No per-tick heap allocation in the storage, runner or queries
      (`FlingTests/Engine/EcsAllocationTests.cpp`).
- [ ] Tick numbers only go up and are never skipped. Clock changes go through
      `ClockCommand`, never shared state.
- [ ] Side effects in resimulated groups go through queues that resim systems consume.
- [ ] Use `SimTime::ElapsedNs`, not `Tick * FixedDeltaNs`.
- [ ] Spawn entities whole; `Add`/`Remove`/`Destroy` apply at the end of the tick.
- [ ] Never edit `external/` (vendored third-party code).
- [ ] Keep the `.clang-format` layout, the comment style
      (`scripts/check_comment_style.py`) and string types (`scripts/check_string_types.py`).

Scope limits for this prototype. Do not add these:

- Networking, remote input, network ids, time sync.
- A player or device mapping framework (`InputState` is one world input).
- Physics rollback.
- A snapshot or resimulation runner (the hooks exist: `RollbackClass`,
  `IsRollbackState`, `Schedule::ResimRunOrder`, `ResimInputs`, `TickHistory`).
- Extrapolation or teleport flags in the blend.

Traps:

- Out-of-order designated initializers in `SystemInfo`/`GroupInfo` fail on MSVC.
- `.Resim = true` on a group inside Simulation is `ResimPlacement`; a game group
  directly in Tick between resim groups without `.Resim` is `ResimGap`.
- A game system in `Finalize` that writes game state is `ResimSkipsState`.
- A stated order against your own group or parent is a `Cycle`.
- A stated order can make a read one tick late; read `one tick late` in the dump.
- Two appenders to one queue push in stage-then-name order; no rule checks this.
- Renaming `.Name` changes order inside a stage and the dump, never the graph.
- Hand-built `SystemDesc`s in tests skip `CheckSystem`.
- A non-input policy writer ordered before the input fold makes the frame side snap.
- A frame system may read any Simulation component, including `InputState`; "read only
  the sealed component" is a convention, not checked.

Known limits and open items:

- `--dump-ticks` `cut` is a raw sequence and can differ across a record and replay.
- Edit-mode ticks share play tick numbers and the input cursor: one `SystemRunner` and
  one tick counter serve Edit and Play, so the first play tick number depends on how
  long the editor stayed in Edit.
- In a replay, the late poll (`InputPlayback::UpdateLate`) sends the messages that the
  next frame would send if its delta equals the last one. Headless every delta is the
  same. With real frame times this is a guess, so a by-time replay in a windowed run can
  give a message to an earlier tick (one frame early, never late).
- `--frames N` is a minimum frame count, not an exact one (section 9).
- Mouse samples are poll-granular (two polls per frame), so real-mouse samples depend on
  frame rate.
- Full copy every published tick; mirror apply is O(all mirror entities).
- Parent links are not kept per blend state; `ReadBlend<T>` is per entity only.
- Untagged threads pass the lane checks.
- `F_LOG_TRACE` in `Sandbox.FirstHit` and `Sandbox.TestSpawn` would repeat in a
  resimulation.
- ThreadSanitizer was run on an earlier revision, on Linux with clang, and not rerun on
  the final code. MSVC has no ThreadSanitizer.

## 13. Mapping to a large engine with a dynamic per-object tick model

Fling has stateless systems, one instance per class, a schedule known at startup and
complete declared access. A large engine with a dynamic per-object tick model has objects
with member state that register tick functions at run time, run-time order links,
several separate orderers, and undeclared access. What carries over:

- Declared access and order. Order references name kinds of work (a tick function
  class or a named group), checked at compile time where possible. Per-object order
  (parent before child) stays a local rule inside a node, or the graph grows with the
  object count. Data edges and conflict checks hold only for declared work; undeclared
  legacy work needs stated order, or a debug pass that records reads.
- One graph with groups. Each existing orderer (tick groups, the physics step, an
  animation pass, a task graph) gets a group with edges at its boundaries. Resimulation
  membership is a group property with a contiguity check.
- Fixed tick with stamps. One tick key on everything that names a tick: input, packets,
  events, histories. Simulation time derived from the tick number. Clock and rate
  changes as commands stamped with the tick they go before. A state hash over the
  restore set only.
- Two roots, root decides the thread. A domain tag per component class or property
  (simulation or presentation), checked by asserts. Objects that do both must split their
  presentation state into a separate proxy.
- Publish and blend. A published set derived from declared frame reads, or explicit
  registration per class. Full-state packets in a latest-wins mailbox. The renderer
  blends the latest two published states on its own clock, never waits, never
  extrapolates, and adds no latency.
- Two-sided input. A sequenced, timestamped input stream with a cut per tick carried in
  the tick stamp. The frame side runs the same pure function as the tick on the
  published state plus the input after the published cut, with no state of its own.
  Gameplay-driven policy reaches the frame side as published data, written after the
  input fold.
- Tick-stamped side effects. Every side effect leaves the simulation through a queue
  stamped with the tick, with one consumer that drops ticks that are not newer than the
  last one it applied, so a resimulation does not repeat them.

## 14. Further reading in this repository

`Docs/architecture-overview.md` (the model on a few pages), `Docs/system-order-api.md`
(every order rule and error), `Docs/time-and-ticks.md`, `Docs/threading-model.md`,
`Docs/input-two-sided.md`, `Docs/tutorial-systems-and-camera.md`, `Docs/ecs-tutorial.md`,
`Docs/update-order-design-notes.md`, `Skills/ecs.md` (storage, memory, hooks),
`Skills/input-playback.md` (the `.input.jsonl` format), `Skills/building.md`,
`Skills/testing.md`, `AGENTS.md`.
