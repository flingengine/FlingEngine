# ECS: components, systems, and the schedule

A step-by-step guide with a worked example, for people new to the ECS:
[`Docs/ecs-tutorial.md`](../Docs/ecs-tutorial.md). This page is the reference.
The whole runtime model (order, ticks, input, the game and presentation
threads) is in [`Docs/architecture-overview.md`](../Docs/architecture-overview.md).

This page explains how to write gameplay and engine code in the Fling ECS. The
design and its history are in [`Plans/ecs-systems.md`](../Plans/ecs-systems.md) (the first
slice) and [`Plans/ecs-plan.md`](../Plans/ecs-plan.md) (the full plan). The
rules are from Timothy Ford, *'Overwatch' Gameplay Architecture and Netcode*,
GDC 2017.

The code is in `FlingEngine/Engine/inc/Ecs/`. `Ecs/Query.h` is the one query
type: `ctx.Query` returns it, and `EntityStorage::View` takes its entities from it. Each `World` owns one
`EntityStorage` (`Ecs/EntityStorage.h`): the entities, components, and
singletons of that world. Code outside a system (the renderer, physics, the
editor, tests) takes an `EntityStorage&`. It never takes a global registry.
Systems do not use `EntityStorage`. They use their typed context.

## Storage

- Entities with the same set of Table and Tag components share an
  archetype. Its rows are in chunks that grow from 1 KiB to 128 KiB, one
  64-byte aligned column per component (`Ecs/detail/NativeStorage.h`). Big
  chunks keep the hardware prefetcher busy. Small archetypes stay small.
- **Iterating is cheap. Adding and removing Table components is not.** A
  structural change moves the whole row (about 19 ns). Use deferred `ctx.Add`
  and `ctx.Remove` (batched for each entity). For components that come and go
  every few ticks, use `Sparse` or `Enableable`.
- **Make an entity with all its components at once:** `storage.Create(a, b, c)`
  and `ctx.Spawn(a, b, c)` put it straight in its archetype, with no move per
  component. The Added hooks run after every component is in place.
- **Change many entities with one call:** `storage.AddToAll(value,
  With<...>{}, Without<...>{})` and `storage.RemoveFromAll<T>(filters...)` move
  whole chunks, one copy per column (about 2 ns per entity). A system calls them
  inside `ctx.Defer<T>(...)`. For a Sparse type, or a type with hooks, the
  engine does one entity at a time. Do not call them while a query iterates.
- In a hot loop, `ctx.Query<...>().Each(fn)` is the fastest form. It is a plain
  array loop for each chunk. The engine reads the Entity column only if `fn` takes
  an `Entity`. The range-for (`for (auto [e, a] : ctx.Query<A>())`) is almost as
  fast.
- **Adding or removing a Table component moves the entity's row.** A
  reference to one of its components is not valid after that. Keep the
  `Entity`, and `Get` the component again. Build the whole component and then add
  it. Do not add it and then write through a reference after more adds.
- Set the storage in the declaration with `.Storage`:
  - `Table` (the default): the fastest to iterate.
  - `Enableable`: `SetEnabled<T>(e, false)` turns it off without a move.
    Queries and views skip entities where it is off.
  - `Sparse`: beside the archetypes, at a stable address. Adding and removing it
    never moves the entity. For components that come and go often on few
    entities. A query that needs a Sparse component (`ctx.Query<Burning>()`, or
    `With<Burning>`) walks the entities that have it, so its cost follows the
    number of those entities, not the size of the world.
- At the flush, the engine groups the deferred `ctx.Add` and `ctx.Remove` calls
  from all systems by entity. An entity moves once for each tick, if one system
  or many systems change it.
- A query sees the archetypes and rows that exist when it is made. A loop does
  not see an entity that the loop spawns.
- To make a query costs the same with 10 systems or 1000. The storage keeps the
  list of archetypes for each kind of query, finds it by hash, and adds a new
  archetype to the lists when the archetype is made.
- `EntityStorage::Stats()` gives the archetype, chunk, and entity counts, the
  number of row moves, and the bytes that the world reserves and uses.

## Memory

- The storage does not use `new` or `malloc` for entities or components. Each
  `EntityStorage` owns a `BlockAllocator` (`Core/inc/Allocators/`): power-of-two
  blocks from 1 KiB to 128 KiB. The blocks come from slabs of OS pages. Each size
  has a free list, so the allocator uses a freed block again for the same size
  and memory does not fragment.
- The slabs of the 128 KiB blocks are one 2 MiB large page each
  (`VirtualMemory::UseLargePages`, Linux only), so a big world has few page
  faults and few TLB entries.
