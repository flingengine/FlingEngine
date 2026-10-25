# Improve input plan

Design for how Fling reads input: input devices produce timestamped messages
into a shared queue, and each consumer (the game simulation, the renderer, UI,
the recorder) reads that queue at its own pace. Later, a dedicated input thread
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

- The old `Input` (`FlingEngine/Core/inc/Input/Input.h`) is a static class with a
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
- Text input and IME. The queue carries `Char` events for them, but the text
  system is designed separately.

## Design

### Overview

```
                   ┌──────────────────────────── consumers (each has its own cursor) ──┐
 Input thread      │                                                                   │
 ────────────      │  Game side:   InputMessageProcessor ─► contexts ─► TickInput      │
 IInputDevice-     │  Render side: InputMessageProcessor ─► late camera look, UI hover │
  Interface ─┐     │  Recorder:    InputMessageProcessor ─► event log file             │
  (GLFW,     ├─► InputMessageHandler ─► InputMessageQueue                             │
   Virtual)  │     │  Editor/UI:   InputMessageProcessor ─► ImGui                      │
 InputDevice ┘     └───────────────────────────────────────────────────────────────────┘
```

- **Devices** (`InputDevice`) are the keyboards, mice, and gamepads. An
  **interface** (`IInputDeviceInterface`) is a backend, for example GLFW. It
  finds devices, makes them through the `InputDeviceFactory`, and pushes their
  input. See [Devices](#devices) and [Interfaces](#interfaces).
- Every `InputMessage` goes through one entry point,
  `InputMessageHandler::HandleMessage`. It sets the sequence and the time and
  pushes the message into one `InputMessageQueue`. All interfaces go through
  **one input thread**, so the queue is in time order (see
  [Ordering](#ordering)).
- **Consumers** each hold an `InputMessageProcessor` with their own cursor and
  their own `InputDeviceStates`. A consumer reads ("pumps") when it wants and
  moves its cursor ("consumes") as far as it wants. Reading never removes a
  message for another consumer.
- **Ownership** is decided on the game side by an input context stack (see
  [Input contexts](#input-contexts)), not by the queue.
- `Input` (a `Singleton`, Core) is the game-facing API. It owns the handler and
  the game's processor.

The headers are in `FlingEngine/Core/inc/Input/`. They are the source of
truth for names and signatures. The code blocks below are short excerpts.

### Devices

An `InputDevice` is one human interface device: one keyboard, one mouse, one
gamepad. The `InputDeviceFactory` makes it. The interface that owns it calls
its `Push*` functions (`PushKey`, `PushRepeat`, `PushChar`, `PushAxis`,
`PushMouseMove`, `PushWheel`, `PushFocus`). Each one builds an `InputMessage`
and sends it to the handler. A push on a device that is not connected does
nothing.

```cpp
struct InputDeviceId { uint32 Value = 0; };   // opaque. 0 is not valid

struct InputDeviceInfo
{
    InputDeviceType Type;                      // Keyboard, Mouse, Gamepad, Touch, Virtual
    std::string DisplayName;                   // for UI
    uint16 VendorId, ProductId;                // 0 if not known
    std::string HardwarePath;                  // GUID, device path, "glfw:keyboard"
    InputDeviceCapabilities Capabilities;      // flags
};
```

- **`InputDeviceId` is opaque** (`uint32`). It stays the same when the same
  device disconnects and connects again, so recordings still match.
- **Identity** of a device is the interface name, the type, the vendor ID, the
  product ID, and the hardware path. The interface **name** (for example
  "GLFW", "Virtual"), not the instance, is used. If an interface is destroyed
  and made again, its devices get their old IDs.
- **`InputDeviceInfo`** is metadata for UI, for example the name to show and
  the vendor and product IDs for glyph sets.
- **`InputDeviceCapabilities`** are flags: `Keys`, `MouseButtons`, `Pointer`,
  `RelativeMotion`, `Wheel`, `Buttons`, `Thumbsticks`, `Triggers`, `Haptics`,
  `Lighting`, `Gyro`, `Accelerometer`, `Touchpad`. UI uses them, for example to
  show gamepad glyphs only when a pad is connected.
- **`InputDeviceFactory`** is the registry. `CreateDevice(info, interface)`
  makes a connected device, or connects again a disconnected device with the
  same identity. `DisconnectDevice(id)` releases the held keys, then
  disconnects. A disconnected device **stays in the registry**, with an
  invalid owner, so it keeps its ID. `Find`, `GetDevices`, and
  `GetConnectedDevices` list it.
- **Connect and disconnect signals are deferred.** The factory sends a
  `DeviceConnected` or `DeviceDisconnected` message at once, so every consumer
  sees it in order. The `OnDeviceConnected` and `OnDeviceDisconnected` signals
  (`entt::sink`) only fire when `InputMessageHandler::DispatchDeferredEvents`
  runs. Game code then never runs from inside a device callback.

### Input keys

An `InputKey` is a platform-agnostic name for one button or axis on a device.

```cpp
InputKey key = Keys::W;
key.GetName();                         // "W". Files and code use this
key.GetDisplayName();                  // for UI
key.IsAxis();  key.IsGamepadKey();     // from InputKeyFlags
InputKey::FromName("LeftShift");       // invalid key if the name is unknown
```

- The built-in keys are in `Keys::` (`Keys::W`, `Keys::MouseLeft`,
  `Keys::GamepadLeftX`, ...). They come from one list,
  `InputKeyList.inl`, with an X-macro `FLING_INPUT_KEY(Id, DisplayName,
  Flags)`. `InputKeyId`, the `Keys::` constants, and the static
  `InputKeyDetails` table are all made from it. To add a key, add one line.
- **Files store keys by name**, never by ID. The ID order can change between
  builds. A recording works on every platform.
- **Keyboard keys name the physical key position** on a US layout, not the
  character it types. `Keys::W` is the key above `S`, also on AZERTY. This is
  how gameplay binds to the scancode position (Rule 6). The OS scancode is
  still in `InputMessage::Scancode` when the backend has one. Text uses `Char`.
- `InputKeyFlags` are `Keyboard`, `Mouse`, `Gamepad`, `Axis`, `Modifier`.

**`InputKeyState`** is the state of one key on one device, as one consumer
sees it:

```cpp
struct InputKeyState
{
    bool IsHeld;
    uint16 PressCount, ReleaseCount, RepeatCount;   // edges since the last ClearEdges
    float Value;                                    // axis value, or 1 / 0 for a button
    int64 LastChangeTimeNs;
};
```

- The counts are per device and per consumer. A press and a release between
  two reads give `PressCount 1, ReleaseCount 1, IsHeld false`.
- A repeat is counted in `RepeatCount`. It is not a press.
- `InputDeviceStates` keeps one `InputDeviceState` per `InputDeviceId`. Each
  has a map from `InputKey` to `InputKeyState`, the summed mouse delta in
  integer counts, the pointer position, the summed wheel, and `Connected`.
  `Apply(message)` updates it. `ClearEdges()` resets the counts and sums but
  keeps held state.

### Input messages

```cpp
enum class InputMessageType : uint8
{
    KeyDown, KeyUp,
    KeyRepeat,          // OS key repeat. UI and text readers only, never gameplay
    Char,               // Text input (a Unicode code point). UI and text readers only
    Axis,               // One sample of one axis (stick, trigger, any analog key)
    MouseMove,          // Relative delta in integer counts, and absolute cursor position
    MouseWheel,
    DeviceConnected, DeviceDisconnected,
    FocusLost, FocusGained,
};

struct InputMessage
{
    uint64 Sequence;    // Increases by 1 for each message. Unique for the session.
    int64 TimeNs;       // InputNowNs() when the message was received. Never decreases
    InputDeviceId Device;   // Stable across reconnects (see Local players)
    InputMessageType Type;
    InputKey Key;
    uint32 Scancode, CodePoint;
    float Value;                  // Axis
    int32 DeltaX, DeltaY;         // MouseMove, integer counts
    float PositionX, PositionY;   // MouseMove, window coordinates
    float WheelX, WheelY;
};
```

- Buttons (keys, mouse buttons, gamepad buttons) use `KeyDown` and `KeyUp`
  with an `InputKey`. There are no separate mouse or gamepad button types.
- `Sequence` lets a consumer say exactly which messages it has used. The
  render side uses this to avoid counting the same mouse movement twice (see
  [Render-side input](#render-side-input)).
- **Key messages carry an `InputKey` and the scancode.** Gameplay binds to
  `InputKey` values, which name the physical key position, so WASD stays in the
  same place on AZERTY and other layouts. UI shortcuts and text use `Char`.
- **Mouse deltas are integer counts**, not floats. Sums are then exact in any
  order, on the game side and the render side. Sensitivity is applied after
  the sum.
- **Mouse moves are merged.** An interface pushes at most one `MouseMove` per
  poll, with the summed delta. A 1–8 kHz mouse would otherwise fill the queue
  in seconds.
- On `FocusLost`, the interface adds release events for every held key and
  button, so no key stays held.

### Interfaces

An `IInputDeviceInterface` is one input backend: GLFW, XInput, GameInput, a
Win32 message pump, or a virtual device.

```cpp
class IInputDeviceInterface
{
public:
    virtual const char* GetName() const = 0;               // same in every session
    virtual void Init(InputMessageHandler& handler) = 0;   // create the devices that exist
    virtual void Shutdown() = 0;                           // handler disconnects its devices after
    virtual bool WantsPoll(int64 nowNs) const { return true; }
    virtual void Poll(int64 nowNs) = 0;
    virtual void OnWindowCreated(WindowHandle window) {}
    virtual void OnWindowDestroyed(WindowHandle window) {}
};
```

| Interface | Source | Notes |
|-----------|--------|-------|
| `GlfwInputDeviceInterface` (Graphics) | GLFW key, char, mouse button, cursor, scroll, focus callbacks, and gamepad state | Replaces both `WindowsInput` and `LinuxInput`. Uses callbacks, not `glfwGetKey`, so short presses are not lost |
| `VirtualInputDeviceInterface` (Core) | The command layer, agent scripts, tests | `CreateDevice` and `Queue(message)`. `Poll` sends the queued messages in order. Works with `--null-render` |
| `EventLogPlayer` (future) | An event log file | Plays a recorded message stream back in real time, as an interface |

- **Lifecycle.** `Init` is called once, when the interface is added. The
  interface creates its devices through the factory. `Shutdown` is called once
  before it is destroyed.
- **Polling.** `WantsPoll(nowNs)` lets a polled device (a gamepad) poll at its
  own rate. `Poll(nowNs)` reads input and pushes it to the interface's devices.
- **Runtime add and remove.** `Input::AddInterface<T>(args...)` returns an
  `InputInterfaceId`. `RemoveInterface(id)` first releases every held key of
  the interface's devices (`KeyUp` from the latest state), then disconnects
  them, then calls `Shutdown`, then destroys the interface. Tests, replays,
  and the command layer can start and stop the virtual interface at any time.
- **Deferred apply during polling.** An add or remove asked for during
  `PollInterfaces`, or from inside a device callback, is queued. It is applied
  at the end of `PollInterfaces`, so the interface list never changes while it
  is iterated.
- **Window handle.** The window is passed as an opaque `WindowHandle` (for GLFW,
  the `GLFWwindow*`). The handler keeps the current window. It calls
  `OnWindowCreated` on an interface that is added after the window exists, so
  a late interface still gets the window. `InputDevice::GetWindow` says which
  window a keyboard or mouse reports for.
- **`GlfwInputDeviceInterface::Poll` is the only place `glfwPollEvents` is
  called.** It was in `DesktopWindow::Update`, and the renderer pumped input.
  That pump also delivers window events (close, framebuffer resize), so the
  GLFW interface **lives as long as the GLFW window**: it is added when the
  window is created and removed before the window is destroyed. `Input::Poll()`
  runs before `VulkanApp::Update` in `Engine::Tick`, so `ShouldClose` and
  resize see this frame's events.
- **`InputMessageHandler` is the single entry point.** Every message, from
  every device, goes through `HandleMessage`.

### Ordering

`Read` by time and the `TickInput` cut at `LastSequence` both assume that
sequence order is time order. Several sources break that if they push
directly: OS events, pad polls, `VirtualInputDeviceInterface` with chosen
timestamps, and later device timestamps from Raw Input. So:

- Only the input thread calls `HandleMessage`. Other sources (virtual devices,
  playback, a later Raw Input thread) hand messages to it through a queue.
  `VirtualInputDeviceInterface::Queue` does this today.
- `HandleMessage` sets `Sequence`, and sets `TimeNs = max(TimeNs,
  lastTimeNs)`, so time never goes back. A `TimeNs` of 0 means "now".

### The message queue

```cpp
/** One producer thread, many readers. Push never waits. */
class InputMessageQueue
{
public:
    explicit InputMessageQueue(uint32 capacity = 4096);
    void Push(const InputMessage& message);     // InputMessageHandler only
    uint64 NewestSequence() const;
    uint64 OldestSequence() const;
    uint32 CopyRange(uint64 afterSequence, InputMessage* out, uint32 max, int64 untilNs) const;
};

class InputMessageProcessor
{
public:
    uint32 Read(InputMessage* out, uint32 max, int64 untilNs) const;   // does not move the cursor
    void Consume(uint64 sequence);          // move the cursor, apply to GetState()
    uint32 ConsumeUntil(int64 untilNs);     // Read + Consume
    void ClearEdges();                      // reset counts and mouse sums, keep held state
    const InputDeviceStates& GetState() const;
    bool Overran() const;                   // the queue overwrote unread messages
    void Resync(const InputDeviceStates& latest, uint64 latestSequence);
};
```

- The queue has a fixed size. `Push` overwrites the oldest message and never
  waits for a slow reader.
- **`Read` copies.** A view into the queue (`std::span`) can wrap, and the
  writer can overwrite it while the reader uses it. That works on one thread
  and tears with two. When the queue moves to its own thread, each slot gets a
  sequence number, and the reader checks it again after the copy (a seqlock).
  A changed number means the slot was overwritten: the reader reports
  `Overran()`. The reader API does not change.
- **Each processor owns its `InputDeviceStates`.** It applies messages as it
  consumes them. The handler cannot know the state at some other processor's
  cursor. Edge counts mean "since this processor last called `ClearEdges`", so
  a 30 Hz game reader sees every press made between its ticks, and a 144 Hz
  render reader keeps its own counts.
- **Overrun recovery.** The processor takes `InputMessageHandler::GetLatestState()`
  and its sequence, and calls `Resync`. Resync makes press and release
  messages from the difference between the old state and the new one
  (`InputDeviceStates::MakeTransitionMessages`), then moves the cursor. Held
  keys are then correct, even though short taps in the lost range are gone.
  With merged mouse moves, 4096 messages is several seconds of input, so this
  only happens on a long hitch. `Input::BeginTick` does this for the game
  processor.
- A new processor starts after the newest message, with the newest device
  state (`InputMessageHandler::CreateProcessor`).
- On one thread (today) the queue is a plain array. The API is the same.

### The input thread and polling

Keyboard and mouse are event devices. The OS queues their events, so they are
not lost if they are read late. Fixed-rate polling adds nothing for them:
waking up on each event already gives the best timestamps. Gamepads are
polled: the interface reads a state snapshot, and a press and release between
two polls is lost. So the input thread:

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
  GLFW interface drops it.
- **ImGui's GLFW backend** sets the cursor shape and reads the clipboard
  through GLFW, so it must run on the main thread. ImGui on another thread
  needs a custom platform backend that queues those requests to the main
  thread.

**Timestamps.** Stamping in the callback is accurate to about 1 ms when the
main thread is not busy. Device timestamps from Windows are not much better:
`GetMessageTime` uses the system tick (about 15.6 ms). If more accuracy is
needed later, the backend changes to a Raw Input message-only window on its
own thread for the mouse, and GameInput or XInput for pads. That thread hands
messages to the handler, behind the same queue.

**Today**, with one thread, the engine calls
`Input::Poll()` once per frame before `RunFrame`. Messages
still go through callbacks, so presses are not lost, but timestamps are only
accurate to the frame. Consumers do not change when input moves to its
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
- **Late arrivals.** With a separate input thread, an event stamped before the end
  of tick n can arrive after tick n was built. It goes to
  `max(tickByTime, firstUnbuiltTick)`. An event never goes to a tick that was
  already built.
- **Dropped ticks under the hitch threshold** (`MaxTicksPerFrame` clamp):
  their presses and releases go into the next tick that runs.
- **A hitch over the threshold** (0.1 s): presses and releases in the gap are
  dropped, but held state is kept from the processor's `InputDeviceStates`. A key held
  through a breakpoint is still held, and a press from a second ago does not
  fire now.
- **Pause.** While paused (`ScaleQ16` is 0), real time cannot be mapped to a
  tick. Gameplay edges are dropped. UI readers still get every event. On
  resume, held state comes from the processor's `InputDeviceStates`.
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

- **Event log** (raw `InputMessage`s with timestamps): good for finding bugs in
  the `TickInput` builder, the render-side look, and UI. Playback is in real
  time, so it is not exact.
- **`TickInput` log**: exact. This is what scenario tests and the state hash
  use. It includes context changes and UI commands (through `TickCommands`).
- The recorder is one more reader. Recording can be turned on at any time.

### Headless

With `--null-render` there is no window and no `GlfwInputDeviceInterface`. Input comes
from `VirtualInputDeviceInterface`, or directly from a `TickInput` log. Headless
scenario tests feed `TickInput`s and skip timestamps entirely, so they do not
depend on real time.

## Rules

1. Every input goes through `InputMessageQueue`, reaching it only through
   `InputMessageHandler::HandleMessage` on the input thread. No code reads GLFW
   or platform input state directly.
2. Every message has a sequence number and a real-time timestamp that never
   decreases.
3. The simulation reads only `InputState`, built from one `TickInput` per
   player per tick, after the input context stack.
4. The render side can use input for look rotation and UI only. It never
   writes sim state, and it computes the late look from the latest snapshot
   every frame.
5. Gameplay checks press counts, not only held state.
6. Gameplay binds to `InputKey` values (physical key positions) and ignores `KeyRepeat` and `Char`.
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
  `Overran()`, and its held state matches the handler's latest state after recovery.
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
| **I1** | **In progress.** Device layer (`InputDevice`, `InputDeviceFactory`, `InputDeviceId`), `InputKey`, `InputMessage` (with scancodes, `KeyRepeat`, `Char`, integer mouse counts), `InputMessageQueue` with copying `Read`, `InputMessageHandler`, `InputMessageProcessor` with its own `InputDeviceStates`. `GlfwInputDeviceInterface` with callbacks replaces `WindowsInput` and `LinuxInput`. `VirtualInputDeviceInterface`. The old static `Input` is removed, with no shim. One thread. See [Phase I1 status](#phase-i1-status) | `PlatformTime` from `improve-time.md` T1 (not yet; `InputNowNs()` is used) |
| **I2** | `TickInput` builder with edge counts and the tick assignment rules (late arrival, hitch, pause). `InputState` | `improve-time.md` T3 |
| **I3** | Event log and `TickInput` log record and replay (`VirtualInputDeviceInterface` exists from I1) | I2. Phase B in `improve-ai-iteration.md` |
| **I4** | Gamepad support, polled by deadline at `input.poll_hz`. Raw mouse motion and cursor capture requests | I1 |
| **I5** | Input context stack, with ImGui capture flags. UI commands into `TickCommands`. Systems read `InputState`. ImGui and the editor get their own reader | I2, `ecs-plan.md` systems |
| **Later** | Main thread becomes the input thread. Render-side late look and UI hover. Local player slots. Chords and input buffers. Rumble. A Raw Input or GameInput backend if timestamps need to be better | Game and render threads from `improve-time.md` |

### Phase I1 status

In progress. The device layer is implemented in Core, with Catch2 tests in
`FlingTests/Core/`. The GLFW interface is in Graphics. The rules that it
establishes move to `Skills/` when I1 is done.

Done:

- **Device layer.** `InputDevice`, `InputDeviceId` (opaque `uint32`),
  `InputDeviceInfo`, `InputDeviceCapabilities`, `InputDeviceFactory` (registry
  that keeps disconnected devices, and reuses the ID on reconnect), and the
  deferred connect and disconnect signals (`DispatchDeferredEvents`).
- **`InputKey`.** Built-in `Keys::` list from `InputKeyList.inl`, stored by
  name, with `InputKeyState` (held, press, release, and repeat counts) per
  device per consumer in `InputDeviceStates`.
- **`InputMessage`** with `InputDeviceId` as the device field, and
  `InputMessageType`.
- **`InputMessageQueue`, `InputMessageHandler`, and
  `InputMessageProcessor`** on one thread. Many processors read at their own
  rate. `Read` copies. Overrun is detected and `Resync` recovers.
- **Interfaces.** `IInputDeviceInterface` with runtime add and remove
  (deferred during polling) and window handle passing.
  `GlfwInputDeviceInterface` uses GLFW callbacks, and its `Poll` is the only
  `glfwPollEvents` call. `VirtualInputDeviceInterface` works with
  `--null-render`.
- **Old `Input` removed.** The static `Input`, `Key`, `KeyState`,
  `WindowsInput`, and `LinuxInput` are gone. `Input` is now a `Singleton` that
  owns the handler and the game processor (`Poll`, `BeginTick`, `EndTick`,
  `IsHeld`, `WasPressed`, ...). `FirstPersonCamera` and `SandboxGame` use it.
  `Input::BindKeyPress` delegates are gone. Callers check `WasPressed`.

Not done:

- **Timestamps.** `PlatformTime` (`improve-time.md` T1) does not exist.
  Messages use `InputNowNs()`, a `steady_clock` helper in
  `InputMessage.h`. Replace it when T1 lands.
- **Scancode binding.** Gameplay binds to `InputKey` values, which name the
  physical key position. The scancode field is filled when the backend has
  one, but nothing reads it yet.
- **Threading.** The queue is a plain array with no seqlock. There is no input
  thread. The main thread polls once per frame.
- **Gamepads, raw mouse motion, and cursor capture requests** are I4.
  `Poll` reads GLFW gamepad state with `glfwGetGamepadState` once per frame, not at `input.poll_hz` (I4).
- `TickInput`, tick assignment, and `InputState` are I2. The input context
  stack is I5. Event log record and replay is I3.

## Open questions

- Does Fling keep GLFW for input, or move to SDL3, which supports more
  gamepads? SDL3 has the same main-thread rule, so it does not change the
  thread design.
- `InputMessage`, `InputMessageQueue`, and the device layer live in Core (no
  window dependency). `GlfwInputDeviceInterface` lives in Graphics. Should
  future platform interfaces (XInput, GameInput) go in Graphics too, or in a
  new platform module?
- Action mapping: hard-coded per game at first, or a config file from the
  start?
- Should the render-side late look be on by default, or opt-in per camera?
- Chords: delay single-button actions, or let the chord cancel them? This can
  be per game.
