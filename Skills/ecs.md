# ECS: components, systems, and the schedule

How to write gameplay and engine code in Fling's ECS. The design and its
history are in [`Plans/ecs-systems.md`](../Plans/ecs-systems.md) (the first
slice) and [`Plans/ecs-plan.md`](../Plans/ecs-plan.md) (the full plan). The
rules come from Timothy Ford, *'Overwatch' Gameplay Architecture and Netcode*,
GDC 2017.

The code is in `FlingEngine/Engine/inc/Ecs/`. Each `World` owns one
`EntityStorage` (`Ecs/EntityStorage.h`): the entities, components, and
singletons of that world. Code outside a system (the renderer, physics, the
editor, tests) takes an `EntityStorage&`. It never takes a global registry.
Systems do not use `EntityStorage`: they use their typed context.

## Storage

- Entities with the same set of Table and Tag components share an
  archetype. Its rows are in chunks that grow from 1 KiB to 128 KiB, one
  64-byte aligned column per component (`Ecs/detail/NativeStorage.h`). Big
  chunks keep the hardware prefetcher streaming; small archetypes stay small.
- **Iterating is cheap. Adding and removing Table components is not.** A
  structural change moves the whole row (about 19 ns), so do it with deferred
  `ctx.Add` and `ctx.Remove` (batched per entity), or use `Sparse` or
  `Enableable` for components that come and go every few ticks.
- **Make an entity with all its components at once:** `storage.Create(a, b, c)`
  and `ctx.Spawn(a, b, c)` put it straight in its archetype, with no move per
  component. The Added hooks run after every component is in place.
- **Change many entities with one call:** `storage.AddToAll(value,
  With<...>{}, Without<...>{})` and `storage.RemoveFromAll<T>(filters...)` move
  whole chunks, one copy per column (about 2 ns per entity). A system calls them
  inside `ctx.Defer<T>(...)`. A Sparse type, or a type with hooks, is done one
  entity at a time. Do not call them while a query is being iterated.
- In a hot loop, `ctx.Query<...>().Each(fn)` is the fastest form: a plain
  array loop per chunk, and the Entity column is read only if `fn` takes an
  `Entity`. The range-for (`for (auto [e, a] : ctx.Query<A>())`) is close.
- **Adding or removing a Table component moves the entity's row.** A
  reference to one of its components is not valid after that. Keep the
  `Entity`, and `Get` the component again. Build a component whole and add it,
  instead of adding it and then writing through the reference after more adds.
- Pick the storage on the declaration with `.Storage`:
  - `Table` (the default): the fastest to iterate.
  - `Enableable`: `SetEnabled<T>(e, false)` turns it off without a move.
    Queries and views skip entities where it is off.
  - `Sparse`: beside the archetypes, at a stable address. Adding and removing it
    never moves the entity. For components that come and go often on few
    entities.
- Deferred `ctx.Add` and `ctx.Remove` from every system are grouped by entity
  at the flush, so an entity moves once per tick however many systems changed
  it.
- A query sees the archetypes, chunks, and rows that exist when it reaches
  them, so an entity spawned inside a loop is not seen by that loop.
- `EntityStorage::Stats()` gives archetype, chunk, and entity counts, the
  number of row moves, and the bytes the world reserved and uses.

## Memory

- The storage does not use `new` or `malloc` for entities or components. Each
  `EntityStorage` owns a `BlockAllocator` (`Core/inc/Allocators/`): power-of-two
  blocks from 1 KiB to 128 KiB, cut from slabs of OS pages, with a free list
  per size, so a freed block is used again for the same size and memory does
  not fragment.
- The slabs of the 128 KiB blocks are one 2 MiB large page each
  (`VirtualMemory::UseLargePages`, Linux only), so a big world takes few page
  faults and few TLB entries.
- A chunk is one block: its header, the column pointers, the enabled bits, the
  Entity column, and one 64-byte aligned column per component (structure of
  arrays). Big chunks start their layout at one of 16 cache lines, so the
  headers do not all map to the same cache set.
- Sparse components come from a `SlotPool` per type (stable addresses, a free
  list). Singletons come from the same `BlockAllocator`.
- Deferred `ctx.Add` values and `ctx.Defer` callables go in each command
  buffer's `LinearArena`, which resets after the flush. The flush and
  `ctx.Query` use scratch lists that keep their memory.
