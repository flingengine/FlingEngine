# Handoff to the Fling authors: fixed tick, update order, two threads and input

This page is for the people who wrote Fling. You know the engine on branch
`feature/ecs-types` well. You have not seen this work. The page tells you what
the work is, what it changed in your tree, how it is built, how to move game
code to it, and how you could take it in stages. It also says, as exactly as we
can, what was tested and what was not.

The first commit of this repository, `e1ea2f0`, is your `feature/ecs-types`
tree as it was before the work. Everything after it is the prototype. Every
number on this page comes from `git diff e1ea2f0 HEAD` (HEAD is `c128f64`), from
the code, or from the test runs listed in section 6. This page, `agent-guide.md`
and a set of small fixes to the other docs were made after `c128f64`; the diff
counts do not include them.

---

## 1. Summary

### What it is

Your ECS already had stateless systems, `System<T, Access<...>>`, a fixed and
a variable update rate, flat phases ordered by name, and input recording. The
prototype keeps that model and builds on it:

- **A deterministic fixed tick.** The simulation runs at a fixed rate (30 Hz by
  default, 1 to 1000 Hz) on a timeline in integer nanoseconds. Each tick has
  one identity, a `TickStamp`: tick number, length, sim time, window, input cut
  and state hash.
- **Compile-time order.** A system names its group with `.In` and orders itself
  with typed references (`Fling::Order::Simulation`,
  `Fling::SystemOf<OtherSystem>`) instead of strings. Groups nest under two
  roots: `Order::Tick` (fixed) and `Order::Frame` (variable). One graph per
  root replaces the phase graph and the system order graph.
- **Two threads.** In threaded play the fixed tick runs on a game thread. The
  main thread keeps the window, input polling, ImGui, the editor, the frame
  systems and Vulkan. The game thread publishes every tick. The frame side
  draws between the latest two published ticks on its own clock. It never waits
  for the game thread and adds no delay beyond that one tick.
- **Two-sided input.** One input stream with two readers. A tick takes a "cut"
  of the stream. The frame side can show input after that cut before a tick
  consumes it, with no loss and no double count. Record and replay store the
  cut of each tick, so a replay is exact.
- **Preparation for rollback.** Rollback classes on components, a state hash
  over exactly the set a rollback would restore, resimulation groups in the
  schedule, and tick-stamped outputs. No rollback runs today.
- **Three execution modes** (`--exec direct|inline|threaded`) with the same
  per-tick ledger in each. Run settings from the command line and
  `Config/EngineConf.ini`. VSync, a frame cap and a "GPU Options" window to
  change them while the game runs.
- **A large test suite:** 653 ctest entries at HEAD (MSVC Debug), many of them
  determinism tests that compare per-tick dumps byte for byte.

### The problem it solves for Fling

On `e1ea2f0`, systems ordered themselves with string names (`.After =
"Sandbox.Rotator"`), and phases were a second, flat name space. A typo or a
rename was found only when the schedule was built. The fixed step ran inside
the frame from a nanosecond accumulator at 60 Hz, and every tick of a frame saw
the input polled before that frame, so per-tick input depended on the frame
rate. There was one thread. The prototype makes order a compile-time property,
makes a tick's result independent of frame rate and threads, and moves the
simulation to its own thread with nothing shared that both sides can write.

### What it deliberately does not do

- No networking: no remote input, no player or device mapping, no network ids.
- No rollback or resimulation runner, no state snapshots, no player or game
  framework, no parallel executor for systems.
- No physics rollback. `Physics::SaveState` and `RestoreState` exist (they were
  in your tree) but are not wired to anything.
- The renderer internals are left as they were. The changes in `Graphics/` are
  touch points only: present mode and VSync, swapchain recreation, the render
  snapshot system, the window refresh rate.

---

## 2. What changed, by area

### Diff size

`git diff --shortstat e1ea2f0 HEAD`: **297 files changed, 34,006 insertions,
5,258 deletions.** 127 files added, 167 modified, 3 deleted. 106 commits after
`e1ea2f0` (98 without merges). Nothing under `external/` changed, and no new
third-party library was added.

| Path | Files | Insertions | Deletions |
| --- | --- | --- | --- |
| `FlingEngine/Engine` | 72 | 8,521 | 1,805 |
| `FlingEngine/Core` | 68 | 4,088 | 454 |
| `FlingEngine/Graphics` | 16 | 304 | 75 |
| `FlingEngine/Physics`, `PhysicsJolt`, `PhysicsBullet` | 11 | 182 | 110 |
| `FlingEngine/Editor` | 4 | 154 | 38 |
| `FlingEngine/Application` | 2 | 485 | 119 |
| `Sandbox` | 42 | 1,479 | 123 |
| `FlingTests` | 55 | 13,280 | 960 |
| `Docs` | 8 | 4,070 | 485 |
| `Skills` | 8 | 1,178 | 910 |
| `Plans` | 3 | 39 | 21 |
| Root CMake, `CMake/`, `.gitignore`, `scripts/` | 4 | 22 | 9 |
| `FlingEngine/Resources` (lane checks in `ResourceManager`), `Config/EngineConf.ini` | 3 | 32 | 0 |
| `AGENTS.md` | 1 | 172 | 149 |

The 3 deleted files: `Engine/inc/Ecs/Phase.h`, `Engine/inc/Systems/InputSystem.h`
and `Engine/src/Systems/InputSystem.cpp`. Of the 316 files under `FlingEngine/`
in `e1ea2f0`, most are untouched, including almost all of the Vulkan renderer
(70 of the 84 files under `Graphics/`), the resource system apart from a few
lines, and both physics providers apart from a few lines.

### Changes by area

