# ECS systems and components: the first slice

Status: work items 1 to 6 (with 3b, open phases, §3.8) are done (branch
`feature/ecs-types`), except `InputSystem` (see item 6). §6 lists what changed from this plan while they were written.

This plan is the first step of [`ecs-plan.md`](ecs-plan.md). It adds the
system interface, component declarations, access declarations, and the
schedule builder. They run **beside EnTT**: the storage is still
`entt::registry` until the native backend replaces it (`ecs-plan.md` Phase F).

Companion plans:

- [`ecs-features.md`](ecs-features.md): who owns what, what a world is,
  features that add and remove systems at runtime, the end of the `Game`
  class, level streaming, and the editor (edit commands, edit and play
  worlds, editor-only systems).
- [`ecs-parallel.md`](ecs-parallel.md): running systems in parallel, and
  parallel level loading.

The ECS rules come from Timothy Ford's GDC 2017 talk, *'Overwatch' Gameplay
Architecture and Netcode*
(https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and),
through the `gameplay-architecture:ecs-architecture` skill. The fixed and
variable update rules were reviewed with the `rollback-netcode-expert` agent.

## Goals

1. An abstract system interface in `Fling`, in the Engine module, with a clear
   lifecycle: create, play begin and end, fixed update, frame update, destroy.
2. A system states its dependencies **as types on the class**. You can read
   them at the top of the class. The compiler checks that the body uses only
   what the class declares.
3. The update order comes from those declarations. If `InputSystem` writes
   `InputState` and `MoverSystem` reads it, `InputSystem` runs first. Nobody
   writes that order by hand.
4. Components are plain structs. They plug into today's
   `ComponentTypeRegistry`, level files, and the editor with no change to
   level JSON.
5. Everything works while EnTT is still the storage, and nothing in the
   public API names EnTT.

## Non-goals (for this slice)

- Native archetype storage (`ecs-plan.md` Phase F).
- Running systems in parallel. The schedule records parallel stages, but
  version 1 runs one system at a time. This slice keeps the rules that make
  parallel execution possible later ([`ecs-parallel.md`](ecs-parallel.md)):
  one command buffer and one queue buffer per system, and every storage pool
  made when the schedule is built.
- Adding and removing systems at runtime ([`ecs-features.md`](ecs-features.md)).
  In this slice the system set is fixed after startup. Nothing here blocks the
  change: the schedule is a value that is rebuilt, never changed in place.
- The edit and play world split ([`ecs-features.md`](ecs-features.md) §6.2).
  In this slice the one world still switches between edit and play with
  today's snapshot.
- Rollback and resim. This slice only makes rules that keep them possible.
- Removing `Game::Update`. It keeps running as a legacy step until each
  Sandbox feature moves to a system.

## What changes from `ecs-plan.md`

| `ecs-plan.md` | This plan | Why |
|---|---|---|
| Access is recorded at runtime in `Declare(SystemDecl&)` | Access is a type list on the class. The runtime table is made from it at compile time | Lets the compiler check each `Query<>` against the declaration |
| A mismatched query asserts in debug builds | A mismatched query does not compile | Same reason |
| `System` has `Declare` and `Update` only | `ISystem` has a full lifecycle (§3) | Systems must make and remove their singleton components |
| "The same component written from two phases is allowed" | Only between two fixed phases. A frame phase that writes a `Simulation` component is an error | Rollback: the sim must not depend on frame rate |
| `RunIf(const World&)` sees the whole world | `RunIf` gets the same typed context, so it reads only what the system declares | It was a way around the access checks |
| Systems have no header | Every system is a header in `inc/Systems/` plus a `.cpp`. Only its own `.cpp`, the module's register file, and tests may include it | Conventional C++ layout, the header is a readable contract, and tests can run one system (§3.3) |

Everything else in `ecs-plan.md` stays: the six phases, singleton components,
deferment queues, `RunIn`, `--validate-schedule`, and the migration
phases. `ecs-features.md` changes the "one world" rule.

---

## 1. Where the code lives

```
FlingEngine/Engine/inc/Ecs/
    TypeList.h          Fling::TypeList, Contains, IndexOf
    TypeHash.h          constexpr FNV-1a; Fling::TypeHash
    Entity.h            Fling::Entity (8-byte handle)
    Component.h         ComponentInfo, ComponentKind, the Fling::Component concept
    Access.h            Read<T>, Write<T>, Append<Q>, Consume<Q>, Spawns, Access<...>
    Phase.h             Phase, UpdateRate, WorldRole
    System.h            ISystem, SystemInfo, SystemDesc
    SystemBase.h        Fling::System<Derived, Access>: the adapter authors derive from
    SimTime.h           SimTime, RealFrameTime (the runner writes them)
    SystemContext.h     SystemContext<Self, Access>: Query, Get, Singleton, Queue, Spawn (EnTT inside)
    detail/EnttBridge.h Fling::Entity <-> entt::entity
    SystemRegistry.h    SystemRegistry (owns the systems one world runs)
    Schedule.h          Schedule, ScheduleEdge, ScheduleError, BuildSchedule()
    SystemRunner.h      Runs a Schedule against a World each frame
FlingEngine/Engine/src/Ecs/
    Schedule.cpp        Kahn's sort, conflict checks, cycle reports (non-template)
    SystemRunner.cpp
    EnttBridge.cpp      All EnTT calls that are not templates
```

Everything is in the Engine module, beside `World`, `Transform`, and
`ComponentTypeRegistry`. Core stays the low-level layer (math, threading,
platform) and gets no ECS types. This is also where `ecs-plan.md` §1.11 put
them. The native storage (`ecs-plan.md` Phase F) also goes in Engine, not
in `Core/src/Ecs/` as Phase F says today.

Graphics, Physics, Editor, and gameplay modules already depend on Engine, so
they can all declare components and systems.

The headers stay small so that a component header is cheap to include:
`Component.h`, `Entity.h`, `TypeHash.h`, and `TypeList.h` include only
`FlingTypes.h` (for `uint8`, `uint32`, and `uint64`; ECS code uses these, not
`std::uint32_t`), `<string_view>`, and `<type_traits>`. `SystemContext.h`
(through `detail/EnttBridge.h`) is the only header that pulls in the EnTT
registry. `SystemRegistry.h` includes it, because `Add<T>()` makes the system.

---

## 2. Components

### 2.1 A table component

A component is a struct with data and one `static constexpr` line that names
it. Nothing else is needed for queries.

```cpp
// Sandbox/Gameplay/inc/Mover.h
#pragma once

#include "Ecs/Component.h"
#include "FlingMath.h"

/** Moves an entity toward a target, then turns around. */
struct Mover
{
    static constexpr Fling::ComponentInfo Component { .Name = "Mover" };

    glm::vec3 TargetPos { -10.0f, 0.0f, 0.0f };
    float Speed = 1.0f;
};
```

`ComponentInfo` in `Engine/inc/Ecs/Component.h`:

```cpp
namespace Fling
{
    enum class ComponentKind : uint8
    {
        Table,      // One per entity. The default.
        Tag,        // Empty struct. Marks an entity. No data.
        Singleton,  // Exactly one per world. Holds what would be system state.
        Queue,      // A Singleton that holds deferred requests (Ford, 18:10).
    };

    enum class ComponentDomain : uint8
    {
        Simulation,     // Written only at the fixed tick. The default.
        Presentation,   // Written only at the frame rate (camera smoothing, UI state).
    };

    struct ComponentInfo
    {
        const char* Name = nullptr;     // Saved name. Never change it; add FormerNames later.
        ComponentKind Kind = ComponentKind::Table;
        ComponentDomain Domain = ComponentDomain::Simulation;
        bool Transient = false;         // Never saved to levels (proxies, caches, queues).
    };

    /** True for a type with a static constexpr ComponentInfo named Component. */
    template<typename T>
    concept Component = requires {
        { T::Component } -> std::convertible_to<const ComponentInfo&>;
    } && std::is_nothrow_move_constructible_v<T> && !std::is_pointer_v<T>;

    /** Stable 64-bit id from the saved name. Computed at compile time. */
    template<Component T>
    inline constexpr TypeHash TypeHashOf = HashName(T::Component.Name);
}
```

The struct line is the only opt-in. For a type you cannot edit (for
example a `glm` type), specialize `Fling::ComponentTraits<T>` instead. The
concept accepts either.

### 2.2 Singletons, tags, and queues

```cpp
// Engine/inc/Components/InputState.h
/** This tick's input. InputSystem writes it. Gameplay reads it. */
struct InputState
{
    static constexpr Fling::ComponentInfo Component {
        .Name = "InputState",
        .Kind = Fling::ComponentKind::Singleton,
        .Transient = true };

    glm::vec2 Move {};
    glm::vec2 Look {};
    uint32 PressedThisTick = 0;     // Bit per action.
    uint32 Held = 0;
};

/** Marks entities that the deferred renderer draws. Replaces entt::tag<"Default"_hs>. */
struct DeferredRenderTag
{
    static constexpr Fling::ComponentInfo Component {
        .Name = "DeferredRenderTag",
        .Kind = Fling::ComponentKind::Tag };
};

/** Spawn requests. Many systems append. ResolveSpawnSystem consumes them once per tick. */
struct SpawnQueue
{
    static constexpr Fling::ComponentInfo Component {
        .Name = "SpawnQueue",
        .Kind = Fling::ComponentKind::Queue,
        .Transient = true };

    std::vector<SpawnRequest> Requests;
};
```

Rules for component structs (these are the Overwatch rules, Ford 01:00 and
03:35):

- Data only. Small inline accessors are allowed. Behavior goes in a system
  or a `*Util` free function.
- No pointers to other components and no raw resource pointers. Use `Entity`
  handles and asset references.
- A `Tag` must be an empty struct. A `Table` must not be empty. These are
  `static_assert`s in `Component.h`.

`Transform` already breaks the first rule (`SetPos`, `CalculateWorldMatrix`).
It keeps its methods during this slice. They move to `TransformUtil` in
`ecs-plan.md` Phase D.

### 2.3 Entities

An entity is an 8-byte handle with no data and no functions of its own.

```cpp
// Engine/inc/Ecs/Entity.h
namespace Fling
{
    /** A handle to an entity in one world. Holds no data. */
    struct Entity
    {
        uint32 Index = UINT32_MAX;        // Slot in the world's location table
        uint32 Generation = UINT32_MAX;   // Goes up each time the slot is used again

        static const Entity Null;

        constexpr bool IsNull() const { return Index == UINT32_MAX; }
        constexpr uint64 ToBits() const { return (uint64(Generation) << 32) | Index; }
        static constexpr Entity FromBits(uint64 bits);

        friend constexpr bool operator==(Entity, Entity) = default;
    };
    static_assert(sizeof(Entity) == 8 && std::is_trivially_copyable_v<Entity>);
}
```

Why an index and a generation, and not one counter:

- **Fast lookup.** `Index` is an array index into the location table (entity
  to archetype, chunk, and row). Getting a component is one array read. A
  counter would need a hash map lookup on every `Get`.
- **Safe stale handles.** When an entity is destroyed, its slot's generation
  goes up. An old handle has the old generation, so `TryGet` returns null.
  It never reads the entity that uses the slot now.

**`EntityGuid`** is a second, stable 64-bit id, stored as a component. Level
files, prefabs, and (later) the network use it. A handle is valid only in
one world, in one run. On load, every `Entity` field in a component is
remapped from its saved guid to a live handle. This keeps references between
entities, which today's level files lose.

**Beside EnTT:** `entt::entity` in EnTT 3.1 is 32 bits (a 20-bit index and a
12-bit version). `Fling::Entity` keeps its final 8-byte layout.
`EnttBridge.cpp` converts between the two, so callers do not change when the
native storage lands.

**Getting components from a handle.** The handle has no `Get` function.
`entity.Get<T>()` would need a global world pointer, which breaks Ford's
"worlds must be instantiable" rule, and it would go around the access
checks. Use the context. The same compile-time rules apply:

```cpp
for (auto [e, missile, xf] : ctx.Query<const Homing, Fling::LocalTransform>())
{
    if (const Fling::LocalToWorld* target = ctx.TryGet<const Fling::LocalToWorld>(missile.Target))
    {
        xf.Position = MoveUtil::Toward(xf.Position, target->Position(), missile.Speed * dt);
    }
    // Null: the target was destroyed. Nothing dangles.
}
```

- `ctx.Get<T>(e)` needs `Write<T>`. `ctx.Get<const T>(e)` needs `Read<T>`.
- `Get` asserts that the component is there. `TryGet` returns a pointer or
  null.
- A lookup by handle is slower than a query. Use it to follow references,
  not as the main loop.
- Editor and tool code outside any system can use
  `EntityRef { World*, Entity }`. Systems never use it.

### 2.4 Nested data and the hierarchy (after this slice)

A component never contains another component. It can contain plain nested
structs, arrays, and enums. Those are data, and they reflect as nested
fields. A part that needs its own components, its own transform, or its own
lifetime (a weapon on a character, a turret on a tank, a light on a lamp,
extra colliders) is a **child entity**.

The hierarchy is these components (`ecs-plan.md` §1.6, Phase D):

```cpp
/** Position, rotation, and scale relative to the parent, or to the world with no parent. Authored. */
struct LocalTransform
{
    static constexpr Fling::ComponentInfo Component { .Name = "Transform" };   // Keeps today's JSON key
    glm::vec3 Position {};
    glm::quat Rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 Scale { 1.0f };
};

/** On the child. The only authored link, so it is the one source of truth. Saved as the parent's EntityGuid. */
struct Parent
{
    static constexpr Fling::ComponentInfo Component { .Name = "Parent" };
    Fling::Entity Value;
};

/** On the parent. Made from Parent by HierarchySystem. Never saved, never edited by hand. */
struct Children
{
    static constexpr Fling::ComponentInfo Component { .Name = "Children", .Transient = true };
    Fling::SmallVector<Fling::Entity, 4> Value;
};

/** The world matrix. Only TransformSystem writes it. */
struct LocalToWorld
{
    static constexpr Fling::ComponentInfo Component { .Name = "LocalToWorld", .Transient = true };
    glm::mat4 Value { 1.0f };
};
```

- **One call site for links.** `HierarchyUtil::SetParent(commands, child,
  newParent)` is deferred to the flush. It is the only way to change a
  parent, so `Parent` and `Children` always agree.
- **`TransformSystem`** computes `LocalToWorld` with parents first. A root
  uses its local matrix. A child uses `parent.LocalToWorld * child local`.
  Version 1 sorts by a cached depth. Later it skips subtrees that did not
  change.
