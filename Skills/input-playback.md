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
| `--replay-input <file>` | Plays the file from the first tick. A file that cannot be read exits with code `3` |
| `--record-input <file>` | Records from the first tick, and saves the file when the engine exits |
| Editor: **Windows > Input Devices > Recording + Playback** | Start and stop a recording, with a start delay and a duration. Select a file and play it. The click on **Stop recording** is not saved |

Recording and playback are only in dev builds. Shipping builds
(`DEFINE_SHIPPING=ON`) compile them out with `WITH_INPUT_RECORDING=0`, and the two
flags exit with code `3` (see [`building.md`](building.md#define_shipping)).

Relative paths start at the project root (`FlingPaths::ProjectDir`). The editor
saves new recordings to `Saved/InputRecordings/`. The `Saved/` folder is not in
source control.

The playback clock moves by the frame time. Under `--null-render` each tick is
exactly 1/60 s, so a headless replay gives the same result every time. A message
at time `t` goes out on tick `floor(t * 60)` (counting from 0). The game sees it
on that same tick.

The recorder uses the same clock: a message gets the time of the frame that
read it. So a recording of a headless replay plays on the same ticks again.

Add `--log-input` to log each message as it goes through the input system.

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
{"format":"fling-input","version":1,"tickRate":60,"duration":3.0}
```

| Field | Required | Meaning |
| --- | --- | --- |
| `format` | yes | Always `"fling-input"` |
| `version` | yes | `1` |
| `tickRate` | no | Ticks per second for the `tick` fields. Default `60` |
| `duration` | no | Seconds. Playback lasts at least this long, so keys held at the end stay held until then. Default: the time of the last message |
| `devices` | no | A list of devices (see below). Recorded files always have it |

When playback ends, every playback device disconnects, and every key that is
still held is released.

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

## For agents

To test a change with input:

1. Write a `.input.jsonl` file (in the scratchpad, or under
   `Sandbox/Tests/Input/` if it becomes a test).
2. Run the game headless with `--replay-input <file>` and `--frames N`, with N
   large enough to cover the file (`duration * 60` ticks).
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
| `InputRecorder` | `Core/inc/Input/InputRecorder.h` | A message queue consumer that writes a recording |
| `InputPlayback` | `Core/inc/Input/InputPlayback.h` | Sends a recording through a `VirtualInputDeviceInterface` named `Playback` |
| `Input::GetRecorder`, `Input::GetPlayback` | `Core/inc/Input/Input.h` | The engine's recorder and playback. `Input::Poll(deltaSeconds)` updates them |

A recording holds raw input messages with times. It is not a per-tick
`TickInput` log, so playback with a window (frame times that change) is close
to the recording, but not exact. Exact per-tick replay is phase I3 in
[`Plans/improve-input.md`](../Plans/improve-input.md).