| Area | New files or folders | Your files modified | Removed | What a current Fling user notices |
| --- | --- | --- | --- | --- |
| ECS and scheduling | `Ecs/Order.h`, `Ecs/EngineOrder.h`, `Ecs/RunnerWritten.h`, `Ecs/StateHash.h`, `Ecs/TickLedger.h`, `Ecs/TickStamp.h`, `src/Ecs/Schedule{Access,Dump,Graph,Groups,Orders}.cpp`, `ScheduleBuilder.h` | `System.h`, `SystemBase.h`, `SystemContext.h`, `Access.h`, `Component.h`, `Schedule.h/.cpp`, `SystemRunner.h/.cpp`, `SystemRegistry`, `EntityStorage.h`, `Reflection`, `TypeRegistry.h`, `World.h/.cpp` | `Phase.h` (`PhaseInfo`, `Phases::*`, `NameList`); `SystemInfo::Phase`, `MainThreadOnly`, `ReplayOnResim`; string `After`/`Before`; `SystemRegistry::Order` (now `RunBefore`) | `.Phase = Phases::X` becomes `.In = Order::X`. Order names are types. `Phases::Publish` is now `Order::Render`. The schedule has 20 error kinds (14 before; the phase and name kinds were replaced). A Transient Simulation component must declare `.Rollback`. |
| Time | `Core/inc/TickTimeline.h`, `TickHistory.h`, `SimRate.h`, `SimRandom.h`, `PlatformTime.h` | `Ecs/SimTime.h`, `Timing.cpp`, `Random`, `Misc/SimulationCheck` | `SimTime::FixedDelta` (float) and `Elapsed`; `RealFrameTime::TickAlpha`; `SystemRunnerConfig` 60 Hz accumulator | Default tick is 30 Hz, not 60 Hz. `SimTime` is in nanoseconds (`FixedDeltaNs`, `ElapsedNs`, `DeltaSeconds()`). Wall clocks and global `Random` assert inside a fixed system in debug. |
| Input | `Input/InputCut.h`, `InputSlice`, `PendingInputReader`, `TickInputProvider`, `Components/PendingInput.h` | `Input.h/.cpp`, `InputMessageQueue`, `InputMessageHandler`, `InputMessageProcessor`, `InputRecorder`, `InputRecording`, `InputPlayback`, `TickInput`, `Components/InputState.h` | `Input::IsHeld`, `WasPressed`, `WasReleased`, `GetKeyState`, `GetAxisValue`, `GetMouseDelta`, `GetPointerPosition`, `GetGameProcessor`, `BeginTick`, `EndTick`; the `Engine.Input` system (`InputSystem`) | Game code reads input only as `Read<InputState>` (fixed) or `Read<PendingInput>` (frame). The runner writes both; no system may. Recordings store per-tick cuts and their tick rate (format version 2). |
| Threading and publish | `Core/inc/Threading/TripleBuffer.h`, `Lane.h`, `FrameLimiter.h`, `SleepTimer.h`, `ThreadJitter.h`; `Engine/inc/GameLane.h`, `TickDriver.h`, `PresentationLink.h`, `TickEvents.h`, `RunControls.h`, `Interpolate.h`, `Ecs/Publish/*` (published set, packet, writer, mirror apply, channel, held values, frame blend) | `Thread`, `ThreadManager`, `ThreadQueue`, `ThreadTypes`, `TickLimiter` | None of your thread primitives | Play is threaded by default. Frame systems read a mirror of the published state. Tick output (`AppRequests`) and clock commands are queues stamped with the tick. |
| Renderer touch points | `Graphics/inc/PresentMode.h`, `src/PresentMode.cpp` | `VulkanApp.h/.cpp`, `SwapChain.h/.cpp`, `DesktopWindow`, `FlingWindow.h`, `IDebugUI.h`, `RenderSnapshot.h`, `Systems/PublishSnapshotSystem`, `ImGuiInputBinding.hpp`, `PointLight.hpp`, `RegisterGraphics.cpp` | `RecreateFrameResourcesForResize` (now `RecreateSwapchain`); raw `VkPresentModeKHR` getter | VSync on (FIFO) by default; off picks IMMEDIATE, else MAILBOX. The render snapshot blends `Transform` between two ticks. The window reports its monitor's refresh rate. |
| Physics touch points | `Physics/inc/PhysicsSystemsFwd.h` | `Physics.h`, `PhysicsProvider.h`, `PhysicsTypes.h`, `PhysicsStepSystem`, `PhysicsSystems.cpp`, `RegisterPhysics.cpp`, both providers | Physics' own clock: `PhysicsSettings::m_FixedStep`, `m_MaxStepsPerTick`, the saved accumulator | One physics step per game tick, of the tick's delta, split into `ceil(dt * 60)` equal collision steps (2 at 30 Hz). Physics components have rollback classes. |
| Editor and ImGui | GPU Options window (in `BaseEditor`) | `BaseEditor.h/.cpp`, `WorldOutlinePanel`, `Sandbox/Editor/SandboxEditor` | `DrawGpuInfo` (now `DrawGpuOptions`) | The editor is read only in play. GPU Options changes VSync, frame cap, sim rate and interpolation live. `IDebugUI::Init` takes `RunControls&`. |
| Settings and command line | `Core/inc/RunSettings.h`, `ExecMode.h` | `LaunchOptions`, `CommandLine`, `Config/EngineConf.ini`, `Application/Engine.h/.cpp` | `Game::Update` (legacy per-frame update), `Game::WantsToQuit`. `World::RequestQuit` and `World::SetTickInputSource` stay; the input source now takes `(TickInput&, const TickWindow&)` | New flags (section 8). One `Run settings: ...` startup line names each value and its source. `[Engine]` keys `Exec`, `SimHz`, `MaxFps`, `VSync`, `Interpolation`. |
| Sandbox | Look camera (`LookCamera*.h`, `Systems/LookCamera*`), stasis (`Stasis*.h`, `Systems/StasisSystem`), `CameraStatus`, `MotionProbe`, `SandboxSystemsFwd.h`, `Tests/Compare*.cmake`, 4 new `.input.jsonl` files | Every Sandbox system header (order API), `RegisterSandbox.cpp` (reflected fields), `SandboxGame.cpp`, 2 recordings | None | A presentation-side look camera, a stasis effect that stops it, a camera status line. |
| Tests | 30 new files in `FlingTests` (for example `ThreadedWorldTests`, `EcsPublishTests`, `EcsBlendTests`, `InputBoundaryTests`, `TickTimelineTests`, `RunSettingsTests`, `PhysicsOrderTests`), `SandboxExampleTests` target, a checked-in schedule dump | 25 of your files in `FlingTests` | None | `TEST_CASE` count in `FlingTests` went from 339 to 527; ECS compile-fail cases from 19 to 68; Sandbox ctests from 10 `add_test` calls to 21 (many in loops). |
| Docs | 7 new pages in `Docs/` (2 more after `c128f64`: this page and `agent-guide.md`) | `Docs/ecs-tutorial.md`, 8 of the 10 files in `Skills/` (not `building.md` or `adding-modules.md`), `AGENTS.md`, 3 `Plans/` files | None | Section 3 links to them. |
| Build | None | `CMakeLists.txt`, `CMake/FlingTargets.cmake`, `Physics/CMakeLists.txt`, `Sandbox/CMakeLists.txt`, `FlingTests/CMakeLists.txt` | None | clang-cl support: no `/MP` for clang-cl, `-Wthread-safety` for clang-cl too, `/clang:-ffp-contract=off` under clang-cl. |

