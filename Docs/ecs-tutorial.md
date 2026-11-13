# Tutorial: your own components and systems

This guide shows how to add gameplay to Fling with the ECS (entity component
system). You make two components and two systems, register them, and test them.
Then you connect them to the components of the engine: time, input, transforms,
rendering, and physics.

The example is real code. It is in
[`FlingTests/Engine/EcsTutorialTests.cpp`](../FlingTests/Engine/EcsTutorialTests.cpp).
It builds and runs with the other tests. If this page and that file are
different, the file is correct.

For the full list of rules and options, read [`Skills/ecs.md`](../Skills/ecs.md).

## The idea

- An **entity** is an id (`Fling::Entity`). It has no data and no behavior.
- A **component** is a plain struct of data that you put on an entity. It has
  no functions.
- A **system** is a function that runs on every tick. It reads and writes the
  components of all entities that have them. It has no member variables.
- Each system lists the components that it reads and writes (its **access
  list**). The engine uses those lists to put the systems in order. A writer of
  a component runs before its readers, so you seldom give an order yourself.
  The compiler stops a system from using a component that is not in its list.

In the example, poisoned entities lose health. `PoisonSystem` asks for damage.
`DamageSystem` is the only place where health decreases. It applies the damage
and destroys entities that reach 0.

## Step 1: the components

```cpp
#include "Ecs/Component.h"
#include "FlingTypes.h"

/** Hit points. Saved in level files. */
struct Health
{
    static constexpr Fling::ComponentInfo Component{.Name = "Tutorial.Health"};

    float Current = 100.0f;
    float Max = 100.0f;
};

/** Damage over time. Saved in level files. */
struct Poison
{
    static constexpr Fling::ComponentInfo Component{.Name = "Tutorial.Poison"};

    float DamagePerSecond = 10.0f;
};
```

The `Component` line makes a struct a component. `.Name` is the name that level
files use. It must end with the struct's name: `"Health"` or, with a prefix that
keeps it apart from other modules, `"MyGame.Health"`. The compiler checks this.
If two different structs have the same saved name, the world does not start
and tells you which two they are.

To rename a component after levels use it, change the struct and `.Name`
together, and give the old name to `FormerNames` when you register it (Step 3).
Old levels then still load, and the next save writes the new name.

There are four kinds of component. To choose one, set `.Kind`:

| Kind | What it is | Example |
|---|---|---|
| `Table` (the default) | One per entity | `Health`, `Transform` |
| `Tag` | An empty struct that marks an entity | `Invulnerable` |
| `Singleton` | One per world, not on an entity | `SimTime`, `InputSnapshot` |
| `Queue` | A singleton list of requests from many systems to one system | `PhysicsCommands` |

The example has one of each of the other kinds:

```cpp
/** Tag: this entity takes no damage. A tag has no data. */
struct Invulnerable
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "Tutorial.Invulnerable", .Kind = Fling::ComponentKind::Tag};
};

/** One request for damage. Not a component: it goes in the DamageRequests queue. */
struct DamageRequest
{
    Fling::Entity Target;
    float Amount = 0.0f;
};

/** Queue component: damage that DamageSystem applies this tick. */
struct DamageRequests
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "Tutorial.DamageRequests", .Kind = Fling::ComponentKind::Queue, .Transient = true};

    std::vector<DamageRequest> Requests; // A queue must have a container named Requests
};

/** Singleton component: the state that DamageSystem keeps between ticks. */
struct DamageStats
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "Tutorial.DamageStats", .Kind = Fling::ComponentKind::Singleton, .Transient = true};

    uint32 Kills = 0;
};
```

`.Transient = true` means that the engine never saves the component. Use it for
runtime state, such as queues and counters.

If a component cannot work without another one, say so on the struct. A poison
that needs a health to decrease would have
`using Requires = Fling::TypeList<Health>;`. Then `ctx.Spawn(Poison{})` with no
`Health` does not compile, and the editor adds a `Health` when you add a
`Poison`. Read "Required components" in [`Skills/ecs.md`](../Skills/ecs.md).

