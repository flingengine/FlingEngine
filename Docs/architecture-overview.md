# Architecture overview: order, time, input and threads

This page is the entry point. It explains, on a few pages, how Fling runs its
ECS systems: in which order, at which rate, on which thread, with which input,
and how two runs are proved to be the same. Each section links to the page with
the full detail (table at the end).

The model in one paragraph: every system declares, at compile time, its
**group**, its **order** against other work, and its **data access**. Groups
nest under two roots. `Order::Tick` is the fixed tick (the simulation, the game
thread); `Order::Frame` is the rendered frame (presentation, the main thread).
The schedule builder makes one graph per root from the declarations. The
simulation never reads what the frame side has, so the declarations alone
decide what must cross from the tick to the frame: the **published set**. Each
tick has one identity (`TickStamp`: number, length and sim time, window, input cut, state hash),
which goes with the published copy, into the tick ledger and into the frame.
Input is one stream with two readers that run one pure function.

## 1. What a system declares

A system is a stateless class with three constants. This is the complete
fixed-rate system of the Sandbox look camera,
`Sandbox/Gameplay/inc/Systems/LookCameraSystem.h` (comment trimmed):

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

| Declaration | Where | In the example |
| --- | --- | --- |
| Group | `SystemInfo::In` | `Fling::Order::Simulation`, a child of `Order::Tick` |
| Order | `SystemInfo::After`, `SystemInfo::Before` (a group or `Fling::SystemOf<T>`) | none: the data decides |
| Data access | `Access<...>`: `Read<T>`, `Write<T>`, `Append<Q>`, `Consume<Q>`, `ReadBlend<T>` (frame systems), `Spawns`, `RawStorage` | reads `InputState`, writes `LookCamera` |
| Update function | `FixedUpdate(Context&)` under Tick, `Update(Context&)` under Frame | `FixedUpdate` |

Its frame side partner, `Sandbox/Gameplay/inc/Systems/LookCameraViewSystem.h` (comment trimmed):

```cpp
class LookCameraViewSystem final
    : public Fling::System<
          LookCameraViewSystem,
          Fling::Access<Fling::Read<LookCamera>, Fling::Read<Fling::PendingInput>, Fling::Write<Fling::CameraView>>>
{
public:
    static constexpr Fling::SystemInfo Info{.Name = "Sandbox.LookCameraView", .In = Fling::Order::Presentation};

    void Update(Context& ctx) const;
};
```

The compiler checks the declarations (`Detail::CheckSystem` in
`Ecs/SystemBase.h`, and the context in `Ecs/SystemContext.h`):

- A system has no member state. Its state goes in a singleton component.
- The group must be under `Order::Tick` or `Order::Frame`. The root decides
  `FixedUpdate` or `Update`.
- A fixed rate system cannot read or write a `Presentation` component, and
  cannot read `RealFrameTime`.
- A variable rate system cannot write, append to or consume a `Simulation`
  component, spawn, or use `RawStorage`.
- A query, `Get`, `Singleton` or `Queue` of a type that is not in the access
  list does not compile.

The schedule builder (`BuildSchedule`, `Ecs/Schedule.h`) then orders the
systems of each root: structure (`In`) and stated orders first, then data edges
(a writer before a reader, an appender before the consumer) for pairs that
nothing else orders. Systems run by stage (the longest chain of systems before
them), and by name inside one stage. Two unordered writers of
one type are an error. Detail: [`system-order-api.md`](system-order-api.md).

## 2. Two roots, two threads

```text
Order::Tick  (Fixed, once per tick)          Order::Frame  (Variable, once per frame)
  (runner: SimTime, InputState)                (runner: RealFrameTime, PendingInput)
  Input            AppKeys, preprocessors      Presentation   LookCameraView
  Simulation (resim)  gameplay                 Render         Graphics.PublishSnapshot
  Physics    (resim)  Physics.Step             Editor         editor tools
  ResolveDeferred (resim)  queue consumers
  Finalize         Engine.Transform
        |                                              ^
        |  published set, in a TickPacket              |
        +-------------- with its TickStamp ------------+
```

