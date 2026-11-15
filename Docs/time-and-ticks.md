# Time and ticks

This page explains how Fling decides when a fixed tick runs, which input it
takes, how it is named, how it is recorded, and what exists for rollback. The
overview is [`architecture-overview.md`](architecture-overview.md). The
threads are in [`threading-model.md`](threading-model.md).

Code: `FlingEngine/Core/inc/PlatformTime.h`, `Core/inc/TickTimeline.h`,
`Core/inc/TickHistory.h`, `Core/inc/Input/InputCut.h`,
`Core/inc/Input/TickInputProvider.h`, `Engine/inc/Ecs/TickStamp.h`,
`Engine/inc/Ecs/TickLedger.h`, `Engine/inc/Ecs/StateHash.h`,
`Engine/inc/Ecs/SimTime.h`, `Engine/inc/Ecs/SystemRunner.h`.

## 1. The clock

There is one clock: `PlatformTime::NowNanoseconds()`. It reads the steady
clock, or the `PlatformTime::VirtualClock` that `UseVirtualClock` installed.
Input message times, tick windows and frame times all come from it.

| Run | Clock |
| --- | --- |
| Windowed | The steady clock. |
| `--null-render` | A `VirtualClock`. Frame `f` polls at `HeadlessFrameNs(f) = step + f * step / N` (`--frames-per-tick N`); its ticks are due at `HeadlessFrameNs(f + 1)`. The timeline starts at `HeadlessFrameNs(1) - step`, so tick 1 is due at the end of the first frame, then one tick every N frames. |
| Tests | `PlatformTime::ScopedVirtualClock`. |

A `VirtualClock` has one writer and any number of readers. It never goes back.
`SteadyNanoseconds()` is always real, for logs only.

What a fixed system may read is `SimTime`: `Tick`, `FixedDeltaNs` and
`ElapsedNs` (the sim time at the end of the tick: `Tick * FixedDeltaNs` until
the rate changes, then the sim time at the last tick of the old rate plus the
ticks since at the new one; computed from the tick number, never a running
sum), with `DeltaSeconds()` and `ElapsedSeconds()`. The runner writes it before
each tick from the tick's stamp (`SimTimeOf`, section 3); no system may write
it. A fixed system cannot read wall time:

- `RealFrameTime` is a Presentation component (compile error).
- The timeline and the ledger are runner objects, not ECS data.
- `PlatformTime::NowNanoseconds`, `Timing::GetDeltaTime`, `Timing::GetTime`
  and the global `Random` assert in debug while fixed systems run, and while
  the changes they deferred are flushed at the end of the tick
  (`FLING_SIMULATION_FORBID`, `SimulationCheck`). There is no compile-time rule
  for a free function, and a release build does not check it. Test (one call
  per guard, so a guard that is taken out fails it): "Simulation check: a
  fixed system that reads a clock or the global Random fails it"
  (`FlingTests/Engine/InputBoundaryTests.cpp`).

## 2. The tick timeline

`TickTimeline` maps tick numbers to windows on the clock. Tick `n` covers
`[Start(n), Start(n) + step)`, with `Start(n) = BaseNs + (n - BaseTick) * step`
and `step` in whole nanoseconds (`SimRate::StepNs`: 33 333 333 at the default
30 Hz, 16 666 667 at 60 Hz). Windows are computed from the tick number, never
summed. The rounding of `step` is the only difference from wall time: at most
half a nanosecond a tick (36 us an hour at 30 Hz, 72 us at 60 Hz). It moves
windows only, never a tick number or a simulation result.

### The run settings

Every setting that decides how a run ticks and shows its frames is read in one
place, `RunSettings` (`Core/inc/RunSettings.h`), once at startup, with one
precedence: the command line, then `[Engine]` in `Config/EngineConf.ini`, then
the default. Every key that is in the file is checked in every run, also when a
flag wins over it, so a typo in the file always shows. A value that is not
valid stops the run with exit code 3 and a message that names the flag or the
key. A flag that does nothing in the run is logged as a warning (`--vsync does
nothing with --null-render: there is no swap chain`), and `--thread-jitter`
outside `--exec threaded` is an error.

| Setting | Flag | `[Engine]` key | Default | Changed while the game runs |
| --- | --- | --- | --- | --- |
| Exec mode | `--exec direct\|inline\|threaded` | `Exec` | threaded | no |
| Tick rate | `--sim-hz N` (1 to 1000) | `SimHz` | 30 | GPU Options, at a tick boundary (below) |
| Frame cap | `--max-fps N` (0: none, up to 1000) | `MaxFps` | the refresh rate of the window's monitor, else 60; none with `--null-render` | GPU Options, from the next frame |
| VSync | `--vsync on\|off` | `VSync` | on | GPU Options, the swap chain is made again |
| Interpolation | `--interpolation on\|off` | `Interpolation` | on | GPU Options, from the next frame |
| Thread jitter (tests) | `--thread-jitter S:U` (`--exec threaded` only) | none | off | no |

