# The system order API

This page is the reference for how Fling decides the run order of ECS systems.
A system says, at compile time, which group it is in, what it must run after or
before, and which data it reads and writes. The schedule builder makes one graph
from that and sorts it.

- Code: `FlingEngine/Engine/inc/Ecs/Order.h` (references, groups, lists),
  `Ecs/EngineOrder.h` (the engine groups), `Ecs/System.h` (`SystemInfo`),
  `Ecs/SystemBase.h` (`Detail::CheckSystem`, the compile-time rules),
  `Ecs/Schedule.h` (the builder's API), and the builder itself in
  `src/Ecs/`: `ScheduleBuilder.h` (its parts), `Schedule.cpp` (the steps in
  order, segments and the resimulation checks), `ScheduleGroups.cpp`,
  `ScheduleGraph.cpp`, `ScheduleOrders.cpp`, `ScheduleAccess.cpp` and
  `ScheduleDump.cpp`.
- How to write a system: [`Skills/ecs.md`](../Skills/ecs.md) and the tutorial
  [`ecs-tutorial.md`](ecs-tutorial.md).
- Why it is built this way, and what broke on the way:
  [`update-order-design-notes.md`](update-order-design-notes.md).
- The input that the two update rates share: [`input-two-sided.md`](input-two-sided.md).
- The whole model (order, time, input, threads) on a few pages:
  [`architecture-overview.md`](architecture-overview.md).

## 1. What a system declares

A system declares three things. All three are constants in its header.

| What | Where | Example |
| --- | --- | --- |
| Its group | `SystemInfo::In` | `.In = Fling::Order::Simulation` |
| Its order against other work | `SystemInfo::After`, `SystemInfo::Before` | `.After = Fling::SystemOf<RotatorSystem>` |
| The data it touches | the `Access<...>` list | `Fling::Write<Fling::Transform>` |

`SystemInfo` (`Ecs/System.h`) has these fields, in this order:

| Field | Type | Meaning |
| --- | --- | --- |
| `Name` | `const char*` | `"Module.Name"`. Errors, the dump, the tie-break, profiling. |
| `In` | `GroupRef` | The group. Its root decides `FixedUpdate` or `Update`. |
| `RunIn` | `WorldRole` | The world roles that run the system (`Play` by default). |
| `After` | `OrderList` | Groups and systems that run before this one. |
| `Before` | `OrderList` | Groups and systems that run after this one. |

Write designated initializers in this order. MSVC rejects another order with
error C7560 ("designators must appear in member declaration order").

### References

An order reference (`OrderRef`) names a group or a system:

- A group: the `inline constexpr GroupInfo` itself, for example `Fling::Order::Physics`.
- A system: `Fling::SystemOf<T>`. `T` can be a forward declaration. The builder
  finds the system by `SystemTypeId<T>()`, a hash of the C++ type name.

An `OrderList` holds one reference or a braced list of up to eight
(`OrderList::Capacity`). A list can mix both kinds:
`.After = {Fling::SystemOf<A>, Fling::Order::Input}`.

### The access list and order

The access list is the same list that the context checks at compile time
(`Skills/ecs.md`, "A system"). The builder also reads it for order:

| Access | Effect on order |
| --- | --- |
| `Read<T>` | Runs after an unordered writer of `T`. |
| `Write<T>` | Runs before an unordered reader of `T`. Two unordered writers are an error. |
| `Append<Q>` | Runs before the consumer of `Q`. Two appenders get no edge and no error. |
| `Consume<Q>` | Runs after every unordered appender. Two consumers are an error. |

A `Read<S>` of a singleton that no system writes is valid: it needs no edge,
and the dump lists it. Every singleton that a system names exists from
`SystemRunner::Create`: the runner makes each one that is not `.External` and
not made yet, with its default values, in the simulation storage for a
`Simulation` type (the publish copies it to a mirror) or in the frame storage
for a `Presentation` type. It keeps that value until code outside the systems
(the application or a module, at setup) sets it.

### A complete example

A game adds vehicle systems that must run inside the engine's physics group,
before the physics step. This is a real test:
`FlingTests/Physics/PhysicsOrderTests.cpp` (trimmed; the file has
`using namespace Fling;`).

```cpp
namespace OrderExample
{
    // What a game's order header holds: its group, and forward declarations of its systems.
    class SuspensionSystem;
    class SteeringSystem;
    class HoverSystem;

    /** Vehicle work runs in the engine's physics group, before the physics step. */
    inline constexpr GroupInfo Vehicles{
        .Name = "Game.Vehicles", .In = Order::Physics, .Before = SystemOf<PhysicsStepSystem>};

    /** Writes Transform, as PhysicsStepSystem does. */
    class HoverSystem final
        : public System<HoverSystem,
                        Access<Read<SimTime>, Read<VehicleSuspension>, Read<VehicleSteering>, Write<Transform>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Vehicles.Hover", .In = Vehicles, .After = {SystemOf<SuspensionSystem>, SystemOf<SteeringSystem>}};

        void FixedUpdate(Context& /*ctx*/) const {}
    };
} // namespace OrderExample
```

`PhysicsStepSystem` comes from `PhysicsSystemsFwd.h`, which only forward-declares
it. The test registers the real `Fling::PhysicsStepSystem` with the three
vehicle systems and checks:

- No error. `Vehicles.Hover` and `Physics.Step` both write `Transform`, but the
  group order joins them, so it is not a `WriteConflict`.
- Run order `Vehicles.Steering, Vehicles.Suspension, Vehicles.Hover, Physics.Step`.
  The names sort after `Physics.Step`, so only the group order can put the step last.
- Exactly two edges, both `.After`. No edge goes into the step: one group
  reference orders every member of the group.

The production example is `Sandbox/Gameplay/inc/Systems/MoverSystem.h`:

```cpp
static constexpr Fling::SystemInfo Info{
    .Name = "Sandbox.Mover", .In = Fling::Order::Simulation, .After = Fling::SystemOf<RotatorSystem>};
```

## 2. Groups

A group is a named span of the schedule. It holds systems and other groups. Any
module can declare one as an `inline constexpr Fling::GroupInfo`.

`GroupInfo` (`Ecs/Order.h`) has these fields, in this order:

| Field | Meaning |
| --- | --- |
| `Name` | `"Module.Name"`. The identity of the group. |
| `In` | The parent group. Every group sets it, except the two roots. |
| `After`, `Before` | Groups and systems that run before or after the whole group. |
| `FlushAfter` | Apply the deferred commands when the group ends. |
| `Resim` | A resimulation runs this group again. Inherited. See "Resim groups". |

### Roots

There are two roots. The root gives the update rate, so no other group states a rate.
A root is a group with no `In` and one of two names; its rate comes from the name.

| Root | Name | Rate | Systems implement |
| --- | --- | --- | --- |
| `Order::Tick` | `Engine.Tick` (`TickRootName`) | Fixed: once per simulation tick | `FixedUpdate(Context&)` |
| `Order::Frame` | `Engine.Frame` (`FrameRootName`) | Variable: once per rendered frame | `Update(Context&)` |

```cpp
// Ecs/EngineOrder.h
inline constexpr GroupInfo Tick{.Name = TickRootName};
inline constexpr GroupInfo Frame{.Name = FrameRootName};
```

Any other group with no `In` is an error: a compile error for a system in it,
and `MissingParent` at schedule build for a group that holds no system.
`RateOf(group)` walks to the root and returns its rate. `Detail::IsRooted(group)`
is true when the chain of parents ends at one of the two.

In threaded play the root also decides the thread: `Order::Tick` runs on the
game thread, `Order::Frame` on the main thread
([`threading-model.md`](threading-model.md)).

### Engine groups

`Ecs/EngineOrder.h`, namespace `Fling::Order`:

| Group | In | After | Resim | What runs there |
| --- | --- | --- | --- | --- |
| `Input` | Tick | | no | Game input preprocessors that read `InputState` (the runner writes it before the tick's systems), and `Sandbox.AppKeys` |
| `Simulation` | Tick | `Input` | yes | Gameplay |
| `Physics` | Tick | `Simulation` | yes | The physics step (`Physics.Step`) |
| `ResolveDeferred` | Tick | `Physics` | yes | Consumers of queues: spawns, damage, effects |
| `Finalize` | Tick | `ResolveDeferred` | no | Data made from the last state, such as `LocalToWorld` |
| `Presentation` | Frame | | no | What the player sees and hears. Readers of `PendingInput` (the runner writes it before the frame's systems) |
| `Render` | Frame | `Presentation` | no | `Graphics.PublishSnapshot` |
| `Editor` | Frame | `Render` | no | Editor tools |

`Order::All` (a `std::to_array` of pointers) lists the two roots and these
eight groups. The builder always adds them, so the dump lists them when they
are empty. A child points to its parent, not the reverse,
so the builder cannot find an empty engine group from the roots.

### Nesting

A group can be inside any group, to any depth. Each group is two nodes in the
graph, a begin and an end. Its members run between them. A child of a group is
not ordered against the other members of its parent unless something says so
(test "Group nesting orders writers in sibling groups, not a child against its parent").

### A module's own group

Put a game group inside the engine group where its work belongs, and order it
against the engine work that it depends on. The `Vehicles` group above is the
pattern. A game group can also be a direct child of `Order::Tick`, ordered
between engine groups (from `FlingTests/Engine/EcsScheduleTests.cpp`):

```cpp
inline constexpr GroupInfo ModAGroup{.Name = "ModA.Abilities",
                                     .In = Order::Tick,
                                     .After = Order::Simulation,
                                     .Before = Order::ResolveDeferred,
                                     .Resim = true};
```

A group directly in `Order::Tick` between two resimulated groups must set
`.Resim = true`, or the first resimulated system
that runs after it makes the build fail with `ResimGap`. Prefer a group inside
`Order::Simulation`.

### Resim groups

A resimulation (rollback) runs some fixed work again. Membership is a property
of the group, not of the system.

- `Simulation`, `Physics` and `ResolveDeferred` set `Resim = true`. Every group
  and system inside them is resimulated (`ScheduleGroup::Resim`). `false` means "as the
  parent". There is no per-system opt-out.
- Only a direct child of `Order::Tick` may set `Resim`. Anywhere else it is
  `ResimPlacement`, also inside a resimulated group, where it would be
  redundant (and a compile error for a system, through
  `Detail::IsResimPlacedRight`).
- A fixed system that is not resimulated must run before every resimulated
  system or after every one, through the graph: structure, stated orders and
  data edges (`ResimGap`). It is a rule on the graph, so a system that nothing
  orders against the resimulated part is an error under any name. So
  `Schedule::ResimRunOrder()` is one unbroken slice of the fixed run order.
- A fixed system that is not resimulated must not make state that a
  resimulation needs (`ResimSkipsState`): it must not write a type that a
  rollback restores (`IsRollbackState`), and it must not write a type that a
  resimulated system that runs before it reads or writes (`Write` implies read). A system before them may write
  a `Rebuild` type that they read.
- A resimulated system appends only to a queue that a resimulated system
  consumes (`ResimAppendsOutput`). A queue that the world takes after the tick
  (`AppRequests`) or that a non-resim system consumes would get the requests of
  each replayed tick again. `Sandbox.AppKeys` is in `Order::Input` for this.
- The schedule gives `ScheduleGroup::Resim` (in `Schedule::Groups`),
  `Schedule::IsResim(system)`, `ResimRunOrder()`, and `Schedule::ResimInputs`:
  what resimulated systems read (or write: `Write` implies read) that systems
  outside them write, so what a rollback must restore or record. A type that the runner writes before each
  tick (`RunnerWritten<UpdateRate::Fixed>` in `Ecs/RunnerWritten.h`: `SimTime`,
  `InputState`) is in it when a resimulated system reads it, with `FromRunner`. The dump marks
  groups `(resim)` and prints `resim runs:` and `resim reads from outside:`.

No runner executes a resimulation yet. Only the schedule side exists. The other
rollback hooks are in [`time-and-ticks.md`](time-and-ticks.md).

### FlushAfter

`FlushAfter = true` applies the deferred commands when the group ends. Every
fixed system must then be ordered against the end of that group, through
structure or stated orders (`FlushNotOrdered`). It is an error on a variable
rate group (`FlushInVariableGroup`). The end of each fixed tick always flushes,
so most groups do not need it.

### Identity

The builder knows a group by its `Name`, not by its address. A module built as a
shared library holds its own copy of each `inline constexpr` object. Two
objects with one name are one group if their fields agree; otherwise it is
`DuplicateGroup` (test "A group is known by its name, not by its address").

The builder finds a group through any `In`, `After`, `Before` or `RunBefore`
reference. `SystemRegistry::AddGroup` is only for a group that nothing refers to.

A group can name only groups that are defined above it in the source. There is
no forward declaration of a `constexpr` variable. Write `B.After = A`, not
`A.Before = B`, when `B` comes later. `SystemOf<T>` can name any class declared above.

### What a group costs

| Where | Cost |
| --- | --- |
| Schedule build | Two nodes and one begin-to-end edge. Two membership edges for each member. One edge for each reference to the group, however many systems it holds. |
| Reachability | One bit per node for each system (one walk of the order graph per system). Fine for hundreds of systems. |
| Run time | Nothing. The runner runs systems in stages; groups exist only in the schedule. A `FlushAfter` group adds a flush. |
| Source | One `inline constexpr` line in an order header. |

## 3. Order headers

Each module publishes one order header. It holds the module's groups and
forward declarations of the systems that other code may order against. Nothing else.

| Module | Header | Contents |
| --- | --- | --- |
| Engine | `FlingEngine/Engine/inc/Ecs/EngineOrder.h` | `Order::Tick`, `Order::Frame`, the eight engine groups, `Order::All`. No system: order against an engine group. |
| Physics | `FlingEngine/Physics/inc/PhysicsSystemsFwd.h` | Includes `Ecs/EngineOrder.h`; `class PhysicsStepSystem;` |
| Sandbox | `Sandbox/Gameplay/inc/SandboxSystemsFwd.h` | Includes `Ecs/EngineOrder.h`; `class LookCameraSystem;` and `class RotatorSystem;` (namespace `Sandbox`) |

`Ecs/System.h` includes `EngineOrder.h`, so every system header sees
`Fling::Order` through `Ecs/SystemBase.h`.

Include rules:

1. A system header includes the order header of its own module and of modules
   below it. It never includes another system's header.
   `scripts/check_system_includes.py` checks this in CI.
2. References point down the module graph. A game names the engine. The engine
   never names a game.
3. Do not forward-declare another module's system in your own file. If its order
   header does not declare the system, it is not part of that module's order
   contract. Order against a group of that module.
4. A forward declaration with the other class key (`struct` for a `class`) is
   fine. `Detail::SystemTypeHash` drops `class `, `struct `, `enum ` and `union ` everywhere in the
   name, also inside template arguments, because MSVC prints the key that each
   file saw.

What recompiles:

| Change | Recompiles |
| --- | --- |
| Body of a system | Its `.cpp` |
| Access list or `Info` of a system | Its `.cpp` and the module's register file |
| An order header | Every file that includes it |

## 4. How the run order is decided

Each update rate is one graph: a node for each system, and a begin and an end
node for each group. Four rules decide the order, in priority order:

1. **Structure.** `In` puts a system or group between the begin and end of its
   parent. A group's `After` and `Before` order the whole group.
2. **Stated order.** A system's `After` and `Before`, and
   `SystemRegistry::RunBefore(first, second)`. Each joins the exit of one thing to
   the entry of the other. A group's entry is its begin, its exit is its end.
   Rules 1 and 2 are edges in the same graph. They do not override each other:
   if they disagree, it is a `Cycle`.
3. **Data edges.** For each pair of systems that rules 1 and 2 do not join by a
   path in either direction: a writer runs before a reader, and an appender
   before the consumer. A path through any number of other systems or groups
   counts. Two unordered writers of one type, in worlds that can meet
   (`RunIn` overlaps), are a `WriteConflict`.
4. **Stage, then name.** The edges of rules 1 to 3 give each system a segment
   (the number of `FlushAfter` group ends before it) and a stage (the longest
   chain of systems before it in its segment). Systems run segment by segment,
   stage by stage, and by name inside one stage. So a system with no edges runs
   in stage 0 of its segment, with the first systems, whatever its name.
   Example: `Test.A` writes X, `Test.B` reads X, `Test.Z` touches nothing. Stage
   0 is `Test.A` and `Test.Z`, stage 1 is `Test.B`, and the run order is A, Z, B
   (test "The run order is by stage, then by name"). The groups
   (`Schedule::Groups`) are in the order of a topological sort of the same graph
   in which a tie goes to the smaller name: `"Name (begin)"` and `"Name (end)"`
   for the begin and end of a group.

### One tick late

Rules 1 and 2 can put a reader before its writer. That is legal and often
wanted. The reader then gets the value of the previous tick (or frame). The
schedule lists each such read in `Schedule::Lags`, and the dump prints it under
the reader. A data edge never points back against a stated order: test "Group
order wins over a data edge that points back".

This includes a pair ordered through a third system. `A.Before = B` and
`B.Before = C`, where C writes what A reads, gives run order A, B, C and a
one-tick-late read for A, not a data edge from C to A (test "An order through a
third system replaces the inferred edge").

### Reading the dump

`Sandbox --null-render --unattended --dump-schedule` prints the schedule.
`Schedule::Dump()` gives the same text in code, and the editor shows it in
**Windows > World Singletons**. The engine and physics schedule is checked in as
`FlingTests/Physics/Schedules/EnginePhysics.schedule.txt` (the whole file):

```text
Fixed (once per tick)
  the runner writes first: SimTime, InputState
  groups:
    Engine.Tick
      Engine.Input
      Engine.Simulation  (resim)
      Engine.Physics  (resim)
      Engine.ResolveDeferred  (resim)
      Engine.Finalize
  stage 0  Physics.Step                 [Engine.Physics]           writes Transform, RigidBody, PhysicsBody, PhysicsWorld, PhysicsSettings, ContactEvents, PhysicsCommands
  stage 1  Engine.Transform             [Engine.Finalize]          writes LocalToWorld, TransformWork
  flush: end of tick
  resim runs: Physics.Step
  resim reads from outside: SimTime (from the runner)
Published each tick, for Order::Frame
  nothing
```

The engine and physics have no frame system, so there is no `Variable` part.
The Sandbox dump has the same shape with the game added: `Sandbox.AppKeys` in
`Engine.Input`, the Sandbox gameplay systems in `Engine.Simulation` (with their
`after ...` lines), a `Variable
(once per frame)` part with the look camera view system and
`Graphics.PublishSnapshot`, `resim reads from outside: InputState (from the
runner)` and the same for `SimTime`, and one line per published type.

How to read it:

| Line | Meaning |
| --- | --- |
| `the runner writes first: ...` | `RunnerWritten<rate>` (`Ecs/RunnerWritten.h`): the singletons that the runner writes before the systems of that rate, `SimTime` and `InputState` per tick, `RealFrameTime` and `PendingInput` per frame. No system can write them, so their readers need no edge. |
| `groups:` | The group tree of that rate. Children are indented under their parent, in run order of their begin. |
| `stage N  Name  [Group]  writes ...` | A system, its group, and every type it writes. Systems in one stage have no edge between them. |
| `after X: .After` / `.Before` / `RunBefore` | A stated order between two systems. Group edges are not listed: they join begin and end nodes, not systems. |
| `after X: it writes T` | A data edge (`EdgeReason::WriteRead`). `it appends to T` is `AppendConsume`. |
| `one tick late: reads T before S writes it` | A `ScheduleLag`. At the variable rate it says `one frame late`. For a queue: `consumes T before S appends to it`. |
| `flush: end of group G` / `flush: end of tick` | Where deferred commands apply. |
| `resim runs:` | `ResimRunOrder()`. |
| `resim reads from outside: T (from S)` | `Schedule::ResimInputs`: what resimulated systems read or write that skipped systems (or `the runner`, for a `RunnerWritten` type) write, so what a rollback must restore or record. |
| `Singletons that no system writes` | `Schedule::ReadOnlySingletons`, one line per type, `T (singleton), read by S`: singletons that systems of either rate read and that no system and not the runner writes. They hold their default values until code outside the systems sets them. |
| `Published each tick, for Order::Frame` | `Schedule::Published`, one line per type, `T (kind), read by S`: what the runtime copies to the frame side ([`threading-model.md`](threading-model.md)). `; held for the blend of S` when a frame system declares `ReadBlend<T>` (threading-model.md, "The two blend states"). |

## 5. Errors

### At compile time

`SystemRegistry::Add<T>` calls `Detail::CheckSystem<T>()`. So these checks run
for every real system class. A `SystemDesc` made by hand (in a test) skips
them, which is why some of them are also schedule errors. A group that holds no
system is checked only at schedule build. The "Check" column names the case in
`FlingTests/EcsCompileFail/EcsCompileFail.cpp`, registered in
`FlingTests/EcsCompileFail/EcsCompileChecks.cmake` with `fling_add_ecs_compile_check`.

| Mistake | Message (or compiler error) | Check | Fix |
| --- | --- | --- | --- |
| Misspelled group | `'Simulaton': is not a member of 'Fling::Order'` (C2039) | `OrderGroupTypo` | Use the group's real name. |
| Misspelled or undeclared system class | `'TransfromSystem': is not a member of 'Fling'` (C2039) | `OrderSystemTypo` | Include the order header that declares it. |
| A constant list with more than eight items | `expression did not evaluate to a constant`, note on `OrderList::TooManyItems` (C2131) | `OrderListTooLong` | Put the items in a group and order against the group. |
| Designators out of order | C7560 "designators must appear in member declaration order" | none | Follow the field order in sections 1 and 2. |
| No `.In` | "A system needs a group in its SystemInfo, for example .In = Fling::Order::Simulation." | `NoGroup` | Set `.In`. |
| A constant list with an empty item | `expression did not evaluate to a constant`, note on `OrderList::EmptyItem` | `OrderListEmptyItem` | Remove the empty item. |
| Group not under Tick or Frame (a group with no `.In`) | "The group of a system must be inside Fling::Order::Tick or Fling::Order::Frame. ..." | `GroupWithoutRoot` | Set `.In` on the group. |
| `.Resim` on a group that is not directly in Tick | "A group sets .Resim but is not directly in Fling::Order::Tick. ..." | `ResimGroupInFinalize`, `ResimGroupInSimulation` | Move the group, or remove `.Resim`. |
| `Update` in a Tick group, or `FixedUpdate` in a Frame group | "A fixed rate system implements void FixedUpdate(Context&) const." and three more for the other cases | `WrongUpdateRate` | Make the function match the root of the group. |
| A fixed system reads or writes a Presentation component | "A fixed rate system cannot read or write a Presentation component. ..." | `FixedReadsPresentation`, `FixedReadsPendingInput` | Drive visuals from the simulation, not the reverse. |
| A variable rate system writes a Simulation component | "A variable rate system cannot write a Simulation component. ..." | `FrameWritesSimulation` | Mark the component Presentation, or move the system to a fixed group. |
| A variable rate system uses `Add`, `Remove`, `Defer`, `Destroy`, `Queue`, `Drain`, `Spawn` or `RawStorage` | "Only a fixed rate system adds, removes, or destroys at the end of the tick ..." and similar, in `Ecs/SystemContext.h` (`Detail::RunsAtRate`) | `FrameDestroy`, `FrameQueue`, `FrameDrain`, `FrameSpawn`, `FrameRawStorage` | Move the work to the right rate. |
| A variable rate system has `Spawns` or `RawStorage` in its access list | "A variable rate system cannot spawn entities." / "... cannot use RawStorage." | `FrameListsSpawns`, `FrameListsRawStorage` | Move the system to a fixed group. |
| A system implements both update functions, the wrong one, or one without `const` | "A fixed rate system does not implement Update. ..." and the others | `FixedWithUpdate`, `FrameWithFixedUpdate`, `FrameWithoutUpdate`, `NonConstUpdate` | Write the one function of the group's rate, as `void F(Context&) const`. |
| No `.Name` | "A system needs a Name in its SystemInfo." | `NoName` | Set `.Name`. |
| A system reaches the command buffer (`ctx.Commands()`) to add or remove a type that it does not write | "cannot access private member" (C2248): the buffer is private, and `Add`, `Remove`, `Defer` check the access list | `CommandsAddUndeclared` | Use `ctx.Add`, `ctx.Remove`, `ctx.Destroy`. |
| A system writes `SimTime`, `InputState`, `RealFrameTime` or `PendingInput` | "No system writes what the runner writes (Fling::RunnerWritten): ..." | `WritesSimTime`, `WritesInputState`, `WritesRealFrameTime`, `WritesPendingInput` | Read them; put derived input (action mapping) in a component of your own. |
| A system asks its context for raw input (`ctx.ReadTickInput()`, `ctx.ReadPendingInput(...)`) | "is not a member" (the context has no input) | `ReadsTickInput`, `ReadsPendingInput` | Declare `Read<InputState>` (fixed) or `Read<PendingInput>` (frame). |
| A `Transient` Simulation component with no `.Rollback` | "A Transient Simulation component must say what a rollback does with it: ..." | `TransientWithoutRollback` | Set `.Rollback` (see [`time-and-ticks.md`](time-and-ticks.md)). |

The other compile checks (member state, access list, components) are in
[`Skills/ecs.md`](../Skills/ecs.md) and the tutorial. Each context call that
the access list must allow has its own case: `QueryUndeclared`,
`WriteThroughRead`, `GetUndeclared`, `HasUndeclared`, `SingletonUndeclared`,
`BlendUndeclared`, `BlendToUndeclared`, `QueueUndeclared`, `DrainUndeclared`,
`AddUndeclared`, `RemoveUndeclared`, `DeferUndeclared`, `SpawnUndeclared`,
`SpawnComponentUndeclared`, `RawStorageUndeclared`, `FilterUndeclared`,
`QuerySingleton`, `SingletonNotOwned`, `QueueMadeByAppender`. Each rule of the
access list itself has one too: `NotAComponent`, `ConstAccessItem`,
`NotAnAccessItem`, `QueueRead`, `AppendNonQueue`, `AccessTagWithData`,
`AccessEmptyTable`, `DuplicateAccess`.

### At schedule build

`BuildSchedule` (`Ecs/Schedule.h`) returns every error at once in
`ScheduleResult::Errors`. It runs in `World::Init`, in tests, and in
`--validate-schedule`, which the build runs after it links Sandbox
(`fling_validate_schedule`), so a bad schedule fails `cmake --build`. On an
error the schedule is incomplete: do not run it.

| `ScheduleErrorKind` | Mistake | Message starts with | Fix |
| --- | --- | --- | --- |
| `MissingName` | No `.Name` on a system or group | "A system has no name." / "A group in the .In of X has no name." | Set `.Name`. |
| `DuplicateName` | Two systems with one name | "Two systems have the name ..." | Rename one. |
| `AmbiguousSystemType` | Two systems with different names and one `SystemTypeId` (two classes with one name in two anonymous namespaces) | "SystemOf<X> matches both A and B. ..." | Give one class a unique qualified name. |
| `MissingGroup` | A hand-built `SystemDesc` with no `.In` | "System X has no group. ..." | Set `.In`. |
| `DuplicateGroup` | Two different groups with one name | "Two different groups have the name ...: their F differs. ..." | Declare a group once; use unique names. |
| `MissingParent` | A group other than Tick and Frame with no `.In` | "Group X has no .In. Put it in a group, ..." | Set `.In`. |
| `WriteConflict` | Two writers of one type with no path between them | "A and B both write T in ..., and neither orders the other. Fix: add .After = Fling::SystemOf<...> to B's SystemInfo (or the reverse), order their groups, or call RunBefore()." | Add the order that the message names. |
| `ConsumeConflict` | Two consumers of one queue | "A and B both consume Q ..." | One consumer per queue. |
| `Cycle` | A loop of edges, or a group inside itself through `In` | "The Fixed groups and systems make a loop: ... Reasons: ..." | Remove one of the listed orders, or split a system. |
| `OrderTargetNotAdded` | `SystemOf<T>` of a system that was not added | "... SystemOf<T>, but no system of that type was added to this schedule. ..." | Add the system, or order against a group (legal when empty). |
| `EmptyOrderRef` | An empty reference in a run-time list or in `RunBefore` | "... with an empty reference. ..." / "... has an empty reference. ..." | Remove it. |
| `OrderListTooLong` | A list made at run time with more than eight items | "X has .After with more than 8 items. Only the first ones are kept. ..." | Use a group. |
| `OrderAcrossUpdateRates` | An order between fixed and variable work | "... orders A (Fixed) before B (Variable). Fixed rate and variable rate work are not ordered against each other. ..." | Remove the order: the frame sees what a tick wrote through the published copy. |
| `UpdateRateAccess` | A hand-built desc that breaks a compile-time rate rule | "X (...) ... A System<> class with this access list does not compile." | Fix the access list. |
| `FlushNotOrdered` | A system not ordered against a `FlushAfter` group | "Group G has .FlushAfter, but nothing orders it against ... (system S). ..." | Add `.After` or `.Before` to one of the groups. |
| `FlushInVariableGroup` | `FlushAfter` on a variable rate group | "Group G is a variable rate group with .FlushAfter. ..." | Remove it. |
| `ResimPlacement` | `.Resim` on a group that is not directly in Tick | "Group G sets .Resim, but it ..." | Remove `.Resim`, or set `.In = Fling::Order::Tick`. |
| `ResimGap` | A non-resim fixed system that the graph does not put before every resim system or after every one | "S (group G) is not resimulated, but it does not run before A or after B, which are. ..." | Put it in Simulation, set `.Resim` on its group in Tick, or order it before Simulation or after ResolveDeferred. |
| `ResimSkipsState` | A non-resim fixed system writes a type that a rollback restores, or a type that a resim system that runs before it reads | "S ... writes T, which a rollback restores, but a resimulation does not run it, ..." | Move the system into a resim group, or make the type `Rebuild`. |
| `ResimAppendsOutput` | A resim system appends to a queue that no resim system consumes (`AppRequests`, or a queue consumed in Finalize) | "S (group G) appends to Q, but no system consumes it: ..." or "..., but C consumes it outside the resimulation. ..." | Put the system in `Order::Input`, or consume the queue in a resim group. |

The `WriteConflict` fix names the class from `SystemDesc::TypeName`, with
`class `/`struct ` removed, for example
`.After = Fling::SystemOf<Sandbox::LookCameraSystem>`. A hand-built desc with no
type name says `add Test.Mover to the .After of Test.Rotator`.

## 6. Determinism

The schedule must be the same on every run, every platform and every build.

- **Inside one stage, systems run by name.** Not by `Add` order, link order or
  address (test "Ties are broken by name, whatever the add order" tries every
  permutation). The stage comes from the graph only.
- **Groups are known by name**, so two copies of a group in two shared libraries
  are one group.
- **A system is known by its type hash**, `SystemTypeId<T>()`, which does not
  depend on the class key that a file saw.
- **The schedule is a checked-in file.** Test "The engine and physics schedule
  matches the checked-in file" (`FlingTests/Physics/PhysicsOrderTests.cpp`)
  builds `RegisterEngine` + `RegisterPhysics`, needs an empty `ErrorReport()`,
  and compares `Dump()` with `FlingTests/Physics/Schedules/EnginePhysics.schedule.txt`.
  On a change it writes `EnginePhysics.schedule.actual.txt` in the working
  directory and fails with the first differing line. Copy the file over the
  checked-in one when the change is wanted. Review then shows the new order as a diff.

Never let these depend on thread, registration or memory order: the run order,
the stages, the order of items in a query (storage order), the flush (sorted by
entity, then by call order), and anything a system decides. One tie can still
show in game state: two appenders to one queue with no path between them push
in run order, so by stage, and by name inside one stage. No rule checks this
yet.

## 7. Rules of use

1. One order header for each module: groups and forward declarations only.
2. A system header includes order headers, never another system's header.
3. Name the engine from a game, never a game from the engine.
4. Across modules, order against a group. Use `SystemOf<T>` only for a system
   that the other module's order header declares.
5. If a system may split later, publish a group around it now.
6. Put a game group inside the engine group where its work belongs, not loose in Tick.
7. Order a system only where data does not already do it, or where data would
   give the wrong order. Most systems need no `.After`.
8. When two modules do not know each other, the application orders them with
   `SystemRegistry::RunBefore(first, second)`.

## 8. Common mistakes

| Mistake | What happens |
| --- | --- |
| A game group directly in Tick, between Simulation and ResolveDeferred, with no `.Resim` | `ResimGap` as soon as a resimulated system runs after it. |
| `.Resim = true` on a group inside Simulation | `ResimPlacement`: it is resimulated already. |
| A game system in Finalize that writes game state | `ResimSkipsState`: a replayed tick would not make it again. |
| An order against your own group or parent (`S.In = G, S.After = G`) | A `Cycle` with a correct path but no specific message. |
| `A.Before = B` where `B` is defined below `A` | Does not compile. Write `B.After = A`. |
| Two writers ordered only because their groups are nested | A child group and its parent do not order each other's systems: `WriteConflict`. Order them. |
| Expecting a data edge against a stated order | There is none. The reader is one tick late; check `one tick late` in the dump. |
| Relying on `.Name` in an order | Order uses the class. Renaming `.Name` changes the order inside a stage and the dump only: no rule depends on it. |
| A system that sets a policy for a non-input reason, before the system that folds the input | The frame side snaps by that tick's input. Run it after. See [`input-two-sided.md`](input-two-sided.md). |
| Side effects (audio, app requests) in a resimulated group | A resimulation would repeat them. A queue drained outside the resimulation is the error `ResimAppendsOutput`; a log line is not checked. |

## 9. Testing

Three kinds of test, all in `FlingTests/`.

**Real systems.** Add real classes to a `SystemRegistry` and call
`BuildSchedule()`. This runs `CheckSystem` too. Examples: the vehicle test above
and test "A nested group runs before a system of its parent through SystemOf"
in `FlingTests/Engine/EcsScheduleTests.cpp`.

**Hand-built systems.** `EcsScheduleTests.cpp` has a `FakeSystems` helper that
makes `SystemDesc`s from a `SystemInfo` and access rows, and calls
`BuildSchedule(ScheduleInput{...})`. A fake system that an order names needs a
type, so each gets a tag type in `namespace Tag` and is added with
`Add<Tag::X>`. From test "An order through a third system orders two writers":

```cpp
FakeSystems systems;
systems.Add(SystemInfo{.Name = "Test.Spawn", .In = Order::Simulation, .Before = SystemOf<Tag::Rotator>},
            {{"Transform", W}});
systems.Add<Tag::Rotator>(Sim("Test.Rotator"), {{"Transform", W}});
systems.Add(SystemInfo{.Name = "Test.Mover", .In = Order::Simulation, .After = SystemOf<Tag::Rotator>},
            {{"Transform", W}});

const ScheduleResult result = systems.Build();
REQUIRE(result.ErrorReport() == "");
REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Spawn", "Test.Rotator", "Test.Mover"});
```

Nothing ties a tag to the name: `Add<Tag::B>(Sim("Test.A"))` compiles. Keep the
two the same by hand. Hand-built descs skip `CheckSystem` and component domain checks.

**Compile-fail checks.** For a rule that must not compile:

1. Add a `#elif defined(FLING_ECS_CASE_<Name>)` block to
   `FlingTests/EcsCompileFail/EcsCompileFail.cpp`. Change one thing about a good
   system or group. If the case defines a `BadSystem`, also
   `#define FLING_ECS_HAS_BAD_SYSTEM`, so that `main` adds it.
2. Add `fling_add_ecs_compile_check( <Name> "<regex>" )` to
   `FlingTests/EcsCompileFail/EcsCompileChecks.cmake`, the one list that every
   build of the tests includes. The test passes only if the build fails and its
   output matches the regex. Match the `static_assert` text, or for a typo, the
   misspelled name in each compiler's wording: MSVC `'X': is not a member`,
   GCC `'X' is not a member` or `has no member named 'X'`, Clang `no member
   named 'X'` (the existing cases show the form).
3. Check that the log has exactly one error. An unguarded check can cascade
   (an unrooted group once gave four errors).

## 10. Time and threads

The order API decides only the order inside one update rate. When a tick runs,
which input it takes, and how it is recorded are in
[`time-and-ticks.md`](time-and-ticks.md). Which thread runs each root, and how
`Schedule::Published` becomes the copy that the frame side reads, are in
[`threading-model.md`](threading-model.md). The schedule needs no change for
threads: the root of a group is its thread.