Use Fling types in components: `uint32` and `int64`, not `uint32_t`. Use `String`
for text and `Name` for identifiers. Use `Fling::Entity` for a reference to
another entity. Read [`Skills/strings.md`](../Skills/strings.md).

## Step 2: the systems

Each system is a header in `inc/Systems/<Name>System.h` and a body in
`src/Systems/<Name>System.cpp`. (The tutorial file puts them in one place, to keep
the example short.)

### A system that reads and appends

```cpp
// inc/Systems/PoisonSystem.h
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"

class PoisonSystem final : public Fling::System<PoisonSystem,
    Fling::Access<Fling::Read<Fling::SimTime>, Fling::Read<Poison>, Fling::Append<DamageRequests>>>
{
public:
    static constexpr Fling::SystemInfo Info{.Name = "Tutorial.Poison", .Phase = Fling::Phases::Simulation};

    void FixedUpdate(Context& ctx) const;
};

// src/Systems/PoisonSystem.cpp
void PoisonSystem::FixedUpdate(Context& ctx) const
{
    const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;
    for (auto [entity, poison] : ctx.Query<const Poison>())
    {
        ctx.Queue<DamageRequests>().Push(DamageRequest{entity, poison.DamagePerSecond * dt});
    }
}
```

What each part does:

- `Fling::System<PoisonSystem, Fling::Access<...>>` is the base class. The
  first argument is the system itself.
- The access list:
  - `Read<T>` lets you read `T`. Use `const T` in the query.
  - `Write<T>` lets you read and change `T`.
  - `Append<Q>` lets you push to queue `Q`. Many systems can append to one queue.
  - `Consume<Q>` lets you drain queue `Q`. Only one system can consume a queue.
  - `Spawns` lets you create entities.
- `Info` gives the system a unique name and a **phase** (the part of the tick
  where it runs). Put gameplay in `Fling::Phases::Simulation`.
- `FixedUpdate` runs one time in each fixed tick (60 each second by default).
  The same input always gives the same result. Use `SimTime::FixedDelta` for
  time. Do not use a clock.
- `ctx.Query<const Poison>()` gives every entity that has a `Poison`. To get
  entities that have more types, add the types:
  `ctx.Query<Transform, const Velocity>()`. Filters select entities by
  components that you do not need to read:
  `ctx.Query<const Health>(Fling::With<Poison>{})` or
  `Fling::Without<Invulnerable>{}`.

### A system that consumes, owns state, and destroys

```cpp
class DamageSystem final : public Fling::System<DamageSystem,
    Fling::Access<Fling::Consume<DamageRequests>, Fling::Read<Invulnerable>,
                  Fling::Write<Health>, Fling::Write<DamageStats>>>
{
public:
    static constexpr Fling::SystemInfo Info{.Name = "Tutorial.Damage", .Phase = Fling::Phases::Simulation};

    void OnPlayBegin(Context& ctx) const;
    void FixedUpdate(Context& ctx) const;
};

// Each play starts with no kills
void DamageSystem::OnPlayBegin(Context& ctx) const
{
    ctx.Singleton<DamageStats>() = DamageStats{};
}

void DamageSystem::FixedUpdate(Context& ctx) const
{
    DamageStats& stats = ctx.Singleton<DamageStats>();
    for (const DamageRequest& request : ctx.Drain<DamageRequests>())
    {
        // A request can point at an entity that is gone, so use TryGet
        Health* health = ctx.TryGet<Health>(request.Target);
        if (health == nullptr || health->Current <= 0.0f || ctx.Has<Invulnerable>(request.Target))
        {
            continue;
        }

        health->Current -= request.Amount;
        if (health->Current <= 0.0f)
        {
            ctx.Commands().Destroy(request.Target); // Destroy waits for the end of the tick
            ++stats.Kills;
        }
    }
}
```

- **Put the state of a system in a singleton component.** A system cannot
  have member variables (it does not compile).
- **You do not make the singleton or the queue.** `Write<DamageStats>` and
  `Consume<DamageRequests>` in the access list tell the engine that this system
  owns them. The engine makes them, with their default values, before the first
  tick, and removes them when the world shuts down. If a singleton must start
  with other values, or has no default constructor, write
  `void OnCreate(Setup& setup) const` and call `setup.AddSingleton<T>(values...)`.
