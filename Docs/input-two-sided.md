# Two-sided input: one stream, a fixed reader and a frame reader

The simulation reads input once per fixed tick. The screen is drawn once per
frame, often more often than the tick, and always after some input that no tick
has seen yet. This page explains how Fling lets the frame side show that input
early, with no loss, no double count, and no state of its own.

Related pages: which tick takes which message, and the live input rule, are in
[`time-and-ticks.md`](time-and-ticks.md). How the cut reaches the frame side
on another thread is in [`threading-model.md`](threading-model.md). The order
API that places the systems is in [`system-order-api.md`](system-order-api.md).
Record and replay is in [`Skills/input-playback.md`](../Skills/input-playback.md).

## 1. One stream, two readers

All input is one queue of messages in `InputMessageHandler`. Each message has a
`Sequence`, a `TimeNs` on the platform clock, the UI capture that applied when
it was written (`InputMessage::Capture`), and its data. The main thread is the
only writer. Readers do not share a cursor.

| Reader | Type | Cursor | Consumes | Runs |
| --- | --- | --- | --- | --- |
| The fixed tick | `TickInputBuilder`, called by `TickInputProvider` (`Core/inc/Input/TickInput.h`, `TickInputProvider.h`) | Its own `InputMessageProcessor` | Yes | Once per fixed tick, on the tick's thread |
| The frame side | `PendingInputReader` (`Core/inc/Input/PendingInputReader.h`) | None | No | Once per frame, on the main thread |

The fixed reader is the only one that changes simulation state. It takes the
messages that no earlier tick took, up to the tick's limit (the window end, or
a recorded cut). A frame with no tick leaves its messages for the next tick
that runs.

The frame reader starts at a sequence that it is given, every time. It never
hides a message from the tick, and it never holds a position of its own.

Both readers build the same type, `InputSlice`, with the same function,
`InputSlice::Append`, and drop the same captured messages, because the capture
is in the stream.

## 2. The fixed reader and the cut

`TickInputProvider::Build` fills one `TickInput` for each tick:

| Field | What it holds |
| --- | --- |
| `Slice.Cut` | The `InputCut` `(AfterSequence, LastSequence]` this tick consumed |
| `Slice.MouseSamples` | Each mouse move (`InputMouseSample`: `Sequence`, `DeltaX`, `DeltaY`), in sequence order |
| `Slice.KeyEvents` | Each key press and release (`InputKeyEvent`: `Sequence`, `Key`, `Down`), in sequence order |
| `Slice.Overran` | The queue overwrote messages before this tick took them (see "Overrun" below) |
| `Keys` | Merged state per key (`TickKeyState`): held and axis value from the device state at the cut; press and release counts from `Slice.KeyEvents`, so `Keys` and `Slice.WasPressed` are one rule |
| `Capture` | The UI capture at the cut |

`Slice.MouseDeltaX()` and `MouseDeltaY()` are sums computed from the samples.
Nothing in `TickInput` is a wall-clock time, so a fixed system that reads it
cannot depend on one.

The runner (`SystemRunner::RunOneTick`) builds the tick input straight into the
`InputState` singleton, before any fixed system of the tick runs. No system can
write `InputState`, and the system context has no input of its own, so a fixed
system gets input only by declaring `Read<InputState>`. The runner puts the cut into the tick's `TickStamp`
(`TickRecord::Stamp.Cut`). The published packet carries that stamp, and the
frame gets it as `RealFrameTime::Latest`. So the cut always reaches the frame
side with the state of the same tick, in every exec mode.

## 3. The frame reader

Once per frame, before the frame systems run, the runner
(`SystemRunner::RunFrameAt`) fills the `PendingInput` singleton with every
message after the cut of the tick that the frame shows:

```cpp
// FlingEngine/Engine/src/Ecs/SystemRunner.cpp, RunFrameAt
PendingInput& pending = m_FrameStorage->Singleton<PendingInput>();
if (shown.Tick > 0 && m_PendingInputSource)
{
    m_PendingInputSource(shown.Cut.LastSequence, pending);
}
else
{
    pending.Clear();
    pending.Cut = InputCut{shown.Cut.LastSequence, shown.Cut.LastSequence};
}
```

`PendingInput` derives from `InputSlice`. It is a Presentation component, so a
fixed system that reads it does not compile (`FixedReadsPendingInput`). No
system can write it (`WritesPendingInput`), and the context has no reader of
the queue (`ReadsPendingInput`): a frame system reads it with
`Read<PendingInput>`.

Several ticks in one frame, or none, need nothing special: the cut moves or
stays. Messages that a tick has consumed but that are not in the shown packet
yet are still pending: correct, nothing is counted twice.

### Early and late poll

Each frame polls input twice (`Core/inc/Input/Input.h`):

1. `Input::Poll`, at the start of the frame.
2. `Input::PollLate`, after the frame's wake and right before the frame
   systems. `World::SetLateInputPoll` holds the call; `Engine.cpp` sets it.

Without the late poll, on a frame that runs a tick, the tick takes everything
up to its own poll and nothing is pending. With it, the input that came in
while the ticks ran is pending for that frame. The tick reader is unchanged: a
late-polled message is consumed once, in order, by a later tick.

Measured when it was built (a flag to turn the late poll off existed then and
was later removed). In the test rig with one tick per frame, pending samples on
tick frames went from 0 to 12. Headless Sandbox, 120 frames,
`look_camera.input.jsonl`:

| Frames per tick | Pending samples on tick frames (late / none) | All frames (late / none) | Simulation per tick |
| --- | --- | --- | --- |
| 1 | 9 / 0 | 9 / 0 | identical |
| 2 | 8 / 0 | 17 / 8 | identical |
| 3 | 0 / 0 | 17 / 8 | identical |

## 4. Presentation keeps no state

The rule that makes this safe:

> The frame side shows the sealed simulation state run through the pending input
> by the same pure function that the simulation uses. It keeps nothing between frames.