`README.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md` and
`.github/workflows/build.yml` are unchanged. `Skills/building.md` is unchanged
up to `c128f64`; after it, the doc fixes added a short clang-cl section and the
two new test executables to it.

---

## 3. Design in brief

This section is enough to review the design. The detail is in `Docs/`; start
with [`architecture-overview.md`](architecture-overview.md).

### The two roots: Tick and Frame

`FlingEngine/Engine/inc/Ecs/EngineOrder.h` declares every engine group:

```cpp
inline constexpr GroupInfo Tick{.Name = TickRootName};
inline constexpr GroupInfo Frame{.Name = FrameRootName};
inline constexpr GroupInfo Input{.Name = "Engine.Input", .In = Tick};
inline constexpr GroupInfo Simulation{.Name = "Engine.Simulation", .In = Tick, .After = Input, .Resim = true};
inline constexpr GroupInfo Physics{.Name = "Engine.Physics", .In = Tick, .After = Simulation, .Resim = true};
// ResolveDeferred (Resim), Finalize under Tick; Presentation, Render, Editor under Frame
```

These are your phases with two roots on top. The root gives the update rate,
and in threaded play the rate gives the thread. No system has a thread
annotation, and no system changes between execution modes. A game adds its own
`inline constexpr GroupInfo` and nests it in an engine group with `.In`.
`Resim = true` marks what a later resimulation would run.

### `System<T, Access<...>>` and `SystemInfo`

The adapter is yours. What changed is `SystemInfo`
(`FlingEngine/Engine/inc/Ecs/System.h`): it now has `Name`, `In` (a
`GroupRef`), `RunIn`, `After` and `Before` (each an `OrderList` of up to eight
groups or `SystemOf<T>` references). `SystemOf<T>` works with a forward
declaration only, so no system header includes another system's header. The
Sandbox look camera (`Sandbox/Gameplay/inc/Systems/LookCameraSystem.h`):

```cpp
class LookCameraSystem final
    : public Fling::System<LookCameraSystem,
                           Fling::Access<Fling::Read<Fling::InputState>, Fling::Write<LookCamera>>>
{
public:
    static constexpr Fling::SystemInfo Info{.Name = "Sandbox.LookCamera", .In = Fling::Order::Simulation};
    void FixedUpdate(Context& ctx) const;
};
```

Access items are your `Read`, `Write`, `Append`, `Consume`, `Spawns` and
`RawStorage`, plus one new item, `ReadBlend<T>`, for frame systems that draw
between two ticks.

### Compile-time checks and startup schedule errors

At compile time (`Detail::CheckSystem` in `Ecs/SystemBase.h` and the context in
`Ecs/SystemContext.h`): a system has no member state; its group is under `Tick`
or `Frame`; a fixed system cannot touch a `Presentation` component or
`RealFrameTime`; a frame system cannot write, append to or consume a
`Simulation` component, spawn, or use `RawStorage`; any query, `Get`,
`Singleton` or `Queue` of a type not in the access list does not compile; a
wrong group or system name does not compile. `FlingTests/EcsCompileFail/`
proves each rule with a case that must fail to build.

At schedule build (`BuildSchedule`, `Ecs/Schedule.h`), each root is one graph:
structure and stated order first, then data edges (writer before reader,
appender before consumer) only for pairs that nothing else orders. Systems run
by segment (the `FlushAfter` group ends before them), then by stage (longest
chain before them), then by name, never by registration order. Errors are reported at startup and by `--validate-schedule`; the 20
kinds include `WriteConflict` (two unordered writers), `Cycle`,
`OrderTargetNotAdded`, `AmbiguousSystemType`, `ResimGap`, `ResimSkipsState` and
`ResimAppendsOutput`. `--dump-schedule` prints the result, and
`FlingTests/Physics/Schedules/EnginePhysics.schedule.txt` is a checked-in dump.
One behaviour change from your builder: order through a third system now counts
(`A before B before C` with C writing what A reads gives A, B, C and a one-tick
late read, not a cycle). Detail: [`system-order-api.md`](system-order-api.md).

### `TickTimeline` and `TickStamp`, in integer nanoseconds

`TickTimeline` (`Core/inc/TickTimeline.h`) computes each tick's window as
`base + (tick - baseTick) * stepNs`, never a running float sum. The step is
`SimRate::StepNs(hz)`, rounded to whole nanoseconds (33,333,333 at 30 Hz).
After a hitch, at most `MaxCatchUp` (250 ms) of ticks run in one wake, and the
rest of the time is dropped; no tick number is skipped. Each tick gets one
`TickStamp` (`Engine/inc/Ecs/TickStamp.h`):