`RunSettings` does not change after startup. The live values are in
`RunControls` (`Engine/inc/RunControls.h`): VSync and the frame cap, which the
GPU Options window sets, and the monitor's refresh rate and the window's focus,
which the engine reports each frame. The world holds the tick rate and
interpolation, and `RunControls` reads them from it. The GPU Options window of
the editor (menu Windows, "GPU Options"; the Sandbox opens it at start in
`--play`) calls `RunControls`, which checks each request; the window holds no
rule of its own: its limiter and measured lines come from
`RunControls::DescribeLimiter` and `DescribeMeasured`.

### The tick rate

The simulation rate is one setting, read in one place (`RunSettings`,
`Core/inc/RunSettings.h`): `--sim-hz N`, else `[Engine] SimHz` in
`Config/EngineConf.ini`, else 30 (`SimRate::DefaultHz`). It must be from 1 to
1000; any other value stops the run with exit code 3 and a message that names
the flag or the key. The engine puts it in `SystemRunnerConfig::SimHz`, which
is the only source of the tick length: the timeline's `step` and
`SimTime::FixedDeltaNs`. Physics has no clock of its own: `Physics.Step` calls `Physics::Tick` with
`SimTime::DeltaSeconds()`, and that runs exactly one physics step of that
length. The provider splits the step into `PhysicsCollisionSteps(dt)` =
ceil(dt * 60) equal collision steps (`PhysicsCollisionStepsPerSecond`): two at
30 Hz, one at 60 Hz and above, so a low rate keeps the contacts stable. The
count depends only on the tick length, so it is deterministic. The headless clock (`--null-render`) moves by the same tick length,
and the input recorder and playback get the rate for the tick cuts
(section 8, "Recorded input").

A test or a tool that needs a rate sets `SystemRunnerConfig::SimHz` (or
`--sim-hz`); a rule that does not depend on the rate is tested at 30 and 60 Hz.

### Changing the rate while the game runs

A rate change is a tick-stamped clock command, never shared state:
`World::RequestSimHz(uint32 t_Hz, String& t_OutError, uint64 t_BeforeTick = 0)`
sends a `ClockCommand` of type `SetRate` (it returns false, with the reason in
`t_OutError`, for a rate outside 1 to 1000 or while a replay or recording runs),
and the tick thread applies it in `World::ApplyClockCommands`, between two
ticks, in every exec mode (`SystemRunner::SetSimHz`). So tick N has one rate:

- Tick N, the first at the new rate, starts where tick N - 1 ended and is one
  tick at the new rate long; every later window too (`TickTimeline::SetStep`).
  Tick numbers go on by one.
- `SimTime::FixedDeltaNs` is the new length from tick N on, so the physics step
  and its collision steps follow. `SimTime::ElapsedNs` goes on from tick N - 1
  (the runner keeps the last tick of the old rate and its sim time), and
  `MaxTicksPerWake` is the 250 ms budget at the new rate.
- The stamp of each tick (in the ledger and the packet) records its
  `FixedDeltaNs` and `ElapsedNs`, so the ledger row alone gives the tick's
  `SimTime`, before and after the change. The blend factor takes its tick
  length from the stamp's window.
- `BeforeTick = N` applies it right before tick N (a test, or a tool that knows
  the tick); `BeforeTick = 0` at the next wake (the GPU Options window, which
  cannot know which tick the game thread is at). The tick it applied at is
  logged: `Simulation rate 60 Hz from tick 25 (was 30 Hz)`.
- `RunControls::RequestSimHz` refuses it while an input replay plays or a
  recording runs, and logs why: a recording (format 2) stores one tick rate for
  its cuts, and the format is not extended. The input recorder and playback get
  the new rate on the next frame, so a recording started later notes it.
- The headless clock (`--null-render`) keeps the startup rate's frame step; no
  headless run changes the rate.

Test (able to fail: a `SetStep` that does not rebase, an `ElapsedNs` with no
base, and a stamp whose `ElapsedNs` leaves out the base, each fail it): "Exec
modes: a simulation rate change applies at one tick boundary, with the same
ledger in every mode" (`FlingTests/Engine/ThreadedWorldTests.cpp`: 30 to 60 Hz
and 60 to 20 Hz before tick 25, in Direct, Inline and Threaded: every window,
`FixedDeltaNs` and `ElapsedNs` before and after, `SimTimeOf` each ledger row
equal to the `SimTime` the tick's systems saw, the same ledger and windows in
every mode), and
"... a live rate request applies at the next wake; a replay or a recording
refuses it".

