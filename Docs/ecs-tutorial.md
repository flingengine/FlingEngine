# Tutorial: your own components and systems

This guide shows how to add gameplay to Fling with the ECS (entity component
system). You make two components and two systems, register them, test them, and
then connect them to the engine's own components: time, input, transforms,
rendering, and physics.

The example is real code. It is in
[`FlingTests/Engine/EcsTutorialTests.cpp`](../FlingTests/Engine/EcsTutorialTests.cpp),
and it builds and runs with the other tests. If this page and that file do not
agree, the file is correct.

For the full list of rules and options, see [`Skills/ecs.md`](../Skills/ecs.md).

## The idea

- An **entity** is an id (`Fling::Entity`). It has no data and no behavior.
- A **component** is a plain struct of data that you put on an entity. It has
  no functions.
- A **system** is a function that runs on every tick. It reads and writes the
  components of all entities that have them. It has no member variables.
- Each system says which components it reads and writes (its **access list**).
  The engine uses those lists to put the systems in order. A writer of a
  component runs before its readers, so you seldom give an order yourself.
  The compiler also stops a system from touching a component that is not in
  its list.

The example: poisoned entities lose health. `PoisonSystem` asks for damage.
`DamageSystem` is the only place where health goes down. It applies the damage
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
files use, so do not change it after you ship. Use a prefix (`MyGame.Health`)
so that the name is unique. Also, a component struct must not have the same
name as any class in `namespace Fling`.

There are four kinds of component. Set `.Kind` to choose one:

| Kind | What it is | Example |
|---|---|---|
| `Table` (the default) | One per entity | `Health`, `Transform` |
| `Tag` | An empty struct that marks an entity | `Invulnerable` |
| `Singleton` | One per world, not on an entity | `SimTime`, `InputState` |
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

`.Transient = true` means "never saved". Use it for runtime state such as queues
and counters.

Use Fling types in components: `uint32` and `int64`, not `uint32_t`; `String`
for text and `Name` for identifiers; `Fling::Entity` for a reference to another
entity. See [`Skills/strings.md`](../Skills/strings.md).

## Step 2: the systems

Each system is a header in `inc/Systems/<Name>System.h` and a body in
`src/Systems/<Name>System.cpp`. (The tutorial file puts them in one place to keep
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
  - `Read<T>` lets you read `T`. Ask for `const T`.
  - `Write<T>` lets you read and change `T`.
  - `Append<Q>` lets you push to queue `Q`. Many systems can append to the same queue.
  - `Consume<Q>` lets you drain queue `Q`. Only one system can consume a queue.
  - `Spawns` lets you create entities.
- `Info` gives the system a unique name and a **phase** (when in the tick it
  runs). Gameplay goes in `Fling::Phases::Simulation`.
- `FixedUpdate` runs once per fixed tick (60 per second by default), so the
  same input always gives the same result. Use `SimTime::FixedDelta` for time,
  not a clock.
- `ctx.Query<const Poison>()` gives every entity that has a `Poison`. Add more
  types to get entities that have all of them:
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

    void OnCreate(Setup& setup) const;
    void OnDestroy(Setup& setup) const;
    void OnPlayBegin(Context& ctx) const;
    void FixedUpdate(Context& ctx) const;
};

// The consumer of a queue makes it, and the owner of a singleton makes it
void DamageSystem::OnCreate(Setup& setup) const
{
    setup.AddSingleton<DamageRequests>();
    setup.AddSingleton<DamageStats>();
}