In threaded play (`--exec threaded`, the default), everything under `Order::Tick` runs on the game
thread (OS name "Game Thread", class `GameLane`), and everything under `Order::Frame` runs on
the main thread. The game programmer chooses only the group. The group gives the
rate, the rate gives the thread, and the compile-time rules above make the two
sides share nothing that both can write. No system has a thread annotation, and
no system source changes between the execution modes.

The main thread keeps everything that is not the fixed tick: the window and
GLFW, input polling, ImGui and the editor, the frame systems and Vulkan. The
reason is that every window-bound call already lives there; the simulation
touches none of them. Detail: [`threading-model.md`](threading-model.md).

## 3. The life of one tick (threaded mode, game thread)

`GameLane::Run` calls `World::RunWake` once per wake. One wake:

1. Read the clock: `now = m_TickClock()` (`PlatformTime::NowNanoseconds` in the
   engine, a virtual clock headless and in tests).
2. `World::ApplyClockCommands(now)`: drain the `ClockCommand` queue (Pause,
   Resume, SetRate) from the main thread. A command stamped `BeforeTick N` waits for
   tick N; one whose tick has run is dropped.
3. `SystemRunner::RunFixedAt(now, role, TickLimit())` runs each due tick, at most
   `MaxTicksPerWake()` (`MaxCatchUp`, 250 ms, in ticks: 8 at 30 Hz), and no tick past `SetLastTick` or a Quit. Per tick
   (`RunOneTick`):
   1. `TickTimeline::Advance(TickTrigger::Clock)` gives the tick's window. The
      tick number goes up by one.
   2. The runner has the tick input source (`TickInputProvider::Build`) fill
      the `InputState` singleton: the recorded cut in a replay, otherwise the
      messages before the window end. It notes the cut to the recorder.
   3. The runner writes `SimTime` (`Tick`, `FixedDeltaNs`).
   4. The fixed groups run in schedule order: Input, Simulation, Physics,
      ResolveDeferred, Finalize. Each system has its own command buffer.
   5. `FlushCommands` applies the deferred adds, removes and destroys.
   6. The runner hashes the state (`HashSimulationState`) and pushes a
      `TickRecord` with its `TickStamp` into the `TickLedger`.
   7. The tick output (`World::SendTickOutput`) moves the tick's `AppRequests`
      into the outbox as one `TickOutboxEntry {Tick, Requests}`. A Quit ends the
      session at this tick.
4. After a hitch (`MaxTicksPerWake()` ticks ran and time is still due), `DropBacklog` drops the
   rest of the time. No tick number is skipped.
5. If ticks ran: `MirrorLink::Publish` fills the header (`Stamp =
   LatestTickStamp()`) and `PublishChannel::Publish` copies the published set
   into a `TickPacket` in the latest-wins mailbox (`TripleBuffer`).
6. `GameLane::WaitForNextWake`: while paused, sleep on an `Event`; in
   `RealTime` pacing, sleep until the next tick is due (at most 100 ms); in
   `AsFastAsPossible` pacing, yield after a wake that ran no tick.

## 4. The life of one frame (threaded mode, main thread)

`Engine::RunFrames` runs the main loop, one iteration per frame
(`FlingEngine/Application/src/Engine.cpp`):

1. `PumpInput`: `Input::Poll` (playback, device interfaces, recorder).
   Headless: move the virtual clock to the end of this
   frame. The frame never waits for the game thread, headless or windowed.