### The frame cap

The main thread's frames are variable rate, with a cap (`FrameLimiter`,
`Core/inc/Threading/FrameLimiter.h`): `--max-fps N`, else `[Engine] MaxFps`,
else the refresh rate of the monitor the window is on (the GLFW video mode),
else 60 when that is 0 or not known. 0 means no cap. A cap that came from the
monitor follows the window to another monitor: the engine reads the refresh
rate again once a second (`RunControls::SetMonitorHz`). GLFW gives the refresh
rate as a whole number, so a 59.94 Hz display reports 59. A `--null-render`
run has no cap unless one of the two settings asks for one, so headless runs
and tests are as fast as before. While the window is in the background the cap
is at most 30 frames a second (`RunSettings::BackgroundMaxFps`).

The limiter sleeps with a `SleepTimer` (`Core/inc/Threading/SleepTimer.h`: the
Windows high-resolution waitable timer when the OS has it) until 1 ms before
the frame is due, then spins, and keeps a fixed schedule (frame N is due at
start + N / cap). The spin is its only CPU cost, at most 1 ms a frame; without
the high-resolution timer a frame can start up to one timer tick (15.6 ms)
late instead. It reads only the steady clock and runs only on the main thread,
so it never touches the game thread.

No frame setting changes a replay or a headless run: the tick dumps at caps of
30, 60, 144 and 1000 are the same (`Sandbox: tick dump exec modes time`), and a
replay takes its recorded cuts. In live windowed play the main thread polls
the input once a frame, so the frame rate (the cap, VSync) decides how late a
message reaches the queue, and so which tick it goes to (the live input rule);
a recording keeps those cuts.

### VSync and the frame cap

One setting says how frames are shown: VSync, on by default. The swap chain
takes its present mode from it and the modes the surface offers
(`ChoosePresentMode`, `Graphics/inc/PresentMode.h`):

| VSync | Present mode |
| --- | --- |
| on | FIFO (every surface offers it) |
| off | IMMEDIATE, else MAILBOX, else FIFO: frames do not wait and may tear, for the lowest latency |