- **It runs at the end of `Simulation`**, not in `PostSimulation`. Gameplay
  and physics read world positions, so the matrix must come from the fixed
  tick. The renderer reads it through extraction.
- **Destroying a parent destroys its subtree** at the destroy flush.
  `HierarchyUtil::Detach` keeps children alive when you want that.
- **Prefabs are subtrees.** Making a prefab instance makes the whole tree at
  once and remaps its internal `Parent` links.
- **The editor outliner** becomes a tree, made by walking `Children`.

Today's `Transform` maps onto this:

| Today | Becomes |
|---|---|
| `m_Pos`, `m_Scale` | `LocalTransform::Position`, `Scale` |
| `m_Rotation` (Euler angles) | `LocalTransform::Rotation` (quaternion). A version upgrader converts old levels |
| `m_worldMat` | `LocalToWorld` |
| `m_Dirty` | Change detection (`Changed<LocalTransform>`) |
| `SetPos`, `CalculateWorldMatrix`, ... | `TransformUtil` free functions |

---

## 3. The system interface

### 3.1 `ISystem`

This is the abstract interface the scheduler owns. Authors do not derive
from it directly. They derive from the `System<>` adapter in §3.3.

The virtuals are named `Run*`, not `OnCreate` or `FixedUpdate`. The author
writes non-virtual `OnCreate(Setup&)` and `FixedUpdate(Context&)`, and a
derived function with the same name as a base virtual would hide it
(`-Woverloaded-virtual`, which `-Wall` turns on in Clang and GCC 13).

```cpp
// Engine/inc/Ecs/System.h
namespace Fling
{
    class SystemContextBase;    // Defined by the module that owns the storage.
    class SystemSetupBase;

    /**
     * A system: functions and no state (Ford 01:00).
     * The scheduler owns one instance per world. The instance has no members,
     * so all state goes in singleton components that the system makes in OnCreate.
     */
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        /** The name, phase, mode mask, and access table. Built at compile time. */
        virtual const SystemDesc& Describe() const = 0;

        /**
         * Called once when the world is made, in schedule order, before the
         * first level loads. The adapter calls the author's OnCreate(Setup&).
         */
        virtual void RunCreate(SystemSetupBase& setup) const = 0;

        /** Called once when the world shuts down, in reverse schedule order. */
        virtual void RunDestroy(SystemSetupBase& setup) const = 0;

        /** Called when the world mode changes to Play, after the edit snapshot. */
        virtual void RunPlayBegin(SystemContextBase& ctx) const = 0;

        /** Called when play stops, before the edit snapshot is restored. */
        virtual void RunPlayEnd(SystemContextBase& ctx) const = 0;

        /** Fixed rate. Called once per sim tick for a system in a fixed phase. */
        virtual void RunFixedUpdate(SystemContextBase& ctx) const = 0;

        /** Variable rate. Called once per rendered frame for a system in a frame phase. */
        virtual void RunUpdate(SystemContextBase& ctx) const = 0;
    };
}
```

### 3.2 The lifecycle

```
World::Init
    BuildSchedule (fails startup and --validate-schedule on error)
    OnCreate         each system, schedule order      -> singletons exist
    load start level
loop, each frame
    [mode change to Play]   snapshot, then OnPlayBegin each system
    for each due sim tick
        fixed phases in schedule order (Engine.Input, Engine.Simulation,
            game phases, Engine.ResolveDeferred, ...)   FixedUpdate
        flush deferred commands (and after any FlushAfter phase)
    Game::Update (legacy, until Sandbox moves to systems)
    frame phases in schedule order (Engine.PostSimulation, Engine.Extract,
        Engine.Editor, game phases)                     Update
    [mode change to Edit]   OnPlayEnd each system, then restore snapshot
World::Shutdown
    OnDestroy        each system, reverse order
```

**The phase decides the update rate.** `Input`, `Simulation`, and
`ResolveDeferred` are fixed rate. `PostSimulation`, `Extract`, and `Editor`
are frame rate. A system has exactly one phase, so it implements exactly one
of `FixedUpdate` or `Update`. The adapter checks this at compile time
(§3.4). A feature that needs both, for example movement plus camera
smoothing, is two systems that share a component.

Rules for the hooks (from the netcode review):

- `OnCreate` makes this system's singletons. Prefer a default value given at
  registration over code in `OnCreate`, so the starting sim state is data and
  goes into snapshots.
- `OnPlayBegin` runs once, live only, with a **fixed** context, after the
  Play snapshot. It counts as setup for tick 0, so a recorded run reproduces
  it. `OnPlayEnd` runs live only.
- A resim (later) calls **no** lifecycle hook. It only re-runs
  `FixedUpdate` on systems that opt in with a class constant, not a hook.
  Simulation code cannot ask "is this a resim?". Branching on that is a
  desync.
- Not hooks, on purpose: per-entity construct and destroy callbacks (EnTT
  `on_construct`; they fire again on every restore), pause and time scale
  (the clock owns them, and the sim must not see them), and runtime enable or
  disable. A hook must never play audio or VFX, send network data, or grant
  achievements.
- Added later with rollback: `OnRestored` (rebuild derived singletons such as
  a spatial hash after a restore), and a confirmed-tick consumer for effects
  that cannot be undone.

Until `improve-time.md` lands, the fixed tick is a plain accumulator in
`SystemRunner`, driven by today's `Fling::TickRate` from
`Threading/ThreadTypes.h`. Under `--null-render` it runs one tick per frame,
as the loop does today. Note: `improve-time.md` wants its own integer
`TickRate`. The threading one (a nanosecond `Period`) should be renamed
`TickPeriod` before that lands.

**Input.** The OS is polled once per frame, outside the schedule.
`InputSystem` (phase `Input`, fixed, live only) turns that tick's
`TickInput` into `InputState`. Dead zones and action mapping run in the
`Input` phase too. When recording arrives, the recorded per-tick commands
are the output of the `Input` phase, so a replay does not run it again.

### 3.3 Writing a system

Every system is a header and a `.cpp`. The header declares the class: its
name, its phase, and its `Access<...>` list. The `.cpp` has the body. This
is the Mover loop from `SandboxGame.cpp`, as a system:

```cpp
// Sandbox/Gameplay/inc/Systems/MoverSystem.h
#pragma once

#include "Components/Transform.h"
#include "Ecs/SystemBase.h"
#include "Mover.h"
#include "SimTime.h"

namespace Sandbox
{
    /** Moves each Mover toward its target, and flips the target when it gets there. */
    class MoverSystem final : public Fling::System<MoverSystem, Fling::Access<
        Fling::Read<Fling::SimTime>,
        Fling::Write<Mover>,
        Fling::Write<Fling::Transform>>>
    {
    public:
        static constexpr Fling::SystemInfo Info {
            .Name = "Sandbox.Mover",
            .Phase = Fling::Phases::Simulation };

        void FixedUpdate(Context& ctx) const;
    };
}
```

```cpp
// Sandbox/Gameplay/src/Systems/MoverSystem.cpp
#include "Systems/MoverSystem.h"

namespace Sandbox
{
    void MoverSystem::FixedUpdate(Context& ctx) const
    {
        const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;

        for (auto [entity, transform, mover] : ctx.Query<Fling::Transform, Mover>())
        {
            const glm::vec3 pos = transform.GetPos();
            if (glm::length(pos - mover.TargetPos) < 0.1f)
            {
                mover.TargetPos = -mover.TargetPos;
            }
            transform.SetPos(glm::lerp(pos, mover.TargetPos, mover.Speed * dt));
        }
    }
}
```