So each frame computes `shown = Rule(sealed state, pending input)` from scratch.
When a tick consumes the input, the input is in the sealed state and no longer
pending, so nothing is counted twice. Input is not lost, because the tick
reader consumes every message, unless the queue overruns (below). The tests
check both at every frame, bit for bit ("Late look: no input is lost and none
is counted twice", `FlingTests/SandboxExample/LookCameraTests.cpp`).

### Overrun

The queue is a ring of 4 096 messages that never waits for a reader. If the
writer gets that far ahead of the tick reader (a long stop at a breakpoint, a
headless main thread far ahead of a slow game thread, a replay that sends a
burst), the oldest messages are gone before a tick takes them. Both sides then
follow one rule: drop what was in the queue.

- The tick goes on from the newest state (`TickInputBuilder::Resync`): held
  keys are right, and the presses and moves in the lost range are gone. No
  press or release count comes from the resync, because the counts come from
  `Slice.KeyEvents` only. `Slice.Overran` is set, and the ledger marks the tick
  (`TickRecord::InputOverran`, `input-overran` in `--dump-ticks`).
- The frame side shows no pending input up to the newest message
  (`PendingInputReader`), so it never shows input that no tick applies.
- A replay that overruns stops with an error (`InputPlayback::NoteTickOverran`),
  and `--replay-input` exits with `1`: the simulation is no longer the
  recording's.

Test: "After an overrun the tick and the frame side drop the same messages"
(`FlingTests/Core/TickInputTests.cpp`).

### Pause

Input that arrives while the ticks are paused (`World::PauseTicks`) belongs to
the first tick after the resume. The timeline moves its windows by the length
of the pause, and the messages' times are before the end of that tick's window,
so that one tick takes all of them, presses included. While paused, the frame
side shows them as pending, so the look camera turns although the simulation
does not run, and it does not snap back on the resume. A pause longer than the
queue holds overruns it (above).

## 5. Fold per sample

The first version applied the sum of a tick's mouse counts once. The frame side
and the next tick then agreed, but the tick result depended on how the input
was split into ticks:

- Pitch 1.4, clamp 1.5, up 30 then down 30: one tick gives **1.4**; split over
  two ticks it gives 1.5, then **1.2**.
- 25 moves over 100 ms with the real loop (poll once per frame), start pitch
  1.2: summed, the final pitch was **1.1001** at 30 fps, **0.9885** at 60 and
  240 fps, **0.9420** at 144 fps. Per sample it was **0.942000** at every rate,
  bit-identical.
- With no clamp (yaw) the summed rule still differs in the last bits, because
  the float sum per tick rounds differently.

Rule: **both sides apply each sample in sequence order.** Then
`Look(Look(v, a), b) == Look(v, a + b)` exactly, so the simulation is the same
for every split and the frame shows what the simulation will make. This is why
the slice carries `MouseSamples`. The summed rule is gone from the code.

This is only as frame-rate independent as the messages. Interfaces merge mouse
moves per poll, and a poll is per frame, so with a real mouse the samples
themselves depend on the frame rate.

## 6. Discrete edges at their place in the stream

A key press that changes how input is applied (freeze, a sensitivity change)
must take effect at its own place among the mouse samples, not at the start of
the tick. `ForEachInOrder(samples, keys, onMouse, onKey)`
(`Core/inc/Input/InputSlice.h`) walks the two lists in sequence order.

Measured, with yaw at -0.10, then move 5, press F (freeze), move 7:

| Rule | Frame before the tick | Tick result | Frame after the tick |
| --- | --- | --- | --- |
| Policy applied to the whole tick | -0.22 | -0.10 (all 12 counts dropped) | -0.10 (a snap of 0.12 rad) |
| Edge applied at its place | -0.15 | -0.15 | -0.15 |

A seeded script of 90 events, split into ticks ten ways and with 0, 1 or 2
frames per event, gives bit-identical cameras in all 30 runs, and at every frame
the view equals the rule run over the whole stream so far (test "Late look: key
edges inside ticks give the same camera for every split").

## 7. How the simulation keeps control

The frame side has no rules of its own. The simulation steers it through data
in a simulation component, which the frame side treats as plain data:

- The policy (`LookPolicy`: `LookScale`, `Frozen`, `PitchMin`, `PitchMax`)
  lives in the sealed component (`LookCamera::Policy`).
- A policy change that comes from input is part of the pure rule, so both sides
  apply it at the key edge.
- A policy change that does not come from input (a cutscene, a stun) is made by
  a fixed system that writes the component. The frame side sees it from the
  first frame after that tick. That system must run **after** the system that
  folds the input. Before it, the tick drops input that the frame already showed:
  in the test a snap of 0.1 rad (test "Late look: a freeze that does not come
  from input is shown from the next sealed state").
- A second writer of the component must be ordered against the first. The
  schedule reports `WriteConflict` until it is, and the fix names
  `SystemOf<Sandbox::LookCameraSystem>`.
- The Sandbox example is stasis (`Sandbox.Stasis`, the G key). After the
  system that folds the input, nothing that a frame showed is taken back when
  each frame shows the newest tick. A frame that runs between a tick's cut and
  its publish (threaded play) still shows the input in that gap under the old
  policy. The tutorial explains it, with the tests:
  [`tutorial-systems-and-camera.md`](tutorial-systems-and-camera.md), sections 7 and 10.

## 8. What the frame side can and cannot predict

| Decision | Predicted? | Why |
| --- | --- | --- |
| A pure function of input and the sealed component | Yes, exactly | The same function runs on both sides. |
| A key rule that also needs other state ("F freezes, but not in a vehicle") | Only if that state is copied into the sealed component as plain data | Otherwise the frame shows it wrongly for up to a tick. |
| A decision not driven by input (cutscene, another player's stun) | No | It reaches the frame side through the sealed state, up to one tick late. |
| Input after the late poll | No | It waits for the next frame. |

## 9. Worked example: the look camera

Sandbox code, compiled into the `SandboxExampleTests` test executable as well (see `FlingTests/CMakeLists.txt`):

| File | What |
| --- | --- |
| `Sandbox/Gameplay/inc/LookCamera.h` | `LookView`, `LookPolicy`, `LookCamera {LookView View; LookPolicy Policy;}` (Simulation, `RollbackClass::Restore`) |
| `Sandbox/Gameplay/inc/LookCameraType.h` | `Sandbox::RegisterLookCameraType` (the reflected fields) |
| `Sandbox/Gameplay/inc/LookUtil.h` | The pure rule, `Run` (its steps are in `LookUtil::Detail`) |
| `Sandbox/Gameplay/inc/Systems/LookCameraSystem.h`, `src/Systems/LookCameraSystem.cpp` | `Sandbox.LookCamera`, fixed, in `Order::Simulation` |
| `Sandbox/Gameplay/inc/Systems/LookCameraViewSystem.h`, `src/Systems/LookCameraViewSystem.cpp` | `Sandbox.LookCameraView`, in `Order::Presentation` |
| `FlingEngine/Engine/inc/Components/CameraView.h` | `CameraView {Active, Yaw, Pitch}`, a Presentation singleton: the view the renderer draws with |
| `Sandbox/Gameplay/src/SandboxGame.cpp` | `Game::OnStartGame` makes the `LookCamera` entity, unless the level has one |

The one rule both sides call, from `LookUtil.h` (comment trimmed):

```cpp
inline LookCamera Run(LookCamera t_Camera, const Fling::InputSlice& t_Input)
{
    const std::span<const Fling::InputMouseSample> Samples = t_Input.MouseSamples;
    std::size_t RunStart = 0;
    std::size_t Next = 0;
    Fling::ForEachInOrder(
        Samples,
        t_Input.KeyEvents,
        [&](const Fling::InputMouseSample&) { ++Next; },
        [&](const Fling::InputKeyEvent& t_Key)
        {
            LookPolicy Changed = t_Camera.Policy;
            if (Detail::ApplyKey(Changed, t_Key))
            {
                t_Camera.View =
                    Detail::Look(t_Camera.View, Samples.subspan(RunStart, Next - RunStart), t_Camera.Policy);
                RunStart = Next;
                t_Camera.Policy = Changed;
            }
        });
    t_Camera.View = Detail::Look(t_Camera.View, Samples.subspan(RunStart), t_Camera.Policy);
    return t_Camera;
}
```

The fixed system (`LookCameraSystem.cpp`):

```cpp
void LookCameraSystem::FixedUpdate(Context& ctx) const
{
    const Fling::InputSlice& input = ctx.Singleton<const Fling::InputState>().Tick.Slice;
    for (auto [entity, camera] : ctx.Query<LookCamera>())
    {
        camera = LookUtil::Run(camera, input);
    }
}
```

The frame system (`LookCameraViewSystem.cpp`). Its header has no `.After`, and
needs none: the runner fills `PendingInput` before any frame system runs.

```cpp
void LookCameraViewSystem::Update(Context& ctx) const
{
    const Fling::PendingInput& pending = ctx.Singleton<const Fling::PendingInput>();
    Fling::CameraView& view = ctx.Singleton<Fling::CameraView>();
    view.Active = false;
    for (auto [entity, camera] : ctx.Query<const LookCamera>())
    {
        const LookView shown = LookUtil::Run(camera, pending).View; // the same rule the tick runs
        view = Fling::CameraView{.Active = true, .Yaw = shown.Yaw, .Pitch = shown.Pitch};
    }
}
```

In Inline and Threaded play this query runs on the mirror: `LookCamera` is in
the published set because this frame system reads it. `CameraView` is made in
the frame storage for the system that writes it. The system source is the same
in every mode.

What the compiler stops: a fixed system that reads `PendingInput` or
`CameraView`, a frame system that writes `LookCamera`, any system that
writes `InputState` or `PendingInput`, and any system that asks its context
for raw input (it has none). The `Input` singleton has no key queries, so
there is no global input for a fixed system to read in any build. What a
debug build stops at run time: a fixed system that calls what `Input` still
has (the capture, the devices, a processor, the handler: the simulation
check, in every exec mode). What nothing stops: a frame system that
reads a Simulation component such as `InputState`. Reads are allowed, so
"read only the sealed component" is a convention.

The renderer (`VulkanApp::Update`) reads `CameraView` from the storage it
draws and, when `Active` is true, applies it with
`FirstPersonCamera::SetLookRotation`. With no writer, or with `Active` false,
it keeps its debug camera. `Game::OnStartGame` makes the `LookCamera` entity in
every play session, unless the level has one, so a windowed play run draws with
it: `Sandbox --play` (threaded by default; `--exec direct` runs the ticks on the
main thread).

## 10. Record and replay

- The recorder reads at `Input::Poll` only. A message from a late poll gets the
  next frame's time, which is the frame of the tick that consumed it. So a
  recording made with the late poll replays on the same ticks.
- Playback in the late poll (`InputPlayback::UpdateLate`) sends the messages
  that the next frame's `Update` would send if that frame has the same delta.
  Under `--null-render` every delta is equal, so the simulation is identical and
  the frame side sees the messages one frame earlier. With real deltas a message
  can go out one frame early, never late.
- A recording saves each tick's cut (`tickCuts`). A replay of it gives every
  tick exactly the same input at any frame rate and in every exec mode.
- A recording starts with the UI capture of its start, and a replay starts
  with no capture, whatever the live UI captures (Skills/input-playback.md).
- A replay that overruns the queue stops with an error (section 4, Overrun).
- `--frames-per-tick N` (with `--null-render`) runs one tick every N frames, so a
  headless run has frames between ticks.
- CTest `Sandbox: look camera replay` plays `Sandbox/Tests/Input/look_camera.input.jsonl`
  at 2 frames per tick and checks the checksum. What the frames show is tested
  in `LookCameraTests.cpp`, and the ledger ctests compare the camera's state
  hash across frame patterns.

## 11. Known limits

- **Input after the late poll** waits a frame: the frame systems, the render
  submit, GPU work and scan-out.
- **Mouse merging per poll.** Samples are poll-granular (two polls per frame).
  Exact frame-rate independence needs merging on a fixed clock on an input thread.
- **Before the first tick there is no cut.** `PendingInput` is empty until
  `RealFrameTime::Latest.Tick > 0`.
- **One camera.** `CameraView` is one view; with several `LookCamera` entities
  the last one in storage order wins.
- **The time rule adds latency**: input polled after the end of the last due
  window goes to the next tick (up to one tick). The frame side still shows it.
- **Live threaded play** takes what the main thread has polled when a tick runs
  (no watermark wait), so it is reproducible only from its recording (see
  [`time-and-ticks.md`](time-and-ticks.md)).
- **Cost.** `Run` runs every frame over the whole pending tail, up to the queue
  length after a long hitch. Not measured. The cost of the extra poll is not
  measured.
- **Each game writes its own `Run`.** There is no engine helper for a pure rule
  over the stream. `KeyRepeat` is not in `KeyEvents` or in `TickKeyState`: it
  is for UI and text only.
- **One world input.** `InputState` has no player or device mapping.
- **Live devices in a windowed replay.** A tick merges every device, so a real
  mouse or keyboard that moves during a windowed replay changes the replayed
  ticks. Keep hands off, or replay headless.
- **Keys held when a recording starts** are written as `KeyDown` at time 0, so
  the first tick of the replay sees a press that the live run did not.