The swap chain logs its mode each time it is made: `Present mode IMMEDIATE
(vsync off)`. A change at run time (the GPU Options "VSync" combo) makes the
swap chain again on the path a resize takes (`VulkanApp::SetVSync`, then
`VulkanApp::RecreateSwapchain` after the frame's present). The window shows the
present mode in use as information.

How the cap and the mode combine is one rule, `LimiterFps` (`RunSettings.h`),
which takes whether the mode in use waits for the display: the lower of the
cap and the display's refresh rate wins.

| Mode in use | Cap | The limiter keeps |
| --- | --- | --- |
| MAILBOX or IMMEDIATE | any | the cap (0: no wait) |
| FIFO or FIFO_RELAXED | at or above the monitor's refresh rate (the default cap is the refresh rate) | no wait: the display paces the frames, so the limiter does not fight it (a limiter at 59 fps against a 59.94 Hz display misses a refresh a second) |
| FIFO or FIFO_RELAXED | below the refresh rate | the cap |
| any, window in the background | | at most 30 |

At startup the engine logs one line with the settings and where each came
from, for example `Run settings: tick rate 30 Hz (default), exec threaded
(default), frame cap 144 fps (the monitor's refresh rate), vsync on (default),
interpolation on (default)`, then what the limiter does (`Frame limiter: The
display paces the frames (cap 144 fps, present mode FIFO)`). At exit it logs
the measured average frame time: `Run (threaded): ... average frame 6.944 ms
(144.0 fps)`.

Tests: `FlingTests/Core/RunSettingsTests.cpp` (precedence, every key checked,
bad values, the thread-jitter rule, an ignored flag, and `LimiterFps` for each
row above), `FlingTests/Resources/RunSettingsConfigTests.cpp` (the keys of an
ini file reach the settings), `FlingTests/Engine/RunControlsTests.cpp` (live
values, the cap that follows the monitor, the limiter and measured text),
`FrameLimiterTests.cpp` (the schedule, the spin, and a sleep that never wakes
early), `FlingTests/Graphics/PresentModeTests.cpp` (the mode for VSync on and
off from each list of offered modes).

| Rule | As implemented |
| --- | --- |
| First tick | The timeline starts at the first `RunFixedAt` time, or at `World::SetTickTimelineStart`. The first tick is 1. |
| Due | A tick is due when `now >= End(next)`. `SystemRunner::RunFixedAt(now, role, tickLimit)` runs the due ticks. |
| Limit per wake | At most `SystemRunnerConfig::MaxTicksPerWake()`: the catch-up budget `MaxCatchUp` (250 ms of clock time) in ticks at `SimHz`, rounded up (1 at 1 Hz, 8 at 30 Hz, 250 at 1000 Hz). A time, not a tick count, so the hitch rule is the same at every rate. And no tick after `World::SetLastTick` (`--frames`) or a Quit. |
| Hitch | If that many ticks ran and a tick is still due, `TickTimeline::DropBacklog(now)` moves the windows forward by whole steps, so the next window holds `now`. The phase is kept. The number of dropped steps goes on the next tick's `TickRecord::DroppedSteps`. No tick number is skipped. A wake that a tick limit ended keeps its due ticks. A tick clock that only moves when frames run (the headless virtual clock, `World::SetTickClockOnlyMovesWithFrames`) has no hitch: a wake reads the clock at most `MaxTicksPerWake()` ticks ahead, and the game thread's next wake runs the rest, so the frames may run ahead of the game thread without changing a tick. |
| Pause | `World::PauseTicks` and `ResumeTicks` send a `ClockCommand`, applied at the start of the next wake in every exec mode. No tick is due while paused. On resume at `r` after a pause at `p`, the windows of the ticks that have not run move by `r - p`. `SimTime` never sees the pause. |
| Stamped command | A `ClockCommand` with `BeforeTick = N` runs right before tick N; the wake runs no tick past N - 1 while it waits. A stamped command whose tick has run is dropped. |
| Rate change | A `SetRate` clock command: from the tick it applies before, the windows and `SimTime::FixedDeltaNs` have the new length ("Changing the rate while the game runs"). |
| Why a tick ran | `TickTrigger::Clock` (the clock reached the window end) or `TickTrigger::Request` (`SystemRunner::RunTick`, `OneTickPerFrame`). |

`TickWindow` is `{StartNs, EndNs, Trigger}`. The window is wall-clock time: for
input assignment, pacing and presentation, never for simulation results.

## 3. TickStamp and InputCut

One tick has one identity. `TickStamp` (`Ecs/TickStamp.h`):

```cpp
struct TickStamp
{
    uint64 Tick = 0;          // the key; the first tick is 1, 0 means none
    int64 FixedDeltaNs = 0;   // the tick's length
    int64 ElapsedNs = 0;      // the sim time at the tick's end
    TickWindow Window;        // wall clock: input assignment and pacing only
    InputCut Cut;             // the input messages the tick consumed
    uint64 StateHash = 0;     // HashSimulationState after the tick
};
```

`Tick`, `FixedDeltaNs` and `ElapsedNs` are all of the tick's `SimTime`:
`SimTimeOf(stamp)` (`Ecs/SimTime.h`) makes it, and the runner writes `SimTime`
with it before the tick's systems run. So a tick can be run again from its
ledger row alone, also after a rate change.

`InputCut` (`Core/inc/Input/InputCut.h`) is the queue range
`(AfterSequence, LastSequence]` that a tick consumed, or that a frame has
pending. When the range is empty the two are equal.

Everything that names a tick holds a `TickStamp`, and nothing else does:

| Holder | Field |
| --- | --- |
| The ledger | `TickRecord::Stamp` |
| The published packet | `TickPacketHeader::Stamp` (with `PacketNumber`) |
| The frame | `RealFrameTime::Latest`: the stamp of the newest tick the frame side holds (its state is in the frame storage); `RealFrameTime::BlendFrom` and `BlendTo`: the two blend states the frame draws between (`ctx.BlendFrom<T>`, `ctx.BlendTo<T>`) |
| The tick's input | `TickInput::Slice.Cut` (the same cut as `Stamp.Cut`) |

`Tick`, `FixedDeltaNs`, `ElapsedNs`, `Cut` and `StateHash` are the same for
the same input at any frame rate. `Window` and `RanAtNs` are not.

### The blend factor

The simulation publishes after every tick; the frame side never asks for a
tick. Each published packet carries the full state of tick N and, for the
types a frame system blends, the values of tick N - 1
(`TickPacket::Previous`, [`threading-model.md`](threading-model.md), "The two
blend states"). So the frame side always has the latest two published ticks:
`RealFrameTime::BlendTo`, tick N (it is `Latest`, the state in the frame
storage that `Read<T>` gives), and `RealFrameTime::BlendFrom`, tick N - 1, also
when it is slower than the simulation or one wake ran several ticks. Each frame
draws `BlendFrom` (factor 0, `ctx.BlendFrom<T>`) blended toward `BlendTo`
(factor 1, `ctx.BlendTo<T>`) by `RealFrameTime::BlendAlpha`. Many frames can
draw from the same two ticks.

The factor belongs to the renderer and uses only the renderer's own clock. The
renderer keeps the point it drew on the last frame, as a tick number with a
fraction, and moves it on each frame (`FrameBlend::Advance`,
`Ecs/Publish/FrameBlend.h`, the one place that computes it):

```text
Drawn      = clamp(DrawnLastFrame + (FrameNs - LastFrameNs) / len, N - 1, N)
BlendAlpha = Drawn - (N - 1)
```

`len` is the length of `BlendTo`'s window: the tick length, from the published
stamp, so no rate is assumed. No other time of the simulation goes in. While
the ticks arrive one tick length apart the point moves at the clock's speed and
the motion is continuous; it never passes `BlendTo` (no extrapolation) and never
falls behind `BlendFrom`. The same rule runs in every exec mode; only when a
tick arrives differs: in Direct and Inline the frame in which the tick ran, in
Threaded the first frame after the game thread published it.

The drawn state is always between the latest two published ticks: at most one
tick behind the latest published tick, less when the ticks come on time. In
Direct and Inline the latest published tick is the newest tick that ran; in
Threaded it is the newest tick the game thread published before the frame's
`ShowLatest`. No margin is added, and the frame side never waits
for the simulation or delays itself because of it: the frame cap is the only
thing that paces frames. Presentation only: no tick, input cut, hash or
checksum depends on the blend. The look camera is not blended: it is the
sealed view of `Latest` plus the input pending after its cut, newer than
either tick (a view that follows a simulation entity must use the entity's
blended transform, or it judders against it). Edit does not blend: the frame
shows the latest state, so an edit shows at once.

Interpolation off (`--interpolation off`, `[Engine] Interpolation`, or the GPU
Options window) is a frame-side flag only (`World::SetInterpolation`,
`FrameBlend::SetInterpolation`): `BlendFrom` is `BlendTo` and the factor is 1,
so the frame draws the latest published state, and the drawn point is there.
Turned on again, the frames hold at that tick until the point reaches it. No
tick reads the flag: the tick hashes and checksums are the same either way
(GraphicsTests "Interpolation off: every frame draws the latest tick, and the
ticks do not change", and the CTests `Sandbox: systems move the lighting test
<mode> interpolation off`).

| Case | `BlendFrom`, `BlendTo`, `BlendAlpha` |
| --- | --- |
| On time: each tick arrives one tick length after the one before | Tick N - 1 and tick N; the point moves by each frame's time over the tick length. Even motion |
| No new tick (a late tick, a pause, the end of the run) | The same two ticks; the point stops at `BlendTo` (factor 1), so the frames repeat. No extrapolation |
| A late tick arrives | The point goes on from where the frames held, by one frame's time: no jump and no extra repeated frame. The next tick, on time, arrives less than a tick length later; the point then moves up to its `BlendFrom`: a skip of the part of the late tick the hold did not draw (at most one tick) |
| Several new ticks at once (a renderer slower than the simulation, a wake that ran several ticks) | Tick N - 1 and tick N. The point moves on by the frame's time; if that leaves it before tick N - 1, it moves up to tick N - 1: a skip of the ticks it did not draw, never a faster blend. A renderer that is steadily slower draws between the two ticks at the clock's speed |
| The first frame of a play session (a new link) with two ticks | The point starts at `BlendFrom` |
| One tick (the start of play), a tick that ran on request (no clock window), an empty window, interpolation off | `BlendTo`, factor 1 |
| Edit, a runner with no presentation link | The latest state, factor 1 |

Tests (all able to fail): `FlingTests/Engine/EcsBlendTests.cpp` (each packet
carries the latest two ticks; after skipped packets `BlendFrom` is still the
tick before `BlendTo`; "Blend factor: ..." at 30, 60 and 120 Hz: the point
moves by the frame time and stays between the two ticks, many frames on the
same two ticks, a late tick, a wake that runs two ticks, a renderer slower than
the simulation), `LookCameraTests.cpp` (the drawn object is between the latest
two ticks while the camera has the input after the latest cut, in one frame),
and `FlingTests/Graphics/InterpolationTests.cpp` (even motion in every exec
mode; the packet path draws what the direct path draws for the same arrivals,
also after Edit ticks; a late game thread; a stalled game thread does not stop
the frames).

## 4. How input is assigned to ticks

`TickInputProvider` (`Core/inc/Input/TickInputProvider.h`) is the one rule.
The engine loop and every test rig use it. `Build(TickInput&, const
TickWindow&)` does this for each tick:

1. **Replay with cuts.** If the playing file has a cut for this tick
   (`InputPlayback::ClaimTickCut`), the tick takes exactly the messages up to
   that cut, whatever their time. Messages that the cut needs and the playback
   clock has not sent yet are sent first.
2. **Time rule.** Otherwise a `TickTrigger::Clock` tick takes, in sequence
   order, the messages that no earlier tick took with `TimeNs < Window.EndNs`.
   The walk stops at the first message at or after the end (times never
   decrease in the queue), so nothing is reordered, dropped or counted twice. A
   message at exactly `EndNs` belongs to the next tick. A message that comes in
   after its tick ran goes to the next tick that runs.
3. **On request.** A `TickTrigger::Request` tick takes every queued message.
4. The cut goes to the recorder (`InputRecorder::NoteTickCut`).

UI capture comes from the stream: each message carries the capture that applied
when it was written (`InputMessage::Capture`), so the tick and the frame drop
the same messages.

### The live input rule

The main thread is the only input writer, in every exec mode. After each poll
it publishes a watermark (`InputMessageHandler::GetWatermark`): every message
before that time is in the queue. A timed tick on the game thread can wait for
`watermark >= Window.EndNs`, as `TickInputProviderConfig::WatermarkWaitNs`
says.

| Run | Wait | What is guaranteed |
| --- | --- | --- |
| `direct`, `inline` | none: the tick runs on the writer's thread, after the poll | The time rule exactly. |
| Threaded headless (`--null-render`) | no limit (`WatermarkWaitNs = -1`) | The same cuts as `direct`. |
| Any replay (`--replay-input`) | no limit | The recorded cuts, or the time rule for a file without cuts: the same as `direct`. |
| Threaded live windowed play | none (`WatermarkWaitNs = 0`) | Each cut is in the ledger and in a recording. A cut can be short of the time rule; the rest goes to the next tick (it is before that tick's end too), so no message is lost or counted twice. A replay of the recorded cuts gives the same ledger in every mode. |

Not guaranteed without a recording: the cuts of live threaded play depend on
when the main thread polls (the message times are poll times) and on a poll
that is in progress when a tick starts. A wait cannot remove the first, and it
would cost up to a frame of latency per tick, because the watermark moves only
once per poll. So live play does not wait, and reproducibility comes from the
recording. Test: "Exec modes: live play that does not wait for input replays
exactly from its recorded cuts" (`FlingTests/Engine/ThreadedWorldTests.cpp`).

The cost of the time rule: input polled after the end of the last due window
goes to the next tick (up to one tick of latency in the real loop). The frame
side still shows it as pending, so the late look is not affected.

## 5. The tick ledger

The runner writes one `TickRecord` at the end of each tick:

| Field | Meaning | Same at any frame rate |
| --- | --- | --- |
| `Stamp` | Tick, length, sim time, window, cut, state hash | All but the window: yes |
| `RanAtNs` | The clock when the tick started to run | no |
| `DroppedSteps` | Tick lengths of time skipped right before this tick | no |
| `MouseSamples`, `KeyEvents` | The counts of the tick's `InputSlice` | yes |

`TickLedger` is `TickHistory<TickRecord, 256>`. `TickHistory<T, N>`
(`Core/inc/TickHistory.h`) is a ring keyed by tick: slot `tick % N`, `Push`
in tick order, O(1) `Find(tick)` (null for a tick that is not kept, and for
tick 0), `Latest()`, `At(i)`. It is not in the ECS, so
no fixed system can read its wall-clock fields. The frame gets the latest stamp
through `RealFrameTime::Latest`; tools get the records through
`World::TakeNewTickRecords` (a queue that the tick thread fills, after
`World::KeepTickRecords`).

## 6. The state hash and rollback classes

`HashSimulationState(storage, types)` (`Ecs/StateHash.h`) is a 64-bit hash of
the simulation state after a tick. It covers **exactly the restore set**:

- Types for which `IsRollbackState` is true: `Domain == Simulation`,
  `RollbackClass::Restore`, and not a queue. `Transient` does not matter.
- For a Table type, each entity that has it, in entity order (index, then
  generation), never memory order. For a singleton, the one object. For a Tag,
  which entities have it.
- Each reflected field, `Transient` or not, by its value, never a padding
  byte, so two equal states hash the same: numbers, enums, `vec2` and `vec4`
  as their bytes (bit-exact: `-0` and `+0` differ), a `vec3` as its x, y and z
  and a quaternion as its x, y, z and w (never their bytes: GLM stores a
  quaternion as w, x, y, z under MSVC and clang-cl), a `bool` as 0 or 1, a `Name` as its
  hash, a `String` as its text, an `Entity` as index and generation, a Custom
  field as its saved JSON (NaN and infinity both save as `null`). The gaps
  between fields are never read.
- Types in `TypeHash` order.

The hash is the same on every run of one build on one platform; it reads
native byte order, so it is not compared across platforms.

It leaves out Presentation types, `Rebuild`, `Keep` and `Subsystem` types,
and queues. Two checks keep the restore set whole:

- `TypeRegistry::Build` reports an error for a `Restore` type with bytes that no
  reflected field covers (`Partial()` does not excuse it). A member that a level
  must not save is a `Transient()` field: not saved, still hashed and restored.
- `World::Init` reports an error, and the world does not start, when a system's
  access list names a `Restore` type that is not registered. A `Restore` type
  that only game code adds, and that nobody registers, is seen by neither.

Nothing random reaches the hashed state at play start. The play snapshot
(`World::SaveToJson`) changes nothing in the storage: an entity with no
`EntityGuid` gets a new id in the document only. `EntityGuid` is `Keep`: it
names an entity for files, no system reads it, and the hash does not cover it.
Only a level file save (`World::OutputLevelFile`, an edit) keeps the new ids on
the entities.

A type that cannot be hashed (a handle into a subsystem, scratch memory) takes
another `RollbackClass`.

It costs about 0.7 ms per tick in a Debug Sandbox (754 transforms: 1200 frames
took 2.51 s with it, 1.70 s without). It is on for `--null-render` and
`--dump-ticks`, off in a windowed game (`World::SetStateHashEnabled`).

Each component says what a rollback does with it (`ComponentInfo::Rollback`,
`Ecs/Component.h`). Left out, it is `Restore`. A `Transient` Simulation type
that is not a queue must set it, or it does not compile.

| `RollbackClass` | Meaning | Examples |
| --- | --- | --- |
| `Restore` | Simulation state: saved per tick, restored before a resimulation, hashed | `Transform`, `LookCamera`, `ContactEvents`, `SandboxToggles`, `SandboxSpawnState`, `SandboxPhysicsState` |
| `Rebuild` | Made again each tick from other state | `InputState`, `SimTime`, `Children`, `LocalToWorld` |
| `Keep` | A rollback leaves it alone | `TransformWork`, `PhysicsSettings`, `EntityGuid` |
| `Subsystem` | A module owns it and must save and restore it itself | `PhysicsWorld`, `PhysicsBody` |

Tests: "Tick time: the state hash covers exactly what a rollback restores",
"Tick time: the state hash reads values, not the padding of a vec3" and "Tick
time: a restored type with members the hash cannot see is an error"
(`FlingTests/Engine/TickTimeTests.cpp`), "A world whose systems use restored
state that is not registered does not start"
(`FlingTests/Engine/WorldSystemTests.cpp`), "A save changes nothing in the
storage: new ids go into the document only"
(`FlingTests/Engine/EcsReflectionTests.cpp`), and the CTests `Sandbox: tick
dump level Level_1` and `... PhysicsTest` (section 7).

## 7. `--dump-ticks` and how to diff two runs

`--dump-ticks <file>` writes one line per tick:

```text
tick=4 dt=33333333 elapsed=133333332 cut=3 samples=1 keys=0 hash=7cadf7229e3241b5
```

(The values are an example: the format is from `Engine::DumpNewTicks`.)

`dt` and `elapsed` are `Stamp.FixedDeltaNs` and `Stamp.ElapsedNs` (a rate
change shows as a new `dt`), `cut` is `Stamp.Cut.LastSequence`, `samples` and
`keys` are the input counts, `hash` is the state hash. To check that a change does not change the
simulation, run the same input twice and compare the files byte for byte:

```bash
Sandbox --null-render --unattended --seed 1 --frames 120 \
    --replay-input Sandbox/Tests/Input/look_camera_cuts.input.jsonl --dump-ticks a.txt
Sandbox --null-render --unattended --seed 1 --frames 360 --frames-per-tick 3 \
    --replay-input Sandbox/Tests/Input/look_camera_cuts.input.jsonl --dump-ticks b.txt --exec inline
cmp a.txt b.txt
```

The ctests do this: `Sandbox: tick dump replay by cuts`, `... by time`, `... by
cuts 60 Hz` and `... by time 60 Hz` (`Sandbox/Tests/CompareTickDumps.cmake`,
two frame patterns in each of the three exec modes), and `Sandbox: tick dump
exec modes cuts`, `... cuts 60 Hz`, `... time`, `... jitter`, `... stasis`
(`Sandbox/Tests/CompareExecModes.cmake`, every exec mode, frame caps and thread
jitter; `stasis` replays `stasis.input.jsonl`, a press of G with F and mouse
moves inside the stasis, for 150 ticks). Each sets `--exec` and `--sim-hz`, and each fails unless its reference
dump has mouse samples and more than one state hash (a dead input path would
also give equal dumps). `Sandbox: tick dump level Default`, `... Level_1` and
`... PhysicsTest` load the start level or a level with entities (`--level`, no
input) and compare two direct runs, inline and threaded: nothing random reaches
the hashed state at load or play start.

With the Jolt provider the SHA-256 of the direct dump is pinned too
(`-DEXPECT_SHA256`, in `Sandbox/CMakeLists.txt`): the three 300-tick level dumps
and the 600-tick `... time` replay of `look_camera.input.jsonl`. So a build
configuration or compiler that simulates differently fails a ctest, and the
failure prints the dump's last tick. MSVC Debug gives the pinned values. MSVC
RelWithDebInfo and clang-cl Debug gave the same dumps as MSVC Debug for the
pins before the stasis change; for the current pins that was reported, not
rerun independently. A deliberate change to the simulation
updates the pins: the test prints the new SHA-256.

Limit: `cut` is a raw queue sequence. Device connect and disconnect messages
take sequences too, so a run and the replay of its recording can differ in that
column while samples, keys and hash agree.

## 8. Hooks for rollback networking

### What exists

| Hook | Where | What it gives |
| --- | --- | --- |
| Tick identity | `TickStamp` | One key per tick for the ledger, the packet and the frame. |
| Per-tick ring | `TickHistory<T, N>` | The type for a state or input history keyed by tick. The ledger is one. |
| Resim groups | `GroupInfo::Resim` on Simulation, Physics, ResolveDeferred; `Schedule::IsResim`, `Schedule::ResimRunOrder()` | What a resimulation runs, as one unbroken slice of the fixed order (`ResimGap`). |
| Resim inputs | `Schedule::ResimInputs` | What resimulated systems read that only systems outside the resimulation make. Sandbox: `InputState` and `SimTime` (from the runner). |
| State rules | `ResimSkipsState` | A schedule error for a fixed system outside the resimulation that writes a `Restore` type, or that runs after the resimulated systems and writes a type that one of them reads. |
| State set | `RollbackClass`, `IsRollbackState`, the state hash | The restore set and a check of it, one rule for both. |
| Tick-stamped events | `TickOutboxEntry {Tick, Requests}`; `ClockCommand::BeforeTick` | The main thread drops an outbox entry whose tick is not newer than the last applied one, so a replayed tick does not repeat its requests. |
| Full-state packet | `TickPacket` | Each packet is the whole published state, so a corrected state after a resimulation reaches the frame in one apply, with no redesign. |
| Recorded input | `InputRecording::TickCuts` and `TickCutRate`, `InputPlayback::ClaimTickCut` | Each tick's input can be replayed exactly, at the rate it was recorded at. A replay at another rate is refused (`CheckTickCutRate`). |

### What is not built

| Missing | Where it would attach |
| --- | --- |
| State snapshots | A `TickHistory<Snapshot, N>` next to the ledger, keyed by the tick it was taken after. The snapshot set is `IsRollbackState`. `Subsystem` types need a save and restore hook per module (`Physics::SaveState` and `RestoreState` exist, but are not registered as such). The entity allocator (free list, generations) is not in any snapshot yet. |
| A resimulation runner | A `SystemRunner` function called inside `World::RunWake` on the tick thread: restore the snapshot after tick K - 1; for each tick t in K..N write `SimTime` (`SimTimeOf` ledger row t) and `InputState` from history, run `ResimRunOrder()`, flush, rebuild `Rebuild` types, hash, overwrite ledger row t. It needs a tick budget of its own, apart from `MaxCatchUp`. Publish nothing during it. |
| Remote input | A per-tick, per-player command history (`TickHistory`), that the runner writes into `InputState` instead of the local stream. `InputState` has no player dimension today. A late remote command marks "resimulate from that tick". |
| Predicted against authoritative compare | A per-type compare on `TypeDesc`, run on the tick thread against the history for the tick that the server sent. The state hash is for same-build desync detection, not for this. |
| Time sync | `TickTimeline`: a step change at the next window (time dilation) and a retarget of the local tick to the server tick plus a lead. `SimTime::FixedDeltaNs` must not change with dilation. |
| Presentation correction | An epoch in the packet header, read by a frame system that smooths the visible correction. Not added until it has a reader. |

Known gaps in what exists: `F_LOG_TRACE` in `Sandbox.FirstHit` and
`Sandbox.TestSpawn` would repeat; ticks in editor Edit mode share the play tick
numbers.