- A chunk is one block: its header, the column pointers, the enabled bits, the
  Entity column, and one 64-byte aligned column per component (structure of
  arrays). A big chunk starts its layout at one of 16 cache lines. This stops all the
  headers from using the same cache set.
- Sparse components come from a `SlotPool` per type (stable addresses, a free
  list). The table from entity to value is in 16 KiB pages that are made when
  an entity in their range gets the component, so its size follows the number
  of entities that have the component, not the size of the world. Singletons
  come from the same `BlockAllocator`.
- **The allocators never return null.** If the OS has no memory, they log who
  asked and stop the program (`VirtualMemory::OutOfMemory`), in every build.
  Code that uses them does not check for null.
- **Alignment.** A component can be aligned to 4096 bytes at most (the compiler
  checks). Each column starts on a 64-byte line, and on the component's own
  alignment if that is more.
- **Platforms.** Pages come from `mmap` or `VirtualAlloc`. The code reads the
  page size from the OS (4 KiB on x86, 16 KiB on Apple silicon, up to 64 KiB on
  some ARM Linux) and does not assume one. On Windows an aligned block reserves
  more address space than it uses, because Windows cannot give back part of a
  reservation.
- Deferred `ctx.Add` values and `ctx.Defer` callables go in the `LinearArena` of
  each command buffer. The arena resets after the flush. The flush uses scratch
  lists that keep their memory. A query keeps its row counts on a list that
  each thread has, which also keeps its memory.
- A steady tick takes no memory from the global heap.
  `FlingTests/Engine/EcsAllocationTests.cpp` shows this with a counting
  `operator new`. Make sure this test passes. Do not make a `std::vector`,
  `std::function`, or `std::string` in each tick in the storage, the runner, or a
  query.
  (`Drain` of a Queue component still gives away the vector of the queue.)
- The world keeps its pages while it lives. After a big change, such as a level
  unload, call `EntityStorage::TrimMemory()` to give empty slabs back to the OS.
- Use the `Fling::Math` helpers (`MathUtils.h`: `Min`, `Max`, `Clamp`,
  `AlignUp`, `IsPowerOfTwo`, ...) and the Fling math types, not `std::min`,
  `std::max`, or `std::clamp`, in engine code.

## The rules

1. **Components are data. Systems are functions with no state.** A component
   has no behavior, except small accessors. A system has no member variables.
   `SystemRegistry::Add` does not compile if a system has one.
2. **System state goes in a singleton component.** The engine makes every
   singleton that a system names.
3. **Systems never call systems.** Put shared code in a `*Util` free
   function. Only the `.cpp` of a system, the register file of its module, and
   tests include a system header (`scripts/check_system_includes.py` checks
   this in CI).
4. **Defer big side effects.** Put spawns, damage, physics requests, and requests
   to the application in a Queue component. One system (or the engine)
   consumes each queue.
5. **No globals in simulation code.** Read `SimTime`, not `Timing`. Read
   `InputState`, not `Input::Get()`. Draw random numbers from a `SimRandom`
   (`Core/inc/SimRandom.h`). A debug build asserts if a fixed rate system calls
   `Timing`, `PlatformTime::NowNanoseconds`, the global `Random` or a query of
   `Input` (`SimulationCheck`, `FLING_SIMULATION_FORBID`). A release build
   compiles this check out, so it is not enforced there. Raw tick input is a
   compile-time rule: the system context has no input.
