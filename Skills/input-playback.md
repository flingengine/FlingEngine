# Input recording and playback

Fling can record the input message stream to a file and play it back through a
virtual device. Playback goes through the same path as a real keyboard, mouse or
gamepad, so the game, the camera, and ImGui all see it. Playback needs no window,
so it works with `--null-render`.

Use it to:

- repeat a bug with the same input every time
- drive a headless run from a script or an AI agent (write the file by hand)
- test that a key or button does what it should

## Run it

| How | What it does |
| --- | --- |
| `--replay-input <file>` | Plays the file from the first tick. A file that cannot be read exits with code `3`, and a replay that lost input (see "Limits") with code `1` |
| `--record-input <file>` | Records from the first tick, and saves the file when the engine exits |
| Editor: **Windows > Input Devices > Recording + Playback** | Start and stop a recording, with a start delay and a duration. Select a file and play it. The click on **Stop recording** is not saved |

Recording and playback are only in dev builds. Shipping builds
(`DEFINE_SHIPPING=ON`) compile them out with `WITH_INPUT_RECORDING=0`, and the two
flags exit with code `3` (see [`building.md`](building.md#define_shipping)).

Relative paths start at the project root (`FlingPaths::ProjectDir`). The editor
saves new recordings to `Saved/InputRecordings/`. The `Saved/` folder is not in
source control.

The playback clock moves by the frame time. Under `--null-render` each tick is
exactly one tick length (1/30 s at the default simulation rate; `--sim-hz` or
`[Engine] SimHz` sets it), so a headless replay gives the same result every
time. A message at time `t` goes out on tick `floor(t * rate)` (counting from
0). The game sees it on that same tick.

How a tick takes its input ([`Docs/time-and-ticks.md`](../Docs/time-and-ticks.md)
has the full rules, and `TickInputProvider` is the code):

- Each message has a time on the platform clock (`PlatformTime::NowNanoseconds`).
  Tick N covers a window `[start, start + 1/rate)` on that clock, and takes, in
  sequence order, the messages that no earlier tick took and whose time is before
  the end of its window. A message that comes in after its tick ran goes to the
  next tick. Under `--null-render` the clock is virtual: frame `f` polls at
  `(1 + f / N)` ticks and runs its ticks at `(1 + (f + 1) / N)` ticks
  (`--frames-per-tick N`), so tick 1 runs on frame 0, then one tick every N frames.
- A recording saves the input cut of each tick (the last line,
  `{"tickCuts":{"tickRate":30,"counts":[...]}}`: the simulation rate of the
  ticks, and for each tick, how many of the file's messages the ticks up to it
  consumed). A replay of such a file gives each tick exactly those messages
  (plus input from live devices, see "Limits" below),
  whatever the frame rate or the speed of the machine: if the clock has not sent
  a message that a tick needs yet, the tick sends it first. A file with no
  `tickCuts` line (hand-written, or recorded before cuts existed) replays by
  time, as above, at any rate. Playback stays connected until every recorded
  tick took its input.
- Cuts replay only at the rate they were recorded at. A cut belongs to a tick,
  so at another rate each tick would get the input of another time. A run at
  another rate does not start the replay: `--replay-input` exits with code `3`
  and the message names the flag to use, for example `its tick cuts were
  recorded at 60 ticks a second, and the simulation runs at 30. Replay it with
  --sim-hz 60`. The editor's playback shows the same message. A replay does not
  change the rate. While a replay plays or a recording runs (a file stores one
  rate), a rate change is refused when it is asked for (the GPU Options window,
  `World::RequestSimHz`), and a change that was asked for before the replay or
  recording started is dropped, with a log line, when the tick thread would
  apply it. `InputRecorder::SetTickRate` and `InputPlayback::SetTickRate`
  refuse a new rate while they are active. A version 1 file stored no rate: its
  cuts are read as 60 Hz, the only rate when it was made.
- `--dump-ticks <file>` writes one line per tick: the tick number, its input cut
  (the last sequence it consumed), its mouse and key message counts, the state
  hash, and `input-overran` at the end of a tick that lost input
  (`TickRecord::InputOverran`, see "Limits"). Two runs of one input file with
  different `--frames-per-tick`, or
  in different exec modes (`--exec direct`, `inline`, `threaded`), give the same
  file (CTests `Sandbox: tick dump replay by cuts`, `... by time`, and
  `Sandbox: tick dump exec modes cuts`, `... time`, `... jitter`).
- UI capture (ImGui owns the keyboard or the mouse) is a message in the stream
  (`CaptureChanged`), so it is recorded and replayed with the input. A recording
  starts with the capture at its start (also when nothing is captured). A replay
  starts with no capture: if the live UI captures when it starts, playback sends
  a `CaptureChanged` with no capture first. During a replay, live UI capture is
  ignored.
- With `--exec threaded`, the main thread still polls and plays the file, and
  the game thread takes the cuts. A tick on the game thread waits until the main
  thread has sent the messages that its recorded cut needs, so a replay is exact
  in every exec mode. Live threaded play (no file) does not wait; its cuts are
  in the recording.

The recorder uses the same clock: a message gets the time of the frame that
read it. So a recording of a headless replay plays on the same ticks again.

Add `--log-input` to log each message as it goes through the input system.

Add `--frames-per-tick N` (with `--null-render`) to run one fixed tick every N
frames. The playback clock then moves 1/(rate N) s per frame, so at `--sim-hz
60` a file with `"tickRate": 120` and `--frames-per-tick 2` puts a message on a
frame between two ticks. Frame systems that read `PendingInput` (the input that no tick has
consumed, which the runner fills each frame) see it on that frame. `--frames` still
counts frames. The Sandbox look camera test (CTest `Sandbox: look camera
replay`, `look_camera.input.jsonl`) uses it.

The engine polls input twice per frame: `Input::Poll` before the fixed ticks,
and `Input::PollLate` after them, right before the frame systems. So input that
comes in while the ticks run is pending for this frame. Playback assigns its
messages to the two polls like this: the late poll sends the messages that the
next frame's `Poll` would send if that frame has the same delta time
(`InputPlayback::UpdateLate`). Under `--null-render` every delta is the same,
so each message is consumed by the same tick as without the late poll, and a
replay gives the same simulation; only frame systems see the message one frame
earlier. With real frame times the next delta is a guess, so a message can go
out one frame early, never late. The recorder reads at `Poll` only, so a
message from a late poll gets the next frame's time, which is the frame of the
tick that consumed it, and a recording plays on the same ticks again.

```bash
./build/Sandbox/bin/Sandbox --null-render --unattended --frames 600 \
    --replay-input Sandbox/Tests/Input/escape_quits.input.jsonl --log-input
```

## Playback devices

Playback does not send input through your real keyboard, mouse or gamepad. It
connects its own devices, one for each device in the file, on an interface
named `Playback`:

- Real and played input never share a key state. A key you hold is not
  released by the file, and stopping playback releases only the played keys.
- Headless runs have no real devices, so playback works the same way there.
- A device connects again with the same `InputDeviceId` on the next playback
  (its identity is the interface name plus `playback:<file id>`), so the device
  list does not grow each time you play a file.

These devices are **simulated**, and " (Simulated)" is added to the end of their display names. The recorder removes it again, so a file keeps the original names. `InputDevice::IsSimulated()` is true for them,
and every message from them has `InputMessage::Simulated` set. The handler sets
the flag from the device, so an interface cannot change it. Any interface whose
`IInputDeviceInterface::IsSimulated()` returns true (every
`VirtualInputDeviceInterface`) makes simulated devices. The Input Devices window
shows this in the **Source** column, and `--log-input` lines show `[simulated]`.

## File format

The file is [JSON Lines](https://jsonlines.org/) with the extension
`.input.jsonl`: one JSON object on each line.

- The first line is the header.
- Each line after it is one message.
- Blank lines, and lines that start with `#`, are skipped. Use `#` for comments.
- Fields that the reader does not know are skipped.
- The reader sorts messages by time. Messages at the same time keep their order
  in the file.

### Header

```json
{"format":"fling-input","version":2,"tickRate":30,"duration":3.0}
```

| Field | Required | Meaning |
| --- | --- | --- |
| `format` | yes | Always `"fling-input"` |
| `version` | yes | `1` or `2`. The recorder writes `2`. The two differ only in the tick cuts line (below) |
| `tickRate` | no | Ticks per second for the `tick` fields of the messages. Default `60`: a file format default for hand-written files, not the simulation rate. The recorder writes the simulation rate here too, but only `tickCuts.tickRate` (below) is the rate the cuts replay at |
| `duration` | no | Seconds. Playback lasts at least this long, so keys held at the end stay held until then. Default: the time of the last message |
| `devices` | no | A list of devices (see below). Recorded files always have it |

When playback ends, every playback device disconnects, and every key that is
still held is released.

### Tick cuts

A recording ends with one line that holds the input cut of each recorded tick:

```json
{"tickCuts":{"tickRate":30,"counts":[0,2,2,5]}}
```

| Field | Meaning |
| --- | --- |
| `tickRate` | The simulation rate, in ticks a second, of the ticks the cuts were recorded on. The cuts replay only at this rate |
| `counts` | For each tick from the first recorded one, how many of the file's messages the ticks up to it consumed. Whole numbers that never decrease, at most the number of messages |

A version 1 file has the older form `{"tickCuts":[0,2,2,5]}`, with no rate. It
still loads, and its cuts are read as 60 Hz. Hand-written files usually have no
cuts line; they replay by time at any rate.

### Devices

```json
{"format":"fling-input","version":1,"devices":[
  {"id":1,"type":"Keyboard","name":"Keyboard"},
  {"id":2,"type":"Gamepad","name":"Xbox Controller","vendorId":1118,"productId":2834,"capabilities":["Buttons","Thumbsticks","Triggers"]}
]}
```

(The header must be on one line. It is split here only so you can read it.)

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | yes | A whole number, unique in the file. Messages refer to it |
| `type` | yes | `Keyboard`, `Mouse`, `Gamepad`, `Touch`, or `Virtual` |
| `name` | no | Display name |
| `vendorId`, `productId` | no | USB IDs |
| `path` | no | Hardware path |
| `capabilities` | no | Names from `InputDeviceCapabilities`: `Keys`, `MouseButtons`, `Pointer`, `RelativeMotion`, `Wheel`, `Buttons`, `Thumbsticks`, `Triggers`, `Haptics`, `Lighting`, `Gyro`, `Accelerometer`, `Touchpad`. Default: the usual set for the type |

You do not have to list devices. A message with no `device` gets one from its
key: keyboard keys go to a keyboard, mouse buttons and mouse movement to a
mouse, and gamepad buttons and axes to a gamepad. A message can also name a
type, for example `"device":"Gamepad"`. The reader uses the first listed device
of that type, or adds one.

### Messages

Each message has a time and a `type`:

| Field | Meaning |
| --- | --- |
| `t` | Seconds from the start. A number, 0 or more |
| `tick` | A whole number of ticks from the start (`t = tick / tickRate`). Use `t` or `tick`, not both |
| `type` | One of the types below |
| `device` | A device `id` from the header, or a device type name. Optional |

| `type` | Fields | Meaning |
| --- | --- | --- |
| `KeyDown` | `key`, `scancode` (optional) | Press a key or button |
| `KeyUp` | `key`, `scancode` (optional) | Release it |
| `Press` | `key`, `duration` or `ticks` (optional, default 0) | A `KeyDown` and, `duration` seconds later, a `KeyUp`. With 0, both happen on the same tick, and the game still sees one press |
| `KeyRepeat` | `key` | Operating system key repeat. UI only |
| `Char` | `codepoint` (a number) or `text` (a string) | Text input. `text` sends one `Char` for each character |
| `Axis` | `key`, `value` | Set an axis. Sticks are -1 to 1, triggers are 0 to 1 |
| `MouseMove` | `dx`, `dy` (whole numbers), `x`, `y` (optional) | Move the mouse. If `x` and `y` are not given, the pointer moves by `dx`, `dy` from where it was |
| `MouseWheel` | `wx`, `wy` | Scroll |
| `FocusLost`, `FocusGained` | none | The window lost or got focus |

`key` is an `InputKey` name. The full list is in
`FlingEngine/Core/inc/Input/InputKeyList.inl` (the first argument of each
`FLING_INPUT_KEY`). Common names:

- Keyboard: `A`-`Z`, `Zero`-`Nine`, `Space`, `Enter`, `Escape`, `Tab`,
  `LeftShift`, `LeftControl`, `Up`, `Down`, `Left`, `Right`, `F1`-`F12`
- Mouse: `MouseLeft`, `MouseRight`, `MouseMiddle`
- Gamepad buttons: `GamepadFaceBottom` (A), `GamepadFaceRight` (B),
  `GamepadFaceLeft` (X), `GamepadFaceTop` (Y), `GamepadLeftShoulder`,
  `GamepadRightShoulder`, `GamepadBack`, `GamepadStart`, `GamepadDPadUp`, ...
- Gamepad axes: `GamepadLeftX`, `GamepadLeftY`, `GamepadRightX`,
  `GamepadRightY`, `GamepadLeftTrigger`, `GamepadRightTrigger`

A key name that does not exist is an error, with its line number.

### Example

```
{"format":"fling-input","version":1,"duration":4.0}
# Walk forward for one second, then strafe left
{"t":0.0,"type":"KeyDown","key":"W"}
{"t":1.0,"type":"KeyUp","key":"W"}
{"t":1.0,"type":"Press","key":"A","duration":0.5}
# Look right with the gamepad stick for half a second
{"t":2.0,"type":"Axis","key":"GamepadRightX","value":1.0}
{"t":2.5,"type":"Axis","key":"GamepadRightX","value":0.0}
# Quit the Sandbox game
{"tick":200,"type":"Press","key":"Escape"}
```

## Limits

- **The input queue holds 4 096 messages.** If more than that go out before a
  tick takes them (a hand-written file with thousands of messages at one time,
  or a headless main thread far ahead of a slow game thread), a tick loses
  input. The replay then stops with an error in the log ("Replay failed: a tick
  lost input"), `--replay-input` exits with `1`, and `--dump-ticks` marks the
  tick `input-overran`. Spread a big file over more ticks.
- **Live devices are not muted.** A tick merges every device, so a real mouse,
  keyboard or gamepad that moves during a windowed replay changes the replayed
  ticks. Keep hands off, or replay headless.
- **Keys held when a recording starts** are written as `KeyDown` at time 0.
  The live run saw them held with no press; the first tick of the replay sees a
  press.

## For agents

To test a change with input:

1. Write a `.input.jsonl` file (in the scratchpad, or under
   `Sandbox/Tests/Input/` if it becomes a test).
2. Run the game headless with `--replay-input <file>` and `--frames N`, with N
   large enough to cover the file (`duration * rate` ticks; the rate is 30 a
   second unless `--sim-hz` says another).
3. Read the exit code and the log. `--log-input` shows each message that went
   through the input system, and `--log-format json` makes the log easy to
   parse.

To make a CTest test of it, add an `add_test` in `Sandbox/CMakeLists.txt` like
`Sandbox: replay input quits`, which plays `escape_quits.input.jsonl` and
checks the tick count where the game quit.

## Code

| Type | Where | Job |
| --- | --- | --- |
| `InputRecording` | `Core/inc/Input/InputRecording.h` | The file in memory. `ParseInputRecording`, `LoadInputRecording`, `WriteInputRecording`, `SaveInputRecording` |
| `InputRecorder` | `Core/inc/Input/InputRecorder.h` | A message queue consumer that writes a recording. `SetTickRate` is the rate it stores with the cuts (refused while it records) |
| `InputPlayback` | `Core/inc/Input/InputPlayback.h` | Sends a recording through a `VirtualInputDeviceInterface` named `Playback`. `ClaimTickCut` gives a tick its recorded cut. `SetTickRate` is the run's simulation rate (refused while it plays): `Start` refuses cuts made at another rate (`CheckTickCutRate`). `NoteTickOverran` stops a replay that lost input |
| `TickInputProvider` | `Core/inc/Input/TickInputProvider.h` | The one rule that gives a tick its input: the recorded cut, or the time rule; notes each cut to the recorder, and an overrun to the playback |
| `Input::GetRecorder`, `Input::GetPlayback` | `Core/inc/Input/Input.h` | The engine's recorder and playback. `Input::Poll(deltaSeconds)` updates them |

A recording holds raw input messages with times and, since the tick ledger, the
input cut of each tick (`tickCuts`). With the cuts, each tick of a replay
consumes the same messages as in the recording at any frame rate; a file without
them is close to the recording, but not exact, when frame times change. A stored
`TickInput` log for resimulation is phase I3 in
[`Plans/improve-input.md`](../Plans/improve-input.md).