```cpp
struct TickStamp
{
    uint64 Tick = 0;
    int64 FixedDeltaNs = 0;
    int64 ElapsedNs = 0;
    TickWindow Window;
    InputCut Cut;
    uint64 StateHash = 0;
};
```

The same stamp goes into the tick ledger, into the published packet and into
the frame (`RealFrameTime::Latest`). `SimTime` is written from it and holds
only what a fixed system may depend on: tick, length, sim time. Detail:
[`time-and-ticks.md`](time-and-ticks.md).

### Publish every tick, blend on the frame side

The schedule computes the **published set**: every Simulation type that some
frame system reads. After each tick the game thread copies that set into a
`TickPacket` in a latest-wins `TripleBuffer`. Each packet carries tick N and,
for the types a frame system blends (`ReadBlend<T>`), tick N - 1 too. The main
thread applies the newest packet to a mirror storage and runs the frame
systems on the mirror. `FrameBlend` (`Engine/inc/Ecs/Publish/FrameBlend.h`)
owns the one blend rule:

```text
Drawn      = clamp(DrawnLastFrame + FrameDeltaNs / TickLengthNs, N - 1, N)
BlendAlpha = Drawn - (N - 1)
```

So the frame never waits for the game thread and adds no margin: the drawn
state is at most one tick behind the latest published tick. A late tick holds
the frames at tick N (no extrapolation); several new ticks at once skip
forward, never blend faster. The render snapshot system declares
`ReadBlend<Transform>` and blends every transform. Detail:
[`threading-model.md`](threading-model.md), "The two blend states".

Four channels carry what the two sides exchange: the packet (game to main),
input messages (main to game, one writer, seqlock slots, 4,096 messages), the
tick outbox of `AppRequests` stamped with the tick (game to main), and clock
commands such as Pause, Resume and SetRate stamped with the tick they go before
(main to game). Besides those, ledger rows go to tools through a
`ThreadQueue<TickRecord>` (game to main), and a few atomics (tick count, sim
rate, last-tick flag, input watermark) are read across. None of these carries
simulation state.

### The input ring, the cut, `InputState` and `PendingInput`

The main thread is the only input writer in every mode. A tick takes a cut of
the stream, an `InputCut {AfterSequence, LastSequence}`: live, the messages
before the end of its window; in a replay, the recorded cut. The runner folds
the cut into the `InputState` singleton before the fixed systems run. On the
frame side the runner fills `PendingInput` (a Presentation singleton) with the
messages after the cut of the tick the frame shows. Both come from the same
packet, so a frame never pairs a new state with an old cut. Both sides fold
mouse input per sample with the same pure function, so the result does not
depend on how input is split into ticks. Detail:
[`input-two-sided.md`](input-two-sided.md).

### Record and replay

`InputRecording` stores the messages and, per tick, the cut as a count of file
messages, plus the tick rate (format version 2). A replay at another rate is
refused with exit code 3 and a message that names the right `--sim-hz`.
Headless runs and replays wait for the input writer, so they are exact. Live
windowed threaded play does not wait; it is reproducible from its own recording
only. Detail: [`../Skills/input-playback.md`](../Skills/input-playback.md).

### `StateHash`

`HashSimulationState` (`Engine/inc/Ecs/StateHash.h`) hashes every reflected
field of every rollback-state component, by value: a `vec3` as x, y, z, a bool
as 0 or 1, never padding bytes. It is bit-exact (`-0.0` and `+0.0` differ). The
hash goes into the stamp and `--dump-ticks`. It costs about 0.7 ms per tick in
a Debug Sandbox with 754 transforms (from the design notes; not re-measured).

### Rollback classes, as preparation

`Ecs/Component.h` adds `RollbackClass` to `ComponentInfo`:

```cpp
enum class RollbackClass : uint8 { Restore, Rebuild, Keep, Subsystem };
std::optional<RollbackClass> Rollback;   // in ComponentInfo; Restore when left out
```

A saved Simulation type defaults to `Restore`. A Transient Simulation type
that is not a queue must say which class it is, or it does not compile.
`IsRollbackState` (Simulation, `Restore`, not a queue) is the one rule for both
the hash and a later restore. A `Restore` type with bytes that no reflected
field covers is a registry error (`TypeRegistry::Build`). The check counts a
gap that alignment can explain as padding, so a missing small member, such as
a `bool` in what looks like tail padding, can pass it. `ResimSkipsState` stops a system outside the
resimulation from writing restore state that the resimulation reads. Nothing
saves or restores today.

### Three execution modes

| `--exec` | Who runs the ticks (`TickDriver`) | What frame systems read (`PresentationLink`) |
| --- | --- | --- |
| `threaded` (default in play) | `GameLane`, the game thread (OS name "Game Thread") | `MirrorLink`: the mirror of the newest packet |
| `direct` | `SameThreadDriver`, inside the frame | `DirectLink`: the simulation storage |
| `inline` | `SameThreadDriver`, inside the frame | `MirrorLink` |

Edit mode in the editor is always `SameThreadDriver` plus `DirectLink`.
`World::Update` and `World::RunWake` have no mode branch; the world picks the
two policy objects in one place.

---

## 4. Migration guide for game code

### Systems: phase and string order to group and typed order

From `Sandbox/Gameplay/inc/Systems/MoverSystem.h`. The new version includes
`SandboxSystemsFwd.h`, which includes `Ecs/EngineOrder.h` and only forward
declares `class LookCameraSystem;` and `class RotatorSystem;`:

```cpp
// before (e1ea2f0)
static constexpr Fling::SystemInfo Info{
    .Name = "Sandbox.Mover", .Phase = Fling::Phases::Simulation, .After = "Sandbox.Rotator"};
// after
static constexpr Fling::SystemInfo Info{
    .Name = "Sandbox.Mover", .In = Fling::Order::Simulation, .After = Fling::SystemOf<RotatorSystem>};
```

