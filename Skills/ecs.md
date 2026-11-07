# ECS: components, systems, and the schedule

A step-by-step guide with a worked example, for people new to the ECS:
[`Docs/ecs-tutorial.md`](../Docs/ecs-tutorial.md). This page is the reference.

This page explains how to write gameplay and engine code in the Fling ECS. The
design and its history are in [`Plans/ecs-systems.md`](../Plans/ecs-systems.md) (the first
slice) and [`Plans/ecs-plan.md`](../Plans/ecs-plan.md) (the full plan). The
rules are from Timothy Ford, *'Overwatch' Gameplay Architecture and Netcode*,
GDC 2017.

The code is in `FlingEngine/Engine/inc/Ecs/`. Each `World` owns one
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
    entities.
- At the flush, the engine groups the deferred `ctx.Add` and `ctx.Remove` calls
  from all systems by entity. An entity moves once for each tick, if one system
  or many systems change it.
- A query sees the archetypes, chunks, and rows that exist when it reaches
  them. A loop does not see an entity that the loop spawns.
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
  list). Singletons come from the same `BlockAllocator`.
- Deferred `ctx.Add` values and `ctx.Defer` callables go in the `LinearArena` of
  each command buffer. The arena resets after the flush. The flush and
  `ctx.Query` use scratch lists that keep their memory.
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
2. **System state goes in a singleton component**, made by the system that
   writes it, in `OnCreate`.
3. **Systems never call systems.** Put shared code in a `*Util` free
   function. Only the `.cpp` of a system, the register file of its module, and
   tests include a system header (`scripts/check_system_includes.py` checks
   this in CI).
4. **Defer big side effects.** Put spawns, damage, physics requests, and requests
   to the application in a Queue component. One system (or the engine)
   consumes each queue.
5. **No globals in simulation code.** Read `SimTime`, not `Timing`. Read
   `InputState`, not `Input::Get()`. A debug build asserts if a fixed rate
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
- `.Transient = true`: the engine never saves it. Register it with
  `t_Modules.Component<T>()` and no fields.
- **Field types:** `bool`, `int32`, `uint32`, `int64`, `uint64`, `float`,
  `double`, `String`, `Name`, `glm::vec2/3/4`, `glm::quat`, enums, and `Entity`.
  For another type, specialize `Fling::FieldTraits` (`Kind = Custom`, with
  `Save` and `Load`). `TypeBuilder::Serialize(save, load)` is the escape hatch
  for a whole type (`MeshRenderer` uses it for its asset paths).
- **Field options:** `Range`, `Tooltip`, `Color`, `Hidden`, `ReadOnly`,
  `Transient` (the engine does not save it), and `Enum({{"name", Value}, ...})`. An enum with a
  table saves by name, so it is safe to change the order.
- **Type options:** `Category`, `Partial` (the engine saves only some members, on
  purpose), and `AfterLoad(fn)` for derived data. `AfterLoad` also runs after an
  editor change.
- **An `Entity` field** saves as the `EntityGuid` of the target. When the world
  saves, it gives every entity an `EntityGuid`. When it loads, it maps the ids
  back to the new entities.
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
- **Names must tell types apart.** `World::Init` fails (and
  `--validate-schedule` fails the build) when two C++ types have one saved
  name, or a former name is another type's name, a removed component's name,
  or claimed by two types. The storage aborts if two types with one saved name
  get a component id, in every build.

## A system

Each system is a header in `inc/Systems/<Name>System.h` and a body in
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
  `RawStorage`. Use each type one time.
- **One entity.** `ctx.TryGet<T>(e)` (null if the entity is gone or has no
  `T`) and `ctx.Get<T>(e)` return Table components. `ctx.Has<T>(e)` checks for a
  Table or Tag component and needs only `Read<T>`.
- **The context checks the access list at compile time.** `ctx.Query<T>()` needs
  `Write<T>`, and `ctx.Query<const T>()` needs `Read<T>`. The same applies to
  `Get`, `TryGet`, `Singleton`, `Queue`, `Drain`, `Spawn`, and `RawStorage`.