- **`OnPlayBegin`** runs when play starts (also when you push Play in the
  editor). Reset the state of each play there.
- **`ctx.Drain<Q>()`** gives each request in the queue one time. Then it empties
  the queue.
- **`ctx.TryGet<T>(entity)`** gives a pointer, or null if the entity is gone or
  does not have `T`. `ctx.Get<T>(entity)` is the same, but the entity must have
  `T`. `ctx.Has<T>(entity)` checks for a component and does not read it. This is
  the only way to check for a tag.
- **Destroy is deferred.** `ctx.Commands().Destroy(e)` removes the entity
  (and its children) at the end of the tick. This does not change a query while
  the query runs. `ctx.Add(e, value)` and `ctx.Remove<T>(e)` are also deferred.
- **Spawn is immediate.** With `Spawns` in the access list,
  `ctx.Spawn(Health{}, Poison{})` makes a new entity at once. Give it every
  component that the entity needs in that one call. The systems that run after
  yours in this tick then see the whole entity. A component that you add later
  with `ctx.Add` is not there until the end of the tick.

You do not need to set the order of `PoisonSystem` and `DamageSystem`. Every
system that appends to a queue runs before the system that consumes it. If
two systems write the same component and nothing sets their order, the schedule
has an error. The error tells you to add `.After = "OtherSystem"` or
`.Before = "OtherSystem"` to `Info`.

### Variable rate systems

`FixedUpdate` is for simulation. For work at the display rate (camera
smoothing, UI, interpolation), put the system in a variable rate phase, such as
`Fling::Phases::Presentation`. Write `Update(Context&)` instead of
`FixedUpdate`. A variable rate system can read simulation components, but it cannot
change them. It writes components that have
`.Domain = Fling::ComponentDomain::Presentation`. To get the frame time, read
`Fling::RealFrameTime`.

## Step 3: register them

Each game has one register function. The register function of the Sandbox is
[`Sandbox/Gameplay/src/RegisterSandbox.cpp`](../Sandbox/Gameplay/src/RegisterSandbox.cpp)
(`Game::Register`). An engine module has `Register<Module>`. Only this
file includes the system headers.

```cpp
void Game::Register(Fling::ModuleRegistration& t_Modules)
{
    t_Modules.Component<Health>([](Fling::TypeBuilder<Health>& t_Type) {
        t_Type.Category("Combat");
        t_Type.Field("current", &Health::Current).Range(0.0f, 1000.0f);
        t_Type.Field("max", &Health::Max).Range(1.0f, 1000.0f).Tooltip("Full health");
    });
    t_Modules.Component<Poison>([](Fling::TypeBuilder<Poison>& t_Type) {
        t_Type.Field("damagePerSecond", &Poison::DamagePerSecond);
    });
    t_Modules.Component<Invulnerable>();
    t_Modules.Component<DamageRequests>();
    t_Modules.Component<DamageStats>();

    t_Modules.System<PoisonSystem>();
    t_Modules.System<DamageSystem>();
}
```

Each `Field` line lists one field. Level save and load, the editor
inspector, and the play snapshot of the editor all use that list. The first
argument is the name of the field in the level file. `Range` and `Tooltip` are
for the editor. If a component has no saved fields (a tag, a queue, a transient
singleton), register it with no builder.

Now a level file can have the component:

```json
{ "Tutorial.Health": { "current": 40.0, "max": 80.0 } }
```

To rename `Health` to `Vitality` later, rename the struct, set
`.Name = "Tutorial.Vitality"`, and register the old name:

```cpp
t_Modules.Component<Vitality>([](Fling::TypeBuilder<Vitality>& t_Type) {
    t_Type.FormerNames({"Tutorial.Health"}); // Old levels still load
    // ...the same Field lines
});
```

To change a saved component later and keep old levels working, use
`FormerNames`, `Removed`, and `Version` with `Upgrade`. Read "Changing a saved
component" in [`Skills/ecs.md`](../Skills/ecs.md#a-component).

## Step 4: test it

A system is a function of its components, so a test does not need a window or a
GPU. Make an `EntityStorage`, register the module, build the schedule, and run
ticks:

```cpp
TEST_CASE("Tutorial: an entity at 0 health is destroyed and counted", "[ecs][tutorial]")
{
    Fling::EntityStorage storage;
    Fling::SystemRegistry systems;
    Fling::ModuleRegistration modules(systems);
    RegisterTutorial(modules);

    Fling::ScheduleResult schedule = systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    Fling::SystemRunner runner(systems, std::move(schedule.Value), storage);
    runner.Create(); // Runs OnCreate

    const Fling::Entity weak = storage.Create();
    storage.Add<Health>(weak, Health{1.5f, 1.5f});
    storage.Add<Poison>(weak, Poison{60.0f}); // 1 point per tick at 60 Hz

    runner.RunTick(Fling::WorldRole::Play);
    REQUIRE(storage.IsValid(weak));

    runner.RunTick(Fling::WorldRole::Play);
    REQUIRE_FALSE(storage.IsValid(weak));
    REQUIRE(storage.Singleton<DamageStats>().Kills == 1);

    runner.Destroy(); // Runs OnDestroy
}
```

Put the test in `FlingTests/<Module>/` (read [`Skills/testing.md`](../Skills/testing.md)).
To test the full game, run it with no window and with recorded input:
`Sandbox --null-render --unattended --frames 120 --replay-input <file>` (read
[`Skills/input-playback.md`](../Skills/input-playback.md)).

## Using the engine's components and systems

Your systems use engine data in the same way as their own data. Add the
component to the access list. Then query it or read the singleton.

### Time

| Component | Kind | Use |
|---|---|---|
| `Fling::SimTime` | Singleton | `FixedDelta` (seconds per tick), `Tick`, `Elapsed`. `Read` only. |
| `Fling::RealFrameTime` | Singleton | Variable rate systems only: the real time of this frame. |

Do not call `Timing` or the global `Random` from a fixed system. They break
replays and rollback. A debug build asserts if you call them.

### Input

`Fling::InputSnapshot` (singleton, from `Components/InputSnapshot.h`) is the input of this tick.
`Engine.Input` builds it once at the start of the tick, before gameplay runs, and it is the only
input a fixed system reads.

```cpp
// Access<Fling::Read<Fling::InputSnapshot>, ...>
const Fling::InputSnapshot& input = ctx.Singleton<const Fling::InputSnapshot>();
if (input.WasPressed(Fling::Keys::Space)) { /* once, on the tick the key goes down */ }
if (input.IsHeld(Fling::Keys::Left)) { /* every tick while it is down */ }
const float stick = input.GetAxisValue(Fling::Keys::GamepadLeftX);
```

If a key goes down and up in one tick, only `WasPressed` sees it.
Example: [`BallControlSystem.cpp`](../Sandbox/Gameplay/src/Systems/BallControlSystem.cpp).

### Position, rotation, and parents

| Component | Use |
|---|---|
| `Fling::Transform` | Local `Position`, `Rotation` (a quaternion), and `Scale`. Write it to move an entity. |
| `Fling::LocalToWorld` | The world matrix (`Value`). `Engine.Transform` computes it after gameplay. Read world positions from here. |
| `Fling::Parent` / `Fling::Children` | The hierarchy. Change it only with `HierarchyUtil::SetParent` (or `SetParentDeferred` from a system). |

`TransformUtil` (`FromEulerDegrees`, `Forward`, `RotateAround`, ...) does the
math, because `Transform` has no functions. Example:
[`MoverSystem.cpp`](../Sandbox/Gameplay/src/Systems/MoverSystem.cpp).

### Rendering

Put a `Fling::MeshRenderer` (mesh and material asset paths) and a `Transform` on
an entity. The engine then draws the entity. Lights are `Fling::PointLight` and
`Fling::DirectionalLight`. The renderer never reads the world: `Graphics.PublishSnapshot`
copies the data that it needs in each frame. Your systems do not touch the GPU.

```cpp
// Access<..., Fling::Write<Fling::MeshRenderer>, Fling::Write<Fling::Transform>, Fling::Spawns>
Fling::Transform transform;
transform.Position = glm::vec3(x, 0.0f, 0.0f);
ctx.Spawn(Fling::MeshRenderer("Models/sphere.obj", "Materials/DeferredBronzeMat.mat"), transform);
```

