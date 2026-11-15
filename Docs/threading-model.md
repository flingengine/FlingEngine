# Threading model: game thread and presentation thread

This page explains how the fixed tick runs on its own thread while the main
thread renders, what crosses between them, and how the tests prove that the
threads do not change the simulation. The overview is
[`architecture-overview.md`](architecture-overview.md). Ticks, cuts and the
ledger are in [`time-and-ticks.md`](time-and-ticks.md). The thread primitives
(`Thread`, `ThreadQueue`, `Event`, `TickLimiter`) are in
[`../Skills/threading.md`](../Skills/threading.md).

Code: `FlingEngine/Engine/inc/World.h`, `Engine/inc/TickDriver.h`,
`Engine/inc/GameLane.h`, `Engine/inc/PresentationLink.h`,
`Engine/inc/TickEvents.h`, `Engine/inc/Ecs/Publish/` (`PublishedSet`,
`TickPacket`, `TickPacketWriter`, `MirrorApply`, `PublishChannel`),
`Core/inc/Threading/Lane.h`, `Core/inc/Threading/TripleBuffer.h`,
`Core/inc/ExecMode.h`, `Application/src/Engine.cpp`.

## 1. Thread roles

| Thread | Engine name | Runs |
| --- | --- | --- |
| Main | `Main Thread` (`Thread::InitMainThread`), lane `Lane::Frame` | The window and GLFW, input polling (it is the only input writer), ImGui and the editor, play requests, the outbox, the frame groups (`Order::Frame`) on the mirror, `RenderSnapshot`, Vulkan |
| Game | `Game Thread` (`GameLane`), lane `Lane::Game` | Only the wakes of the world (`World::RunWake`): clock commands, the fixed groups (`Order::Tick`), the ledger, the outbox entries, the publish |
| Physics workers | `Physics Worker N` (`JoltJobSystem`) | Jolt jobs inside `Physics::Tick`, fork and join. Never systems |

The game thread exists only during threaded play, the default for play in a
game build and in the editor (`--exec` or `[Engine] Exec` picks another mode;
`RunSettings`). Edit mode, and the `direct` and `inline` modes, run the wakes on
the main thread. The main thread's frames are capped (`FrameLimiter`,
[`time-and-ticks.md`](time-and-ticks.md) section 2); the cap never touches the
game thread.

## 2. The publish path

### The published set

The schedule decides what crosses. `Schedule::Published` lists every
Simulation component that a variable rate system declares `Read<T>` for, with
its readers. The compile rules make this complete: a frame system cannot
write a Simulation component, and a fixed system cannot touch a Presentation
one, so nothing else can cross. Nothing crosses the other way.

`World::Init` turns it into a storage-level `PublishedSet` (`Tables`,
`Singletons`, by component id) with `MakePublishedSet` in `World.cpp`:

- each published type, and the types it `Requires`;

`SimTime` is not added unless a frame system reads it. No Presentation type is
published. What the frame side makes, it keeps in Presentation singletons in the
frame storage (`PendingInput`, `RenderSnapshot`, `CameraView`), made for the
system that writes each one.

It is built and checked in every exec mode, so a frame read of a type that
cannot be copied fails in `direct` too. A queue is never publishable (a
`static_assert` in `PublishedSet::Add`).