The header is the system's contract. What you see in it is everything the
system touches, with no body in the way. That is the readable dependency
list. The adapter calls `FixedUpdate` through the derived type, so the
out-of-line definition links like any other member function.

Each module has one **register file** that includes its system headers and
adds them. Reading it lists every system in the module:

```cpp
// Sandbox/Gameplay/src/RegisterSandboxSystems.cpp
#include "SandboxGame.h"
#include "Systems/MoverSystem.h"
#include "Systems/PlayerMoveSystem.h"
#include "Systems/RotatorSystem.h"

void Fling::Game::RegisterSystems(Fling::SystemRegistry& systems)
{
    systems.Add<Sandbox::MoverSystem>();
    systems.Add<Sandbox::RotatorSystem>();
    systems.Add<Sandbox::PlayerMoveSystem>();
}
```

**Keeping "systems never call systems" with headers.** Overwatch's rule is
that systems never call other systems (Ford 14:55). A header makes a system
includable, so a CI check keeps the rule:

- System headers live in `inc/Systems/`, one class per file, named
  `<Name>System.h`, with the body in `src/Systems/<Name>System.cpp`.
- Only three kinds of file may include `Systems/...`: the system's own
  `.cpp`, a module register file (`Register*Systems.cpp`), and tests under
  `FlingTests/`. Another system, a component header, or a `*Util` file never
  includes one.
- `scripts/check_system_includes.py` (CI, beside `check_comment_style.py`)
  fails on any other include.
- Shared code between systems goes in a `*Util` free function.

Changing a system's body recompiles one `.cpp`. Changing its header
recompiles that `.cpp`, the register file, and the tests that include it.

Tests include the header and run one system against a small registry. In a
strict ECS a system is a function of its components, so this is the main
way to test gameplay.

The only exception is a fixture system that exists only for one test. It may
be declared inside that test's `.cpp`.

A system that depends on input does not name `InputSystem`. It reads the
data that `InputSystem` writes, and the order follows from that:

(The next two examples show the header and the body together, to keep them
short. In the repo the body goes in the `.cpp`.)

```cpp
class PlayerMoveSystem final : public Fling::System<PlayerMoveSystem, Fling::Access<
    Fling::Read<Fling::InputState>,     // -> runs after InputSystem (it writes InputState)
    Fling::Read<Fling::SimTime>,
    Fling::Read<ControlledByLocal>,     // Tag
    Fling::Write<Fling::Transform>,
    Fling::Append<SpawnQueue>>>         // -> runs before ResolveSpawnSystem
{
public:
    static constexpr Fling::SystemInfo Info {
        .Name = "Sandbox.PlayerMove",
        .Phase = Fling::Phases::Simulation };

    void FixedUpdate(Context& ctx) const
    {
        const Fling::InputState& input = ctx.Singleton<const Fling::InputState>();
        const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;

        for (auto [entity, transform] : ctx.Query<Fling::Transform>(Fling::With<ControlledByLocal>{}))
        {
            transform.SetPos(transform.GetPos() + glm::vec3(input.Move, 0.0f) * dt);

            if (input.PressedThisTick & ActionBit::Fire)
            {
                // Ford 18:10: do not spawn here. Queue it; one system resolves all spawns.
                ctx.Queue<SpawnQueue>().Push({ .Prefab = "Bullet", .At = transform.GetPos() });
            }
        }
    }
};
```

`InputSystem` is ordinary. It is the only writer of `InputState`, and it
makes that singleton in `OnCreate`:

```cpp
class InputSystem final : public Fling::System<InputSystem, Fling::Access<
    Fling::Write<Fling::InputState>>>
{
public:
    static constexpr Fling::SystemInfo Info {
        .Name = "Engine.Input",
        .Phase = Fling::Phases::Input,
        .RunIn = Fling::WorldRole::Play };

    void OnCreate(Setup& setup) const
    {
        setup.AddSingleton<Fling::InputState>();
    }

    void FixedUpdate(Context& ctx) const
    {
        Fling::InputState& state = ctx.Singleton<Fling::InputState>();
        state = InputUtil::Sample(ctx.TickInput());   // This tick's input, not "now"
    }
};
```

`Input` is its own phase, so `InputSystem` is before every `Simulation`
system even with no edge. The edge from `Read<InputState>` matters when two
systems in one phase share data, and it is what the schedule dump shows as
the reason.

### 3.4 The adapter: `Fling::System<Derived, Access>`

A virtual function cannot be a template, so a plain
`virtual void Update(SystemContext&)` could not check a `Query<T>` at
compile time. The adapter solves that. It implements the `ISystem` virtuals,
makes the typed context for that system's access list on the stack, and
calls the author's non-virtual function. The scheduler sees only `ISystem`.
Authors never see `SystemContextBase`.

`Context` is `SystemContext<Derived, Access>`. It is one type for both
update rates. The base class cannot choose a type from `Derived::Info.Phase`,
because `Derived` is not complete yet when its base is made. Instead, the
functions that only a fixed phase may call (`Spawn`, `Commands`, `Queue`,
`Drain`) have a `static_assert` on the phase. A class template's member
function body is compiled only when it is called, so a frame system that
calls `ctx.Spawn()` fails with "Only a fixed phase system spawns entities",
and one that does not call it compiles.

- All systems: `Query`, `Get`, `TryGet`, `IsAlive`, `Singleton`.
- Fixed phases only: `Queue`, `Drain`, `Spawn`, `Commands`. A frame rate
  system that wants to change the sim (a UI button, an editor action) sends
  a command that the next tick reads.

```cpp
// Engine/inc/Ecs/SystemBase.h  (shortened)
template <typename Derived, typename AccessList>
class System : public ISystem
{
public:
    using Access = AccessList;
    using Context = SystemContext<Derived, AccessList>;
    using Setup = SystemSetup<AccessList>;

    const SystemDesc& Describe() const final { return Detail::DescOf<Derived, AccessList>; }

    void RunFixedUpdate(SystemContextBase& core) const final
    {
        if constexpr (Detail::HasFixedUpdate<Derived>)
        {
            Context ctx(core);      // A reference to the core. Costs nothing.
            Self().FixedUpdate(ctx);
        }
    }
    // RunUpdate, RunCreate, RunDestroy, RunPlayBegin, RunPlayEnd: same shape.
};
```

`DescOf` is a variable template outside the class, so it is made only when
`Describe` is compiled, after `Derived` is complete.

`SystemRegistry::Add<T>()` is where `T` is a complete type, so the checks
that need it go there:

```cpp
template <typename T>
void SystemRegistry::Add()
{
    static_assert(Detail::CheckSystem<T>());   // Each rule is its own static_assert, with its own message.
    m_Systems.push_back(std::make_unique<T>());
}
```

`FlingTests/EcsCompileFail/EcsCompileFail.cpp` has one case per rule. Each
`ECS compile check: <Case>` CTest builds it and passes only if the build
fails with that rule's message.

### 3.5 `SystemInfo` fields

