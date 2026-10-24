# Improve input plan

Design for how Fling reads input: devices produce timestamped events into a
shared ring, and each consumer (the game simulation, the renderer, UI, the
recorder) reads that ring at its own pace. Later, a dedicated input thread
polls devices at a fixed, high rate, so no press is lost between game ticks,
and the render thread can use the newest input for low-latency camera look
and UI hover.

This is a plan, not an established standard. When a phase is done, move the
rules that it establishes into [`Skills/`](../Skills/).

Related plans: [`improve-time.md`](improve-time.md) (`SimTick`,
`PresentationClock`, `TickInput`, threads),
[`improve-ai-iteration.md`](improve-ai-iteration.md) (record, replay, and
injected input), and [`ecs-plan.md`](ecs-plan.md) (`InputState`, the `Input`
phase).

## Problem

The current input code is naive and is not a base to build on. Expect a full
rewrite. Do not keep its API, its structure, or its behavior (for example,
key repeat counts as a new press) only because it exists. The list below shows
why it must be replaced; it is not a list of things to fix in place.

- `Input` (`FlingEngine/Core/inc/Input/Input.h`) is a static class with a
  virtual implementation and string-keyed maps.
- `WindowsInput::PollImpl` and `LinuxInput::PollImpl`
  (`FlingEngine/Graphics/src/Input/`) are almost the same code. Both read key
  **state** with `glfwGetKey` and `glfwGetMouseButton` once per frame. A key
  pressed and released between two polls is lost.
- Both treat `GLFW_REPEAT` as a press, so a held key can fire "pressed" more
  than once.
- Input has no timestamps. A consumer cannot tell when a press happened inside
  a frame.
- `Input::BindKeyPress` calls delegates from inside `Poll`. Gameplay side
  effects happen outside any system, at a time that depends on the frame.
- Gameplay and camera code read the global directly
  (`FirstPersonCamera.cpp`, `SandboxGame.cpp`). There is no way to inject,
  record, or replay input.
- Nothing decides who owns an input. A click on UI can also reach gameplay.
- There is no gamepad support.
- The platform implementation lives in Graphics, so input does not work with
  `--null-render`.

## Goals

1. No lost presses. Every press and release reaches the simulation, on the
   correct tick.
2. Every input event has a real-time timestamp.
3. More than one consumer reads the same input, and each decides when to read
   and how far.
4. The render thread can use the newest input for camera look and UI hover,
   with lower latency than the game tick, without the simulation and the
   image disagreeing.
5. Polled devices (gamepads) are read at a fixed rate that is higher than the
   tick rate and does not depend on it.
6. Recorded, replayed, and injected input use the same path as real devices.
7. The API stays the same when input moves to its own thread. Today
   everything runs on one thread.
8. Each input has one owner at a time (UI, editor, gameplay), so one press
   never does two things.

## Non-goals

- Input remapping UI, rebinding, and platform-specific devices (touch,
  motion, wheels). These build on this design later.
- Text input and IME. The ring carries `Char` events for them, but the text
  system is designed separately.

## Design

### Overview

```
                 ┌──────────────────────────── consumers (each has its own cursor) ─┐
 Input thread    │                                                                  │
 ────────────    │  Game side:   InputReader ─► contexts ─► TickInput ─► InputState │
 OS events  ─┐   │  Render side: InputReader ─► late camera look, UI hover         │
 Pad polling ─┼─► InputEventRing ─► Recorder:    InputReader ─► event log file     │
 Virtual dev ─┘   │  Editor/UI:   InputReader ─► ImGui                             │
                 └──────────────────────────────────────────────────────────────────┘
```