The dump prints the set under `Published each tick, for Order::Frame`. For the
engine with the look camera systems it is `LocalToWorld`, `LookCamera`,
`Transform` (test "The engine and look camera systems publish what the frame
reads", `FlingTests/Engine/EcsScheduleTests.cpp`).

### TickPacket and the mailbox

`TickPacket` (`Ecs/Publish/TickPacket.h`) is the full published state of one
tick, as one unit:

| Field | Content |
| --- | --- |
| `Header` | `PacketNumber` and the tick's `TickStamp` (tick, window, cut, state hash) |
| `Alive` | Every living simulation entity, in storage order |
| `Tables` | Per published Table, Tag, Enableable or Sparse type: the entities that have it and their values |
| `Singletons` | Per published singleton: its value, if present |
| `Previous` | Tick N - 1: its stamp and its values of the blended types (`PublishedSet::Blended`), for the blend ("The two blend states"). The writer keeps a copy of them from its last fill. Empty in a writer's first packet |

The game thread fills one with `TickPacketWriter::Fill` after every tick
(`World::OnTickDone`, before the tick's outbox entry, so a frame that applies
the entry can show its tick). The writer only reads the simulation storage. Trivial types are copied with `memcpy` per chunk run;
others are copy-assigned into values the buffer keeps, so a steady tick takes
no memory. `MirrorLink::Publish` is the one place that fills the header:
`Header.Stamp = SystemRunner::LatestTickStamp()`, from the ledger.

The mailbox is a `TripleBuffer<TickPacket>`: a write slot, a ready slot and a
read slot. `Publish` and `AcquireLatest` are one atomic exchange each. Neither
side waits for the other. The newest packet wins; a reader that skips packets
loses nothing, because each packet is the full state.

### The mirror

The frame side owns a second `EntityStorage`, the **mirror** (owner
`Lane::Frame`). `PublishChannel::ApplyLatest` gives the newest packet to
`MirrorApplier::Apply`, which reconciles the mirror:

1. Entities: make the ones that are new with the exact handle
   (`EntityStorage::CreateWithHandle`), destroy the ones that are gone (their
   Presentation components go with them).
2. Per published type: add, assign or remove, with one archetype move per
   entity.
3. Singletons: set or assign.

A reconcile does not touch a type that is not in the packet.

**Entity handles.** The mirror keeps the simulation's handle bits (index and
generation). So an `Entity` stored in a component (`Parent`, `Children`) is
valid in both storages. After the first `CreateWithHandle` the mirror takes
handles only from outside: its own `Create` asserts. So the frame side cannot
make entities of its own in the mirror.

**Cost** (`EngineTests "[publish][cost]"`, one 48-byte `Transform` per entity,
Debug build): 1 000 entities, 64 096 bytes per packet, fill 4.7 µs, apply
39 µs; 10 000 entities, 640 096 bytes, fill 21 µs, apply 392 µs. The first
apply, which makes every entity, is 7 ms for 10 000. When a frame system blends
`Transform`, each packet also carries tick N - 1's Transforms (10 000 entities:
1 200 096 bytes), the fill copies this tick's for the next packet (47 µs), and
the frame side's blend take indexes the new tick (84 µs). No change tracking:
every tick is a full gather, and every packet the main thread takes is a full
reconcile.

### The two blend states

A frame system that draws between ticks declares `ReadBlend<T>` next to
`Read<T>`, and reads `ctx.BlendFrom<T>(entity)` and `ctx.BlendTo<T>(entity)`, the
values at the two ends of the frame's blend. This is the render snapshot
system (`FlingEngine/Graphics/inc/Systems/PublishSnapshotSystem.h`):

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

The schedule marks each published type that a frame system blends
(`PublishedType::BlendReaders`), and `World::Init` copies the marks into
`PublishedSet::Blended`. Nobody lists them by hand. The dump shows them:
`Transform (table), read by Graphics.PublishSnapshot; held for the blend of
Graphics.PublishSnapshot`. Only a frame system can declare it, only for a
Simulation Table type, and only next to `Read<T>` (compile errors,
[`../Skills/ecs.md`](../Skills/ecs.md)).