| Field | Default | Meaning |
|---|---|---|
| `Name` | required | `"Module.Name"`. Used in errors, the schedule dump, `After`, and profiling |
| `Phase` | required | One of the six phases. It decides `FixedUpdate` or `Update` |
| `RunIn` | `WorldRole::Play` | A mask of world roles (`Edit`, `Play`, `Preview`). In this slice it is checked each frame against the one world's current mode. After the edit/play split ([`ecs-features.md`](ecs-features.md) §6.3) a system that does not match a world's role is never added to that world |
| `After`, `Before` | none | Names of systems in the same phase. Replaces the inferred edge between the two |
| `MainThreadOnly` | `false` | For systems that touch GLFW or ImGui. Read by the parallel executor ([`ecs-parallel.md`](ecs-parallel.md) §1) |
| `ReplayOnResim` | `false` | Later, with rollback |

`RunIf` is a static member function, not a field, so it can take the typed
context: `static bool RunIf(const Context& ctx)`.

### 3.6 What the compiler checks, and what the build checks

**Compile time** (a mistake does not compile, and the message names the
system and the type):

| Check | Where |
|---|---|
| Every type in `Access<...>` is a `Fling::Component` | `Access<...>` |
| No type appears twice. `Write<T>` already implies read | `Access<...>` |
| `Append`/`Consume` only name `Queue` kinds. `Read`/`Write` never name a `Queue` | `Access<...>` |
| `ctx.Query<T>()` needs `Write<T>`. `ctx.Query<const T>()` needs `Read<T>` or `Write<T>` | `SystemContext::Query` |
| `ctx.Singleton<T>()` needs a declared singleton. `Query` of a singleton does not compile | `SystemContext` |
| `With<T>` / `Without<T>` filters need `Read<T>` | `SystemContext::Query` |
| `ctx.Get<T>(e)` needs `Write<T>`. `ctx.Get<const T>(e)` and `TryGet` need `Read<T>` or `Write<T>` | `SystemContext` |
| `ctx.Spawn()` needs `Fling::Spawns` in the access list | `SystemContext::Spawn` |
| `ctx.Queue<Q>()` needs `Append<Q>`. `ctx.Drain<Q>()` needs `Consume<Q>` | `SystemContext` |
| No member state | `SystemRegistry::Add` |
| Phase and update rate agree. A fixed system does not read `RealFrameTime` | `SystemRegistry::Add` |
| A frame rate system does not write a `Simulation` component | `SystemRegistry::Add` |
| A fixed rate system does not read a `Presentation` component ("drive visuals, don't depend on them") | `SystemRegistry::Add` |
| A frame rate system does not spawn, destroy, or append to a sim queue | `static_assert` on the phase in those functions |
| No system writes `SimTime` or `RealFrameTime` (the runner does) | `Access<...>` |
| `setup.AddSingleton<T>()` needs `Write<T>` (the owner makes it) | `SystemSetup` |

The checks are `consteval` helpers (`Allows<Access, AccessMode::Write, T>`)
plus `static_assert` at the call site. Tests check the helpers with
`STATIC_REQUIRE`, so a rule that stops working fails CTest.

**Build time** (needs every system at once, so it cannot be one compile):

| Check | Result |
|---|---|
| Two systems in one phase write the same component, and their `RunIn` masks overlap, and neither orders the other | error |
| A cycle in one phase | error, with the path and each system's file |
| An `After`/`Before` name that does not exist or is in another phase | error |
| A component or singleton that a system reads, but no system writes and nothing registers | warning |
| Two systems in one stage that both `Append` to the same queue | no edge; the consumer sees them in name order |
| A component that a fixed phase writes and a frame phase also writes | error (phase order does not make that safe) |

Some rules are runtime asserts in debug builds, because no type shows them:
a fixed rate system must not call `PlatformTime`, the global `Random`, or
`Timing`. The runner sets an "in simulation" flag that those functions
check.

**`TransformSystem` note.** `ecs-plan.md` puts it in `PostSimulation`, a frame
phase. If gameplay or physics reads world matrices, it must run at the end
of `Simulation` instead. Decide when it is written (`ecs-plan.md` Phase D).

`BuildSchedule` runs at startup and in `--validate-schedule`.
`fling_validate_schedule(Sandbox)` runs it as a POST_BUILD step, so
`cmake --build` fails on a schedule error before anyone launches the game.

**Why not sort at compile time?** It is possible in C++20: put every system
type in one `TypeList` and run Kahn's algorithm in a `consteval` function.
But that list must see every system class, so one file must include every
system header in the game. A change to any system header then recompiles
that file, and the file breaks the "only the register file includes a
system" rule across modules. Systems are spread across Engine, Graphics,
and game modules, so the build-step check gives the same
guarantee (a bad schedule never runs) without that cost. The access table
itself *is* made at compile time; only the graph sort runs later.

### 3.7 How the order is made

`BuildSchedule` gets each system's `SystemDesc`: a name, a phase, a
`RunIn` mask, and a `constexpr` span of `{TypeHash, AccessMode}`. Between
two systems that phase order does not already order (§3.8):

| Declarations | Edge |
|---|---|
| A `Write<X>`, B `Read<X>` | A before B |
| A `Append<Q>`, B `Consume<Q>` | every appender before the consumer |
| A `Read<X>`, B `Read<X>` | none |
| A `Write<X>`, B `Write<X>` | error, unless `.After = "A"` on B (or `.Before`) |
| `.After = "Name"` in `SystemInfo` | replaces the inferred edge between those two |

Then Kahn's algorithm, with ties broken by name, gives the stages. The
output is stable across platforms and runs, which rollback needs later.

`Sandbox --dump-schedule` prints it:

```
Phase Input
  stage 0  Engine.Input              writes InputState
Phase Simulation
  stage 0  Sandbox.Mover             writes Mover, Transform
  stage 1  Sandbox.Rotator           writes Transform  (.After = "Sandbox.Mover")
  stage 2  Sandbox.PlayerMove        writes Transform  (.After = "Sandbox.Rotator")
Phase ResolveDeferred
  stage 0  Engine.ResolveSpawn       after Sandbox.PlayerMove: it appends SpawnQueue, consumed here
```

Without the two `.After` lines this is a build error, because three systems
write `Transform` and nothing orders them:

```
error: Sandbox.Mover and Sandbox.Rotator both write Transform in Phase Simulation
       and neither orders the other.
  Sandbox/Gameplay/inc/Systems/MoverSystem.h:12    Write<Fling::Transform>
  Sandbox/Gameplay/inc/Systems/RotatorSystem.h:11  Write<Fling::Transform>
  fix: add .After = "Sandbox.Mover" to Sandbox.Rotator's SystemInfo (or the reverse)
```

That is the point of the check: today the order of those loops in
`Game::Update` is an accident of where they were typed.

### 3.8 Open phases: one graph per update rate

The first version of §3.7 had a closed `enum class Phase` with six values,
and made edges only inside one phase. That repeats the problem of a fixed
set of tick groups: a game that needs "after abilities, before damage
resolves" must edit the engine. Two modules that both want to run after
`Simulation` also cannot be ordered against each other unless one knows the
other. This section replaces the enum.

**Three ideas, kept apart:**

| Idea | Who owns it | Open? |
|---|---|---|
| Update rate: `Fixed` (once per sim tick) or `Frame` (once per rendered frame) | Engine | No. Determinism, rollback, and the compile-time rules depend on it |
| Phases: named groups of systems, placed with `After` and `Before` | Any module | Yes |
| Order inside an update rate | The schedule builder, from phases, access lists, and `After`/`Before` | Yes |