2. `World::Update`:
   1. `ProcessPlayRequest`: a pending Play or Stop runs here.
   2. `m_Driver->OnFrame()`: nothing for the game thread. (With a
      `SameThreadDriver`, the wake of section 3 runs here.)
   3. `ApplyTickOutbox`: apply each outbox entry once. An entry whose tick is
      not newer than the last applied one is dropped.
   4. `RunFrame`: `m_Link->ShowLatest()` applies the newest packet to the
      mirror (`MirrorApplier`) and returns its `TickStamp`; then the late poll
      (`Input::PollLate`); then `SystemRunner::RunFrameAt(now, delta, role,
      shown)` writes `RealFrameTime` (with `Latest = shown`) and runs the frame
      groups on the mirror: the runner fills `PendingInput` with the
      input after `shown.Cut`, the Presentation systems run, and
      `Graphics.PublishSnapshot` fills `RenderSnapshot`.
   The simulation publishes after every tick, and each packet carries tick N
   and, for the blended types, tick N - 1, so the frame side always has the
   latest two published ticks: `BlendTo` (tick N) and `BlendFrom` (tick N - 1).
   A frame system that draws between ticks declares `ReadBlend<T>` next to
   `Read<T>` and blends `ctx.BlendFrom<T>(entity)` toward `ctx.BlendTo<T>(entity)`
   by `RealFrameTime::BlendAlpha`. The render snapshot system does this for every
   `Transform` (`FlingEngine/Graphics/inc/Systems/PublishSnapshotSystem.h`):

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

   The schedule marks those types in the published set; the packet carries
   them for both ticks (in `direct`, `DirectLink` copies them after each tick).
   The blend factor is the renderer's own (`FrameBlend`): each frame moves the
   drawn point on by the frame's time over the tick length and keeps it between
   the two ticks. Many frames can draw from the same two ticks; with no new tick
   the point stops at `BlendTo` and the frames repeat. The drawn state is at most
   one tick behind the latest published tick, with no added margin. The look
   camera is not blended (the sealed view plus the input after the latest cut),
   and Edit does not blend. Detail:
   [`threading-model.md`](threading-model.md), "The two blend states", and
   [`time-and-ticks.md`](time-and-ticks.md), "The blend factor".
3. `Input::EndFrame`, then `DumpNewTicks` writes the new ledger rows
   (`World::TakeNewTickRecords`) for `--dump-ticks`.
4. `HandleAppRequests` (cursor toggle). Quit if the world asks.
5. `Present`: `VulkanApp::Update` on `World::GetPresentedStorage()` (the
   mirror). Headless: only the frame cap, if one is set.

## 5. What crosses between the threads

Only four things cross, and each has one direction:

| What | Direction | How | Type |
| --- | --- | --- | --- |
| The published state of the newest tick | game to main | latest-wins triple buffer; a full copy, not a delta | `TickPacket` (`Header.Stamp` is the `TickStamp`) |
| Input messages | main to game | one queue, one writer (the main thread), seqlock slots; the tick takes a cut | `InputMessageQueue`, `InputCut` |
| What a tick asks of the application | game to main | `ThreadQueue`, one entry per tick, stamped with the tick | `TickOutboxEntry` |
| Clock control | main to game | `ThreadQueue`, applied at the start of a wake | `ClockCommand` |

```text
 main thread                                   game thread
 ---------------------------------------      -------------------------------------
 Input::Poll -> queue (seq, time) ---------->  TickInputProvider: cut (After, Last]
 ShowLatest <- mailbox <---------------------  Publish(TickPacket{Stamp, state})
 ApplyTickOutbox <- outbox <-----------------  SendTickOutput(TickOutboxEntry{Tick})
 PauseTicks/ResumeTicks -> clock commands -->  ApplyClockCommands
```

The packet carries the tick's cut in its stamp. The frame side shows the input
after that cut, from the same packet as the state it shows. So the state, the
tick number and the cut always belong to one tick: a "torn pair" (new state with
an old cut, or the reverse) cannot happen. The schedule also proves that
nothing crosses from the frame into the simulation through components:
`Schedule::Published` lists every Simulation type that a frame system reads,
and the compile rules forbid a frame system to write one.

## 6. Three execution modes, one ledger

| `--exec` | Who runs the wakes (`TickDriver`) | What the frame reads (`PresentationLink`) |
| --- | --- | --- |
| `threaded` (default) | `GameLane`, the game thread | `MirrorLink`: the mirror of the newest packet |
| `direct` | `SameThreadDriver`, in the frame | `DirectLink`: the simulation storage |
| `inline` | `SameThreadDriver`, in the frame | `MirrorLink` |

Play, in a game build and in the editor, is threaded unless `--exec` or
`[Engine] Exec` says another mode (`RunSettings`, `Core/inc/RunSettings.h`).
Edit mode in the editor is always `SameThreadDriver` + `DirectLink`. A test or
a tool that needs one mode sets it (`World::SetExecMode`, `--exec`). The world
chooses the two policy objects in one place (`World::StartGame`,
`World::StartPlayDriver`). `World::Update` and `World::RunWake` have no mode
branch.

