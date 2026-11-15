# Tutorial: systems, components, and a camera on the presentation side

This page is for an engine programmer who has not seen this code. It shows how
to write components and systems in Fling, and how a camera can turn on the
presentation side at the frame rate while the simulation keeps authority over
it.

The example is real Sandbox code: a mouse look camera, and **stasis**, a
gameplay effect that stops the camera for 2 seconds when the player presses G.
Each code block is copied from the file named above it. `// ...` marks a cut.
If this page and a file differ, the file is correct.

## 1. The model

The schedule has two roots. Every group and every system is under one of them.

| Root | Rate | Thread in threaded play | Systems implement | May read | May write |
| --- | --- | --- | --- | --- | --- |
| `Order::Tick` | Fixed: once per simulation tick (30 Hz by default) | The game thread | `FixedUpdate(Context&)` | Simulation components, `SimTime`, `InputState` | Simulation components |
| `Order::Frame` | Variable: once per rendered frame | The main thread | `Update(Context&)` | The published copy of Simulation components, `RealFrameTime`, `PendingInput` | Presentation components |

A system names its group with `.In`. The root of the group decides the rate,
and the rate decides the thread. No system names a thread.

The fixed side is deterministic. A fixed system reads no clock, no frame data
and no presentation state. It gets input only through `InputState`, which the
runner fills from the tick's cut of the input queue. After each tick, the
runtime copies the Simulation types that frame systems read into a packet (the
**published set**). The frame side reads that copy. It cannot change
simulation state.

The data flow for the camera:

```text
 main thread                                            game thread (fixed rate)
 -----------                                            ------------------------
 Input::Poll --> input queue (Sequence, TimeNs) ------> tick N takes its cut (After, Last]
                                                         runner writes SimTime, InputState
                                                         Input       Sandbox.AppKeys
                                                         Simulation  Sandbox.LookCamera: InputState -> LookCamera
                                                                     Sandbox.Stasis:     InputState -> Stasis, LookCamera
                                                         Physics, ResolveDeferred, Finalize
                                                                     |
                     publish: TickPacket {TickStamp with the cut, LookCamera, Stasis, ...}
                                                                     |
 Order::Frame (variable rate) <--------------------------------------+
   runner writes RealFrameTime, and PendingInput = the queue after the shown tick's cut
   Presentation  Sandbox.LookCameraView: LookCamera + PendingInput -> CameraView
                 Sandbox.CameraStatus:   LookCamera + Stasis -> CameraStatus (for the UI)
   Render        Graphics.PublishSnapshot; the renderer turns its camera by CameraView
```

## 2. Write a component

A component is a plain struct with a `static constexpr Fling::ComponentInfo
Component` member.

`Sandbox/Gameplay/inc/Stasis.h`:

```cpp
struct Stasis
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "Stasis", .Transient = true, .Rollback = Fling::RollbackClass::Restore};

    /** Simulation time left, in nanoseconds. 0: not in stasis. */
    int64 RemainingNs = 0;
};
```

`.Name` is the name in level files; it must end with the struct's name.
`.Kind` is `Table` (one per entity) by default; the others are `Tag`,
`Singleton` and `Queue`. `.Domain` is `Simulation` by default; a
`Presentation` component belongs to the frame side. `.Transient = true` means
a level never saves it. `.Rollback` says what a rollback does with it.

The rollback classes (`FlingEngine/Engine/inc/Ecs/Component.h`):