**A phase is data.** Any module declares one, the same way it declares a
system:

```cpp
// Game/inc/GamePhases.h
inline constexpr Fling::PhaseInfo AbilityResolvePhase {
    .Name = "Game.AbilityResolve",
    .UpdateRate = Fling::UpdateRate::Fixed,
    .After = "Engine.Simulation",
    .Before = "Engine.ResolveDeferred" };

class CastSystem final : public Fling::System<CastSystem, Fling::Access<...>>
{
public:
    static constexpr Fling::SystemInfo Info { .Name = "Game.Cast", .Phase = AbilityResolvePhase };
    void FixedUpdate(Context& ctx) const;
};
```

`SystemInfo::Phase` is a `PhaseRef`: a pointer to an `inline constexpr`
`PhaseInfo`. The update rate is still known at compile time, so every rule
in §3.6 still compiles. The engine's six phases are ordinary `PhaseInfo`
values in `Fling::Phases` (`Phases::Input`, `Phases::Simulation`, ...),
with the names `Engine.Input`, `Engine.Simulation`, and so on. They are
stable anchors that other modules place themselves against. They are not a
list that only the engine can add to.

**One graph per update rate.** `BuildSchedule` makes one graph for all
`Fixed` phases and one for all `Frame` phases:

- Each phase is two marker nodes, `Begin` and `End`. A system in phase P
  gets the edges `P.Begin -> system -> P.End`.
- "B is after A" (between phases) is the edge `A.End -> B.Begin`.
- Data edges between systems (§3.7 table) are added **across phases**, but
  only between two systems whose phases are the same, or not ordered
  against each other.
- When the phases are ordered (A is before B, directly or through other
  phases), phase order wins. A data edge that points the other way is not
  added: B writes, and A reads the value from the previous tick. That is
  legal, the same as today's `Extract` writing something `Simulation` reads.
- The write/write and consume/consume checks apply to the same pairs as the
  data edges. Two modules that both write `Transform` in the same unordered
  gap are a build error that names both systems.
- `SystemInfo::After` and `Before` can name a system in another phase of the
  same update rate. If that contradicts phase order, it is a cycle, reported
  with its path.
- Nothing crosses update rates. A phase `After` a phase of the other update
  rate, or a system `After` a system of the other update rate, is an error.

Two modules that do not know each other:

```
Engine.Simulation.End
      |-> ModA.Abilities  (CooldownSystem writes Cooldown) --+
      +-> ModB.Combo      (ComboSystem reads Cooldown)    <--+   inferred edge: Cooldown before Combo
Engine.ResolveDeferred.Begin
```

Neither module names the other. The data decides. If they share no data,
they are independent and a parallel executor can run them together.

**Order from outside.** When two modules must be ordered but share no data,
and neither should depend on the other, the project (or any module that
knows both) adds the edge:

```cpp
// The game's register file
systems.Order("ModA.Abilities", "ModB.Combo");     // phases or systems, by name
```

**Sync points.** The end of each fixed tick always applies the deferred
commands. A phase can ask for another barrier with `.FlushAfter = true`.
Such a phase must be ordered against every other phase in its update rate
(directly or through other phases), so that every system is clearly before
or after the flush. `BuildSchedule` reports a phase that is not. The engine's
own phases have no `FlushAfter`, so a game that adds phases never has to
place itself against an engine flush.

**Stages and segments.** The flush points cut each update rate into
segments. Inside a segment, each system's stage is the longest chain of
systems before it (marker nodes count zero), so systems in unrelated phases
can share a stage. Ties are broken by name. The result is the same on every
platform and run.

**Errors added:** a system with no phase; two phases with one name; a phase
`After`/`Before` name that does not exist; a phase ordered against the other
update rate; a `FlushAfter` phase that is not ordered against another
phase; an `Order()` name that does not exist. Cycles (through phases or
systems) use the existing cycle report, with phase markers shown as
`Engine.Simulation (end)`.

**What stays fixed, on purpose:**

- The two update rates, and "a frame rate system cannot write a Simulation
  component".
- The engine anchor names, once published. Treat them like an API.
- That phase order, where it exists, wins over data edges, so a new system
  cannot move whole blocks of the frame.

**The outer loop (after this slice).** `SystemRunner` runs the fixed
segments for each due tick and then the frame segments. Later, the frame
loop itself becomes a list of registered schedules with hooks for work that
is not a system (input polling, network send and receive, the render), and
the pacing policy (accumulator, catch-up cap, one tick per frame, lockstep)
becomes a tick source ([`improve-time.md`](improve-time.md)). Then a
dedicated server registers only the fixed schedule, and a tool registers
only the editor phase.

---

## 4. The context: queries over EnTT

`SystemContext<Access>` is a thin template over a non-template core. During
this slice the core wraps `entt::registry&`. The public header does not
include EnTT: `Query<Ts...>()` returns a `Fling::QueryRange<Ts...>` whose
iterator wraps `entt::view<Ts...>` in `Ecs/detail/EnttQuery.inl`, which only
`SystemContext.h` includes.