| Old | New |
| --- | --- |
| `.Phase = Fling::Phases::Input` (and the other fixed phases) | `.In = Fling::Order::Input`, `Simulation`, `Physics`, `ResolveDeferred`, `Finalize` |
| `.Phase = Fling::Phases::Presentation` | `.In = Fling::Order::Presentation` |
| `.Phase = Fling::Phases::Publish` | `.In = Fling::Order::Render` |
| `.Phase = Fling::Phases::Editor` | `.In = Fling::Order::Editor` |
| `.After = "Module.System"` | `.After = Fling::SystemOf<SystemClass>` (forward declaration is enough) |
| `.After = "Engine.Physics"` (a phase name) | `.After = Fling::Order::Physics` |
| A `PhaseInfo` of your own | An `inline constexpr Fling::GroupInfo` with `.In` set; designators in the order `Name, In, After, Before, FlushAfter, Resim` (MSVC C7560) |
| `.After = "Engine.Input"` to read input | Nothing: `Engine.Input` the system is gone, and `InputState` is written by the runner before any system |
| `.ReplayOnResim`, `.MainThreadOnly` | Removed. The group decides resimulation; the root decides the thread |

A system that appends to `AppRequests` (or any other output to the
application) must sit outside the resimulated groups, for example in
`Order::Input`, or the schedule reports `ResimAppendsOutput`. That is why
`Sandbox.AppKeys` moved from `Simulation` to `Input`.

### Time

From `Sandbox/Gameplay/src/Systems/MoverSystem.cpp`:

```cpp
// before
const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;
// after
const float dt = ctx.Singleton<const Fling::SimTime>().DeltaSeconds();
```

`SimTime::Elapsed` becomes `ElapsedSeconds()` (or `ElapsedNs`).
`RealFrameTime::TickAlpha` becomes `BlendAlpha`, used together with
`ReadBlend<T>` and `ctx.BlendFrom<T>(e)` / `ctx.BlendTo<T>(e)`. A fixed system
must not call `PlatformTime`, `Timing` or the global `Random`; use `SimRandom`.
If game code assumed 60 Hz, set `[Engine] SimHz=60` or `--sim-hz 60`.

### Components

Every Transient Simulation component that is not a queue needs `.Rollback`.
`SandboxToggles` (`Sandbox/Gameplay/inc/SandboxToggles.h`):

```cpp
// before
static constexpr Fling::ComponentInfo Component{
    .Name = "SandboxToggles", .Kind = Fling::ComponentKind::Singleton, .Transient = true};
// after
static constexpr Fling::ComponentInfo Component{.Name = "SandboxToggles", .Kind = Fling::ComponentKind::Singleton,
                                                .Transient = true, .Rollback = Fling::RollbackClass::Restore};
```

A `Restore` type must have reflected fields that cover all of its bytes, so the
registration gains fields (`Sandbox/Gameplay/src/RegisterSandbox.cpp`):

```cpp
// before
t_Modules.Component<SandboxToggles>();
// after
t_Modules.Component<SandboxToggles>(
    [](Fling::TypeBuilder<SandboxToggles>& t_Type)
    {
        t_Type.Field("rotate", &SandboxToggles::Rotate);
        t_Type.Field("moveLights", &SandboxToggles::MoveLights);
    });
```

Use `Rebuild` for data made again every tick (caches, world matrices), `Keep`
for settings and debug counters, `Subsystem` for handles into a library that
saves its own state. Note that adding a `Restore` type changes every state
hash, so pinned dump hashes in tests change too.

### Input

The global per-frame queries on the `Input` singleton are gone. Your engine's
`FirstPersonCamera` used them (`FlingEngine/Engine/src/FirstPersonCamera.cpp`
at `e1ea2f0`):

```cpp
bool SlowModifier = Input::Get().IsHeld(Keys::LeftControl);
if (Input::Get().IsHeld(Keys::W)) { /* ... */ }
```

There are three replacements, by where the code runs:

1. **A fixed system** declares `Read<Fling::InputState>` and asks it:
   `ctx.Singleton<const Fling::InputState>().WasPressed(Fling::Keys::G)`
   (`Sandbox/Gameplay/src/Systems/StasisSystem.cpp`). `IsHeld`, `WasPressed`
   and `GetAxisValue` are on `InputState`; presses are counts per tick.
2. **A frame system** that wants input the next tick has not consumed yet
   declares `Read<Fling::PendingInput>` and folds it with the same pure rule as
   the fixed side (see `LookCameraViewSystem` and
   [`tutorial-systems-and-camera.md`](tutorial-systems-and-camera.md)).
3. **Main-thread code outside the ECS** (a debug camera, a tool) makes its own
   reader with `Input::Get().CreateProcessor()` and consumes the queue itself.
   `FirstPersonCamera` now does this (`ReadInput`).

### Game class and quitting

`Game::Update(EntityStorage&, float)` (the legacy per-frame update) and
`Game::WantsToQuit` are removed. Put the work in systems. To quit, a fixed
system appends `AppRequest::Quit` to `AppRequests`; the world applies it from
the tick outbox.

During threaded play, the editor and frame code read
`World::GetPresentedStorage()` (the mirror), not the simulation storage. The
mirror holds only published types. The debug lane checks report a main-thread
access to the game thread's storage.

---

## 5. Suggested adoption plan

Each stage leaves the engine building and running. Later stages depend on the
earlier ones as shown. The test files named are the ones to take with each
stage.