The fixed tick runs at 30 Hz unless `--sim-hz` or `[Engine] SimHz` says another
rate, and the main thread's frames are capped at the refresh rate of the
window's monitor (else 60; none headless) unless `--max-fps` or `[Engine]
MaxFps` says another cap. VSync is on (FIFO) unless `--vsync off` or
`[Engine] VSync=off` turns it off (IMMEDIATE, else MAILBOX: no wait, may tear);
render interpolation (`--interpolation`, `[Engine] Interpolation`) is on by
default.
The startup log line `Run settings: ...` names each value and where it came
from. While the game runs, the editor's GPU Options window changes the rate (a
tick-stamped `SetRate` clock command, applied between two ticks), the cap,
VSync and interpolation. No setting changes a replay or a headless run; in live
windowed play the frame rate decides when input is polled, and so which tick
a message goes to. Detail:
[`time-and-ticks.md`](time-and-ticks.md) section 2.

The simulation never reads the mirror, the frame side or the main thread's
timing. So the per-tick ledger (tick, cut, input counts, state hash) must be
the same in all three modes and at any frame rate. The tests check this, at the
default 30 Hz (the recorded cuts at 60 Hz too): `--dump-ticks` files of
`direct`, `inline`, `inline --frames-per-tick 3`, `threaded`, threaded at
`--max-fps` 30, 60, 144 and 1000, and threaded with 10 seeds of
`--thread-jitter`, are compared **byte for byte**
(`Sandbox/Tests/CompareExecModes.cmake`). In the harness
(`FlingTests/Engine/ThreadedWorldTests.cpp`) the main thread writes the input
through the engine's input path (`Input::Poll`, the engine's `TickInputProvider`
config from `World::MakeTickInputConfig`, `InputRecorder`, `InputPlayback`) and
a free game thread cuts it by the timeline, for 30000 ticks.

## 7. Determinism rules

1. **Inside one stage, systems run by name**, never by registration order,
   address or thread.
2. **A fixed system reads no wall clock.** It reads `SimTime` (`Tick`,
   `FixedDeltaNs`). `RealFrameTime` is a Presentation component. `TickInput`
   and its samples have no time field. `PlatformTime::NowNanoseconds`, `Timing`
   and the global `Random` assert in debug while a fixed system runs (not
   checked in release builds).
3. **Input reaches a tick only through its cut**: the time rule (messages
   before the window end) live, or the recorded cut in a replay. The runner
   writes the cut's input into `InputState` (and the frame's into
   `PendingInput`); a system reads them only by declaring `Read<...>`. No
   system can write them and the system context has no input, so a fixed
   system cannot obtain raw tick input (compile errors). `Input` has no
   per-frame key queries (`IsHeld`, `WasPressed` and `GetAxisValue` are on
   `InputState`). A fixed system that calls the `Input` entry points that are
   left (`GetHandler`, `CreateProcessor`, `GetDevice`, `GetDevices`,
   `IsKeyboardCaptured`, `IsMouseCaptured`) fails the simulation check in a
   debug build (and the lane check on the game thread). A release build
   (`NDEBUG`) compiles that check out: there it is not enforced (a known
   limit, section 9).
4. **The simulation never reads presentation state** (compile error), and
   nothing crosses from the frame into the simulation except the input stream.
5. **Systems are stateless.** All state is component data, so a copy of the
   components is a copy of the state.
6. **Storage order only.** A query returns entities in storage order; the flush
   sorts by entity, then by call order.
7. **Random numbers for the simulation come from `SimRandom`**, a pure function
   of seed, stream and draw number.
8. **The state hash is bit-exact** and covers exactly the state that a rollback
   restores (`IsRollbackState`): every reflected field of each `Restore` type,
   `Transient()` fields too. A `Restore` type with bytes that no reflected field
   covers is a registry error, and an unregistered `Restore` type that a system
   uses stops `World::Init`. `-0.0` and `+0.0` differ. It reads values, never
   padding (a `vec3` as x, y, z), so equal states hash equal.
9. **The same bits on every platform.** Windows and Linux must simulate the
   same ticks. Simulation code uses only exact float operations, with `SimMath`
   for trigonometry, exp, log and pow; Jolt is built with
   `CROSS_PLATFORM_DETERMINISTIC`; contraction and fast math are off for every
   target; GLM types are packed everywhere. ARM is not verified.

## 8. Where Fling is today

| Built and tested | Not built |
| --- | --- |
| Typed order API, groups, one graph, checked-in schedule dump | A parallel executor for stages |
| Tick timeline, `TickStamp`, ledger, state hash, `--dump-ticks` | State snapshots and a resimulation runner |
| Input by time and by recorded cut; two-sided input | Remote input, player mapping |
| Published set, packet, mirror, three execution modes, interpolation between the latest two published states (`ReadBlend<T>`) | Extrapolation past the latest tick, teleport flags |
| Outbox and clock commands, stamped with ticks | Time sync, client lead, time dilation |
| Rollback classes on components, `ResimSkipsState` | Network ids, predicted against authoritative compare |

The hooks for rollback are listed in
[`time-and-ticks.md`](time-and-ticks.md), "Hooks for rollback networking".

## 9. Known limits and open items

- **The per-frame input check is debug only.** A fixed system cannot obtain
  raw tick input: the context has none and no system can write `InputState`
  (compile time). `Input` has no per-frame key queries; `IsHeld`,
  `WasPressed` and `GetAxisValue` are on `InputState`. A fixed system that
  calls the `Input` entry points that are left (`GetHandler`,
  `CreateProcessor`, `GetDevice`, `GetDevices`, `IsKeyboardCaptured`,
  `IsMouseCaptured`) is caught by the simulation check in debug builds only. Release builds (`NDEBUG`) compile the
  check out, so there it is not enforced.
- **The late poll of a replay guesses the next frame delta** (it assumes the
  last one), so a by-time replay in a windowed run can give a message to an
  earlier tick.
- **Edit-mode ticks share the play tick numbers** and the input cursor. The
  first play tick number depends on how long the editor stayed in Edit.
- **Interpolation is at most one tick behind the latest published tick, with
  no added delay, no extrapolation and no teleport flag.** A tick that arrives
  late shows as held frames, and the tick after it as a skip of the part the
  hold did not draw (at most one tick of motion). A jump (respawn, re-parent, a
  correction) is blended over one tick.
  Values that are not blended (`Read<T>` gives the newest tick) and spawns and
  removals show up to one tick before the blended transforms.
- **Names.** `Lane` and `GameLane` are proposed to become `ThreadRole` and
  `GameThread`; not done.
- **Sanitizer coverage:** a ThreadSanitizer run on Linux (clang 18) of the
  four test suites and the headless Sandbox, made before the interpolation was
  merged, found no data race in engine code; it did not cover the
  interpolation, GraphicsTests, the windowed Sandbox or the editor (MSVC has no
  ThreadSanitizer).
- **Out of scope on purpose:** state snapshots and a resimulation runner,
  remote input, player and device mapping, physics rollback, and extrapolation
  past the latest packet.

## 10. How to run

Build with plain CMake from the repository root (setup, generators and options
in [`../Skills/building.md`](../Skills/building.md)). On Windows, use a
Developer Command Prompt so that the MSVC tools and the Vulkan SDK are found.
With Ninja and a Debug build:

| What | Command |
| --- | --- |
| First-time setup (submodules, local Catch2), no configure | `Init.bat -DFLING_INIT_SKIP_CONFIGURE=ON` (or `./Init.sh ...`) |
| Configure and build everything | `cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build` |
| All tests | `ctest --test-dir build -j16 --output-on-failure` |
| The determinism tests, 10 times | `ctest --test-dir build -R "Exec modes\|tick dump\|threaded headless\|threaded replay\|look camera replay\|Input boundary" --repeat until-fail:10` |
| The thread tests | `build\FlingTests\bin\EngineTests.exe "[threads]"` |

Start the Sandbox from the project root (this repository); the windowed demo
must be started from there. With Ninja the program is
`build\Sandbox\bin\Sandbox.exe` (with the Visual Studio generator,
`build\Sandbox\bin\Debug\Sandbox.exe`):

| What | Command (from the project root) |
| --- | --- |
| Headless run, checksum in the last lines | `build\Sandbox\bin\Sandbox.exe --null-render --unattended --frames 120 --seed 1` (threaded at 30 Hz; also `--exec direct`, `--exec inline`, `--sim-hz 60`) |
| Dump the schedule | `build\Sandbox\bin\Sandbox.exe --null-render --unattended --dump-schedule` |
| Dump ticks | `build\Sandbox\bin\Sandbox.exe --null-render --unattended --seed 1 --frames 120 --replay-input Sandbox/Tests/Input/look_camera_cuts.input.jsonl --dump-ticks ticks.txt` (a recording replays only at its rate: `look_camera_cuts_60hz.input.jsonl` needs `--sim-hz 60`) |
| Windowed demo (look camera; threaded, 30 Hz, frames capped at the monitor's refresh rate) | `build\Sandbox\bin\Sandbox.exe --play` |
| The same, unattended (the exit line has the average frame time) | `build\Sandbox\bin\Sandbox.exe --play --unattended --frames 300` (it runs until tick 300 is shown, so for about 10 s at 30 Hz) |
| Other settings | `--exec direct` or `inline`, `--sim-hz N` (1 to 1000), `--max-fps N` (0: no cap), `--vsync on\|off`, `--interpolation on\|off`, `--frustum-culling on\|off`, `--occlusion-culling on\|off`; or the same keys in `[Engine]` of `Config/EngineConf.ini` (`Exec`, `SimHz`, `MaxFps`, `VSync`, `Interpolation`, `FrustumCulling`, `OcclusionCulling`) |
| Change them while the game runs | The editor UI is drawn in `--play` too, and the Sandbox opens the "GPU Options" window at start in `--play` (menu Windows closes and opens it): VSync (On or Off, with the present mode in use), frame cap, simulation rate, render interpolation, frustum culling (with the meshes drawn and culled), with what the frame limiter does and the measured rates. Press M first if the cursor is hidden |

## 11. Where to read more

| Page | Read it for |
| --- | --- |
| [`system-order-api.md`](system-order-api.md) | Groups, `.In`, `.After`, `.Before`, `SystemOf<T>`, order headers, the run order rules, every error, the dump, schedule tests |
| [`time-and-ticks.md`](time-and-ticks.md) | The clock, tick windows, hitch and pause, `TickStamp`, the input rule, the ledger and state hash, rollback classes, `--dump-ticks`, rollback hooks |
| [`threading-model.md`](threading-model.md) | Thread roles, the publish path, the mirror, policy objects, Play and Stop, shutdown, lane checks, the shared-state table, determinism tests |
| [`input-two-sided.md`](input-two-sided.md) | One input stream, the cut, `PendingInput`, per-sample fold, key edges, the look camera worked example |
| [`ecs-tutorial.md`](ecs-tutorial.md) | A first component and system, step by step, with a tested example |
| [`tutorial-systems-and-camera.md`](tutorial-systems-and-camera.md) | Components, fixed and presentation systems, order, and a camera that the presentation side turns while the simulation keeps authority (the Sandbox look camera and stasis) |
| [`update-order-design-notes.md`](update-order-design-notes.md) | Why it is built this way, what broke, the reviews, and how each part maps to a large engine with a dynamic per-object tick model |
| [`agent-guide.md`](agent-guide.md) | One self-contained guide for an AI coding agent: the runtime model, the ECS API, recipes and the rules not to break |
| [`handoff-to-fling-authors.md`](handoff-to-fling-authors.md) | For the original Fling authors: what changed since the first commit, migration, adoption stages, verification status and known limits |
| [`../Skills/ecs.md`](../Skills/ecs.md) | The ECS reference: storage, components, systems, hooks |
| [`../Skills/threading.md`](../Skills/threading.md) | The thread layer in Core (threads, queues, sync types) |
| [`../Skills/input-playback.md`](../Skills/input-playback.md) | Record and replay, the `.input.jsonl` format, tick cuts |