void DamageSystem::OnDestroy(Setup& setup) const
{
    setup.RemoveSingleton<DamageStats>();
    setup.RemoveSingleton<DamageRequests>();
}

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
        // A request can point at an entity that is gone, so follow it with TryGet
        Health* health = ctx.TryGet<Health>(request.Target);
        if (health == nullptr || health->Current <= 0.0f || ctx.Has<Invulnerable>(request.Target))
        {
            continue;
        }

        health->Current -= request.Amount;
        if (health->Current <= 0.0f)
        {
            ctx.Commands().Destroy(request.Target); // Destroy waits until the end of the tick
            ++stats.Kills;
        }
    }
}
```

- **The state of a system goes in a singleton component.** A system cannot
  have member variables (this does not compile). The system that writes a
  singleton makes it in `OnCreate` and removes it in `OnDestroy`.
- **`OnPlayBegin`** runs when play starts (also when you press Play in the
  editor). Reset per-play state there.
- **`ctx.Drain<Q>()`** gives each request in the queue once, and then empties it.
- **`ctx.TryGet<T>(entity)`** gives a pointer, or null if the entity is gone or
  does not have `T`. `ctx.Get<T>(entity)` is the same, but the entity must have
  `T`. `ctx.Has<T>(entity)` checks for a component without reading it. This is
  the only way to check a tag.
- **Destroy is deferred.** `ctx.Commands().Destroy(e)` removes the entity
  (and its children) at the end of the tick, so it does not change a query
  while the query runs. `ctx.Add(e, value)` and `ctx.Remove<T>(e)` are deferred
  in the same way.
- **Spawn is immediate.** With `Spawns` in the access list,
  `ctx.Spawn(Health{}, Poison{})` gives a new entity at once.

You do not need to say that `PoisonSystem` runs before `DamageSystem`. Every
system that appends to a queue runs before the system that consumes it. If
two systems write the same component and nothing orders them, the schedule
reports an error and tells you to add `.After = "OtherSystem"` or
`.Before = "OtherSystem"` to `Info`.

### Frame systems

`FixedUpdate` is for simulation. For work at the display rate (camera
smoothing, UI, interpolation), put the system in a frame phase such as
`Fling::Phases::PostSimulation` and write `Update(Context&)` in place of
`FixedUpdate`. A frame system can read simulation components but cannot change
them. It writes components with `.Domain = Fling::ComponentDomain::Presentation`.
For the frame time, read `Fling::RealFrameTime`.

## Step 3: register them

Each game has one register function. The Sandbox's is
[`Sandbox/Gameplay/src/RegisterSandbox.cpp`](../Sandbox/Gameplay/src/RegisterSandbox.cpp)
(`Game::Register`). An engine module has `Register<Module>`. This is the only
file that includes the system headers.

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

Each `Field` line lists a field once. Level save and load, the editor
inspector, and the editor's play snapshot all use that list. The first argument
is the name of the field in the level file. `Range` and `Tooltip` are for the
editor. A component with no saved fields (a tag, a queue, a transient
singleton) is registered with no builder.

Now a level file can hold the component:

```json
{ "Tutorial.Health": { "current": 40.0, "max": 80.0 } }
```

To change a saved component later without breaking old levels, use
`FormerNames`, `Removed`, and `Version` with `Upgrade`. See "Changing a saved
component" in [`Skills/ecs.md`](../Skills/ecs.md#a-component).

## Step 4: test it

A system is a function of its components, so a test needs no window and no
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

Put the test in `FlingTests/<Module>/` (see [`Skills/testing.md`](../Skills/testing.md)).
To test the full game, run it headless with recorded input:
`Sandbox --null-render --unattended --frames 120 --replay-input <file>` (see
[`Skills/input-playback.md`](../Skills/input-playback.md)).

## Using the engine's components and systems

Your systems use engine data in the same way as their own: add the
component to the access list, then query it or read the singleton.

### Time

| Component | Kind | Use |
|---|---|---|
| `Fling::SimTime` | Singleton | `FixedDelta` (seconds per tick), `Tick`, `Elapsed`. `Read` only. |
| `Fling::RealFrameTime` | Singleton | Frame systems only: the real time of this frame. |

Do not call `Timing` or the global `Random` from a fixed system. A debug build
asserts if you do, because it breaks replays and rollback.

### Input

`Fling::InputState` (singleton, from `Components/InputState.h`) is this tick's
input. `Engine.Input` writes it before gameplay runs.

```cpp
// Access<Fling::Read<Fling::InputState>, ...>
const Fling::InputState& input = ctx.Singleton<const Fling::InputState>();
if (input.WasPressed(Fling::Keys::Space)) { /* once, on the tick the key goes down */ }
if (input.IsHeld(Fling::Keys::Left)) { /* every tick while it is down */ }
const float stick = input.GetAxisValue(Fling::Keys::GamepadLeftX);
```

A key that is pressed and released in one tick is seen only by `WasPressed`.
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
an entity, and it draws. Lights are `Fling::PointLight` and
`Fling::DirectionalLight`. The renderer never reads the world: `Graphics.Extract`
copies what it needs each frame, so your systems do not touch the GPU.

```cpp
// Access<..., Fling::Write<Fling::MeshRenderer>, Fling::Write<Fling::Transform>, Fling::Spawns>
Fling::Transform transform;
transform.Position = glm::vec3(x, 0.0f, 0.0f);
ctx.Spawn(Fling::MeshRenderer("Models/sphere.obj", "Materials/DeferredBronzeMat.mat"), transform);
```

Example: [`TestSpawnSystem.cpp`](../Sandbox/Gameplay/src/Systems/TestSpawnSystem.cpp).

### Physics

Give an entity a `Transform`, a `Fling::RigidBody` (mass 0 is static), and a
`Fling::Collider`, and physics moves it. Physics bodies must be root entities
(no `Parent`).

- **To push a body**, append to the `Fling::PhysicsCommands` queue.
  `Physics.Step` consumes it, so your system runs before the step
  automatically:

  ```cpp
  // Access<..., Fling::Append<Fling::PhysicsCommands>>
  ctx.Queue<Fling::PhysicsCommands>().Push(
      Fling::PhysicsCommand{entity, Fling::PhysicsCommandType::Impulse, glm::vec3(0.0f, 5.0f, 0.0f)});
  ```

- **To react to hits**, read the `Fling::ContactEvents` singleton. It holds the
  contacts of the last step. Example:
  [`FirstHitSystem.cpp`](../Sandbox/Gameplay/src/Systems/FirstHitSystem.cpp).

### Asking the application for something

Gameplay cannot close the window. It appends a request to `Fling::AppRequests`,
and the application does it after the tick:

```cpp
// Access<..., Fling::Append<Fling::AppRequests>>
ctx.Queue<Fling::AppRequests>().Push(Fling::AppRequest::Quit);
```

Example: [`AppKeysSystem.cpp`](../Sandbox/Gameplay/src/Systems/AppKeysSystem.cpp).

### Phases

The engine's phases run in this order. Put your system in one, and use
`.After` / `.Before` with system names to order it inside the phase.

| Rate | Phase | What runs |
|---|---|---|
| Fixed | `Phases::Input` | Engine: fills `InputState` |
| Fixed | `Phases::Simulation` | Your gameplay |
| Fixed | `Phases::ResolveDeferred` | Consumers of queues: spawns, damage, physics step |
| Fixed | `Phases::Transform` | Engine: computes `LocalToWorld` |
| Frame | `Phases::PostSimulation` | Camera, interpolation, UI |
| Frame | `Phases::Extract` | Engine: copies the world for the renderer |
| Frame | `Phases::Editor` | Editor tools |

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
| "A frame rate system cannot write a Simulation component" | Write a `Presentation` component, or move the system to a fixed phase |
| "Only a writer of a singleton makes it" | `AddSingleton<T>` needs `Write<T>` |
| "is not a Fling::Component" | Add the `static constexpr Fling::ComponentInfo Component{...}` line |

If the schedule has an error (two writers with no order, a cycle), the build
fails when it links Sandbox and prints the report. `--dump-schedule` shows it
too.