| Stage | Content | Depends on | Tests that come with it | Can be left out |
| --- | --- | --- | --- | --- |
| 0. Build fixes | clang-cl support in `CMakeLists.txt` and `CMake/FlingTargets.cmake` | None | Your existing suite under clang-cl | Yes, if you do not use clang-cl |
| 1. Order API | `Order.h`, `EngineOrder.h`, the split schedule builder, typed `SystemInfo`, `SystemOf<T>`, removal of `Phase.h`, schedule errors and dump, order headers per module | None | `EcsScheduleTests`, `EcsSystemTests`, `PhysicsOrderTests`, `EnginePhysics.schedule.txt`, the order cases in `EcsCompileFail` | No: the rest builds on it |
| 2. Time | `TickTimeline`, `SimRate`, `TickStamp`, `TickLedger`, `TickHistory`, nanosecond `SimTime`, `SimRandom`, simulation checks, `--sim-hz`, `--dump-ticks`, one physics step per tick with collision steps | 1 (resim groups) | `TickTimelineTests`, `TickTimeTests`, `SimRandomTests`, the physics test level ctests | The rate change at run time (`SetRate`) |
| 3. Rollback preparation | `RollbackClass`, `StateHash`, the reflected-field coverage check, `ResimGap`, `ResimSkipsState`, `ResimAppendsOutput` | 1, 2 | `EcsReflectionTests`, `EcsTypesTests`, the tick dump level ctests | Yes, if you never want rollback; then also drop the `.Rollback` requirement |
| 4. Input | `InputCut`, `InputSlice`, `TickInputProvider`, runner-written `InputState` and `PendingInput`, removal of `InputSystem` and the global queries, recording format 2 | 2 | `InputBoundaryTests`, `TickInputTests`, `TickInputWaitTests`, `InputRecordingTests`, `InputHandlerTests`, the look camera replay ctests | `PendingInput` and the frame reader, if no frame system predicts input |
| 5. Publish, single thread | Published set, `TickPacket`, mirror apply, `PresentationLink`, `SameThreadDriver`, `--exec direct` and `inline`, tick outbox | 1, 2, 4 | `EcsPublishTests`, `FrameStorageTests`, `PlaySessionTests`, the exec mode ctests for direct and inline | `inline`, once threaded works; it exists to test the mirror without threads |
| 6. Threaded play | `GameLane`, `TripleBuffer`, lane checks, seqlock input queue, clock commands, `--thread-jitter` | 5 | `ThreadedWorldTests`, `TripleBufferTests`, `InputThreadTests`, `ThreadTests`, the jitter and frame-cap ctests | `--thread-jitter` (test aid) |
| 7. Interpolation | `ReadBlend<T>`, `FrameBlend`, `HeldValues`, blended `PublishSnapshotSystem`, `--interpolation` | 5 (works in every mode) | `EcsBlendTests`, `InterpolationTests`, the "rendered motion is even" ctests | Yes; the frame then draws the latest tick |
| 8. Run settings and display | `RunSettings`, `FrameLimiter`, `SleepTimer`, `PresentMode`, VSync, `RunControls`, GPU Options window | 2; `SetRate` needs 6's clock commands | `RunSettingsTests`, `RunSettingsConfigTests`, `FrameLimiterTests`, `PresentModeTests`, `RunControlsTests` | The GPU Options window |
| 9. Sandbox examples and docs | Look camera, stasis, `CameraStatus`, `MotionProbe`, `Docs/*` | 4, 5, 7 | `SandboxExampleTests` (`LookCameraTests`, `StasisTests`) | Yes; `MotionProbe` is test instrumentation |

Stage 1 alone is a mechanical change for game code (section 4); your 26
original schedule tests passed on it with renames only (design notes, section
5). The alternative to staging is to take this tree as it is: then read
section 7 first and run the full suite on your own machines and compilers.

---

## 6. Verification status

Stated exactly. Nothing here is stronger than what was run.

- **HEAD `c128f64`, MSVC Debug: 653 of 653 ctest entries pass.** Verified by
  the owner's orchestrator.
- **At commit `578e242`:** MSVC Debug 643 of 643, MSVC RelWithDebInfo 642 of
  642, and clang-cl 20.1.8 Debug 643 of 643 passed. 300-tick `--dump-ticks`
  output was byte-identical across those three builds on the default level
  (EmptyLevel), PhysicsTest and Level_1, and `578e242` pinned the SHA-256 of
  those dumps in `Sandbox/CMakeLists.txt` for the Jolt provider. The next
  commit, `9b67841`, added a registered `Restore` type (`Stasis`), which
  changes every state hash, and replaced the pins. The current pins pass as
  part of the 653 MSVC Debug passes at HEAD. For the current pins, the only
  evidence from other builds is the agent report in the next item.
- **After the last two commits** (`9b67841`, `c128f64`) an agent reported
  clang-cl 653 and Release 652 passing. That was not independently rerun.
- **Why Release has one test fewer:** the Catch2 case "Input writer calls on
  another lane than the writer lane are violations"
  (`FlingTests/Core/InputThreadTests.cpp`) is inside `#if FLING_LANE_CHECKS`.
  `Core/inc/Threading/Lane.h` sets `FLING_LANE_CHECKS` to 1 only when neither
  `NDEBUG` nor `FLING_SHIPPING` is defined, so a Release or RelWithDebInfo
  build does not compile that case, and `catch_discover_tests` lists one test
  fewer. Other build-dependent tests stay registered and pass with a
  `SUCCEED` or a different expected count.
- **Headless checksum:** `--play --null-render --frames 120 --seed 1` prints
  `Sandbox: 754 transforms, checksum 44871.195499957` in `--exec direct`,
  `inline` and `threaded`. (`--play` has no effect with `--null-render`: there
  is no editor UI, so play starts on the first tick anyway.)
- **ThreadSanitizer:** it was run on an earlier revision, on Linux with
  clang. It has not been rerun on the final code. MSVC has no
  ThreadSanitizer.
- **Not run on the final code:** any Linux or GCC build, the Bullet provider,
  the CI workflow in `.github/workflows/build.yml`.

---

## 7. Known limits, risks and open items

- **Only Windows was tested on the final code:** MSVC Debug at HEAD, and
  clang-cl and Release by an agent report that was not rerun. The Linux build
  was compiled only on an earlier revision, for the ThreadSanitizer run. The
  Bullet provider and other providers were not compiled; their source changes
  are small and untested.
- **Debug is 10 to 17 times slower than Release** in the owner's
  measurements, on one machine.

