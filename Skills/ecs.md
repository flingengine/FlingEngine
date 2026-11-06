# ECS: components, systems, and the schedule

How to write gameplay and engine code in Fling's ECS. The design and its
history are in [`Plans/ecs-systems.md`](../Plans/ecs-systems.md) (the first
slice) and [`Plans/ecs-plan.md`](../Plans/ecs-plan.md) (the full plan). The
rules come from Timothy Ford, *'Overwatch' Gameplay Architecture and Netcode*,
GDC 2017.

The code is in `FlingEngine/Engine/inc/Ecs/`. Storage is still EnTT 3.1, but
no ECS header that gameplay uses names EnTT.

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

    void Serialize(Fling::JsonArchive& Ar);   // Until reflection replaces it (ecs-plan.md Phase C)
};
```

- `.Name` is the saved name in level files. Do not change it.
- `.Kind`: `Table` (the default, one per entity), `Tag` (an empty struct),
  `Singleton` (one per world), or `Queue` (a singleton of requests, with a
  container named `Requests`).
- `.Domain`: `Simulation` (the default, written at the fixed tick) or
  `Presentation` (written at the frame rate; the simulation never reads it).
- `.Transient = true`: never saved. It then needs no `Serialize`.
- Register it with `ComponentTypeRegistry::Get().Register<T>()`. The
  overload that takes a name is deprecated and logs a warning.
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
  the tick). **Spawn is immediate** (`ctx.Spawn(components...)`).
- **Register it.** One line in the module's register file
  (`Register*Systems.cpp`): `systems.Add<MoverSystem>();`.

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

A system is a function of its components, so a test builds a registry, adds
the systems to a `SystemRegistry`, makes a `SystemRunner`, and runs ticks.
See `FlingTests/Engine/EcsSystemTests.cpp`. A rule that must not compile goes
in `FlingTests/EcsCompileFail/EcsCompileFail.cpp`, with an
`ECS compile check` CTest.
