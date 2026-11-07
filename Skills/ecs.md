# ECS: components, systems, and the schedule

How to write gameplay and engine code in Fling's ECS. The design and its
history are in [`Plans/ecs-systems.md`](../Plans/ecs-systems.md) (the first
slice) and [`Plans/ecs-plan.md`](../Plans/ecs-plan.md) (the full plan). The
rules come from Timothy Ford, *'Overwatch' Gameplay Architecture and Netcode*,
GDC 2017.

The code is in `FlingEngine/Engine/inc/Ecs/`. Each `World` owns one
`EntityStorage` (`Ecs/EntityStorage.h`): the entities, components, and
singletons of that world. Code outside a system (the renderer, physics, the
editor, tests) takes an `EntityStorage&`. It never takes a global registry, and
it never names EnTT. Systems do not use `EntityStorage`: they use their typed
context.

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