- **Producers** write `InputEvent`s into one `InputEventRing`. All sources go
  through **one producer thread**, so the ring is in time order (see
  [Ordering](#ordering)).
- **Consumers** each hold an `InputReader` with their own cursor. A consumer
  reads ("pumps") when it wants and moves its cursor ("pops") as far as it
  wants. Reading never removes an event for another consumer.
- **Ownership** is decided on the game side by an input context stack (see
  [Input contexts](#input-contexts)), not by the ring.

### Input events

```cpp
enum class InputEventType : uint8
{
    KeyDown, KeyUp,
    KeyRepeat,          // OS key repeat. UI and text readers only, never gameplay
    Char,               // Text input (a Unicode code point). UI and text readers only
    MouseButtonDown, MouseButtonUp,
    MouseMove,          // Relative delta in integer counts, and absolute cursor position
    MouseWheel,
    GamepadButtonDown, GamepadButtonUp,
    GamepadAxis,        // One sample of one axis, int16
    DeviceConnected, DeviceDisconnected,
    FocusLost, FocusGained,
};

struct InputEvent
{
    uint64 Sequence;    // Increases by 1 for each event. Unique for the session.
    int64 TimeNs;       // PlatformTime when the producer received the event. Never decreases
    uint16 DeviceId;    // Stable across reconnects (see Local players)
    InputEventType Type;
    // Payload: scancode and key code, button code, axis id and value,
    // mouse delta and position, code point, ...
};
```

- `Sequence` lets a consumer say exactly which events it has used. The render
  side uses this to avoid counting the same mouse movement twice (see
  [Render-side input](#render-side-input)).
- **Key events carry the scancode and the key code.** Gameplay binds to
  scancodes, so WASD stays in the same place on AZERTY and other layouts. UI
  shortcuts and text use the key code and `Char`.
- **Mouse deltas are integer counts**, not floats. Sums are then exact in any
  order, on the game side and the render side. Sensitivity is applied after
  the sum.
- **Mouse moves are merged.** The producer pushes at most one `MouseMove` per
  pump, with the summed delta. A 1–8 kHz mouse would otherwise fill the ring
  in seconds.
- On `FocusLost`, the producer adds release events for every held key and
  button, so no key stays held.

### Ordering

`Read` by time and the `TickInput` cut at `LastSequence` both assume that
sequence order is time order. Several sources break that if they push
directly: OS events, pad polls, `VirtualInputDevice` with chosen timestamps,
and later device timestamps from Raw Input. So:

- Only the producer thread calls `Push`. Other sources (virtual devices,
  playback, a later Raw Input thread) hand events to it through a queue.
- `Push` sets `TimeNs = max(TimeNs, lastTimeNs)`, so time never goes back.

### The event ring

```cpp
/** One producer thread, many readers. The producer never waits. */
class InputEventRing
{
public:
    explicit InputEventRing(uint32 capacity);   // for example 4096 events
    void Push(const InputEvent& event);          // producer thread only
    InputReader CreateReader();

    /** The producer's newest device state and the sequence it is valid at. For overrun recovery. */
    DeviceStateSnapshot LatestState() const;
};

class InputReader
{
public:
    /**
     * Copies events after the cursor with TimeNs < untilNs into out, up to max.
     * Does not move the cursor. Returns the number copied.
     */
    uint32 Read(InputEvent* out, uint32 max, int64 untilNs);

    /** Moves the cursor past every event up to and including sequence, and applies them to State(). */
    void Consume(uint64 sequence);

    /** True if the producer overwrote events that this reader did not read. */
    bool Overran() const;

    /** Held keys and buttons, axis values, and cursor position at this reader's cursor. */
    const DeviceState& State() const;
};
```

- The ring has a fixed size. The producer overwrites the oldest events and
  never waits for a slow reader.
- **`Read` copies.** A view into the ring (`std::span`) can wrap, and the
  producer can overwrite it while the reader uses it. That works on one thread
  and tears with two. Each slot has a sequence number, and the reader checks
  it again after the copy (a seqlock). A changed number means the slot was
  overwritten: the reader reports `Overran()`.
- **Each reader owns its `DeviceState`.** It applies events as it consumes
  them. The producer cannot know the state at some other reader's cursor.
- **Overrun recovery.** The reader takes `LatestState()` (a `{state,
  sequence}` pair), moves its cursor to that sequence, and makes press and
  release events from the difference between its old state and the new one.
  Held keys are then correct, even though short taps in the lost range are
  gone. With merged mouse moves, 4096 events is several seconds of input, so
  this only happens on a long hitch.
- On one thread (today) the ring is a plain array. The API is the same.

### Producers

| Producer | Source | Notes |
|----------|--------|-------|
| `GlfwInputProducer` | GLFW key, char, mouse button, cursor, scroll, focus callbacks, and gamepad state | Replaces both `WindowsInput` and `LinuxInput`. Uses callbacks, not `glfwGetKey`, so short presses are not lost |
| `VirtualInputDevice` | The command layer, agent scripts, tests | Hands events to the producer thread. Works with `--null-render` |
| `EventLogPlayer` | An event log file | Plays a recorded event stream back in real time, through the producer thread |

### The input thread and polling

Keyboard and mouse are event devices. The OS queues their events, so they are
not lost if they are read late. Fixed-rate polling adds nothing for them:
waking up on each event already gives the best timestamps. Gamepads are
polled: the producer reads a state snapshot, and a press and release between
two polls is lost. So the producer:

- **Wakes up on every OS event**, not once per frame, so event timestamps are
  accurate.
- **Polls gamepads by deadline** at `input.poll_hz` (default **250**, set in
  config). 120 Hz catches real taps at a 60 Hz tick, because human taps are
  rarely shorter than 30 ms, but it beats against the 125 Hz USB report rate
  of many pads. 250 Hz does not.
- Pushes only changes. With XInput, a pad's `dwPacketNumber` shows that
  nothing changed, so the compare is skipped.
- Polls **disconnected** XInput slots about once per second.
  `XInputGetState` on an empty slot is slow.

**The thread.** GLFW requires event processing and its joystick functions on
the main thread, and window creation must also be on the main thread on some
platforms. SDL3 has the same rule for video events. So **the input thread is
the main thread**, which is the normal design:

```
Main thread:   create window, then loop:
                 glfwWaitEventsTimeout(nextPadDeadline - now), poll pads if due, push events
Game thread:   RunFrame (ticks, extract)          -- started by the main thread
Render thread: render from SnapshotRing           -- started by the main thread
```

- `glfwWaitEventsTimeout` returns early on every event, so a fixed timeout
  gives an uneven pad rate. Wait until the next pad deadline instead.
- **Mouse look uses raw motion**: `GLFW_RAW_MOUSE_MOTION` with the cursor
  disabled. Without it, Windows gives OS-accelerated cursor movement.
- **Cursor capture** (disable, hide, release) changes only on the main
  thread. Other threads send a request through a command queue. GLFW reports
  one large false delta on the first motion after a capture change. The
  producer drops it.
- **ImGui's GLFW backend** sets the cursor shape and reads the clipboard
  through GLFW, so it must run on the main thread. ImGui on another thread
  needs a custom platform backend that queues those requests to the main
  thread.

**Timestamps.** Stamping in the callback is accurate to about 1 ms when the
main thread is not busy. Device timestamps from Windows are not much better:
`GetMessageTime` uses the system tick (about 15.6 ms). If more accuracy is
needed later, the backend changes to a Raw Input message-only window on its
own thread for the mouse, and GameInput or XInput for pads. That thread hands
events to the producer, behind the same ring.

**Today**, with one thread, the engine calls
`InputProducer::PollOnCallerThread()` once per frame before `RunFrame`. Events
still go through callbacks, so presses are not lost, but timestamps are only
accurate to the frame. Consumers do not change when the producer moves to its
own thread.

### Input contexts

Every reader sees every event, so the ring alone cannot stop a UI click from
also firing a weapon. Ownership is a game-side layer (Game Engine
Architecture 8.5.8 and 8.5.9: control ownership, and disabling input at the
consumer, not at the device):

- An **input context stack** with priorities, for example UI, editor
  viewport, and gameplay. A context claims the inputs it uses while it is
  active.
- The `TickInput` builder asks the stack. For anything another context has
  claimed, gameplay gets **neutral input**: not held, no edges, zero axes.
- When gameplay loses an input it was holding, the builder sends a release on
  that tick, so no action stays on.
- ImGui's `WantCaptureMouse` and `WantCaptureKeyboard`, and editor viewport
  focus, feed the stack.
- **UI clicks that change the game** (`ButtonPressed{id}`) are resolved by
  the code that owns the UI layout. They become commands, stored in
  `TickCommands`, so they happen on a tick and replay includes them.
- Which contexts are active is part of the recorded input, so a replay gives
  the same neutral input.

### Game-side consumer: `TickInput`

The simulation needs input that is the same on every replay, so it does not
read events directly. Before each tick, the engine builds one `TickInput` per
local player slot:

```cpp
struct TickInput
{
    SimTick Tick;
    uint64 LastSequence;       // The last event this tick used

    // Per key (by scancode), button, and action:
    // held state at the end of the tick, presses this tick, releases this tick.
    // Per axis: last sample in the tick, int16 (held from the previous tick if none).
    // Mouse: sum of relative deltas this tick in integer counts, cursor position at the end.
};
```

- **Edge counts** are the reason no press is lost. A press and release inside
  one tick give `presses = 1, releases = 1, held = false`. Gameplay checks
  `presses > 0`, not only `held`.
- Axes are int16 and mouse values are integer counts, so the same
  `TickInput` can go over the network later without change.
- The `Input` phase (once per live tick, see `ecs-plan.md`) turns `TickInput`
  into `InputState`. Dead zones, response curves, and action mapping are pure
  functions here, with Catch2 tests that do not need an engine.
- A recording of `TickInput`s is what deterministic replay uses
  (`improve-ai-iteration.md`). A resim uses the stored `TickCommands` and does
  not read events.
- `Input::BindKeyPress` delegates go away. A system reads the press count in
  `InputState` instead, at a fixed point in the tick.

**Assigning events to ticks.**

- **By time.** For tick n, the builder reads events with `TimeNs` before the
  real time of the end of tick n. That time comes from `PresentationClock` and
  `TickTimeline` in `improve-time.md`.
- **Late arrivals.** With a separate producer, an event stamped before the end
  of tick n can arrive after tick n was built. It goes to
  `max(tickByTime, firstUnbuiltTick)`. An event never goes to a tick that was
  already built.
- **Dropped ticks under the hitch threshold** (`MaxTicksPerFrame` clamp):
  their presses and releases go into the next tick that runs.
- **A hitch over the threshold** (0.1 s): presses and releases in the gap are
  dropped, but held state is kept from the reader's `DeviceState`. A key held
  through a breakpoint is still held, and a press from a second ago does not
  fire now.
- **Pause.** While paused (`ScaleQ16` is 0), real time cannot be mapped to a
  tick. Gameplay edges are dropped. UI readers still get every event. On
  resume, held state comes from the reader's `DeviceState`.
- **Single steps** (`RequestSteps` while paused) take every event up to now.
- **Re-anchoring.** Time scale and rate nudge changes are fine, because each
  `PresentationClock` anchor has a version number and only moves forward.

**Chords, sequences, and input buffers** (a combo, a jump pressed slightly
early) are pure functions over the `TickInput` history, measured in ticks,
not milliseconds (Game Engine Architecture 8.5.4). Their state is rollback
state and goes into `SimHistory` (`improve-time.md`). For a chord, either
delay single-button actions by 2–3 ticks, or let the chord cancel them.

### Local players

- Each player slot has its own `TickInput`.
- Which device belongs to which player is **sim state**. A tick-tagged
  command changes it, so replays and rollback get it right.
- `DeviceId` stays the same when a device reconnects, so recordings still
  match.
- When a player's device disconnects, the game pauses and asks the player to
  reconnect (Game Engine Architecture 8.5.5).

### Render-side input

Camera look is the input that players feel most. The render side can apply
mouse movement that the simulation has not seen yet. This is like VR "late
latching":

1. Each `RenderSnapshot` stores the `LastSequence` of the `TickInput` that
   made it (`LastInputSequence`), and the sim camera: position, the sim view
   (`SimView`, yaw and pitch), `LookScale`, and a `LateLook` flag.
2. Each render frame, the render side sums the raw mouse counts of look
   events after the **latest** snapshot's `LastInputSequence`, up to the
   present time.
3. It **computes the view from scratch** every frame:
   `view = Clamp(SimView + Look(pendingCounts, LookScale))`, using the same
   pure function as the simulation (`LookUtil`). It never adds to the
   previous frame's offset.
4. When a newer snapshot arrives, its `LastInputSequence` is higher. Those
   events are now in `SimView`, so the pending sum gets smaller. No movement
   is counted twice and none is lost, and any difference disappears with the
   next snapshot instead of building up.

Rules for this:

- **Why from scratch.** Pitch clamp, look acceleration, and aim-assist
  slowdown are not linear. Applying them one event at a time on the render
  side gives a different result from the simulation applying them to one sum
  per tick. Example: a flick up past the clamp and back down within one tick.
  Computing from the latest `SimView` with one clamp keeps the error to the
  pending input only.
- **Do not interpolate camera rotation** while late look is on. Interpolating
  between `prev` and `next` and then adding the events after
  `next.LastInputSequence` applies the events between `prev` and `next` only
  partly, and the look lags. Use the latest snapshot's `SimView` plus pending
  input. Interpolate only the camera **position**.
- **Only look rotation and UI.** The render side never changes sim state. The
  simulation still uses every look event on its own tick.
- **Movement stays in the simulation.** Moving the camera position on the
  render side would need collision and movement rules, and the image would
  disagree with the simulation when they differ. Interpolation between
  snapshots already makes movement smooth.
- **Sim-driven view changes** (recoil, a cutscene camera) are already in
  `SimView`, so they work without change. Aim-assist **slowdown** changes
  sensitivity from sim state, so the simulation writes it to `LookScale`.
  Aim-assist **pull** stays in the simulation only. A cutscene sets `LateLook`
  to false.
- **Hit checks.** The camera the player sees is about 1–2 ticks ahead of the
  world they see, because the world is drawn one tick behind. The simulation
  checks a hit with view(n) against world(n), but the player aimed at about
  world(n - 1.5). Locally this is small. For networked play it needs lag
  compensation: the client sends the tick it was drawing, and the server
  checks the hit against that tick.
- **UI hover** uses the newest cursor position from the render side's reader.
  UI clicks that change the game go to the game side as commands (see
  [Input contexts](#input-contexts)).
- **ImGui and the editor** get their input from their own reader. They do not
  need `InternalImGui::SetImGuiCallbacks` from inside the input
  implementation.

### Haptics

- Rumble is output, but it follows the same rules as audio in
  `improve-time.md`: the simulation writes a `RumbleEvent` tagged with its
  tick.
- A resim does not send a `RumbleEvent` again for a tick that already sent
  it.
- Events are scheduled at presentation time, and sent to the device from the
  thread that owns it (the main thread with GLFW).

### Recording

- **Event log** (raw `InputEvent`s with timestamps): good for finding bugs in
  the `TickInput` builder, the render-side look, and UI. Playback is in real
  time, so it is not exact.
- **`TickInput` log**: exact. This is what scenario tests and the state hash
  use. It includes context changes and UI commands (through `TickCommands`).
- The recorder is one more reader. Recording can be turned on at any time.

### Headless

With `--null-render` there is no window and no `GlfwInputProducer`. Input comes
from `VirtualInputDevice`, or directly from a `TickInput` log. Headless
scenario tests feed `TickInput`s and skip timestamps entirely, so they do not
depend on real time.

## Rules

1. Every input goes through `InputEventRing`, pushed only by the producer
   thread. No code reads GLFW or platform input state directly.
2. Every event has a sequence number and a real-time timestamp that never
   decreases.
3. The simulation reads only `InputState`, built from one `TickInput` per
   player per tick, after the input context stack.
4. The render side can use input for look rotation and UI only. It never
   writes sim state, and it computes the late look from the latest snapshot
   every frame.
5. Gameplay checks press counts, not only held state.
6. Gameplay binds to scancodes and ignores `KeyRepeat` and `Char`.
7. Mouse deltas stay integer counts until after they are summed.

## Tests

- **No lost presses:** a press and a release 2 ms apart, inside one tick,
  give one press and one release on that tick.
- **Tick assignment:** events just before and just after a tick boundary go
  to the correct ticks.
- **Late arrival:** an event stamped inside a tick that was already built goes
  to the next unbuilt tick.
- **Dropped ticks:** after a short hitch, the edges from dropped ticks are in
  the next tick that runs.
- **Long hitch:** after a gap over the threshold, edges in the gap are dropped
  and a key held through it is still held.
- **Pause:** presses while paused do not reach gameplay. A key held across
  resume is held on the first tick.
- **Several readers:** two readers at different speeds each see every event
  once.
- **Overrun:** a reader that falls behind by more than the ring size reports
  `Overran()`, and its held state matches the producer's after recovery.
- **Torn read:** with a second thread pushing, `Read` never returns an event
  that was overwritten during the copy.
- **Ordering:** an event pushed with an earlier timestamp than the last one
  gets the last timestamp.
- **Focus loss:** a held key gets a release event when the window loses focus.
- **Contexts:** a click claimed by UI gives neutral input to gameplay, and a
  gameplay key held when UI takes focus gets a release.
- **Key repeat:** a held key gives one press, no matter how many
  `KeyRepeat` events arrive.
- **Late look:** total rotation from the render side and the simulation, over
  many snapshots, equals the sum of all mouse deltas. Nothing is counted
  twice.
- **Late look clamp:** a flick past the pitch clamp and back inside one tick
  gives the same view on the render side and in the simulation after the
  next snapshot.
- **Gamepad polling:** a simulated pad with a 10 ms tap, polled at 250 Hz,
  gives one press and one release.

## Phases

Each phase must leave the engine, Sandbox, and FlingTests building and passing
on every CI platform.

| Phase | Work | Depends on |
|-------|------|------------|
| **I1** | `InputEvent` (with scancodes, `KeyRepeat`, `Char`, integer mouse counts), `InputEventRing` with copying `Read`, `InputReader` with its own `DeviceState`. `GlfwInputProducer` with callbacks replaces `WindowsInput` and `LinuxInput`. A temporary shim lets the old `Input::IsKeyHeld` callers read from a reader until I5 deletes them. Do not extend the shim. One thread | `PlatformTime` from `improve-time.md` T1 |
| **I2** | `TickInput` builder with edge counts and the tick assignment rules (late arrival, hitch, pause). `InputState` | `improve-time.md` T3 |
| **I3** | `VirtualInputDevice`, event log and `TickInput` log record and replay | I2. Phase B in `improve-ai-iteration.md` |
| **I4** | Gamepad support, polled by deadline at `input.poll_hz`. Raw mouse motion and cursor capture requests | I1 |
| **I5** | Input context stack, with ImGui capture flags. UI commands into `TickCommands`. Remove `Input::BindKeyPress` and the static `Input`. Systems read `InputState`. ImGui and the editor get their own reader | I2, `ecs-plan.md` systems |
| **Later** | Main thread becomes the input thread. Render-side late look and UI hover. Local player slots. Chords and input buffers. Rumble. A Raw Input or GameInput backend if timestamps need to be better | Game and render threads from `improve-time.md` |

## Open questions

- Does Fling keep GLFW for input, or move to SDL3, which supports more
  gamepads? SDL3 has the same main-thread rule, so it does not change the
  thread design.
- Where do `InputEvent` and `InputEventRing` live: Core (no window
  dependency) with producers in a platform module?
- Action mapping: hard-coded per game at first, or a config file from the
  start?
- Should the render-side late look be on by default, or opt-in per camera?
- Chords: delay single-button actions, or let the chord cancel them? This can
  be per game.