6. **Only exact math in simulation code.** Windows and Linux must simulate the
   same bits (a Linux server can run a Windows client's game). IEEE 754 defines
   `+`, `-`, `*`, `/`, `sqrt`, `floor`, `fmod` and comparisons exactly; it does
   not define `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`,
   `log` or `pow`, and each C library rounds them its own way. Simulation code
   (fixed systems, what they call, level loading) uses `SimMath`
   (`Core/inc/SimMath.h`) for those, and its `AngleAxis`, `FromYawPitchRoll`,
   `ToYawPitchRoll` and `Slerp` instead of `glm::angleAxis`, `glm::rotate`,
   `glm::yawPitchRoll`, `glm::extractEulerAngle*` or `glm::slerp`, which call
   the C library inside. Presentation code may use std and glm freely. The
   ctest "Simulation math is the same on every platform"
   (`FlingTests/SimMath/CheckSimMath.cmake`) fails when a source outside its
   presentation list names a banned function. Every first-party target builds
   with floating point contraction and fast math off
   (`fling_set_deterministic_math`). Iterate an unordered container only where
   the order cannot change the result, and give `std::sort` a full order (or
   use `std::stable_sort` on an input in a fixed order): the Microsoft and GNU
   standard libraries differ in both.

## A component

```cpp
struct Mover
{
    static constexpr Fling::ComponentInfo Component{ .Name = "Mover" };

    glm::vec3 TargetPos{ -10.0f, 0.0f, 0.0f };
    float Speed = 1.0f;
};
```

A component has no functions. You describe its fields one time, in the
register file of the module (or in a feature `.cpp` that the file calls):

```cpp
void Game::Register(Fling::ModuleRegistration& t_Modules)
{
    t_Modules.Component<Mover>([](Fling::TypeBuilder<Mover>& t_Type) {
        t_Type.Category("Movement");
        t_Type.Field("targetPos", &Mover::TargetPos);
        t_Type.Field("speed", &Mover::Speed).Range(0.0f, 100.0f).Tooltip("Units per second");
    });
    t_Modules.System<MoverSystem>();
}
```

Save, load, the play snapshot, and the editor inspector all use that
description.

- `.Name` is the saved name in level files. To rename, change it and add the old name with `t_Type.FormerNames`.
- `.Kind`: `Table` (the default, one per entity), `Tag` (an empty struct),
  `Singleton` (one per world), or `Queue` (a singleton of requests, with a
  container named `Requests`).
- `.Domain`: `Simulation` (the default, written at the fixed tick) or
  `Presentation` (written at the frame rate; the simulation never reads it).
- `.Transient = true`: the engine never saves it to a level. Register it with
  `t_Modules.Component<T>()`, with no fields unless a rollback restores it.
- `.Rollback`: what a rollback does with it. `RollbackClass::Restore` (the
  default: game state, saved per tick, restored, and hashed), `Rebuild` (made
  again each tick: `InputState`, `SimTime`, `LocalToWorld`), `Keep` (left alone:
  settings, scratch), or `Subsystem` (a module saves it: `PhysicsWorld`). A
  `Transient` Simulation type that is not a queue must set it, or it does not
  compile. The state hash covers exactly the `Restore` Simulation types
  (`IsRollbackState`), with every member: a `Restore` type must reflect all its
  members (a `Transient()` field is not saved but still hashed), and a system
  that uses an unregistered `Restore` type stops `World::Init`. See
  [`Docs/time-and-ticks.md`](../Docs/time-and-ticks.md).
- `.External = true` (singletons and queues): code outside the systems makes
  and removes it, so the engine does not (the physics module makes
  `PhysicsWorld`). See "Singletons and queues" below.
- **Field types:** `bool`, `int32`, `uint32`, `int64`, `uint64`, `float`,
  `double`, `String`, `Name`, `glm::vec2/3/4`, `glm::quat`, enums, and `Entity`.
  For another type, specialize `Fling::FieldTraits` (`Kind = Custom`, with
  `Save` and `Load`). `TypeBuilder::Serialize(save, load)` is the escape hatch
  for a whole type (`MeshRenderer` uses it for its asset paths).
- **Field options:** `Range`, `Tooltip`, `Color`, `Hidden`, `ReadOnly`,
  `Transient` (the engine does not save it), and `Enum({{"name", Value}, ...})`. An enum with a
  table saves by name, so it is safe to change the order.
- **Type options:** `Category`, `Partial` (the engine saves only some members, on
  purpose; an error on a `Restore` type), and `AfterLoad(fn)` for derived data.
  `AfterLoad` also runs after an editor change.
- **An `Entity` field** saves as the `EntityGuid` of the target. When the world
  saves, an entity with no `EntityGuid` gets one in the document (a level file
  save also keeps it on the entity; the play snapshot changes nothing). When it
  loads, it maps the ids back to the new entities. `EntityGuid` is not
  simulation state (`RollbackClass::Keep`, not hashed).
- **Changing a saved component (back compatibility):**
  - Add a field: do nothing. An old file keeps the default.
  - Remove a field: `t_Type.Removed("oldName")`, so that load does not show a warning.
  - Rename a field: `.FormerNames({"oldName"})` on the field.
  - Rename a component: `t_Type.FormerNames({"OldName"})`.
  - Delete a component: `t_Modules.RemovedComponent("OldName", convert)`.
  - Change the meaning or shape: `t_Type.Version(2).Upgrade(1, fn)`. The
    upgrade rewrites the old JSON object before the fields load. The engine
    saves the version as `"$v"` when it is more than 1. Write each upgrade one
    time. Do not change it.
- **The saved name must end with the C++ type name**, after an optional
  prefix and a dot: `"Health"` or `"MyGame.Health"` for `struct Health` (for a
  template, the last part starts with the name: `"MyGame.Part3"` for
  `Part<3>`). `ComponentInfoOf` checks it with a `static_assert`, so a C++
  rename without a `.Name` change does not compile.
- **Required components.** `using Requires = Fling::TypeList<Fling::Transform>;`
  on the struct says that an entity needs a `Transform` to have this component.
  See "Required components" below.
- **Names must tell types apart.** `World::Init` fails (and
  `--validate-schedule` fails the build) when two C++ types have one saved
  name, or a former name is another type's name, a removed component's name,
  or claimed by two types. The storage aborts if two types with one saved name
  get a component id, in every build.

## Required components

Some components do nothing alone. A `PointLight` with no `Transform` has no
place, so no query finds it and nothing tells you why. Say what a component
needs on the struct:

```cpp
struct PointLight
{
    static constexpr Fling::ComponentInfo Component{.Name = "PointLight"};
    using Requires = Fling::TypeList<Fling::Transform>;
    ...
};
```

- **Make the entity whole.** `storage.Create(Transform{}, PointLight{})` and
  `ctx.Spawn(...)` do not compile if a required component is not in the call.
  This is the normal way to make an entity, and the compiler checks it.
- **One at a time: required first, removed last.** `Add<PointLight>` on an
  entity with no `Transform`, and `Remove<Transform>` from an entity that has a
  `PointLight`, are errors. A debug build asserts. Every build logs the two
  names, one time. The deferred `ctx.Add` and `ctx.Remove` have the same rule
  when they run. `Destroy` needs no order.
- **The editor** adds the required components with the component, and does not
  remove a component while another one on the entity requires it.
- **A level file** that has a component without what it requires still loads.
  The missing component gets its default values, and the log has a warning. A
  component loads after the components that it requires, so its Added hook
  sees them with their saved values.
- **No loops.** Two components that require each other do not compile. A longer
  loop is an error when the world starts (and fails the build, through
  `--validate-schedule`). Make them one component.
- Only Table and Tag components (not Sparse ones) can require, or be required.
- Keep the lists short. Require what the component cannot work without, not
  what it is often used with.

## The life of an entity in a tick

The rules are Timothy Ford's: make entities at once, and destroy them later.
Overwatch first made entities at the end of the frame, and got bugs where one
system made an entity and the next system could not find it.

| What | When it happens | Who sees it |
|---|---|---|
| `ctx.Spawn(a, b, c)` | At once | Every query that is made after it, in this tick. Not the loop that is running |
| `ctx.Destroy(e)` | At the end of the tick | The entity and its handle stay good for the whole tick. The next tick does not have it, and the frame that is drawn after the tick does not draw it |
| `ctx.Add(e, c)`, `ctx.Remove<C>(e)`, `ctx.Defer` | At the end of the tick | The next tick |
| A queue (`ctx.Queue<Q>().Push`) | When its consumer runs | This tick, if the consumer runs after the appender. The schedule makes that so, unless group order or a stated order says the opposite |
| `LocalToWorld` | `Engine.Transform`, in `Order::Finalize`, the last fixed group | Variable rate systems and the renderer in this frame. Fixed systems in the next tick |

How to keep a delay of one tick out of your code:

- **Spawn the entity whole.** Give `ctx.Spawn` every component the entity
  needs. A component that you add later with `ctx.Add` is not there until the
  end of the tick, so the systems after you see half an entity. Required
  components make the compiler check the parts that must be there.
- **A slot is not used again in the tick that frees it.** Destroy is at the end
  of the tick, so a handle cannot point at a different entity in the middle of
  one. Keep an `Entity`, and use `TryGet`: it gives null when the entity is gone.
- **A new entity has no `LocalToWorld` until the end of its first tick.** It is
  never there with a wrong value. A fixed system that needs the world position
  of an entity in the tick that made it reads `Transform` (for a root), or
  calls `TransformUtil::WorldFromParents`. The renderer does this itself, so an
  entity that the editor or a level load makes between ticks is drawn in its
  right place in the same frame.
- **A reader in an earlier group gets the data of the tick before.** Where
  nothing else orders them, the schedule puts the writer first. Group order and
  `.Before` can put the reader first, and that is legal. `--dump-schedule` lists every such read
  under its system, as `one tick late: reads ContactEvents before Physics.Step
  writes it`. Read that list when a value seems late.
- **Removed hooks run one time.** When an entity is destroyed, each component's
  Removed hook runs one time, also if another hook removes that component
  during the destroy.

## A system

Each system is a header in `inc/Systems/<Name>System.h` and a body in
`src/Systems/<Name>System.cpp`:

```cpp
// Sandbox/Gameplay/inc/Systems/MoverSystem.h, which includes "SandboxSystemsFwd.h"
class MoverSystem final : public Fling::System<MoverSystem,
                                               Fling::Access<Fling::Read<Fling::SimTime>,
                                                             Fling::Read<SandboxToggles>,
                                                             Fling::Write<Mover>,
                                                             Fling::Write<Fling::Transform>>>
{
public:
    static constexpr Fling::SystemInfo Info{
        .Name = "Sandbox.Mover", .In = Fling::Order::Simulation, .After = Fling::SystemOf<RotatorSystem>};

    void FixedUpdate(Context& ctx) const;
};
```

- **Group and order.** `.In` is the group (`Fling::Order::Simulation`). `.After`
  and `.Before` take a group or `Fling::SystemOf<T>`, or a braced list of up to
  eight. `T` needs only a forward declaration, from the order header of its
  module (`SandboxSystemsFwd.h` declares `RotatorSystem`). Never include another
  system's header to order against it. The full reference is
  [`Docs/system-order-api.md`](../Docs/system-order-api.md).
- **Access list.** `Read<T>`, `Write<T>` (implies read), `Append<Q>`,
  `Consume<Q>`, `ReadBlend<T>` (frame systems, next to `Read<T>`), `Spawns`,
  and, for engine bridges to a subsystem only, `RawStorage`. Use each type one
  time.
- **Interpolation (the blend).** A frame system that draws between ticks
  declares `ReadBlend<T>` next to `Read<T>`, reads `ctx.BlendFrom<T>(e)` and
  `ctx.BlendTo<T>(e)` (T at the two ends of the frame's blend; a new entity
  gives its latest value), and blends from the first toward the second by
  `RealFrameTime::BlendAlpha` with the helpers in `Interpolate.h` (`Lerp`,
  `Slerp`, `Blend`, `BlendedWorld`). The simulation publishes after every tick,
  and each packet carries the latest two ticks of the blended types (`BlendTo`
  is tick N, the value `Read<T>` gives; `BlendFrom` is tick N - 1). The factor
  is the renderer's own (`FrameBlend`): the drawn point moves on by each frame's
  time over the tick length and stays between the two ticks. The drawn state is
  at most one tick behind the latest published tick, with no added margin, and
  the frame side never waits for the simulation. Blend the two ends, not
  `BlendFrom` toward the query's value. The schedule holds values only
  for the declared types, in every exec mode
  ([`Docs/threading-model.md`](../Docs/threading-model.md), "The two blend states";
  [`Docs/time-and-ticks.md`](../Docs/time-and-ticks.md), "The blend factor").
  Compile errors: in a fixed system ("Only a frame system (under
  Fling::Order::Frame) declares ReadBlend<T>"), of a Presentation type
  ("ReadBlend<T> needs a Simulation component"), of a singleton or tag
  ("ReadBlend<T> takes a Table component"), with no `Read<T>` ("so the list
  also needs Read<T>"), and `ctx.BlendFrom<T>` with no `ReadBlend<T>`
  ("blends a component that its Access<...> does not allow").
- **One entity.** `ctx.TryGet<T>(e)` (null if the entity is gone or has no
  `T`) and `ctx.Get<T>(e)` return Table components. `ctx.Has<T>(e)` checks for a
  Table or Tag component and needs only `Read<T>`.
- **The context checks the access list at compile time.** `ctx.Query<T>()` needs
  `Write<T>`, and `ctx.Query<const T>()` needs `Read<T>`. The same applies to
  `Get`, `TryGet`, `Singleton`, `Queue`, `Drain`, `Spawn`, and `RawStorage`.
- **Hooks.** `FixedUpdate(Context&)` for a group in `Order::Tick`,
  `Update(Context&)` for a group in `Order::Frame` (use only one of them). Optional: `OnCreate(Setup&)`,
  `OnDestroy(Setup&)`, `OnPlayBegin(Context&)`, `OnPlayEnd(Context&)`.
- **Singletons and queues.** When the world starts, the engine makes each
  singleton that a system has `Read<T>` or `Write<T>` for, and each queue that
  a system has `Consume<Q>` for, if it does not exist yet, with its default
  values, before any `OnCreate`: a `Simulation` type in the simulation storage
  (a mirror gets it from the publish), a `Presentation` type in the frame
  storage. It removes them after every `OnDestroy`. So most systems have no
  `OnCreate`. A writer writes one, and calls
  `setup.AddSingleton<T>(values...)`, only for a singleton that starts with
  other values or has no default constructor. The engine does not make a type
  that is `.External`. A singleton that systems only read (a setting) is
  valid: it has its default values until the application or a module sets
  it, for example in `Game::Init`. The schedule dump lists it under
  "Singletons that no system writes".
- **Update rates.** The root of the group gives the rate, and in threaded play
  the thread: `Order::Tick` on the game thread, `Order::Frame` on the main
  thread. A system never names a thread.
  - A fixed rate system cannot read or write `RealFrameTime` or any `Presentation`
    component.
  - A variable rate system cannot write a `Simulation` component, spawn, add,
    remove, destroy, or append to a queue.
  - No system writes `SimTime`, `RealFrameTime`, `InputState` or
    `PendingInput`: the runner writes them.
- **Destroy is deferred.** `ctx.Destroy(e)` destroys the entity and its
  children at the end of the tick. It is the one structural change that needs
  no access: it removes every component of the entity, whatever their types.
  **Spawn is immediate.** Use
  `ctx.Spawn(components...)`.
- **Structural changes are deferred:** `ctx.Add(e, value)` and
  `ctx.Remove<T>(e)` need `Write<T>`. They run at the end of the tick in the
  order of the calls. `ctx.Defer<Ws...>(fn)` runs a change that keeps several
  types in step. It names every type that it writes, but `fn` gets the whole
  storage, so the compiler cannot check that list: like `RawStorage`, it is an
  escape hatch. Prefer `Add` and `Remove`.
- **Register it.** One line in the module's register file
  (`Register<Module>.cpp`): `t_Modules.System<MoverSystem>();`.

## Transforms and the hierarchy

- `Transform` is the local position, rotation (a quaternion), and scale. It
  has no functions. Use `TransformUtil` (`LocalMatrix`, `FromEulerDegrees`,
  `ToEulerDegrees`, `RotateAround`, `Forward`).
- `Parent` (saved) links a child to its parent. `Children` (transient) comes
  from it. To change a parent, use `HierarchyUtil::SetParent` only (or
  `SetParentDeferred` from a system, with `Write<Parent>` and `Write<Children>`).
  This keeps the two in step. `Engine.Transform` finds children through
  `Children`. If a `Parent` was set by hand and `Children` does not agree, the
  matrices are still right, but the system uses a slower path and logs a
  warning one time.
- `Engine.Transform` runs in the `Engine.Finalize` group, after `Engine.ResolveDeferred`, on
  every tick and in every world role. It writes `LocalToWorld` (the world
  matrix, parents first). When the matrix changes, it increases `Version`.
  Read world positions from `LocalToWorld`, not from `Transform`.
- Physics treats a body's `Transform` as its world pose, so a physics body
  must be a root entity.

## Threads, determinism, and rollback

One system runs at a time. In threaded play (the default; `--exec`) the fixed groups run on a
game thread and the frame groups on the main thread, on a mirror of the
published simulation data ([`Docs/threading-model.md`](../Docs/threading-model.md)).
The per-tick ledger is the same in every exec mode and at any frame rate
([`Docs/time-and-ticks.md`](../Docs/time-and-ticks.md)). Ticks run at
`SystemRunnerConfig::SimHz` (default 30; `--sim-hz`, `[Engine] SimHz`): a fixed
system takes its step from `SimTime::DeltaSeconds()` and assumes no rate: the
rate can change at a tick boundary while the game runs (a `SetRate` clock
command, [`Docs/time-and-ticks.md`](../Docs/time-and-ticks.md), "Changing the
rate while the game runs"), so read the delta each tick and use
`SimTime::ElapsedNs`, never `Tick * FixedDeltaNs`. The main thread's frames
are capped by `--max-fps` (`[Engine] MaxFps`; default the monitor's refresh
rate, else 60, and no cap with `--null-render`). The design is also for
systems that run at the same time ([`Plans/ecs-parallel.md`](../Plans/ecs-parallel.md))
and prediction with rollback ([`Plans/ecs-plan.md`](../Plans/ecs-plan.md),
"Resimulation"). Keep these properties when you change the ECS:

- **A read does not write.** `ctx.Query`, `Get`, `TryGet`, `Has`, and
  `Singleton` do not change the storage. Systems in one stage have no edge
  between them, so they can do these at the same time
  (`FlingTests/Engine/EcsQueryTests.cpp` runs queries from eight threads, and
  passes under ThreadSanitizer). The first query of a new kind adds a list to
  the storage's cache, behind a lock.
- **Structural changes happen on one thread.** Deferred adds, removes, and
  destroys run at the flush, in schedule order. `ctx.Spawn` is immediate, so a
  future parallel executor would run a system with `Spawns` alone.
- **Order never comes from memory addresses, hash tables, or threads.** Entities
  in a query come in storage order, which depends only on the order of the
  changes that made them. The schedule runs systems by stage, and by name inside
  one stage. The flush sorts by
  entity and then by the order of the calls.
- **The simulation reads no globals.** Time is `SimTime`, input is
  `InputState`, and a random number comes from a `SimRandom` stream or from
  state in a component.
- **The simulation never reads the frame side.** So the frame side can run on
  another thread from a copy, and the tick does not change.
- **The renderer reads only `RenderSnapshot`.** No render code holds a pointer
  into the storage, so the simulation can run while a frame is drawn.
- **State for rollback is component data.** To restore an entity, write its
  components back. Do not keep simulation state in a system, a static, or a
  hook.

Not done yet, and the API already allows for them:

- `ctx.Queue<Q>().Push` writes to the one queue. Before two appenders of one
  queue run at the same time, each needs its own buffer, merged in schedule
  order. The call does not change.
- A resimulation's systems are known (`GroupInfo::Resim` on Simulation, Physics
  and ResolveDeferred, `Schedule::IsResim`, `Schedule::ResimRunOrder`), with
  what they read from outside (`Schedule::ResimInputs`), and the runner does not
  run one yet. State snapshots are not built.
- A rollback that destroys and makes entities again gives them new places in
  storage. Two peers must make and destroy the same entities in the same order,
  or logic that depends on the order of a query must sort by an id that both
  peers share.

## Who owns what

- The engine owns the `World` and the `Game` as `std::shared_ptr`. The world
  shares the game, so the game lives as long as the world calls it.
- A reference back to an owner is a `std::weak_ptr`: the game to its world, and
  the editor and its panels to the world and the game. Lock it where you use
  it, and do not keep the locked pointer.
- Inside a world, the storage, the systems, the type registry, and the runner
  are members of the world. They refer to each other with references, and the
  order of the members is the order they are made and destroyed in.
- A raw pointer is for a lookup that can fail and is used at once
  (`TryGet<T>(e)`, `TypeRegistry::Find`), and for data that lives as long as the
  program (`SystemDesc`, `GroupInfo`). Do not keep a component pointer: keep
  the `Entity`.
- The memory of entities and components is not owned through pointers at all.
  It is blocks from the world's `BlockAllocator`, and the storage runs the
  destructors.

## Rendering

- The renderer never reads the world, apart from the `CameraView` singleton
  (the game's view, written by its frame system). `Graphics.PublishSnapshot` (a
  variable rate system in the `Engine.Render` group) copies each `MeshRenderer`
  with its world matrix, and the lights, into the `RenderSnapshot` singleton:
  `LocalToWorld` for what did not move, and a blend between the latest two
  published states for what moved (`Interpolate::BlendedWorld`). The
  render passes read only that snapshot.
- `MeshRenderer` is authoring data: the mesh and material asset paths. The
  GPU side (model, material, UBO, descriptor set) is a proxy that the render
  pass keeps in a `MeshProxyTable`. If a mesh leaves the snapshot (its
  entity is destroyed, or loses its `MeshRenderer`), the pass frees its proxy.
  The pass does not need a storage hook.
- The renderer writes a mesh UBO only when `LocalToWorld::Version` changes,
  and every frame for a blended mesh (version 0).
- The ImGui pass of the editor is the one exception. It gets the storage,
  because the editor changes the world. During Inline or Threaded play it shows
  the mirror (`World::GetPresentedStorage`), read-only.

## Registration

Each module has one function, `Register<Module>(ModuleRegistration&)`
(`RegisterEngine`, `RegisterGraphics`, `RegisterPhysics`), and a game
overrides `Game::Register`. `Engine::Startup` calls the module functions on
`World::GetModules()`. `World::Init` then calls `Game::Register` and builds
the world's own `TypeRegistry` and schedule. There is no global type
registry. A test makes a `ModuleRegistration` and calls
`TypeRegistry::Build`.

The editor draws a component from its reflected fields. If a type needs
more, override `BaseEditor::RegisterWidgets` and call
`t_Widgets.Override<T>(fn)`.

## Groups and the order

The full reference, with every error and how to read the dump, is
[`Docs/system-order-api.md`](../Docs/system-order-api.md). The reasons for the
design are in [`Docs/update-order-design-notes.md`](../Docs/update-order-design-notes.md).

- Each update rate is one graph, under one root group: `Fling::Order::Tick`
  (fixed) and `Fling::Order::Frame` (variable). They are the only roots. The
  engine groups (`Ecs/EngineOrder.h`) are `Input`, `Simulation`, `Physics`,
  `ResolveDeferred`, `Finalize` in `Tick`, and `Presentation`, `Render`,
  `Editor` in `Frame`.
- What each one is for:
  - `Input`: input preprocessors that read `InputState` (the runner writes
    it before the tick's systems; no system can write it).
  - `Simulation`: gameplay.
  - `Physics`: the physics step. Gameplay asks for forces before it, and the
    contacts of this tick exist after it.
  - `ResolveDeferred`: the consumers of queues (spawns, damage). They see the
    contacts of this tick.
  - `Finalize`: the simulation has its last state for this tick. Systems here
    make the data that comes from that state (`LocalToWorld`). They do not
    change what the simulation decided.
  - `Presentation`: frame rate work for what the player sees and hears. Its
    systems write `Presentation` components.
  - `Render`: gives the renderer its copy of the world (`RenderSnapshot`).
  - `Editor`: editor tools, last in the frame.
- A resimulation for rollback runs `Simulation`, `Physics`, and
  `ResolveDeferred` again, and not `Input` or `Finalize`. These three set
  `GroupInfo::Resim`, and everything inside them inherits it. There is no
  per-system opt-out. A fixed system that is not resimulated must not run
  between two that are (`ResimGap`), and must not write state that a
  resimulation needs (`ResimSkipsState`). Only a group directly in
  `Order::Tick` may set `.Resim`. No runner runs a resimulation yet.
- Any module can declare a group: an `inline constexpr Fling::GroupInfo` with
  `.In` (its parent), and `.After` and `.Before`. Put it inside the engine group
  where its work belongs, at any depth. `.FlushAfter = true` adds a sync point.
  Each module declares its groups, and forward declarations of the systems that
  others may name, in one order header (`Ecs/EngineOrder.h`,
  `PhysicsSystemsFwd.h`, `SandboxSystemsFwd.h`).
- Inside an update rate, `.In`, group order, `.After`, `.Before` and
  `RunBefore` come first. For two systems that these leave unordered, the order comes from
  the data: a writer runs before a reader, and every appender before the
  consumer. These edges give each system a stage (the longest chain of systems
  before it); systems run stage by stage, and by name inside one stage.
- Two systems that write the same type need an order (`.After`, `.Before`, a
  chain through other systems or groups, group order, or
  `SystemRegistry::RunBefore`). If they do not have one, the schedule has an
  error whose fix names the class: `.After = Fling::SystemOf<...>`.
  `RunBefore(first, second)` takes any mix of groups and systems, for a module
  that knows both.

## Tools

- `Sandbox --null-render --unattended --dump-schedule` prints the group tree,
  stages, the reason for each edge, each read that is one tick late, what a
  resimulation runs and reads from outside, and the published set.
- `--dump-ticks <file>` writes the tick ledger (tick, cut, input counts, state
  hash), one line per tick, to diff two runs.
- The engine and physics schedule is checked in as
  `FlingTests/Physics/Schedules/EnginePhysics.schedule.txt`. A test fails when
  it changes. Copy the `.actual.txt` file that the test writes over it when the
  change is wanted.
- `--validate-schedule` exits with code 1 on a schedule error. The build runs
  it after linking Sandbox (`fling_validate_schedule`), so a bad schedule fails
  `cmake --build`. To turn it off, use `-DFLING_VALIDATE_SCHEDULE=OFF`.
- **Windows > World Singletons** in the editor shows each singleton and the
  schedule.

## Testing a system

A system is a function of its components. To test it, make an `EntityStorage`,
add the systems to a `SystemRegistry`, make a `SystemRunner`, and run ticks.
See `FlingTests/Engine/EcsSystemTests.cpp`. Put a rule that must not compile
in `FlingTests/EcsCompileFail/EcsCompileFail.cpp`, and list it in
`FlingTests/EcsCompileFail/EcsCompileChecks.cmake` (an `ECS compile check` CTest). How to test a schedule (hand-built systems with tag
types, the checked-in dump) is in "Testing" of
[`Docs/system-order-api.md`](../Docs/system-order-api.md).

## Input on both update rates

A fixed system reads `InputState`. A variable rate system that must show input
before a tick consumes it reads `PendingInput` and runs it, with the sealed
simulation component, through the same pure function as the fixed system. It
keeps no state of its own. See [`Docs/input-two-sided.md`](../Docs/input-two-sided.md).
Which tick takes which message is in [`Docs/time-and-ticks.md`](../Docs/time-and-ticks.md).