| Call | EnTT 3.1 today |
|---|---|
| `ctx.Query<A, const B>()` | `registry.view<A, B>()` |
| `With<T>` / `Without<T>` | extra view types / `entt::exclude<T>` |
| `ctx.Singleton<T>()` | `registry.ctx<T>()` |
| `setup.AddSingleton<T>()` | `registry.set<T>()` |
| `ctx.Queue<Q>().Push(r)` | push into `registry.ctx<Q>()` |
| `ctx.Commands().Destroy(e)` | stored, then `registry.destroy` at the phase flush |
| `ctx.Spawn()` | `registry.create()` (immediate, Ford's rule) |
| `ctx.Get<T>(e)` / `ctx.TryGet<T>(e)` | `registry.get<T>` / `registry.try_get<T>`, after a `valid(e)` check |
| `Fling::Entity` | `entt::entity`, converted in `EnttBridge.cpp` |

**Type ids.** EnTT 3.1 numbers unnamed types with a counter
(`entt::family`) that is two inline variables with no fixed initialization
order. A Clang Release build gave `Position`, `Velocity`, and `Frozen` the
same number, so they shared one pool and the runner tests crashed.
`Component.h` makes every `Fling::Component` an EnTT named type, with an id
from `TypeHashOf<T>`. Legacy types that are not `Fling::Component` yet (for
example `Transform`) still use the counter.

`BuildSchedule` makes the EnTT pool for every component in every access
table (`registry.assure<T>()` through a function pointer stored in the
`SystemDesc`). EnTT 3.1 makes a pool on first use, which changes the
registry. With every pool made up front, a query never changes the registry.
That keeps parallel execution possible later.

Each system has its own deferred command buffer and its own buffer for each
queue it appends to. The runner merges them in schedule order. With one
thread this costs almost nothing, and it is what makes the parallel
executor safe later.

When the native backend lands, only `EnttQuery.inl` and `EnttBridge.cpp`
change.

Legacy code that still takes `entt::registry&` (the renderer, the editor,
`Game::Update`) keeps working, because it is the same registry. That code is
not in the schedule, so the schedule cannot order it. Its place is fixed in
§3.2: `Game::Update` runs between the fixed phases and `PostSimulation`, and
the renderer runs after the whole frame.

---

## 5. Registration, serialization, and the editor

### 5.1 One registration call per feature

Today a game registers components in `Game::RegisterComponents()` with
`ComponentTypeRegistry::Get().Register<T>("Name")`. This slice adds systems
next to that, and reads the name from the struct so it is written once:

```cpp
// Sandbox/Gameplay/src/SandboxGame.cpp
void Game::RegisterComponents()
{
    auto& types = Fling::ComponentTypeRegistry::Get();
    types.Register<Mover>();        // name from Mover::Component.Name
    types.Register<Rotator>();
}

// Game::RegisterSystems is a new virtual (default empty). Sandbox defines it
// in RegisterSandboxSystems.cpp (§3.3).
```

The engine does the same with `RegisterEngineSystems(SystemRegistry&)` in
`Engine/src/Ecs/RegisterEngineSystems.cpp`
(`InputSystem`, `ResolveSpawnSystem`, later `TransformSystem`) and Graphics
with `RegisterGraphicsSystems`. `World` is in Engine and cannot include
Graphics, so `Engine::Startup` (Application, which sees every module) fills
the `SystemRegistry`: engine systems, then Graphics, then
`Game::RegisterSystems`. It gives the registry to the `World`. `World::Init`
then builds the schedule and calls `OnCreate`, after `RegisterComponents`
and before `Seal()`. In this slice the set does not change after that.
[`ecs-features.md`](ecs-features.md) replaces these register calls with
features that can be activated and deactivated at runtime, and deletes
`Game`.

`ecs-plan.md` Phase C later replaces both calls with one
`ModuleRegistration` object per module. Feature code keeps the same shape.

### 5.2 `ComponentTypeRegistry` changes

```cpp
template<Component T>
bool Register();                            // new: name, kind, transient from T::Component

template<typename T>
bool Register(const char* name);            // kept for types not yet ported; logs a deprecation warning
```

The new overload also stores `TypeHashOf<T>`, `Kind`, and `Transient` in
`ComponentTypeInfo`, so:

- `World::SaveToJson` skips `Transient` types and writes singletons that are
  not transient into a new optional `"singletons"` object. Old levels have
  no such key and load unchanged.
- The editor's Add Component menu shows tags and table components, and hides
  singletons and queues. A new **World Singletons** panel lists the
  singletons with the same widget path.
- At startup, every `TypeHash` in a `SystemDesc` must match a registered
  component, or `BuildSchedule` reports it. That finds a component that a
  system uses but that nobody registered.

### 5.3 Serialization, two steps

**Step 1 (this slice):** a component that has `Serialize(JsonArchive&)` keeps
it. Level JSON does not change. `Mover` keeps its method during the move:

```cpp
struct Mover
{
    static constexpr Fling::ComponentInfo Component { .Name = "Mover" };

    glm::vec3 TargetPos { -10.0f, 0.0f, 0.0f };
    float Speed = 1.0f;

    void Serialize(Fling::JsonArchive& Ar)     // Removed in step 2.
    {
        Ar << Fling::MakeNVP("targetPos", TargetPos);
        Ar << Fling::MakeNVP("speed", Speed);
    }
};
```

**Step 2 (`ecs-plan.md` §1.3, Phase C):** fields are described once in the
`.cpp` next to the component, and the save, load, and editor widget come from that.
`Serialize` leaves the struct, so the component has no functions:

```cpp
// Sandbox/Gameplay/src/Components/Mover.cpp
void RegisterMover(Fling::ModuleRegistration& modules)
{
    modules.Component<Mover>([](Fling::TypeBuilder<Mover>& type) {
        type.Version(1).Category("Movement");
        type.Field("targetPos", &Mover::TargetPos);
        type.Field("speed", &Mover::Speed).Range(0.0f, 100.0f).Tooltip("Units per second");
    });
}

// The register file then calls RegisterMover(modules) and
// modules.System<Sandbox::MoverSystem>().
```

The JSON names stay `targetPos` and `speed`, so `Assets/Levels/*.json` loads
the same. The level file looks the same as today:

```json
{
  "version": 1,
  "title": "Level_1",
  "entities": [
    {
      "name": "Mover 0",
      "Transform": { "pos": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
      "Mover": { "targetPos": [-10, 0, 0], "speed": 1.0 }
    }
  ],
  "singletons": { }
}
```

### 5.4 Adding a new component and system: the checklist

1. Write the struct with `static constexpr Fling::ComponentInfo Component`.
2. Add one `types.Register<T>()` line (step 2: one `modules.Component<T>`).
3. Write `inc/Systems/<Name>System.h` (class, `Info`, `Access<...>`) and
   `src/Systems/<Name>System.cpp` (the body).
4. Include it and add one `systems.Add<T>()` line in the module's register
   file.
5. Build. A wrong access does not compile. A schedule conflict fails the
   POST_BUILD step.
6. `Sandbox --dump-schedule` shows where it runs and why.

---

## 6. Work items

Each item is one PR off `main`, green on all CI platforms.

1. **ECS types in Engine.** `TypeList.h`, `TypeHash.h`, `Component.h`, `Access.h`,
   `Phase.h`, and `scripts/check_system_includes.py` in CI. Catch2
   `STATIC_REQUIRE` tests for the concept and every
   `Allows<>` rule. No engine code changes.
2. **`ISystem`, `SystemRegistry`, `BuildSchedule`** in Engine. Tests build
   schedules from fake `SystemDesc` arrays: write-before-read, appender
   before consumer, write/write error, explicit `After`, cycle path, stable
   tie order.
   Done: `Ecs/System.h` (`ISystem`, `SystemInfo`, `SystemDesc`),
   `Ecs/SystemRegistry.h`, `Ecs/Schedule.h`, and `src/Ecs/Schedule.cpp`,
   with `FlingTests/Engine/EcsScheduleTests.cpp`. `BuildSchedule` also
   reports duplicate and missing names, two consumers of one queue, and a
   component written at both update rates. `SystemRegistry::Add` checks only
   "derives from `ISystem`" and "no member state" so far. The phase and
   update-rate checks (`FixedUpdate` against `Update`, `RealFrameTime`,
   `Simulation` writes from a frame system) need the adapter's `Info` and
   `Context`, so they move to item 3. `WritesSimulation<A>` and
   `ReadsPresentation<A>` in `Access.h` are ready for them.
3. **`SystemContext` and the `System<>` adapter** in Engine, over EnTT. Tests
   build an `entt::registry`, run one system, and check the result. Add
   `SimTime` and a 60 Hz accumulator in `SystemRunner`.
   Done: `SystemContext.h`, `SystemBase.h`, `SimTime.h`, `SystemRunner.h`,
   with `FlingTests/Engine/EcsSystemTests.cpp` and the compile checks in
   `FlingTests/EcsCompileFail/`. `SystemRegistry::Add` now has every rule
   in §3.6. Not done yet, and why:
   - `ctx.TickInput()`: no `TickInput` type yet. It comes with
     `InputSystem` in item 6.
   - `RunIf`: no system needs it yet. `RunIn` works.
   - Queue buffers: `ctx.Queue<Q>().Push` writes to the queue directly. With
     one thread that gives the same order as one buffer per system. The
     per-system queue buffers come with the parallel executor.
     Command buffers are already one per system.
   - The debug "in simulation" flag for `Timing` and the global `Random`:
     with the `World` wiring (item 4).
   - The accumulator counts `std::chrono::nanoseconds`, not
     `Threading/ThreadTypes.h`'s `TickRate`, so the `TickPeriod` rename can
     happen without touching the runner.
3b. **Open phases (§3.8).** `PhaseInfo`, `PhaseRef`, and `Fling::Phases`
   replace `enum class Phase`. `BuildSchedule` makes one graph per update
   rate, with segments and stages. `SystemRegistry::AddPhase` and `Order`.
   Tests: two module phases ordered by data, a write conflict across
   unordered phases, phase order winning over a reverse data edge, `Order()`
   on phases and systems, phase cycles, unknown anchors, update-rate
   mismatch, and a `FlushAfter` phase splitting a segment.
   Done: `Ecs/Phase.h` (`PhaseInfo`, `PhaseRef`, `NameList`,
   `Fling::Phases`), the rewritten `BuildSchedule` (`ScheduleInput`,
   segments, `Schedule::Phases`), and `SystemRunner::RunSegments`.
4. **Wire it into `World`.** `World` owns a `SystemRegistry` and a
   `Schedule`. `World::Init` calls `OnCreate`. `World::Update` runs the
   fixed phases, then `Game::Update`, then the frame phases.
   `RequestGameStart`/`Stop` call `OnPlayBegin`/`OnPlayEnd`. Add
   `Game::RegisterSystems`, `--dump-schedule`, `--validate-schedule`, and
   `fling_validate_schedule()`.
   Done:
   - `World` owns the `SystemRegistry` (`GetSystems()`, filled before
     `Init`) and the `SystemRunner`. `World::Init` calls
     `Game::RegisterSystems`, builds the schedule, and returns false on
     errors (`GetScheduleErrors()`), before any level loads.
   - `World::Update` runs `RunFixed`, then `Game::Update` (play only), then
     `RunFrame`. The role is `Play` during play and `Edit` otherwise, so
     default systems (`RunIn = Play`) do nothing in the editor.
   - `StartGame` calls `PlayBegin` after the snapshot and before
     `Game::OnStartGame`. `StopGame` calls `PlayEnd` before
     `Game::OnStopGame` and the restore. `Shutdown` calls `Destroy`.
     Singletons live in the registry context, so the play snapshot restore
     (`registry.reset()`) keeps them.
   - `Engine::Startup` sets `OneTickPerFrame` under `--null-render`.
     Engine and Graphics systems will be added to `World::GetSystems()`
     there, when they exist.
   - `--dump-schedule` and `--validate-schedule` start the world, report,
     and exit with 0 or 1. `fling_validate_schedule(Sandbox)` runs the
     check after each link (`-DFLING_VALIDATE_SCHEDULE=OFF` turns it off).
   - `Misc/SimulationCheck.h` (Core): the runner marks the thread during
     fixed systems and `OnPlayBegin`. `Timing::GetDeltaTime`,
     `Timing::GetTime`, and the global `Random` assert that it is not set.
5. **`ComponentTypeRegistry::Register<T>()`** from `T::Component`, plus
   kinds, transient, and the `"singletons"` level key. Existing levels load
   unchanged (test with `Assets/Levels/Level_1.json`).
   Done:
   - `ComponentTypeRegistry::Register<T>()` takes the name, kind, and
     transient flag from `T::Component`, and stores `hash`, `kind`, and
     `transient` in `ComponentTypeInfo`.
   - A transient type needs no `Serialize` and is never saved. A tag saves
     as `{}`. Singletons and queues get `hasSingleton`, `saveSingleton`, and
     `loadSingleton` (in the registry context), and no entity functions.
   - `World::SaveToJson` writes non-transient singletons into a
     `"singletons"` object only when there is one, so levels with no
     singletons save byte for byte as before. `LoadFromJson` reads the key
     if it is there. The play snapshot goes through the same code, so play
     stop restores saved singletons.
   - The editor's Add Component menu skips singletons and queues.
   - `FlingTests/Engine/ComponentRegistrationTests.cpp`.
   Not done yet:
   - `Register<T>(name)` does not log a deprecation warning yet. Every
     engine type still uses it, so the warning would show on every run. Add
     it once the engine types declare a `ComponentInfo`.
   - The **World Singletons** editor panel.
   - The startup check that each `TypeHash` in a `SystemDesc` matches a
     registered component. `AccessEntry` has no transient flag yet, and a
     transient singleton needs no registration, so the check needs that flag
     first.
6. **Port Sandbox.** `MoverSystem` and `RotatorSystem` replace the loops in
   `Game::Update`. `InputSystem` and `InputState` read from `Input`.
   Headless check: `Sandbox --null-render --unattended --frames 120 --seed 1`
   gives the same final transforms before and after (write them to a log and
   compare).
   Done:
   - `Sandbox.Rotator` and `Sandbox.Mover` (`Sandbox/Gameplay/inc/Systems/`,
     `src/Systems/`, and `src/RegisterSandboxSystems.cpp`) replace the two
     loops in `Game::Update`. They use `SimTime::FixedDelta`. Mover has
     `.After = "Sandbox.Rotator"`, the order the loops had.
   - `Transform`, `Mover`, and `Rotator` declare a `ComponentInfo` with
     their old saved names, so level JSON does not change. `Mover` and
     `Rotator` use `Register<T>()`.
   - The game's `m_DoRotations` and `m_MovePointLights` members are the
     `SandboxToggles` singleton now. `Game::Update` still reads the keys and
     flips it.
   - Check: `Game::Shutdown` logs a checksum of every transform. With the
     replay `Sandbox/Tests/Input/toggle_rotate_and_lights.input.jsonl`, the
     new code gives the same checksum, digit for digit, as the old code with
     each key press one tick later. The tick of delay is expected: the
     fixed systems run before `Game::Update`, so a toggle that
     `Game::Update` sets is seen on the next tick. The CTest "Sandbox:
     systems move the lighting test" runs that replay.
   - The ECS command buffer is `DeferredCommands`, not `CommandBuffer`.
     Graphics already has a `Fling::CommandBuffer`, and two classes with one
     name in one namespace are an ODR violation: the linker used the Vulkan
     destructor for the ECS vector, and the game crashed at exit. Check new
     ECS type names against the other modules.
   Not done yet:
   - `InputSystem` and `InputState`. The input for a fixed tick needs the
     per-tick input of [`improve-input.md`](improve-input.md): today
     `Input::BeginTick` runs once per frame, so a frame with two ticks would
     see a key press twice. Until then, `Game::Update` handles the keys.

After item 6, `ecs-plan.md` continues at Phase B (the `World` facade
removes `g_Registry`) and Phase C (`TypeBuilder` reflection).

## 7. Questions for you

1. **Access in the base class arguments, or a member alias.**
   `System<Self, Access<...>>` gives the typed `Context` for free. A member
   `using Access = ...;` reads a bit cleaner, but then each class must also
   write `using Context = SystemContext<Access>;`. This plan uses the base
   arguments.
2. **The context's name.** The examples call it `ctx`. `world` might read
   better (`world.Query<...>()`), but it is not the real `World`, so it can
   mislead. This plan keeps `ctx`.

Resolved: "a frame rate system cannot write a Simulation component" stays a
compile error. Editor edits go through queued edit commands. Only
`Editor.ApplyEdits`, in a world with role `Edit`, is exempt
([`ecs-features.md`](ecs-features.md) §6.1).