- **Window pacing in focus** (frame cap, VSync, the background cap) and the
  **GPU Options controls** were not checked by a human-driven run. The editor
  Play and Stop buttons and pause by hand were not either.

- **The existing renderer's `vkWaitForFences`** (`Graphics/src/VulkanApp.cpp`)
  took about 1.6 ms per frame in Release on the default level, at about 420
  fps, in the owner's measurements on one machine. No other machine was
  measured. The prototype did not change it.
- **Open optimisations:** change tracking for publish and mirror apply (today a
  full copy of the published set every tick, and an apply that is O(all mirror
  entities)); a `Transform` dirty check; physics gathers. Reference sizes from
  the design notes: 1,000 entities give a 64 KB packet, 2.2 µs fill, 12.4 µs
  apply; 10,000 give 640 KB, 14.3 µs and 126 µs.
- **The state hash** costs about 0.7 ms per tick in Debug at Sandbox size.

- **1/30 s is not a whole number of nanoseconds.** A tick at 30 Hz is
  33,333,333 ns, so 30 ticks are 999,999,990 ns, not one second. The tick count
  is the identity; sim time is derived from it and never summed.
- **Edit-mode ticks share the play tick numbers** and the input cursor: one
  `SystemRunner` counts the ticks of Edit and Play, so the first play tick
  number depends on how long the editor stayed in Edit.
- **`--frames N` is a minimum frame count.** No tick runs after tick
  `ceil(N / frames-per-tick)`, and the run exits at the first frame where at
  least N frames have run and that tick is shown. Headless `direct` and
  `inline` run exactly N frames; headless `threaded` usually runs more (467
  and 491 frames for `--frames 120` in two runs); a windowed run lasts until
  that tick is due. The flag's description in `Core/inc/LaunchOptions.h`
  ("Run N frames, then exit, in every exec mode") reads as an exact count;
  the code was left as it is.
- **The late poll of a replay guesses the next frame delta** (it assumes the
  last one), so a by-time replay in a windowed run can give a message to an
  earlier tick. Replays by cut and headless runs are not affected.

- **A late publish shows some input under the old policy.** In threaded play,
  input that arrives on the presentation side between a tick's cut and that
  tick's publish is shown under the policy of the previous tick. If the new
  tick starts a stasis, that input turns the view and is taken back when the
  tick arrives (pinned by the test "Stasis: with a late publish, ...").
- **The per-frame input check is debug only.** Release builds compile the
  simulation check out.
- **Live threaded play is reproducible only from its recording.** Live play
  does not wait for the input writer, by design, to avoid up to a frame of added
  latency.
- **The input queue holds 4,096 messages.** A headless main thread far ahead of
  a slow game thread can overrun it; the tick is marked `input-overran` and a
  replay then stops with an error.
- **Interpolation:** no extrapolation and no teleport flag. A respawn or a
  correction is blended over one tick. `DirectionalLight` and light colors are
  not blended. `ReadBlend<T>` is per entity, not for singletons.
- **One world input, one camera view.**

- **Skipped minor items:** live devices mixed into a windowed replay change the
  replayed ticks; keys held when a recording starts are written as a press at
  time 0; some timing-dependent stress tests were left out.
- Logs in fixed systems (`Sandbox.FirstHit`, `Sandbox.TestSpawn`) would repeat
  in a resimulation.
- Hand-built `SystemDesc`s skip `CheckSystem`; untagged threads pass the lane
  checks.

- **The final rounds of fixes have not had a fresh independent review.** Three
  structured reviews (order; time, input and publish; threads) and an audit
  against public rollback rules were done earlier, and their fixes merged. The
  last fix branches and the two final commits were reviewed only by the agents
  that made them and by the test suite.

### Known inaccuracies in the other docs

None known. The lags found while this page was written (the `Input` queries
in `architecture-overview.md`, the ThreadSanitizer sentence in
`update-order-design-notes.md`, the meaning of `--frames N`, the test
executable lists, the order header contents and the stasis determinism test)
were fixed in those docs after `c128f64`.

---

## 8. How to build and run

The steps below follow `README.md` and `Skills/building.md` (whose new
clang-cl section has the same settings).

Prerequisites: CMake 3.21 or newer; Ninja (used for all runs above) or Visual
Studio 2022; a C++20 compiler, which was Visual Studio 2022 (MSVC 14.44) or
clang-cl 20.1.8 with lld-link and the MSVC environment for headers and
libraries; the Vulkan SDK with `glslangValidator` (in `$VULKAN_SDK/Bin` or on
`PATH`). GCC 13 and Clang 16 are the documented Linux minimums; not tested on
this work.

### Submodules, Catch2, configure and build

`Init.bat` (or `Init.sh`) runs `scripts/init.cmake`: `git submodule update
--init --recursive` for everything under `external/`, a local Debug and Release
install of Catch2 to `external/Catch2/install`, and a default configure into
`build/`. To pick the generator yourself, skip that last step. From a Developer
Command Prompt, MSVC then clang-cl, Ninja, Debug:

```bat
Init.bat -DFLING_INIT_SKIP_CONFIGURE=ON
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug -DFLING_BUILD_TESTS=ON
cmake --build build
cmake -G Ninja -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug -DFLING_BUILD_TESTS=ON ^
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link
cmake --build build-clang
```

For Release use `-DCMAKE_BUILD_TYPE=Release` or `RelWithDebInfo`. The physics
provider is `-DFLING_PHYSICS_PROVIDER=Jolt` (default), `Bullet` or `None`; the
pinned dump hashes apply to Jolt only.

### Run the tests

```bat
ctest --test-dir build -j16 --output-on-failure
```

Useful subsets: `-R "Exec modes|tick dump|threaded headless|threaded replay|look camera replay|Input boundary"`
for the determinism tests (add `--repeat until-fail:10` to shake out timing),
and `build\FlingTests\bin\EngineTests.exe "[threads]"` for the thread tests.
Run tests and the Sandbox from the repository root.

### Run the Sandbox