Example: [`TestSpawnSystem.cpp`](../Sandbox/Gameplay/src/Systems/TestSpawnSystem.cpp).

### Physics

Give an entity a `Transform`, a `Fling::RigidBody` (mass 0 is static), and a
`Fling::Collider`. Physics then moves the entity. Physics bodies must be root
entities (no `Parent`).

- **To push a body**, append to the `Fling::PhysicsCommands` queue.
  `Physics.Step` consumes it, so your system runs before the step
  automatically:

  ```cpp
  // Access<..., Fling::Append<Fling::PhysicsCommands>>
  ctx.Queue<Fling::PhysicsCommands>().Push(
      Fling::PhysicsCommand{entity, Fling::PhysicsCommandType::Impulse, glm::vec3(0.0f, 5.0f, 0.0f)});
  ```

- **To react to hits**, read the `Fling::ContactEvents` singleton. It has the
  contacts of the last step. Example:
  [`FirstHitSystem.cpp`](../Sandbox/Gameplay/src/Systems/FirstHitSystem.cpp).

### Asking the application for something

Gameplay cannot close the window. It appends a request to `Fling::AppRequests`.
The application does the request after the tick:

```cpp
// Access<..., Fling::Append<Fling::AppRequests>>
ctx.Queue<Fling::AppRequests>().Push(Fling::AppRequest::Quit);
```

Example: [`AppKeysSystem.cpp`](../Sandbox/Gameplay/src/Systems/AppKeysSystem.cpp).

### Phases

The phases of the engine run in this order. Put your system in one phase. To set
its order inside the phase, use `.After` and `.Before` with system names.

| Rate | Phase | What runs |
|---|---|---|
| Fixed | `Phases::Input` | Engine: fills `InputSnapshot` |
| Fixed | `Phases::Simulation` | Your gameplay |
| Fixed | `Phases::Physics` | Engine: the physics step. Moves bodies and writes the contacts of this tick |
| Fixed | `Phases::ResolveDeferred` | Consumers of queues: spawns, damage. They see the contacts of this tick |
| Fixed | `Phases::Finalize` | Data that comes from the last state of the tick. Engine: computes `LocalToWorld` |
| Variable | `Phases::Presentation` | What the player sees and hears: camera, interpolation, UI |
| Variable | `Phases::Publish` | Engine: gives the renderer its copy of the world |
| Variable | `Phases::Editor` | Editor tools |

To see the full order and the reason for each edge, run
`Sandbox --null-render --unattended --dump-schedule`.

## When it does not compile

The access checks are `static_assert`s. The message tells you what to change:

| Message contains | Fix |
|---|---|
| "queries a component that its Access<...> does not allow" | Add `Read<T>` (and ask for `const T`) or `Write<T>` |
| "gets a component that its Access<...> does not allow" | The same, for `Get` and `TryGet` |
| "checks a component that its Access<...> does not allow" | Add `Read<T>` for `Has<T>` |
| "Add Fling::Spawns to the access list" | Add `Fling::Spawns` |
| "A system cannot have member state" | Move the members to a singleton component |
| "A fixed phase system implements FixedUpdate" | The phase and the function do not agree |
| "A variable rate system cannot write a Simulation component" | Write a `Presentation` component, or move the system to a fixed phase |
| "Only a writer of a singleton makes it" | `AddSingleton<T>` needs `Write<T>` |
| A debug assert: "Singleton: nobody made it" | No system has `Write<T>` for that singleton, or the singleton is `.External` and its module did not make it |
| "is not a Fling::Component" | Add the `static constexpr Fling::ComponentInfo Component{...}` line |
| "makes an entity with a component, and without a component that it requires" | Add the missing component to the `Spawn` or `Create` call. The `Requires` list of each component says what it needs |
| "saved .Name must end with its C++ type name" | Make `.Name` end with the struct name. If you renamed the struct, add the old `.Name` to `FormerNames` |

If the schedule has an error (two writers with no order, a cycle), or two
component types have names that a level cannot tell apart, the build fails
when it links Sandbox and prints the report. `--dump-schedule` also shows
the error.