| Class | Meaning |
| --- | --- |
| `Restore` | Simulation state: saved per tick, and restored before a resimulation. |
| `Rebuild` | Made from other state each tick (a cache, the tick's input). |
| `Keep` | Not touched by a rollback (settings, a debug counter). |
| `Subsystem` | Owned by a subsystem outside the ECS, which saves its own state. |

A saved Simulation type that leaves `.Rollback` out is `Restore`. A
`Transient` Simulation type that is not a queue must set it, or it does not
compile ("A Transient Simulation component must say what a rollback does",
check `TransientWithoutRollback`). The reason: "not saved to a level" does not
tell whether it is game state. `Stasis` is game state, so it is `Restore`.

Stasis also adds one flag to the camera's policy, next to the player's own
freeze. The policy is part of `LookCamera` (`LookView View; LookPolicy
Policy;`), a `Restore` component.

`Sandbox/Gameplay/inc/LookCamera.h`:

```cpp
struct LookPolicy
{
    // ...
    /** The player's own freeze (the F key, at its place in the input). True: mouse input is thrown away. */
    bool Frozen = false;

    /**
     * A freeze that gameplay sets (Sandbox.Stasis), not input. True: mouse input is thrown
     * away. The frame side sees it from the first frame that shows the tick that set it.
     */
    bool InStasis = false;
    // ...
};
```

### Register the reflected fields

Register each field that is state. Level save and load, the editor inspector
and the state hash all read this list.

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

A field of a nested struct takes two member pointers. `Transient()` on a field
means that a level does not save it.

`Sandbox/Gameplay/inc/LookCameraType.h`:

```cpp
t_Type.Field("frozen", &LookCamera::Policy, &LookPolicy::Frozen);
// Gameplay state that Stasis drives: not saved to a level, still in the state hash
t_Type.Field("inStasis", &LookCamera::Policy, &LookPolicy::InStasis).Transient();
```

The state hash (`HashSimulationState`, `Ecs/StateHash.h`) covers exactly the
`Restore` Simulation types, with every reflected field, `Transient()` fields
too. The runner hashes after every tick, and `--dump-ticks` writes the hash.
The determinism tests compare these hashes. A field that is not registered is
a field that no determinism test sees.

`TypeRegistry::Build` rejects a `Restore` type with bytes that no field covers,
but it treats a gap as padding when an alignment could explain it. `InStasis`
sits in the three bytes after `Frozen`. With its `Field` line removed, the
registry still builds, and the hash test "Stasis: the state hash covers Stasis
and the InStasis flag" fails. Register every field by hand.

## 3. Write a fixed-tick system

A system is a class with no member state. It derives from
`Fling::System<T, Access<...>>`, has a `static constexpr Fling::SystemInfo
Info`, and implements one update function.

`Sandbox/Gameplay/inc/Systems/StasisSystem.h`:

```cpp
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

The first template argument is the system itself. The access list names
every component the system touches: `Read<T>` gives `const T`, `Write<T>` gives
`T`, and queues use `Append<Q>` and `Consume<Q>`. `SystemInfo` fields are
`Name`, `In`, `RunIn`, `After`, `Before`; designated initializers keep that
order. `Order::Simulation` is under `Order::Tick`, so the system implements
`FixedUpdate`. `.After` is section 4.

`Sandbox/Gameplay/src/Systems/StasisSystem.cpp`:

```cpp
void StasisSystem::FixedUpdate(Context& ctx) const
{
    const bool pressed = ctx.Singleton<const Fling::InputState>().WasPressed(Fling::Keys::G);
    const int64 tickNs = ctx.Singleton<const Fling::SimTime>().FixedDeltaNs;
    for (auto [entity, stasis, camera] : ctx.Query<Stasis, LookCamera>())
    {
        StasisUtil::Step(stasis, camera.Policy, pressed, tickNs);
    }
}
```

`ctx.Singleton<const T>()` reads a singleton; the runner writes `InputState`
and `SimTime` before the tick's systems run. `ctx.Query<Stasis, LookCamera>()`
gives each entity that has both; a `const` type reads, a plain type writes.
Time comes from `SimTime::FixedDeltaNs`, the length of this tick. The rule
itself is a pure function, so a test can call it without a world.

`Sandbox/Gameplay/inc/Stasis.h`:

```cpp
inline void Step(Stasis& t_Stasis, LookPolicy& t_Policy, const bool t_Pressed, const int64 t_TickNs)
{
    t_Stasis.RemainingNs = std::max<int64>(t_Stasis.RemainingNs - t_TickNs, 0);
    if (t_Pressed)
    {
        t_Stasis.RemainingNs = LengthNs(t_TickNs);
    }
    t_Policy.InStasis = t_Stasis.RemainingNs > 0;
}
```

`LengthNs` rounds `DurationNs` (2 s) to whole ticks: 60 ticks at 30 Hz, 120 at
60 Hz, 240 at 120 Hz. So a stasis covers the same simulated time at any
`--sim-hz`.

### What the compiler refuses

Each access rule has a compile-fail check. `FlingTests/EcsCompileFail/EcsCompileFail.cpp`
changes one thing in a correct system, and `EcsCompileChecks.cmake` requires
the build to fail with the rule's message. Two of them, with their MSVC output.

Case `QueryUndeclared`: a fixed system calls `ctx.Query<const Display>()`, and
its access list does not name `Display`.

```text
FlingEngine\Engine\inc\Ecs/SystemContext.h(347): error C2338: static_assert failed: 'This system queries a component that its Access<...> does not allow. Add Write<T>, or Read<T> and query const T.'
```

Case `FrameWritesSimulation`: a frame system writes a Simulation component.

```cpp
// FlingTests/EcsCompileFail/EcsCompileFail.cpp
class BadSystem final : public Fling::System<BadSystem, Fling::Access<Fling::Write<Position>>>
// ...
        static constexpr Fling::SystemInfo Info{.Name = "Check.Bad", .In = Fling::Order::Presentation};
```

```text
FlingEngine\Engine\inc\Ecs/SystemBase.h(219): error C2338: static_assert failed: 'A variable rate system cannot write a Simulation component. Mark the component Presentation, or move the system to a fixed rate group.'
```

The second rule keeps the frame side from changing the simulation. The camera's
view system only reads `LookCamera` and writes `CameraView`, a Presentation
singleton. Every rule is listed in [`system-order-api.md`](system-order-api.md),
section 5.

## 4. Order systems

A group is a named span of the schedule. The engine groups
(`Ecs/EngineOrder.h`) run in this order: under `Order::Tick`, `Input`,
`Simulation`, `Physics`, `ResolveDeferred`, `Finalize`; under `Order::Frame`,
`Presentation`, `Render`, `Editor`.

Inside a group the data decides the order: a writer of a type runs before its
readers. Most systems need no `.After` or `.Before`. Two writers of one type
have no such rule, so the schedule cannot choose. It is an error until one of
them states an order.

`Sandbox.LookCamera` and `Sandbox.Stasis` both write `LookCamera`. Without
`.After`, the builder returns `ScheduleErrorKind::WriteConflict` with this
message (from the test in section 9):

```text
Sandbox.LookCamera and Sandbox.Stasis both write LookCamera in group Engine.Simulation, and neither orders the other. Fix: add .After = Fling::SystemOf<Sandbox::LookCameraSystem> to Sandbox.Stasis's SystemInfo (or the reverse), order their groups, or call RunBefore().
```

`SystemOf<T>` names a system by its class and needs only a forward
declaration. A system header never includes another system's header. It
includes its module's order header, which declares the systems that others may
order against.

`Sandbox/Gameplay/inc/SandboxSystemsFwd.h`:

```cpp
namespace Sandbox
{
    // The Sandbox systems that other Sandbox systems order against with SystemOf<T>.
    class LookCameraSystem;
    class RotatorSystem;
} // namespace Sandbox
```

Here the order is not a free choice: section 7 explains why stasis must run
after the look system. `Sandbox --null-render --unattended --dump-schedule`
prints the result. The build also checks the schedule after it links the
Sandbox (`--validate-schedule`), so a write conflict fails the build.

```text
  stage 1  Sandbox.LookCamera           [Engine.Simulation]        writes LookCamera
  ...
  stage 2  Sandbox.Stasis               [Engine.Simulation]        writes Stasis, LookCamera
             after Sandbox.LookCamera: .After
```

## 5. Register it all in the module

Each game has one register function. Only that file includes system headers.
The order of the calls does not matter.

`Sandbox/Gameplay/src/RegisterSandbox.cpp`:

```cpp
void Game::Register(Fling::ModuleRegistration& t_Modules)
{
    // ...
    RegisterLookCameraType(t_Modules);
    RegisterStasisType(t_Modules);
    t_Modules.Component<CameraStatus>();
    // ...
    t_Modules.System<LookCameraSystem>();
    t_Modules.System<LookCameraViewSystem>();
    t_Modules.System<StasisSystem>();
    t_Modules.System<CameraStatusSystem>();
    // ...
}
```

`t_Modules.System<T>()` runs the compile-time checks of section 3. Singletons
need no setup: the runner makes each one that a system names, with default
values. The game makes the entities: `Game::OnStartGame` in
`Sandbox/Gameplay/src/SandboxGame.cpp` calls `t_Reg.Create(LookCamera{},
Stasis{})` when play starts, unless the level has a `LookCamera`.

## 6. The camera

### The problem

The simulation runs at 30 Hz by default. A camera that turned only in the tick
would show a mouse move up to one tick (33 ms) late, in 30 steps a second on a
144 Hz display. Mouse look would feel late and rough.

### The split

The camera has two halves that run one pure function:

- `Sandbox.LookCamera` (fixed) folds each tick's input into the `LookCamera`
  component, which is simulation state.
- `Sandbox.LookCameraView` (presentation) takes the last published
  `LookCamera` and runs the same function over the input that no tick has
  consumed yet (`PendingInput`). It writes `CameraView`. The renderer draws
  with it.

The function walks mouse samples and key presses in stream order. A key that
changes the policy (F, Minus, Equal) takes effect at its own place among the
samples.

`Sandbox/Gameplay/inc/LookUtil.h`:

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

`Sandbox/Gameplay/src/Systems/LookCameraSystem.cpp`, the fixed half:

```cpp
const Fling::InputSlice& input = ctx.Singleton<const Fling::InputState>().Tick.Slice;
for (auto [entity, camera] : ctx.Query<LookCamera>())
{
    camera = LookUtil::Run(camera, input);
}
```

`Sandbox/Gameplay/src/Systems/LookCameraViewSystem.cpp`, the presentation half:

```cpp
const Fling::PendingInput& pending = ctx.Singleton<const Fling::PendingInput>();
Fling::CameraView& view = ctx.Singleton<Fling::CameraView>();
view.Active = false;
for (auto [entity, camera] : ctx.Query<const LookCamera>())
{
    const LookView shown = LookUtil::Run(camera, pending).View; // the same rule the tick runs
    view = Fling::CameraView{.Active = true, .Yaw = shown.Yaw, .Pitch = shown.Pitch};
}
```

### The cut between consumed and pending input

All input is one queue of messages with sequence numbers. Each tick takes a
cut: the messages after the previous tick's cut, up to its own limit. The cut
is part of the tick's `TickStamp`, which travels in the packet with the state.
Before the frame systems run, `SystemRunner::RunFrameAt` fills `PendingInput`
with every message after the cut of the tick that the frame shows. Before the
first tick, `PendingInput` is empty.

### Why nothing is counted twice

The frame side keeps no state between frames. Each frame computes the view
from scratch: the published camera, run over the input after that camera's
cut. When a tick consumes a message, the message is in the published camera
and no longer pending. So every frame applies each message exactly once.

The rule applies each sample in order and clamps after each one, so
`Run(Run(c, a), b)` equals `Run(c, a + b)` bit for bit. The view a frame shows
is exactly the camera that the ticks make from the same input, however the
input is split into ticks.

## 7. Simulation authority

The policy lives in `LookCamera`, a simulation component. The presentation
side uses it as plain data. The look rule drops mouse input when either flag
is set.

`Sandbox/Gameplay/inc/LookUtil.h`, `Detail::Apply`:

```cpp
if (t_Policy.Frozen || t_Policy.InStasis)
{
    return t_View;
}
```

The two flags get there by different paths:

| | `Frozen` (F) | `InStasis` (G) |
| --- | --- | --- |
| Comes from | Input | Gameplay (`Sandbox.Stasis`) |
| Applied by | `LookUtil::Run`, on both sides | The simulation only |
| Takes effect | At the key's place in the input stream | At the end of the tick that sets it |
| The frame side sees it | At once: the press is in `PendingInput` | From the first frame that shows that tick |

The frame side can predict F, because F is input and the rule is shared. It
cannot predict G. G is a key too, but its effect is a gameplay decision: a game
would check cooldowns, items or game state. So the frame side waits for the
published flag.

This is why `Sandbox.Stasis` runs after `Sandbox.LookCamera`. Tick N, which
takes the press, first turns the camera by all of its own input under the old
policy. Then it sets `InStasis`. The frames before tick N arrived showed that
input under the same policy. The frames after it drop pending input, and tick
N + 1 drops the same input. So when each frame shows the newest tick, nothing
the player saw is taken back. Section 10 covers a frame that shows an older
tick.

One stasis start at 0.01 rad per mouse count, from the test "Stasis: the
presented view never moves backwards at the start or the end":

| Step | What the tick consumed | State the frame uses | Yaw shown |
| --- | --- | --- | --- |
| Frame after tick N - 1 | | Tick N - 1, free | Y |
| Move 2 counts, frame | | Tick N - 1, free | Y - 0.02 |
| Move 2 counts, frame | | Tick N - 1, free | Y - 0.04 |
| Press G, move 2 counts, frame | | Tick N - 1, free (G is not a look key) | Y - 0.06 |
| Tick N | 6 counts and G: the look turns 6 counts, then stasis sets `InStasis` | | |
| Frame | | Tick N: yaw Y - 0.06, in stasis | Y - 0.06 |
| Move 2 counts, frame | | Tick N, in stasis: the 2 counts are dropped | Y - 0.06 |
| Tick N + 1 | 2 counts, dropped by the policy of tick N | | |
| Frame | | Tick N + 1 | Y - 0.06 |

The end works the same way. The last tick of stasis still drops its input, as
the frames before it did. It publishes `InStasis = false`, and input after its
cut turns the camera on both sides.

The test runs the same script with stasis ordered before the look system. That
order snaps back by 0.06 rad at tick N: the tick drops the 6 counts that the
frames showed.

### A general recipe

To give anything that the presentation side predicts a policy that the
simulation owns:

1. Put the predicted state and its policy in one Simulation component of class
   `Restore`. Register every field.
2. Write one pure function from state and input to state. Call it from the
   fixed system on `InputState` and from the frame system on `PendingInput`.
   Keep no state in the frame system.
3. Apply input-driven policy changes inside that function, at their place in
   the input stream.
4. Make each gameplay-driven policy change a separate fixed system that writes
   the policy data.
5. Order that system after the system that folds the input, with
   `.After = Fling::SystemOf<...>` through the module's order header. The
   schedule refuses the pair until you do.
6. Test frame by frame: the view must never step back, and each tick must make
   exactly what the frame before it showed.

## 8. Try it

Start the Sandbox from the project root (this repository). With a Ninja build
in `build/` (see [`../Skills/building.md`](../Skills/building.md)):

```text
build\Sandbox\bin\Sandbox.exe --play
```

In another build tree, the program is `Sandbox/bin/Sandbox` under the build
folder (with the Visual Studio generator, `Sandbox/bin/Debug/Sandbox.exe`). Play runs threaded at 30 Hz. Keys: M hides or shows the cursor, the
mouse turns the camera, F freezes or unfreezes it, G gives 2 s of stasis, `-`
and `=` halve and double the look speed, T rotates the models, Esc stops.

What to look for:

- The "Demo Info" window shows "Camera: free", "Camera: frozen (F)" or
  "Camera: stasis 1.4 s" with the time left. It reads `CameraStatus`, which
  `Sandbox.CameraStatus` writes on the frame side from published state.
- Press G while you move the mouse. The camera stops without a jump back, and
  turns again after 2 s.
- Press F during stasis. The camera stays frozen after the stasis ends.

The "GPU Options" window opens at start in `--play`. It has a "Simulation rate
(Hz)" box and a "Render interpolation" checkbox. To see the split:

1. Press T so the models turn.
2. Type 10 in "Simulation rate (Hz)" and press Enter. The new rate applies
   from the next tick.
3. Clear "Render interpolation". The models move in steps, ten a second. The
   camera still turns on every frame, because the frame side applies pending
   input at the frame rate.
4. Set "Render interpolation" again. The models move smoothly, drawn between
   the latest two ticks. The camera is not blended: it is the latest published
   camera plus pending input.

At 10 Hz a stasis is still 2 s: 20 ticks. `--sim-hz 10` and
`--interpolation off` set the same values at start.

## 9. How to test a system

A test needs no window and no GPU. The camera tests use a rig,
`FlingTests/SandboxExample/LookRig.h`: a virtual mouse and keyboard on the real
input queue, the engine's tick and pending input readers, a `SystemRunner` and
one camera entity, on a virtual clock. `Move` and `Press` add input, `Tick`
runs one fixed tick, `Frame` runs the frame systems. `Sim()` is the simulation
camera; `View()` is the `CameraView` of the last frame.

A behavior test, from `FlingTests/SandboxExample/StasisTests.cpp`:

```cpp
LookRig Rig(MakeCamera(0.01f), AddStasis, LookRigConfig{.WithStasis = true});
Rig.Tick();
Rig.Press(Keys::G);
Rig.Tick();
REQUIRE(Rig.Sim().Policy.InStasis);
const LookView Held = Rig.Sim().View;

size_t MostPending = 0;
for (int i = 0; i < 24; ++i)
{
    Rig.Move(7, -5);
    Rig.Frame();
    MostPending = std::max(MostPending, Rig.PendingSamples());
    REQUIRE(SameBits(Rig.View(), Held));
    // ...
}
// The frames had the moves pending, and dropped them by the published flag
REQUIRE(MostPending == 3);
```

The last line proves that the frames had input pending. `SameBits` compares
floats bit for bit: the two sides must agree exactly.

A schedule test builds the schedule from real system classes and needs no rig.
This one copies the real descriptor of `Sandbox.Stasis` and removes its
`.After`:

```cpp
SystemDesc Unordered = Real.Describe();
Unordered.Info.After = OrderList{};
const SystemDesc* Descs[] = {&Look.Describe(), &Unordered};
const ScheduleResult Result = BuildSchedule(std::span<const SystemDesc* const>(Descs));
REQUIRE(Result.Errors.size() == 1);
REQUIRE(Result.Errors[0].Kind == ScheduleErrorKind::WriteConflict);
```

Other tests in the file cover the duration at 30, 60 and 120 Hz, F with G,
the state hash, and the frame-by-frame order of section 7. Run them with
`SandboxExampleTests "[stasis]"`. The CTest `Sandbox: tick dump exec modes
stasis` replays `Sandbox/Tests/Input/stasis.input.jsonl` headless and requires
the same cut and state hash on every tick in every exec mode and frame cap.

## 10. Limits

What is not built:

- **State snapshots and a resimulation runner.** The rollback classes, the
  state hash and the resimulated groups exist. Nothing saves the `Restore`
  state per tick or replays ticks yet.
- **Networking.** There is no remote input, no player or device mapping and no
  network id. `InputState` is one input for the world.

What the presentation side cannot do:

- It predicts only from input, with the shared rule. It shows a gameplay
  decision only from the first frame that shows the tick that made it.
- **A late publish shows some input wrongly.** In threaded play a tick is
  published after it runs. A frame in between shows the tick before, with the
  input after the new tick's cut pending under the old policy. If the new tick
  starts a stasis, that input turns the view and is taken back when the tick
  arrives; at the end of a stasis it shows late. The test "Stasis: with a late
  publish, ..." pins it: -0.15 rad, then -0.05 rad. The amount is the input
  that came in while the game thread ran and published that tick; it is not
  measured in live play. No order fixes it: the frame side cannot know a
  gameplay decision before its tick arrives. `Frozen` has no such gap.

What not to copy blindly:

- `InStasis` repeats `Stasis::RemainingNs > 0`. Only `Sandbox.Stasis` writes
  them, which keeps them equal.
- Only the camera that the Sandbox makes has a `Stasis`. A level's own
  `LookCamera` gets no stasis.
- After a rate change during a stasis, the rest rounds up to whole new ticks.
- `WasPressed` is per tick: two presses of G in one tick start one stasis.
- `CameraStatus` reads the published `Frozen`, not the predicted one. After a
  press of F the view stops at once, and the status line changes when the tick
  that took the press arrives.
- A new registered `Restore` type changes every state hash, even when no
  entity has it: the hash includes the type hash of each such type. Adding
  `Stasis` changed the pinned tick dump SHA-256 values in
  `Sandbox/CMakeLists.txt`. Expect that when you add game state.
- `CameraView` is one view. With several `LookCamera` entities, the last one
  in storage order wins.

More detail: [`input-two-sided.md`](input-two-sided.md) for the input rules,
[`system-order-api.md`](system-order-api.md) for order and every error,
[`ecs-tutorial.md`](ecs-tutorial.md) for queues, singletons and the engine's
components, and [`threading-model.md`](threading-model.md) for the publish
path.