With Ninja the program is `build\Sandbox\bin\Sandbox.exe`; with the Visual
Studio generator it is `build\Sandbox\bin\Debug\Sandbox.exe`.

| What | Command |
| --- | --- |
| Windowed demo, play starts at once | `Sandbox.exe --play` |
| Headless run with checksum | `Sandbox.exe --null-render --unattended --frames 120 --seed 1` |
| Another mode or rate | add `--exec direct`, `--exec inline`, `--sim-hz 60` |
| Dump the schedule | `Sandbox.exe --null-render --unattended --dump-schedule` |
| Dump the tick ledger | `Sandbox.exe --null-render --unattended --seed 1 --frames 120 --replay-input Sandbox/Tests/Input/look_camera_cuts.input.jsonl --dump-ticks ticks.txt` |

The flags this work added or uses (all in `Core/src/LaunchOptions.cpp`, ranges
in `Core/inc/RunSettings.h` and `Core/inc/SimRate.h`):

| Flag | Meaning |
| --- | --- |
| `--play` | With the editor UI, start play at once as if Play were pressed; also opens GPU Options (new; read in `Engine.cpp` and `SandboxEditor.cpp`) |
| `--exec direct\|inline\|threaded` | Execution mode; default `threaded`. Key `[Engine] Exec` |
| `--sim-hz N` | Tick rate, 1 to 1000; default 30. Key `SimHz` |
| `--max-fps N` | Frame cap on the main thread, 0 for none, at most 1000; default the monitor's refresh rate, else 60, none headless. Key `MaxFps` |
| `--vsync on\|off` | On: FIFO. Off: IMMEDIATE, else MAILBOX. Default on. Key `VSync` |
| `--interpolation on\|off` | Blend between the latest two ticks; default on. Key `Interpolation` |
| `--null-render` | No window, no GPU (existing) |
| `--frames N` | No tick after tick `ceil(N / frames-per-tick)`; exit once that tick is shown and at least N frames have run (existing flag, new meaning; see section 7) |
| `--frames-per-tick N` | Headless: one tick every N frames, so frame systems run between ticks (new) |
| `--seed S` | Seed (existing) |
| `--dump-ticks F` | Write the tick ledger, one line per tick (new) |
| `--record-input F`, `--replay-input F` | Record and replay input, now with per-tick cuts and the tick rate (existing flags); refused in shipping builds |
| `--thread-jitter S:U` | Threaded only: random sleeps up to U µs, seed S, to test timing (new) |
| `--dump-schedule`, `--validate-schedule`, `--level F`, `--unattended` | Existing |

`--render-hz` and `--present-mode` are refused with a message that names the
flag to use instead. The startup line `Run settings: ...` names every value and
where it came from. In `--play`, press M to show the cursor before you use the
GPU Options window.

---

## 9. Open design questions, and what would come next

### Questions that are your call

1. **Default tick rate:** 30 Hz with interpolation replaced 60 Hz. Is that the default you want?
2. **Names:** `Lane` and `GameLane` read like scheduling lanes; `ThreadRole` and `GameThread`
   were proposed, not done. `Order::Render` replaced the phase name `Publish`.
3. **Live input rule:** live threaded ticks do not wait for the input writer. Exact live
   reproducibility, or up to a frame less latency per tick?
4. **Publish cost:** full-state packets survive dropped packets; change tracking cuts the copy.
5. **Rollback classes as a hard rule:** `.Rollback` is required on Transient Simulation types,
   and `Restore` types must be fully reflected, before any rollback exists.
6. **Release enforcement:** the simulation and lane checks are debug only.
7. **Three execution modes:** `inline` exists mainly to test the mirror without threads.
8. **Physics:** one step per tick with `ceil(dt * 60)` collision steps, or a physics rate of its own?
9. **Edit mode** shares the play tick numbers. Should it have its own?
10. **Compile-time weight** of the templates and `consteval` checks at your project sizes.

### What would come next for rollback and resimulation

**Not implemented.** This list only names where each part would attach
(detail in [`time-and-ticks.md`](time-and-ticks.md), "Hooks for rollback
networking"):

- **State snapshots:** a `TickHistory<Snapshot, N>` of the `IsRollbackState`
  set, plus save and restore hooks for `Subsystem` types (register
  `Physics::SaveState` and `RestoreState`) and the entity allocator.
- **A resimulation runner** in `SystemRunner`, called from `World::RunWake`:
  restore after tick K - 1, then for each tick write `SimTime` and `InputState`
  from history, run `Schedule::ResimRunOrder()`, flush, hash and overwrite the
  ledger row. No publish during it.
- **Remote input:** a per-tick, per-player command history that the runner
  writes into `InputState`. `InputState` has no player dimension today.
- **Predicted against authoritative compare,** per type, on the tick thread.
- **Time sync:** retarget the `TickTimeline` to the server tick plus a lead.
- **Presentation correction:** an epoch in the packet header and a frame system
  that smooths a visible correction.
- **Network ids** for entities, and deduplication of side effects by tick.

---

## 10. Licensing and attribution

- `LICENSE` is the MIT License, with a 2019 copyright line that names Fling's
  original author. This work did not change it.
- `THIRD_PARTY_NOTICES.md` is unchanged. It lists SDL_GameControllerDB (zlib
  style license), the Garden skybox by Humus (CC BY 3.0), and the libraries
  under `external/` with their licenses: Bullet (zlib), Catch2 (Boost 1.0),
  GLFW (zlib), GLM (MIT or Happy Bunny), Dear ImGui (MIT), Jolt (MIT), inih
  (BSD 3-Clause), JSON for Modern C++ (MIT), spdlog (MIT), SPIRV-Cross
  (Apache 2.0), stb (MIT or public domain), tinyobjloader (MIT). The prototype
  adds no library and no data file from another project.
- Much of the prototype code, tests and docs were written with AI assistance,
  under human direction and review. Treat it as you would any contribution: read
  it and test it before you take it.
- This page states what the files say. It is not legal advice.