**The storage rule.** The simulation publishes after every tick to the
publish buffer (the packet and the latest-wins mailbox; `DirectLink` has no
packet, its publish point is the same). Each packet carries tick N and, for
the marked types, tick N - 1 (`TickPacket::Previous`), so whichever packet the
frame side takes, it has the latest two published ticks: `BlendTo` (tick N, the
state in the frame storage) and `BlendFrom` (tick N - 1), also when the frame
side is slower than the simulation, the mailbox dropped packets, or one wake
ran several ticks. Only the marked types travel twice; a type that no frame
system blends costs nothing more. The frame side does not ask for ticks. Its
blend state has one owner, `FrameBlend` (`Ecs/Publish/FrameBlend.h`, owned by
the world): the two ticks it took and the point it drew between them, which it
moves on its own clock ([`time-and-ticks.md`](time-and-ticks.md), "The blend
factor"). Several frames drawing from the same two ticks is normal.

- `ctx.BlendFrom<T>` and `ctx.BlendTo<T>` give the values of the two ticks
  (`RealFrameTime::BlendFrom` and `BlendTo`). `HeldValues` looks them up in
  the link's tables by entity; it copies nothing.
- An entity that did not have `T` then (it is new, or got `T` since) has no
  value in tick N - 1: `ctx.BlendFrom<T>` gives `ctx.BlendTo<T>`, so it does
  not move in from the origin. A new entity in a reused slot is a new entity
  (the generation is checked).
- `Read<T>` always gives the latest tick, which is `BlendTo` in Play: a value
  that is not blended (a material, a light's color) and a destroyed or new
  entity show up to one tick before the blended transforms reach that tick.
- An entity that is gone is gone: the frame storage destroyed it, so no query
  returns it.

The link gives the frame side the two ticks (`PresentationLink::ShowLatest`
returns `ShownTicks`), so a frame system is the same in every mode:

| Link | Tick N | Tick N - 1 |
| --- | --- | --- |
| `MirrorLink` (Inline, Threaded) | The tables of the packet the mirror holds | The same packet's `Previous` |
| `DirectLink` (Direct) | Its copy of the blended types, taken in `Publish` after each tick | Its copy from the publish before |

`World::ShowLatest` gives what the link shows to `FrameBlend::Show`; in Play,
`FrameBlend::Advance` moves the drawn point on and returns the frame's two
ticks and factor, and `RunFrameAt` writes them to `RealFrameTime` (`BlendFrom`,
`BlendTo`, `BlendAlpha`) and gives the frame contexts their values. Edit shows
the latest state (factor 1), and its `DirectLink` copies nothing. Each play
session starts with a new link, so the first frame of play blends from no
state of Edit, in every mode.

**Use.** `Graphics.PublishSnapshot` draws each mesh and point light with
`Interpolate::BlendedWorld`: it blends each `Transform` up the parent chain
(position and scale linearly, rotation by slerp the short way), then composes
the matrices. It blends the local transforms, not the published world data:
a child of a rotating parent moves on an arc, and a blend of two world
matrices (or world positions) cuts the chord, so the child would come closer
to its parent between ticks and its matrix would shear. What did not move
between the two ticks of the blend and the latest tick keeps its
`LocalToWorld` and its version, so the renderer still skips its upload. The camera is not blended: `CameraView` is the sealed
`LookCamera` run through the pending input, newer than either tick
([`input-two-sided.md`](input-two-sided.md), section 4), and `LookCamera`
is not held.

A game's own frame system does the same with the pure helpers in
`FlingEngine/Engine/inc/Interpolate.h`: `Lerp`, `Slerp`, `Blend` (a
`Transform`) and `BlendedWorld` (a world matrix through the parents), each
exact at 0 and at 1.

**Cost:** the marked types travel twice in each packet, and the game thread
copies them once per tick for the next packet (754 `Transform`s in the Sandbox
lighting test: 42 224 bytes). The frame side copies nothing: it indexes the
new tick of each packet it takes. The render snapshot does a blend and a
matrix per moving entity, and only a compare per entity that did not move.

**Tests.** `EngineTests "[interp]"` (`EcsBlendTests.cpp`: each packet
carries the latest two ticks of the marked types only; new, destroyed, changed
and reused entities; after skipped packets `BlendFrom` is still the tick before
`BlendTo`; steady publishes take no memory; `ctx.BlendFrom` and `ctx.BlendTo`;
and the blend factor: the point moves by the frame time between the two ticks,
many frames on one pair, a late tick, a wake that runs two ticks, a renderer
slower than the simulation; `EcsScheduleTests.cpp`: the marks and the dump);
`SandboxExampleTests` (`LookCameraTests.cpp`: in one frame the drawn object is
between the latest two ticks while the camera has the input after the latest
cut);
`GraphicsTests "[interp]"` (`InterpolationTests.cpp`: a world in Direct,
Inline and Threaded play on a virtual clock at 4 frames per tick, at 30, 60 and
120 Hz: even, strictly monotonic positions, the short way round, a rigid child;
the packet path draws what the direct path draws when the ticks arrive at the
same frames, also after Edit ticks before play; a pause; and a game thread
driven by the stepping harness that publishes every tick late, or stalls for
three ticks while the frames go on); the compile checks `Blend*`; and the CTests
`Sandbox: rendered motion is even direct|inline` (`Sandbox.MotionProbe` logs
how evenly the drawn spheres turn). Which frame first has a tick is the only
difference between the modes; the rule is one function, so the parity test
checks the two paths that deliver the ticks, not the rule. There is no threaded
rendered-motion CTest: a threaded headless frame does not wait for the game
thread, so which frame first has a tick depends on thread timing. The stepping
harness tests threaded motion instead.

## 3. How `Read<T>` in a frame system reaches the mirror

The system does not change. `SystemRunner` runs the fixed groups on the
simulation storage and the frame groups on a **frame storage**
(`SystemRunner::SetFrameStorage`). The context of a frame system
(`SystemContextBase`) is made over the frame storage, so
`ctx.Query<const LookCamera>()` reads the mirror's `LookCamera`. The frame
systems' own singletons (`PendingInput`, `RenderSnapshot`, `CameraView`, `RealFrameTime`) are
made in the frame storage. In `direct`, the frame storage is the simulation
storage. `World::UseLink` moves the frame side at play start and stop.

`RunFrameAt(now, delta, role, latest)` writes `RealFrameTime::Latest =
latest`, the stamp of the packet the mirror holds, and fills `PendingInput`
with the input after `latest.Cut`. So the state, the tick and the pending input all come from
one packet.

## 4. Two policy objects, three execution modes

The mode is two small objects, chosen only at play start
(`World::StartGame`, `World::StartPlayDriver`):

- `TickDriver` (`TickDriver.h`): who runs the wakes. `SameThreadDriver::OnFrame`
  runs one wake on the main thread, in the frame, under the tick lane's scope.
  `GameLane` runs wakes on the game thread; its `OnFrame` does nothing.
- `PresentationLink` (`PresentationLink.h`): what the frame reads.
  `DirectLink` reads the simulation storage, and its shown stamp is the
  ledger's latest. `MirrorLink` owns a `PublishChannel`; `Publish` fills a
  packet, `ShowLatest` applies the newest one and returns its stamp.

| Mode (`--exec`) | Driver | Link | Simulation storage owner |
| --- | --- | --- | --- |
| Edit (editor, not playing) | `SameThreadDriver` | `DirectLink` | `Lane::None` |
| `direct` | `SameThreadDriver` | `DirectLink` | `Lane::None` |
| `inline` | `SameThreadDriver` | `MirrorLink` | `Lane::Game` (the wake runs under a `Lane::Game` scope) |
| `threaded` | `GameLane` | `MirrorLink` | `Lane::Game` |

`GameLanePacing` (`GameLaneDesc::Pacing`): `RealTime` sleeps until the next tick
is due (windowed); `AsFastAsPossible` runs each tick as soon as the virtual
clock makes it due (headless); `Stepped` waits for a permit from
`World::StepGameLane` (tests). `World::Update` and `World::RunWake` have no
mode branch.

## 5. Play, Stop and Edit mode

Play start (`World::StartGame`, main thread), one order in every mode:

1. Save a snapshot of the entities (`SaveToJson`).
2. Use a new link: a `DirectLink` in `direct`, a `MirrorLink` otherwise.
3. `OnPlayBegin` of the fixed systems, then `Game::OnStartGame`.
4. Publish the first state and show it (`Publish`, `ShowLatest`).
5. `OnPlayBegin` of the frame systems, on that state.
6. Give the simulation storage to the link's owner lane, then start the
   driver. If the game thread cannot start, play stops again with an error; it
   does not fall back to another mode.

Play stop (`World::StopGame`): `UseEditDriver` (the game thread stops after its
wake and joins; the storage is the main thread's again), the session's clock
state is dropped (a pause, and the clock commands that wait or are queued), so
Edit and the next play session tick again; then `OnPlayEnd`,
`Game::OnStopGame`, back to a `DirectLink`, and the snapshot loads. A start or
stop request runs at the start of the next `World::Update`, never during the
call.

In play, in every mode, the editor is read-only (`BaseEditor`: panels disabled,
`WorldOutlinePanel::SetReadOnly`): no edit changes the simulation outside a
tick. In Inline and Threaded play it shows the mirror, so only the published
types.

## 6. Start-up, shutdown and quit

- **Start-up.** `Engine::Startup` runs completely on the main thread (config,
  input, Vulkan, the module registrations, `Physics::InitWorld`, `World::Init`)
  before any game thread exists. The thread start is the happens-before edge
  for all of it.
- **Quit from a tick.** A system appends `AppRequest::Quit` to `AppRequests`.
  The tick output puts it in the outbox and ends the session at that tick: no
  tick runs after it, in any mode. The main thread applies the entry: it quits
  the application, or in editor play (`World::SetGameQuitStopsPlay`) stops play.
  Test: `Sandbox: threaded replay input quits` (the run ends at tick 31).
- **Shutdown.** `World::Shutdown` stops play first (the game thread joins),
  then changes the state, destroys the driver, the link and the runner, and
  calls `Game::Shutdown`. It runs once. `Engine::Shutdown` calls it before
  `Input::Shutdown`, the renderer and physics, because the game thread reads
  the input queue. `~Engine` calls it too, for an exception out of
  `Engine::RunFrames`, and `~World` destroys the driver first.
- **`--frames N`.** No tick runs after tick `ceil(N / frames-per-tick)`. The
  run ends at the first frame where at least N main-thread frames have run and
  the frame side shows that tick (`World::GetShownTick`). So N is a minimum
  frame count. A headless `direct` or `inline` run has exactly N frames. A
  headless `threaded` run usually has more (467 to 491 frames for
  `--frames 120` in two runs), because the game thread runs its ticks on its
  own. A windowed run follows the clock: it lasts until that tick is due (4 s
  for `--frames 120` at 30 Hz), at whatever frame rate.

## 7. Ownership checks in debug

`Lane` (`None`, `Game`, `Frame`) is a debug tag per thread
(`LaneCheck::Scope`). `FLING_LANE_CHECK(owner)` calls the violation handler
when a thread tagged with one lane touches data that another lane owns. It is
empty when `NDEBUG` or `FLING_SHIPPING` is defined.

| Checked | Owner |
| --- | --- |
| `NativeStorage` entry points (create, destroy, add, remove, get, singletons, queries, and the reads `IsValid`, `Count`, `EntityCount`, `AllEntities`, `IsEnabled`, `Locate`, `Archetypes`) | The storage's owner lane (`EntityStorage::SetOwnerLane`) |
| The mirror apply | `Lane::Frame` |
| Input writer calls (poll, handle a message) | The writer lane (`Input::SetWriterLane`) |
| `TickInputProvider::Build` with `TickCutSender::TickLane` | `TickInputProviderConfig::TickLane` |
| `Timing`, the global `Random`, `Stats::Frames`, `ResourceManager` | `Lane::Frame` |

A thread with no tag passes, and data with no owner passes (`LaneCheck`, in
`Core/inc/Threading/Lane.h`, is the one place in code that says so). This is on
purpose: a test thread, a tool thread and a physics worker (Jolt jobs run
inside `Physics::Tick`, fork and join, so they borrow the game thread's
ownership) have no lane, and many tests drive a storage from an untagged
thread. Only the two engine threads are tagged, and a tagged thread is checked
at every storage entry point, the reads too. Tests: "Exec modes: the lane
checks see the main thread touch the game thread's storage" (every read entry
point).

A second debug check guards the simulation itself: while the runner runs fixed
systems it marks the thread (`SimulationCheck::Scope`), and the wall clock
(`PlatformTime`, `Timing`), the global `Random` and every query of the `Input`
singleton call `FLING_SIMULATION_FORBID`. It fires in every exec mode, also in
Direct where the lane check cannot see anything. Test: "Input boundary: a fixed
system that reads the Input singleton fails the simulation check". Like the lane
check it is a debug check only: a release build (`NDEBUG`) does not catch a
fixed system that calls `Input::Get()`. This is a known limit.

## 8. Shared state

| State | Written by | Read by | Protection |
| --- | --- | --- | --- |
| `TripleBuffer` ready slot | game (`Publish`) | main (`AcquireLatest`) | atomic exchange, acquire/release |
| `TickPacket` slots | game (write slot) | main (read slot) | hand-off through the ready slot |
| Simulation storage | main (Edit, start, stop); game (play) | the same | thread start and join; owner lane in debug |
| Mirror, applier, shown header | main | main | one thread |
| Runner fixed part (timeline, ledger, tick input) | the tick thread | the tick thread; others through `TakeNewTickRecords` | convention; a queue for ledger rows |
| Runner frame part (frame storage, frame clock) | main | main | one thread |
| Input queue | main (the one writer) | game (tick cut), main (pending, ImGui, recorder) | per-slot seqlock; watermark atomic; an `Event` at the end of each poll wakes a waiting tick (`InputMessageHandler::WaitForPoll`) |
| Playback cuts, recorder cuts | main / game | the other | `ClaimTickCut` checks an atomic and locks only while a replay with cuts plays; `NoteTickCut` pushes to a `ThreadQueue` only while recording |
| Outbox | game | main | `ThreadQueue<TickOutboxEntry>` |
| Clock commands (Pause, Resume, SetRate) | main | game | `ThreadQueue<ClockCommand>`, plus an `Event` that wakes a paused lane |
| `World::m_SimHz` (the rate in effect, for the GPU Options window and the recorder's rate) | game | any | atomic |
| Interpolation flag, run settings (`RunControls`) | main | main | one thread; the simulation never reads them |
| Ledger rows for tools | game | main | `ThreadQueue<TickRecord>` |
| `World::m_TickCount`, `m_RanLastTick`, `m_GameLane` | game / main | any | atomics |
| `World::m_CurrentState`, play requests | main | main | one thread (the game thread never reads them) |
| System instances | main (create, destroy) | fixed ones on game, frame ones on main | disjoint sets; systems are stateless |
| Virtual clock (headless) | main | any | atomic, one writer |
| Vulkan, ImGui, `ResourceManager`, `Timing`, global `Random` | main | main | lane checks |

### Locks and blocking

In live play the two threads share three things: the triple buffer of tick
packets (one atomic exchange on each side), the input ring (a seqlock per slot
and a watermark atomic, so a reader retries and never waits) and the command
queues (`ThreadQueue`: the outbox, clock commands and ledger rows). A queue's
mutex is held only for a `push_back` or a vector swap, and both vectors keep
their capacity, so a steady push does not allocate under it. No other mutex is
taken by both threads in live play: the playback's cut lock is taken only while
a replay with cuts plays, and the recorder's cut queue only while recording.
Test: "Exec modes: live play that does not wait for input replays exactly from
its recorded cuts" counts the cut lock.

The game thread waits for the main thread by design in two cases only. In a
headless run and in `--replay-input`, a tick waits for the main thread's input
poll (the watermark rule, and a replayed tick's recorded cut;
`InputMessageHandler::WaitForPoll`); live play has a watermark wait of 0. At
Stop, the main thread joins the game thread after its current wake. The main
thread never waits for the game thread in play. An unfocused window caps the
main thread at 30 frames per second (`RunSettings::BackgroundMaxFps`); the
game thread's ticks are not capped.

## 9. The determinism tests

**What they check.** The per-tick ledger (tick, cut, input counts, state hash)
is the same in every exec mode, at every frame rate, under thread jitter, and
the frame side never shows a torn state.

**Why they can fail.** The main thread is the input writer in every mode, also
headless and in the harness, and the game thread cuts the queue that the main
thread writes, by the timeline and the watermark. So the frame thread has a
path into the simulation, and a wrong cut changes the ledger. The harness has no mutex that would order the two
threads on every tick: its tick log has one slot per tick, written before the
packet is published and read after it is applied.

**The engine's rule, not a copy.** The harness (`ThreadedWorldHarness.h`) has
no input rule of its own. It drives the `Input` singleton as an engine frame
does: after each clock step it queues the script input and calls `Input::Poll`
(playback, device interfaces, recorder); the ticks take their input from the
engine's `TickInputProvider` with the config of `World::MakeTickInputConfig`
(the same call `Engine::Startup` makes); the frames read through
`PendingInputReader` after `Input::PollLate`. A live run records with
`InputRecorder`, and a replay plays that recording with `InputPlayback`, the
path of `Sandbox --replay-input`.

| Test | Where | What | A broken rule it catches (checked by breaking it) |
| --- | --- | --- | --- |
| Exec modes: the tick ledger is the same in Direct, Inline and Threaded | `FlingTests/Engine/ThreadedWorldTests.cpp` | 240 ticks of seeded look input at 30 and at 60 Hz, four frame patterns (one or three frames per tick, bursts of 1 to 7 ticks per wake, no frames), and hitches | The time rule: a tick that takes every queued message instead of its window (checked) |
| Exec modes: the main thread writes the input and a free game thread cuts it by the timeline | same | 30 000 ticks against `direct`; and 5 000 with 300 µs jitter on both threads | The watermark rule: a game thread that does not wait for the writer (checked) |
| Exec modes: live play that does not wait for input replays exactly from its recorded cuts | same | the live rule (no watermark wait) recorded with `InputRecorder`; then the recording replayed with `InputPlayback` in every mode and two frame patterns: each tick's input counts and state hash equal the live run, and all replays are equal to the sequence | The replay rule: a tick that ignores its recorded cut (checked) |
| Exec modes: clock commands, Quit, stop and restart, shutdown during play, frame `OnPlayBegin`, lane checks | same | one case each |
| Publish cases | `FlingTests/Engine/EcsPublishTests.cpp` | round trips, skipped packets, idempotent apply, randomized changes, no writes to the simulation storage, steady-state memory |
| Input with threads | `FlingTests/Core/InputThreadTests.cpp` | one writer and three readers, overrun, watermark, capture as a message |
| `Sandbox: tick dump exec modes cuts`, `... cuts 60 Hz`, `... time`, `... jitter`, `... stasis` | `Sandbox/CMakeLists.txt`, `Sandbox/Tests/CompareExecModes.cmake` | byte-identical `--dump-ticks` files in every mode, at `--max-fps` 30 to 1000, with 10 jitter seeds, at 30 Hz (and the 60 Hz recording at 60 Hz); `stasis` replays `stasis.input.jsonl` (G, then F and mouse moves inside the stasis) for 150 ticks in every mode and at `--max-fps` 30 to 1000 |
| `Sandbox: tick dump level Default`, `... Level_1`, `... PhysicsTest` | `Sandbox/CMakeLists.txt`, `Sandbox/Tests/CompareExecModes.cmake` | the start level or a level with entities and no input: byte-identical dumps from two direct runs, inline and threaded; with Jolt, the dump's SHA-256 is pinned (also for `... exec modes time`) |
| `Sandbox: tick dump replay by cuts`, `... by time`, `... by cuts 60 Hz`, `... by time 60 Hz` | `Sandbox/CMakeLists.txt`, `Sandbox/Tests/CompareTickDumps.cmake` | byte-identical dumps for two frame patterns in each of the three modes, at 30 and at 60 Hz |

Every frame of the harness checks that the packet's tick and cut and
`RealFrameTime::Latest` agree with the shown tick (`World::GetShownTick`), that
the cut is the cut the simulation had at that tick, that the mirror's
`LookCamera` is bit-identical to the camera the simulation had at the end of
that tick, and that the pending input starts right after that cut.

How to run them:

```bash
./build/FlingTests/bin/EngineTests "[threads]"
ctest --test-dir build -R "Exec modes|exec modes|threaded" --repeat until-fail:10
```

MSVC has no ThreadSanitizer, so the race net on Windows is the jitter runs and
the stress cases. A ThreadSanitizer run on Linux (clang 18) of CoreTests,
ResourcesTests, EngineTests, PhysicsTests and the headless Sandbox, made on the
code before the interpolation was merged, found no data race in engine code;
it did not cover the interpolation (`ReadBlend<T>`, the blend states),
GraphicsTests, the windowed Sandbox or the editor.

## 10. What a game programmer needs to know

Declare the group. A system in a group under `Order::Tick` runs on the game
thread in threaded play; a system in a group under `Order::Frame` runs on the
main thread. The access list and the compile rules do the rest. Do not name a
thread, take a lock, or read another storage. A frame system that reads a
Simulation component gets the copy from the newest packet, with no code
change. Keep the frame side stateless, and steer it from the simulation through
data in a published component (see [`input-two-sided.md`](input-two-sided.md)).

## 11. Known limits

- **One rate on one thread.** The schedule has no lane type: one update rate
  is one thread. A second thread for one rate needs that type again.
- **Live threaded play** is reproducible only from its recording (the live
  input rule in [`time-and-ticks.md`](time-and-ticks.md)).
- **A threaded replay is paced by the main thread's input polls.** A replayed
  tick waits until the main thread's next poll (`Input::Poll` or
  `Input::PollLate`, two a frame) sends its recorded cut, and a timed tick of a
  replay or a headless run waits until the poll's watermark passes the end of
  its window. The game thread sleeps while it waits
  (`InputMessageHandler::WaitForPoll`, re-checking a stop every 10 ms), so it
  uses no CPU, but a replay runs at most two ticks a frame. While the window is
  minimised the main thread waits in `glfwWaitEvents` and does not poll: the
  replayed ticks stop until the window is restored. In the background the
  frame cap drops to `RunSettings::BackgroundMaxFps`, and the replay slows with
  it. Live play does not wait (watermark wait 0).
- **Recorder start and stop** racing a tick decide only whether the boundary
  tick's cut is in the file. The file stays self-consistent.
- **Untagged threads pass** the lane checks (section 7). `World::GetStorage`
  still returns the game-owned storage during play; the storage's own entry
  points then catch a tagged thread that uses it.
- **The frame side cannot make entities in the mirror.**
- **Full copy every published tick**, and the apply is O(all mirror entities).
- **Interpolation** (section 2, "The two blend states") draws between the
  latest two published ticks, so at most one tick behind the latest published
  tick, with no added delay. A tick that arrives late shows as held frames, and
  the tick after it as a skip of the part the hold did not draw (at most one
  tick of motion): nothing hides the game thread's lateness. Not built: extrapolation, and a teleport or
  discontinuity flag (a respawn, a re-parent or a correction after a
  resimulation is blended over one tick). Parent links are not kept per state:
  after a re-parent both transforms blend under the new parent.
  `DirectionalLight` is not blended (it has no `Transform`), nor are light
  colors and intensities. `ReadBlend<T>` is per entity only (no singletons).
- **The editor in play** shows only published types, and the outline is a flat
  list.
- **Edit ticks share the play tick numbers.**
- **The headless main thread** never waits for the game thread: it runs its
  frames as fast as the frame cap allows, and the virtual clock may run ahead
  of the ticks. The ticks do not change: a wake on that clock never drops time
  (`World::SetTickClockOnlyMovesWithFrames`). The input queue holds 4 096
  messages: a main thread that queues more than that ahead of a slow game
  thread overwrites input. The tick that lost it is marked
  (`TickRecord::InputOverran`, `input-overran` in `--dump-ticks`), and a
  replay stops with an error (`--replay-input` then exits with `1`). Test
  "Exec modes: a main thread that runs ahead of the game thread past the input
  queue marks the tick".
- **Not verified:** the editor Play and Stop buttons and pause by hand. The
  ThreadSanitizer run on Linux (section 9) covered the code before the
  interpolation; the Sandbox window was not run on Linux.