- **Hooks.** `FixedUpdate(Context&)` for a fixed phase, `Update(Context&)` for
  a frame phase (use only one of them). Optional: `OnCreate(Setup&)`,
  `OnDestroy(Setup&)`, `OnPlayBegin(Context&)`, `OnPlayEnd(Context&)`.
- **Update rates.**
  - A fixed rate system cannot read `RealFrameTime` or any `Presentation`
    component.
  - A frame rate system cannot write a `Simulation` component, spawn, use
    `Commands`, or append to a queue.
  - No system writes `SimTime` or `RealFrameTime`.
- **Destroy is deferred.** `ctx.Commands().Destroy(e)` destroys the entity and
  its children at the end of the tick. **Spawn is immediate.** Use
  `ctx.Spawn(components...)`.
- **Structural changes are deferred:** `ctx.Add(e, value)` and
  `ctx.Remove<T>(e)` need `Write<T>`. They run at the end of the tick in the
  order of the calls. `ctx.Defer<Ws...>(fn)` runs a change that keeps several
  types in step. It names every type that it writes.
- **Register it.** One line in the module's register file
  (`Register<Module>.cpp`): `t_Modules.System<MoverSystem>();`.

## Transforms and the hierarchy

- `Transform` is the local position, rotation (a quaternion), and scale. It
  has no functions. Use `TransformUtil` (`LocalMatrix`, `FromEulerDegrees`,
  `ToEulerDegrees`, `RotateAround`, `Forward`).
- `Parent` (saved) links a child to its parent. `Children` (transient) comes
  from it. To change a parent, use `HierarchyUtil::SetParent` only (or
  `SetParentDeferred` from a system, with `Write<Parent>` and `Write<Children>`).
  This keeps the two in step.
- `Engine.Transform` runs in its own phase after `Engine.ResolveDeferred`, on
  every tick and in every world role. It writes `LocalToWorld` (the world
  matrix, parents first). When the matrix changes, it increases `Version`.
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
  pass keeps in a `MeshProxyTable`. If a mesh leaves the snapshot (its
  entity is destroyed, or loses its `MeshRenderer`), the pass frees its proxy.
  The pass does not need a storage hook.
- The renderer writes a mesh UBO only when `LocalToWorld::Version` changes.
- The ImGui pass of the editor is the one exception. It gets the storage,
  because the editor changes the world.

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

## Phases and the order

- Each update rate (`Fixed`, `Frame`) is one graph. The engine phases are
  anchors in `Fling::Phases`: `Input`, `Simulation`, `ResolveDeferred` (fixed);
  `PostSimulation`, `Extract`, `Editor` (frame).
- Any module can declare a phase: an `inline constexpr Fling::PhaseInfo` with
  `.After` and `.Before`. `.FlushAfter = true` adds a sync point.
- Inside an update rate, the order comes from the data: a writer runs before
  a reader, and every appender before the consumer. Phase order has priority where it
  exists. If two systems tie, the order of their names decides.
- Two systems that write the same type need an order (`.After`, `.Before`, a
  chain through other systems, phase order, or `SystemRegistry::Order`). If they
  do not have an order, the schedule has an error. `Order(first, second)` orders two systems or two
  phases from a module that knows both.

## Tools

- `Sandbox --null-render --unattended --dump-schedule` prints the phases,
  stages, and the reason for each edge.
- `--validate-schedule` exits with code 1 on a schedule error. The build runs
  it after linking Sandbox (`fling_validate_schedule`), so a bad schedule fails
  `cmake --build`. To turn it off, use `-DFLING_VALIDATE_SCHEDULE=OFF`.
- **Windows > World Singletons** in the editor shows each singleton and the
  schedule.

## Testing a system

A system is a function of its components. To test it, make an `EntityStorage`,
add the systems to a `SystemRegistry`, make a `SystemRunner`, and run ticks.
See `FlingTests/Engine/EcsSystemTests.cpp`. Put a rule that must not compile
in `FlingTests/EcsCompileFail/EcsCompileFail.cpp`, with an
`ECS compile check` CTest.