- A steady tick takes no memory from the global heap.
  `FlingTests/Engine/EcsAllocationTests.cpp` proves it with a counting
  `operator new`. Keep it passing: no `std::vector`, `std::function`, or
  `std::string` made per tick in the storage, the runner, or a query.
  (`Drain` of a Queue component still gives up the queue's vector.)
- The world keeps its pages while it lives. Call
  `EntityStorage::TrimMemory()` after a big change, such as a level unload, to
  give empty slabs back to the OS.
- Use the `Fling::Math` helpers (`MathUtils.h`: `Min`, `Max`, `Clamp`,
  `AlignUp`, `IsPowerOfTwo`, ...) and the Fling math types, not `std::min`,
  `std::max`, or `std::clamp`, in engine code.
- `scripts/EcsBenchmark/run.sh` compares the storage with EnTT 3.1 (the
  storage it replaced) on iteration, create, add, remove, destroy, and memory.
  Run it after a change to the storage.

## The rules

1. **Components are data. Systems are functions with no state.** A component
   has no behavior beyond small accessors. A system has no member variables:
   `SystemRegistry::Add` does not compile if it has one.
2. **System state goes in a singleton component**, made by the system that
   writes it, in `OnCreate`.
3. **Systems never call systems.** Shared code goes in a `*Util` free
   function. Only a system's own `.cpp`, its module's register file, and
   tests include a system header (`scripts/check_system_includes.py` checks
   this in CI).
4. **Defer big side effects.** Spawns, damage, physics requests, and requests
   to the application go into a Queue component. One system (or the engine)
   consumes each queue.
5. **No globals in simulation code.** Read `SimTime`, not `Timing`. Read
   `InputState`, not `Input::Get()`. Debug builds assert if a fixed rate
   system calls `Timing` or the global `Random`.

## A component

```cpp
struct Mover
{
    static constexpr Fling::ComponentInfo Component{ .Name = "Mover" };

    glm::vec3 TargetPos{ -10.0f, 0.0f, 0.0f };
    float Speed = 1.0f;
};
```

A component has no functions. Its fields are described once, in the
module's register file (or a feature `.cpp` that it calls):

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

Save, load, the play snapshot, and the editor inspector all come from that
description.

- `.Name` is the saved name in level files. Do not change it.
- `.Kind`: `Table` (the default, one per entity), `Tag` (an empty struct),
  `Singleton` (one per world), or `Queue` (a singleton of requests, with a
  container named `Requests`).
- `.Domain`: `Simulation` (the default, written at the fixed tick) or
  `Presentation` (written at the frame rate; the simulation never reads it).
- `.Transient = true`: never saved. Register it with
  `t_Modules.Component<T>()` and no fields.
- **Field types:** `bool`, `int32`, `uint32`, `int64`, `uint64`, `float`,
  `double`, `std::string`, `glm::vec2/3/4`, `glm::quat`, enums, and `Entity`.
  For another type, specialize `Fling::FieldTraits` (`Kind = Custom`, with
  `Save` and `Load`). `TypeBuilder::Serialize(save, load)` is the escape hatch
  for a whole type (`MeshRenderer` uses it for its asset paths).
- **Field options:** `Range`, `Tooltip`, `Color`, `Hidden`, `ReadOnly`,
  `Transient` (not saved), and `Enum({{"name", Value}, ...})`. An enum with a
  table saves by name, so a reorder is safe.
- **Type options:** `Category`, `Partial` (only some members are saved, on
  purpose), and `AfterLoad(fn)` for derived data. `AfterLoad` also runs after an
  editor change.
- **An `Entity` field** saves as the target's `EntityGuid`. The world gives
  every entity an `EntityGuid` when it saves, and maps the ids back to the new
  entities when it loads.
- **Changing a saved component (back compatibility):**
  - Add a field: nothing to do. An old file keeps the default.
  - Remove a field: `t_Type.Removed("oldName")`, so load does not warn.
  - Rename a field: `.FormerNames({"oldName"})` on the field.
  - Rename a component: `t_Type.FormerNames({"OldName"})`.
  - Delete a component: `t_Modules.RemovedComponent("OldName", convert)`.
  - Change the meaning or shape: `t_Type.Version(2).Upgrade(1, fn)`. The
    upgrade rewrites the old JSON object before the fields load. The version
    is saved as `"$v"` when it is above 1. Write each upgrade once and do not
    change it.
- A component type name must not match another class in namespace `Fling`
  in any module. Two classes with one name are an ODR violation, and the
  linker mixes them up.

## A system

Every system is a header in `inc/Systems/<Name>System.h` and a body in
`src/Systems/<Name>System.cpp`:

```cpp
class MoverSystem final : public Fling::System<MoverSystem, Fling::Access<
    Fling::Read<Fling::SimTime>, Fling::Read<SandboxToggles>,
    Fling::Write<Mover>, Fling::Write<Fling::Transform>>>
{
public:
    static constexpr Fling::SystemInfo Info{
        .Name = "Sandbox.Mover", .Phase = Fling::Phases::Simulation, .After = "Sandbox.Rotator" };

    void FixedUpdate(Context& ctx) const;
};
```

- **Access list.** `Read<T>`, `Write<T>` (implies read), `Append<Q>`,
  `Consume<Q>`, `Spawns`, and, for engine bridges to a subsystem only,
  `RawStorage`. Each type once.
- **The context checks the list at compile time.** `ctx.Query<T>()` needs
  `Write<T>`, and `ctx.Query<const T>()` needs `Read<T>`. The same applies to
  `Get`, `TryGet`, `Singleton`, `Queue`, `Drain`, `Spawn`, and `RawStorage`.
- **Hooks.** `FixedUpdate(Context&)` for a fixed phase, `Update(Context&)` for
  a frame phase (exactly one). Optional: `OnCreate(Setup&)`,
  `OnDestroy(Setup&)`, `OnPlayBegin(Context&)`, `OnPlayEnd(Context&)`.
- **Update rates.**
  - A fixed rate system cannot read `RealFrameTime` or any `Presentation`
    component.
  - A frame rate system cannot write a `Simulation` component, spawn, use
    `Commands`, or append to a queue.
  - No system writes `SimTime` or `RealFrameTime`.
- **Destroy is deferred** (`ctx.Commands().Destroy(e)`, applied at the end of
  the tick, and it destroys the entity's children too). **Spawn is immediate**
  (`ctx.Spawn(components...)`).
- **Structural changes are deferred:** `ctx.Add(e, value)` and
  `ctx.Remove<T>(e)` need `Write<T>`, and run at the end of the tick in the
  order they were recorded. `ctx.Defer<Ws...>(fn)` runs a change that keeps
  several types in step, and names every type it writes.
- **Register it.** One line in the module's register file
  (`Register<Module>.cpp`): `t_Modules.System<MoverSystem>();`.

## Transforms and the hierarchy

- `Transform` is the local position, rotation (a quaternion), and scale. It
  has no functions: use `TransformUtil` (`LocalMatrix`, `FromEulerDegrees`,
  `ToEulerDegrees`, `RotateAround`, `Forward`).
- `Parent` (saved) links a child to its parent. `Children` (transient) is made
  from it. Change a parent only with `HierarchyUtil::SetParent` (or
  `SetParentDeferred` from a system, with `Write<Parent>` and `Write<Children>`),
  so the two stay in step.
- `Engine.Transform` runs in its own phase after `Engine.ResolveDeferred`, on
  every tick and in every world role. It writes `LocalToWorld` (the world
  matrix, parents first) and raises its `Version` when the matrix changes.
  Read world positions from `LocalToWorld`, not from `Transform`.
- Physics treats a body's `Transform` as its world pose, so a physics body
  must be a root entity.

## Rendering

- The renderer never reads the world. `Graphics.Extract` (a frame system in
  the `Engine.Extract` phase) copies each `MeshRenderer` with its
  `LocalToWorld`, and the lights, into the `RenderSnapshot` singleton. The
  render passes read only that snapshot.
- `MeshRenderer` is authoring data: the mesh and material asset paths. The
  GPU side (model, material, UBO, descriptor set) is a proxy that the render
  pass keeps in a `MeshProxyTable`. A mesh that leaves the snapshot (its
  entity was destroyed, or lost its `MeshRenderer`) frees its proxy, so no
  storage hook is needed.
- A mesh UBO is written only when `LocalToWorld::Version` changes.
- The editor's ImGui pass is the one exception: it gets the storage, because
  the editor changes the world.

## Registration

Each module has one function, `Register<Module>(ModuleRegistration&)`
(`RegisterEngine`, `RegisterGraphics`, `RegisterPhysics`), and a game
overrides `Game::Register`. `Engine::Startup` calls the module functions on
`World::GetModules()`. `World::Init` then calls `Game::Register` and builds
the world's own `TypeRegistry` and schedule. There is no global type
registry: a test makes a `ModuleRegistration` and calls
`TypeRegistry::Build`.

The editor draws a component from its reflected fields. For a type that needs
more, override `BaseEditor::RegisterWidgets` and call
`t_Widgets.Override<T>(fn)`.

## Phases and the order

- Each update rate (`Fixed`, `Frame`) is one graph. The engine phases are
  anchors in `Fling::Phases`: `Input`, `Simulation`, `ResolveDeferred` (fixed);
  `PostSimulation`, `Extract`, `Editor` (frame).
- Any module can declare a phase: an `inline constexpr Fling::PhaseInfo` with
  `.After` and `.Before`. `.FlushAfter = true` adds a sync point.
- Inside an update rate, the order comes from the data: a writer runs before
  a reader, and every appender before the consumer. Phase order wins where it
  exists. Ties are broken by name.
- Two systems that write the same type need an order (`.After`, `.Before`, a
  chain through other systems, phase order, or `SystemRegistry::Order`), or
  the schedule is an error. `Order(first, second)` orders two systems or two
  phases from a module that knows both.

## Tools

- `Sandbox --null-render --unattended --dump-schedule` prints the phases,
  stages, and the reason for each edge.
- `--validate-schedule` exits with code 1 on a schedule error. The build runs
  it after linking Sandbox (`fling_validate_schedule`), so a bad schedule fails
  `cmake --build`. Turn it off with `-DFLING_VALIDATE_SCHEDULE=OFF`.
- The editor's **Windows > World Singletons** shows each singleton and the
  schedule.

## Testing a system

A system is a function of its components, so a test makes an `EntityStorage`, adds
the systems to a `SystemRegistry`, makes a `SystemRunner`, and runs ticks.
See `FlingTests/Engine/EcsSystemTests.cpp`. A rule that must not compile goes
in `FlingTests/EcsCompileFail/EcsCompileFail.cpp`, with an
`ECS compile check` CTest.
