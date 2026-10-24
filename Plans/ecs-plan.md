# ECS design and EnTT replacement plan

Status: proposal for review. Nothing here is implemented yet.

The ECS rules here come from Timothy Ford's GDC 2017 talk, *'Overwatch' Gameplay Architecture and Netcode* (https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and). Timestamps refer to that talk.

## Context

Fling uses EnTT 3.1.0, but the engine is not built as an ECS. Today:

- There is one `entt::registry g_Registry` in `Engine` (`Engine/inc/Engine.h`). It is passed as `entt::registry&` through `Game`, `World`, `VulkanApp`, `RenderPipeline`, every `Subpass`, and the editor panels.
- There is no list of systems and no update order. Gameplay runs ad hoc in `Game::Update`. The transform matrix calculation and the light-position copy happen inside the renderer (`OffscreenSubpass::Draw`, `GeometrySubpass::UpdateLightingUBO`). The editor changes the registry in the middle of the render pass (`ImGuiSubpass::Draw`).
- Two `on_construct` hooks do the reactive work (MeshRenderer and PointLight). There is no `on_destroy`, so MeshRenderer UBOs probably leak when an entity is destroyed or a level loads.
- Serialization is `ComponentTypeRegistry`: a table of function pointers per type that wraps `T::Serialize(JsonArchive&)`. Editor widgets are hand-written lambdas. Saved levels lose entity IDs and references between entities.
- EnTT reaches far beyond the ECS: `Guid` is an `entt::hashed_string`, and `Input` uses `entt::delegate`. EnTT 3.1 also forces GCC 13+ / Clang 16+ in C++20 mode.

**Goal:** a small, fast ECS made for Fling that removes EnTT completely. It follows Overwatch's rules (Timothy Ford, GDC 2017, consulted through the `gameplay-architecture:ecs-architecture` skill). It is friendly to the editor and to serialization, it is cache-efficient, and it compiles in small pieces. It also gives us a first-class way to declare **systems** and **the order they run in**.

This document covers both parts: the ideal design (Part 1) and the migration that replaces EnTT one step at a time (Part 2). It sits next to [`module-split-plan.md`](module-split-plan.md).

---

# Part 1 — The ideal design

## 1.1 Rules of engagement (Overwatch, enforced in review)

1. **Components have state and no functions. Systems have functions and no state.** A component is a plain struct. Allowed extras: small accessors, plus lifetime hooks declared in its type registration (not on the struct). (Ford 01:00, 03:35)
2. **System state lives in singleton components**, reached through the world and never through a global. Examples: `InputState`, `SimTime`, `ContactQueue`. (11:47) Type names have no `Singleton` prefix; the registration and `world.Singleton<T>()` already say what the type is.
3. **Systems never call systems.** Shared behavior goes in `*Util` free functions. Many call sites → reads few components, no side effects. Many side effects → few call sites. (14:55, 16:32)
4. **Defer big side effects.** Callers push to a queue singleton. One resolver system drains it at one point in the frame. (18:10, 20:35)
5. **Explicit tuples with read/write access.** The scheduler reads them to order systems and, later, to run them in parallel. (43:27)
6. **Proxy ("iceberg") components for specialist subsystems.** The renderer, and later physics and audio, keep their own internals. ECS sees only a handle. (43:27+)
7. **The engine runs one `World`.** Storage, singletons, the scheduler, and the accumulator all live on it. Edit and play are modes of that object. A snapshot restores play back into the same object. Streaming cells and a resim are contents of that object, not more worlds (§1.5). A test may construct a `World` of its own. The running game does not.
8. **Entity creation is immediate. Destruction and structural changes are deferred.** (Ford: deferred creation caused off-by-one-frame bugs.)

## 1.2 Core data model

**Entity.** `struct Entity { uint32_t Index; uint32_t Generation; }`, 8 bytes, trivially copyable. `Entity::Null` is all ones. Debug builds also check the owning world's ID, which catches handles used in the wrong world. The `Generation` check makes a stale handle fail safely.

**Storage: archetypes in chunks (structure of arrays).** This is the main change from EnTT's sparse sets.
- An *archetype* is one unique sorted set of component types. Its entities live in fixed **16 KiB chunks**. Each chunk has one 64-byte-aligned column per component plus an `Entity` column.
- Iterating a query is a tight loop over raw column pointers, chunk by chunk. It has no per-entity indirection, its loads are linear and prefetch-friendly, and the compiler can vectorize it. It is also the fastest form for multi-component tuples, which are what strict-ECS systems are made of.
- **Tags** (empty structs) are part of the archetype signature but take no column space. They replace `entt::tag<"Default"_hs>`.
- **Entity location table:** `Index → {Archetype*, chunk, row}`, which makes `Get<T>(e)` O(1).
- An **archetype graph** caches add/remove edges, so a structural change is a lookup plus a row move (swap-and-pop).
- **Query cache:** each query keeps its list of matching archetypes. The list updates only when a new archetype is created, which is rare.
- **Pointers are not stable across structural changes.** This is the same rule the existing test `owning groups can invalidate assign Transform references` already documents. Hold `Entity` handles, not `T*`. Debug builds keep a structural-change counter and assert when a stale reference is used.
- **Three ways to store a component, chosen per type** (details and the decision table are in §1.10):
  - **Table** (the default): an archetype column.
  - **Enableable**: also a column, plus an enabled bit per row. Toggling a bit doesn't move the entity.
  - **Sparse**: a sparse set beside the archetypes. Add and remove are O(1) and never move the entity.
  - Queries can mix all three. The table part drives the iteration, and sparse parts are probed per row, or drive the iteration when they hold fewer entities.

**Singleton components.** These live in the world, in a separate slot array indexed by component ID: `world.Singleton<T>()`. They are reflected and serialized like any other component (opt-in), so the editor can inspect them and snapshots include them.

**Change detection.** Each column in each chunk has a `ChangeVersion`. Write access bumps it. Queries can filter with `Changed<T>`. The renderer uses this to skip unchanged UBO uploads, the editor uses it for dirty tracking, and networking can use it later for replication.

**Component type IDs.**
- A dense `ComponentId` (uint16) indexes the tables of one `TypeRegistry`. `TypeRegistry::Build` assigns it. It is never saved. Two registries in one process can number the same C++ type differently.
- A **stable `TypeHash`** (64-bit FNV-1a of the registered name) is what goes into saved files, snapshots, and (later) the network. `.FormerNames("OldName")` keeps renamed types loadable.
- A query resolves `T` to a `ComponentId` through the world's `TypeRegistry` when the query is created, and the iterator uses that id. `modules.Component<T>` is what makes `T` legal to resolve. A type that was never registered fails that lookup in debug builds, and the empty-world system test catches it.

## 1.3 Declaring components

**The problem with today's code.** `ComponentTypeRegistry::Get()` is a mutable process-wide singleton that needs `Seal()` and `ResetForTests()`. `RegisterEngineComponents`, `RegisterGraphicsComponents`, and `Game::RegisterComponents` all write into that one object, so a test cannot build a registry of its own.

**A module registers its own types.** Each module exports `void Register(Fling::ModuleRegistration&)`. Startup calls the functions for the modules that target linked. `fling_add_game` generates the call list from the target's link line: a server target does not call `RegisterGraphics`, and `RegisterEditor` runs only in the editor target. The result is a `TypeRegistry` object. The world holds a `const TypeRegistry&`. Tests pass their own `ModuleRegistration` and build their own registry. There is no `Get()`, no `Seal()`, and no `ResetForTests()`.

Static initializers are the wrong tool here. `fling_add_module` builds static libraries, and the linker drops an object file that nothing calls, so a self-registering static would vanish unless every module were linked `--whole-archive`. A shared library later would also get its own copy of an inline catalog head. A normal function call keeps the `.cpp` and stays one list in the executable.

```cpp
// Movement/inc/Mover.h
struct Mover
{
    glm::vec3 TargetPos{};
    float Speed = 1.0f;
};

// Movement/src/Mover.cpp  (the system class lives in this file; see §1.5)
void RegisterMover(Fling::ModuleRegistration& modules)
{
    modules.Component<Mover>("Mover", [](Fling::TypeBuilder<Mover>& type) {
        type.Version(1).Category("Movement");
        type.Field("TargetPos", &Mover::TargetPos);
        type.Field("Speed", &Mover::Speed).Range(0.f, 100.f).Tooltip("Units per second");
    });

    modules.System<MoverSystem>();
}
```

`TypeBuilder<T>` records the saved name, the version, and one entry per field: the field name, the member offset from the pointer-to-member, and the field's type id. `&Mover::Speed` has to be a member of `Mover` or the call does not compile. `TypeRegistry::Build` copies the finished descriptions out of the `ModuleRegistration`.

- Once `Register` has run, the component is available to queries, serialization, the editor's Add Component menu (grouped by `Category`), prefabs, and snapshots.
- **`modules.Component<T>` checks the type at compile time:** nothrow-movable, not a pointer, and (for Table storage) it fits in a chunk.
- **Forgotten fields:** if the reflected field sizes sum to clearly less than `sizeof(T)` minus padding, registration logs a warning. `.Partial()` marks a type that reflects on purpose only some of its fields.
- The module's `Register` function is the table of contents: one line per feature. Adding a feature edits that file and the new `.cpp`. Changing a field list recompiles the `.cpp` that contains the builder, not the engine.

**Feature folders.** Group gameplay by feature, inside the module that owns it:

```
Movement/
    inc/Movement.h             (void Register, and one declaration per feature)
    inc/Mover.h
    src/Movement.cpp           (Register calls RegisterMover and RegisterRotator)
    src/Mover.cpp              (struct reflection, MoverSystem, RegisterMover)
    src/MoveUtil.h / .cpp      (shared helpers, called by several systems)
```

`CONFIGURE_DEPENDS` globs pick up a new `.cpp`. Deleting a feature deletes its files and its line in `Movement::Register`.

- **Field types** are registered once (`float`, `int`, `bool`, `std::string`, `glm::vec2/3/4/quat`, enums, `Entity`, `AssetRef<T>`, `std::vector<T>`, nested reflected structs). Each field type has save/load/draw/compare/copy functions. This replaces `JsonArchive`'s `if constexpr` chain, and users can add their own types.
- **Metadata:** `Range`, `Tooltip`, `Hidden`, `ReadOnly`, `Transient` (not saved), and `Category`.
- **Singletons** set `static constexpr Fling::Kind Kind = Fling::Kind::Singleton` on the struct, and are recorded with `modules.Singleton<T>(name, builder)`. The constant is what `decl.Read<T>()`, `decl.Write<T>()`, and `ctx.Singleton<T>()` check. A `Query` of a singleton does not compile. Entity components leave `Kind` unset and default to Table.
- **Type options on the builder:** `.Transient()` for runtime-only data (proxies, caches, queues): never written to level or prefab files, and read-only in the editor. Also `.Tag()`, `.Storage(Fling::Storage::Sparse)` or `Enableable`, and `.Hooks({OnAdd, OnRemove})`, which only manage lifetime as Overwatch allows. Custom `.Serialize(fn)` is an escape hatch, and existing `Serialize(JsonArchive&)` methods adapt to it during migration.
- **No raw resource pointers in saved components.** `MeshRenderer` splits into authoring data (`AssetRef<Model> Mesh; AssetRef<Material> Material;`) and a transient renderer proxy `GpuMeshProxy { uint32_t Handle; }`.
- **Editor widgets** are attached by the Editor module (`EditorWidgets::Override<T>(fn)`), so Editor stays a leaf. The default inspector is generated from reflection, so `Mover`, `Rotator`, and `NameComponent` stop showing "missing widget".

## 1.4 Serialization

- **Level format:** the current JSON shape stays (`{version, title, entities:[{name, <TypeName>:{...}}]}`) so existing levels load. We add:
  - an `EntityGuid` component (stable 64-bit ID, saved), so `Entity` fields in components are remapped on load. This preserves references, which is the gap today.
  - a `version` per component, plus an optional upgrade function per type. Loading by field name already ignores unknown or missing fields.
- **Archive abstraction:** both JSON (authoring) and binary (cooked levels, snapshots) go through the same reflected field visitor.
- **World snapshot / restore:** chunks are copied with `memcpy` for trivially-copyable columns and with reflected copy for the rest. Pressing Play stores that snapshot. Stop writes it back into the same `World`. A network resim does not snapshot the whole world (§1.5).
- **Prefabs = entity definitions** (Overwatch: an entity is an ID plus an optional entity-definition asset). A prefab is a JSON entity template. `Instantiate(world, prefab)` creates the entity immediately. Instances save only their overrides.

## 1.5 Systems, the scheduler, and the tick loop

The class that orders systems and runs a frame is `Fling::Scheduler`. A world owns one. `Schedule` is the immutable result of the build: phases, stages, systems, and the reason for each edge. Games never subclass the scheduler and never reorder it at runtime.

### How a system registers

A system is a class with `Declare` and `Update`. It has no header. The module's feature `.cpp` defines the class and passes it to `ModuleRegistration::System<T>()`, which is what puts it in the world.

```cpp
// Engine/inc/Ecs/System.h
class System
{
public:
    virtual ~System() = default;

    /** Records the name, the phase, and every access. Called once from ModuleRegistration::System. */
    virtual void Declare(SystemDecl& decl) const = 0;

    /** Runs one tick. Const because a system has no member state. */
    virtual void Update(SystemContext& ctx) const = 0;
};
```

```cpp
// Movement/src/Mover.cpp
class MoverSystem final : public Fling::System
{
public:
    void Declare(Fling::SystemDecl& decl) const override
    {
        decl.Name("Movement.Mover");
        decl.Phase(Fling::Phase::Simulation);
        decl.RunIn(Fling::WorldMode::Play);
        decl.Write<Fling::Transform>();
        decl.Read<Mover>();
        decl.Read<Fling::SimTime>();
    }

    void Update(Fling::SystemContext& ctx) const override
    {
        const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta();

        for (auto [entity, transform, mover] : ctx.Query<Fling::Transform, const Mover>())
        {
            transform.Position = MoveUtil::Toward(transform.Position, mover.TargetPos, mover.Speed * dt);
        }
    }
};

void RegisterMover(Fling::ModuleRegistration& modules)
{
    modules.Component<Mover>("Mover", [](Fling::TypeBuilder<Mover>& type) {
        type.Version(1).Category("Movement");
        type.Field("TargetPos", &Mover::TargetPos);
        type.Field("Speed", &Mover::Speed).Range(0.f, 100.f);
    });

    modules.System<MoverSystem>();
}
```

`System<T>()` is the whole registration. It checks that `T` derives from `System` and that `sizeof(T) == sizeof(System)`, constructs a temporary `T`, and calls `Declare` into a `SystemDesc` stored on the `ModuleRegistration`. The update pointer is `[](SystemContext& ctx) { T{}.Update(ctx); }`. A member variable fails the size check. `Update` being const means the temporary cannot write members either.

`Movement::Register` calls `RegisterMover`. That is the one line the module's table of contents has for this feature. `MoverSystem` stays inside `Mover.cpp`.

```cpp
template<typename T>
void ModuleRegistration::System()
{
    static_assert(std::is_base_of_v<System, T>);
    static_assert(sizeof(T) == sizeof(System),
        "A system cannot have member state. Put it in a singleton component.");

    SystemDesc& desc = AddSystem();
    SystemDecl decl(desc);
    T{}.Declare(decl);
    desc.Update = [](SystemContext& ctx) { T{}.Update(ctx); };
}
```

`SystemDecl` writes that `SystemDesc`. Every `decl.*` call stores a `std::source_location`, so a later schedule error names the file and line.

| Call | What it records |
|---|---|
| `Name`, `Phase` | Required. `Phase` is one of the six fixed phases. |
| `Read<T>` / `Write<T>` | A component access, or a singleton access when `T::Kind` is `Singleton`. |
| `Append<Q>` / `Consume<Q>` | Push into a queue singleton, or drain it. Each appender has its own buffer. The consumer sees those buffers concatenated in schedule order. |
| `Spawns()` | This system may create entities immediately. |
| `After("Name")` / `Before("Name")` | An edge to another system in the same phase. Replaces the inferred edge between those two. |
| `RunIn(WorldMode)` | Bitmask. Default is `Play`. The one world has a mode. `Edit \| Play` runs in both. |
| `ReplayOnResim()` | Off by default. Set it on systems that must run during a replay of `Simulation` and `ResolveDeferred`. |
| `RunIf(fn)` | `bool(const World&)`, checked when the system's turn comes. A false result skips the call and does not move the others. |

`Commands` and `With<>` / `Without<>` need no declaration. Archetype membership changes at flush points.

`SystemContext` is built for one call. It holds `World&` and the `SystemDesc` just declared. `Query<T>` (write), `Query<const T>` (read), `Singleton<T>()`, `Lookup<T>()`, and `Queue<Q>()` assert in debug builds when that access was not declared, and the message names the system and the component. `Commands()` and `Spawn()` are the other two entry points. A CTest runs every registered system once against an empty world, so a mismatched access fails even if nobody runs the game.

A long `Declare` is the warning sign Ford describes.

### What `Scheduler::Build` produces

```cpp
enum class Phase : uint8_t
{
    Input,
    Simulation,
    ResolveDeferred,
    PostSimulation,
    Extract,
    Editor,
    Count
};

struct ScheduledSystem
{
    std::string Name;
    WorldMode RunIn = WorldMode::Play;
    bool ReplayOnResim = false;
    void (*Update)(SystemContext&);
    bool (*RunIf)(const World&);
    std::vector<SystemAccess> Access;
    std::source_location Site;
};

struct Stage
{
    /** Ready at the same time. Version 1 runs them in name order. */
    std::vector<ScheduledSystem> Systems;
};

struct ScheduleEdge
{
    std::string Before;
    std::string After;
    std::string Reason;
};

struct PhaseSchedule
{
    Phase Phase;
    std::vector<Stage> Stages;
    std::vector<ScheduleEdge> Edges;
};

struct ScheduleError
{
    std::string Message;
    std::vector<std::source_location> Sites;
};

class Schedule
{
public:
    const PhaseSchedule& Get(Phase phase) const;

private:
    friend class Scheduler;
    std::array<PhaseSchedule, static_cast<size_t>(Phase::Count)> m_Phases;
};

struct SchedulerConfig
{
    /** The tick rate at startup. The tick clock owns pacing (see improve-time.md). */
    TickRate Rate;
};

class Scheduler
{
public:
    /**
     * Copies every registered system into one schedule.
     * On failure Errors is non-empty and the Scheduler is empty.
     */
    static Scheduler Build(
        const ModuleRegistration& modules,
        SchedulerConfig config,
        std::vector<ScheduleError>& errors);

    /** One frame: loads, an optional resim, the due ticks, then extract and editor. */
    void RunFrame(World& world, TickClock& clock, int64 nowNs);

    const Schedule& GetSchedule() const { return m_Schedule; }

private:
    Schedule m_Schedule;
    int64 m_LastFrameNs = 0;
};
```

`Build` keeps every registered system in one schedule. `RunPhase` skips a system whose `RunIn` misses the world's current mode, so edit and play share the schedule. Edges never cross a phase. Inside one phase:

| Declarations | Edge |
|---|---|
| A `Write<X>`, B `Read<X>` | A runs before B |
| A `Append<Q>`, B `Consume<Q>` | every appender runs before the consumer |
| A `Read<X>`, B `Read<X>` | no edge |
| A `Write<X>`, B `Write<X>` | error when their `RunIn` masks overlap, unless one names the other with `After` or `Before` |
| `After` / `Before` | that edge replaces the inferred one between those two systems |

`SimTime` and `RealFrameTime` are reserved. `RunFrame` writes them. A system that `Write`s either one is a build error. Systems only `Read` them. A system in `Simulation` or `ResolveDeferred` that declares `Read<RealFrameTime>` is also a build error, so the sim cannot depend on frame time (see [`improve-time.md`](improve-time.md)). Two writers of any other component are an error when their `RunIn` masks overlap and neither orders the other. The same component written from two different phases is allowed, because the phase order already sequences those writes.

`Build` then runs Kahn's algorithm per phase:

1. Seed the ready set with every system whose indegree is 0, sorted by name.
2. That ready set is one stage. Record it, then drop those edges.
3. Repeat until the phase is empty.
4. If systems remain and the ready set is empty, the remainder is a cycle. `Build` records the cycle path and each declaration's `file:line`.

An unknown `After` / `Before` name, or one that names a system in another phase, is also an error. `fling_validate_schedule(<target>)` runs `<target> --validate-schedule` as a POST_BUILD step. That command calls `Build` once and exits non-zero when `errors` is non-empty, so `cmake --build` fails. Startup does the same check. The checks that happen at compile time are the size check, component versus singleton, and a non-component passed to `Read` or `Write`.

The stages are the parallel groups. Systems in one stage have no edge between them, so their writes are disjoint. Version 1 runs a stage one system at a time, in the name order already recorded. A job system can later run the stage together without a system changing.

The editor schedule panel and `--dump-schedule` read `Schedule`: phases, stages, access sets, and each `ScheduleEdge::Reason` (`"writes Transform, read by Movement.Mover"`). A test builds a scheduler from the modules it cares about and calls `RunFrame`. It selects one system by the name in `Declare`, through a `ModuleRegistration` that only that test filled in.

### The frame

The engine owns one `World`. Edit and play are a mode on it. Pressing Play stores a snapshot and sets the mode to `Play`. Stop writes that snapshot back and returns the mode to `Edit`. Streamed content is a cell in this world. A resim is a pass over `Simulation` and `ResolveDeferred` on the prediction set, in this same object.

`RunFrame` is the whole tick. Phases stay the schedule from `Declare`. Games do not add phases.

```cpp
void Scheduler::RunFrame(World& world, TickClock& clock, int64 nowNs)
{
    SimTime& time = world.Singleton<SimTime>();
    RealFrameTime& frameTime = world.Singleton<RealFrameTime>();
    frameTime.RealDelta = float(nowNs - m_LastFrameNs) / 1e9f;
    ++frameTime.Frame;
    m_LastFrameNs = nowNs;

    world.ApplyCompletedLoads();

    if (ResimRequest request = world.TakeResim())
    {
        RestorePredicted(world, request);
        for (const TickCommands& commands : request.Replay)
        {
            world.BindReplayInput(commands);
            RunPhase(world, Phase::Simulation, SimPass::Resim);
            RunPhase(world, Phase::ResolveDeferred, SimPass::Resim);
        }
    }

    // Pause, time scale, step requests, and the hitch clamp live in the clock.
    const uint32 ticks = (world.Mode() == WorldMode::Play) ? clock.TicksDue(time.Tick) : 0;
    for (uint32 i = 0; i < ticks; ++i)
    {
        // Builds this tick's TickInput from timestamped input events (see improve-time.md).
        world.BindTickInput(time.Tick, clock.Presentation());
        RunPhase(world, Phase::Input, SimPass::Live);
        RunPhase(world, Phase::Simulation, SimPass::Live);
        RunPhase(world, Phase::ResolveDeferred, SimPass::Live);
        ++time.Tick;
    }

    RunPhase(world, Phase::PostSimulation, SimPass::Live);
    if (ticks > 0)
    {
        // Writes a RenderSnapshot of the last live tick to the SnapshotRing.
        RunPhase(world, Phase::Extract, SimPass::Live);
    }
    RunPhase(world, Phase::Editor, SimPass::Live);
}

void Scheduler::RunPhase(World& world, Phase phase, SimPass pass)
{
    for (const Stage& stage : m_Schedule.Get(phase).Stages)
    {
        for (const ScheduledSystem& system : stage.Systems)
        {
            if ((system.RunIn & world.Mode()) == 0)
            {
                continue;
            }
            if (pass == SimPass::Resim && !system.ReplayOnResim)
            {
                continue;
            }
            if (system.RunIf != nullptr && !system.RunIf(world))
            {
                continue;
            }

            SystemContext ctx(world, system, pass);
            system.Update(ctx);
        }
    }

    world.FlushCommands();
}
```

`ReplayOnResim` defaults off. A system that must replay opts in with `decl.ReplayOnResim()`. `Input`, `PostSimulation`, `Extract`, and `Editor` are only called as `Live`. `Input` runs once per live tick. The other three run once per frame, after any replay and after the due ticks. When the game is paused, the tick clock returns 0 ticks: nothing new is extracted, and the renderer keeps drawing the latest snapshots. `ApplyCompletedLoads` spends a budget of main-thread time turning finished streaming descriptions into `Commands` on this world. Workers never create entities.

A query captures the chunk count and the last chunk's row count when it starts, so an entity spawned during the query is invisible to that query and visible to every later system.

| Phase | When | What moves here |
|---|---|---|
| `Input` | Once per live tick, before that tick's `Simulation`. Not on a resim | `InputSystem` reads the tick's `TickInput` (built from timestamped input events) and writes `InputState`. Game preprocessors (dead zones, action mapping) read it here. |
| `Simulation` | Each live tick, and each replayed `TickCommands` | Gameplay, movement, AI. On a resim pass only systems that opted in run, and they query the prediction set. |
| `ResolveDeferred` | After each of those ticks | Queue consumers, then that phase's `Commands` flush. Damage, audio, and VFX consumers leave `ReplayOnResim` off. |
| `PostSimulation` | Once per frame, after resim and the due ticks | `TransformSystem` (hierarchy to `LocalToWorld`). Interpolation is not here: the renderer does it from two snapshots (see `improve-time.md`). |
| `Extract` | Once per frame, only when at least one tick ran | Writes a `RenderSnapshot` of the last live tick to the `SnapshotRing`. Proxy and UBO extraction move out of the subpasses. The renderer reads only the ring. |
| `Editor` | Once per frame, editor builds only | Queued editor edits, at a sync point before `VulkanApp::Update`. |

### `Engine::Tick`

Registration happens once, before the loop. Today's `Input::Poll()`, `World::Update`, `Game::Update`, and `Timing` go away. `RunFrame` owns the load budget, the resim, and the due ticks. The input producer writes timestamped events that `RunFrame` reads per tick. The renderer runs after it and reads only the `SnapshotRing`.

```cpp
void Engine::Startup(int argc, const char* argv[])
{
    ModuleRegistration modules;
    RegisterLinkedModules(modules);

    m_Types = TypeRegistry::Build(modules);

    std::vector<ScheduleError> errors;
    Scheduler scheduler = Scheduler::Build(modules, {}, errors);
    if (!errors.empty())
    {
        // Log each error with its file:line, then abort startup.
    }

    m_World = new World(m_Types, std::move(scheduler));
    m_World->SetMode(WorldMode::Edit);
    m_World->Init();
}

void Engine::Tick()
{
    // Window open, ticks left (--frames N), or not quit. Works without a window.
    while (m_Runner->ShouldContinue(*m_World))
    {
        // One thread today: poll the OS here. Later the input thread does this.
        m_InputProducer->PollOnCallerThread();

        m_World->RunFrame(*m_TickClock, PlatformTime::NowNanoseconds());

        if (m_World->ShouldQuit())
        {
            break;
        }

        if (m_Renderer)   // null under --null-render
        {
            m_Renderer->Render(m_Snapshots, m_InputEvents);
        }
    }
}
```

`RegisterLinkedModules` is generated per target from the modules that target links. `InputSystem` is the writer of `InputState`. `SystemContext::RequestQuit()` sets the flag `Tick` reads after the frame.

### Resimulation

Resim is those two phases in the middle of `RunFrame`, not a later redesign and not a second world. The default network model is server-authoritative prediction (Ford, GDC 2017, netcode half): the client runs ahead, and on a mismatch restores predicted state and replays inputs. A peer-to-peer game (GGPO, NetherRealm) uses the same pass and puts every gameplay entity in the set.

`BindReplayInput` points the replayed tick at that tick's recorded `TickCommands`. The `Input` phase does not run again, so no input events are read for a replayed tick. `PostSimulation` and `Extract` run once, after the replay and the new live step, so the renderer sees the corrected state. Consumers that play audio, spawn VFX, or apply damage do not set `ReplayOnResim`. Predicted gameplay reads scenery in this world. It does not rewrite it.

The prediction set is the entities that carry predicted state, not a distance test:

- The local mover, and the projectiles and abilities it predicted, carry a `MovementState` ring and are in the set. Streamed scenery, remote entities, and health are not. Remotes stay on interpolation. Damage stays server-authoritative. (Ford: predict what the player does, not what costs someone else. NetherRealm, GDC 2018: opt objects out of the simulated frame when they are not gameplay.)
- The rings are components on those entities, indexed by command frame. There is no whole-world save per frame.
- When the server snapshot for frame N matches the recorded state, `TakeResim` is empty and the replay loop does not run. When it does not match, restore that entity and replay from N+1 through now. The compare is equality of the quantized state. A 1 mm miss is still a replay. Ford's hit bugs were position-agreement bugs. Smoothing only hides what the player sees after the replay, and simulation must not read it (NetherRealm: drive visuals, don't depend on them).

A system that sets `ReplayOnResim` and queries every `Transform` walks the streamed world to touch a handful of movers. Its tuple includes the prediction component, so a chunk with none is never visited. `Changed<>` does not define the set.

A fighting game that must be bit-exact puts every gameplay entity in the set and still leaves scenery and VFX out. Both peers agree on membership ahead of the rollback window (NetherRealm's promotable resim). The engine records that membership in the component.

## 1.6 Worlds, editor, and play-in-editor

- **`Fling::World`** is the one object the engine ticks. It owns storage, singletons, the `Scheduler`, the mode (`Edit` or `Play`), and the pause flag. It takes a `const TypeRegistry&` built from the `ModuleRegistration` (§1.3, §1.5). `g_Registry` goes away. Streamed levels are cells in this world. A resim is two phases inside `RunFrame` (§1.5).
- **Play in the same world:** pressing Play snapshots the world and sets the mode to `Play`. Stop writes the snapshot back and returns the mode to `Edit`. This replaces `OnStopGame`'s "destroy everything with Rotator/Mover" hack.
- **Editor edits are commands:** a widget edits a copy of the value, and if it changed, the editor pushes `EditCommand{entity, type, before, after}` (reflected bytes). They apply in the `Editor` phase. This gives **undo/redo** for free and removes the mid-render mutation. Selection becomes editor singleton state, and `EditableComponent` is deleted.
- **Hierarchy:** `Parent` plus `Children` (or first-child/next-sibling) components, `LocalTransform` / `LocalToWorld`, and `TransformSystem` computing them in order. The outliner shows a tree.

## 1.7 Renderer integration (proxy components)

- Subpasses take a **read-only** `const World&` (or the extracted render data) and never mutate it.
- `MeshRenderer` (authoring) gets a `GpuMeshProxy` (transient) from an `Extract` system that runs on `Query<Read<MeshRenderer>, Without<GpuMeshProxy>>`. This is ordinary ECS, not a construct signal. Its `OnRemove` hook releases the UBO and descriptor set, which **fixes the leak**.
- `PointLight` needs a `Transform` because of `.Requires<Transform>()` in its registration. The editor and the loader add the required component. Light position is extracted into the lighting UBO by a system. `PointLight::Pos` leaves the component.
- The debug gizmo mesh for lights comes from an editor-only extraction system, not from adding a `MeshRenderer` inside a hook.

## 1.8 Compile-time isolation (the anti-Unreal rules)

- **No umbrella header.** Each component is in its own header and includes only its field types. Component headers never include `World.h`.
- **Systems have no headers.** A change to a system's `Update` recompiles that one `.cpp`. Adding or removing a system also edits one line in the module's `Register` function, which recompiles `Movement.cpp` and nothing else.
- **Reflection lives in `.cpp` files.** Adding or changing metadata recompiles the `.cpp` that contains the `TypeBuilder` lambda. Component headers include only their field types.
- **Thin templates over a type-erased core.**
  - `Query<...>` expands to a small array of `ComponentId`s plus an inlined `ForEach` loop.
  - Archetype matching, chunk management, and the scheduler are non-template code in `.cpp` files.
  - There is no EnTT-style variadic metaprogramming tower.
- **Header budget:** `Entity.h`, `Query.h`, and `World.h` include only `<cstdint>`, `<cstddef>`, `<span>`, `<type_traits>`, and `<utility>`. CI keeps an include-what-you-use style check (a grep-based script like `check_comment_style.py`).
- **Measure it:** a `-ftime-trace` check on the ECS headers, plus a documented edit→rebuild test: touch `MoverSystem.cpp` → 1 TU; touch `Mover.h` → only the files that include it.

## 1.9 What "complete" means (feature checklist)

Entities with generations · archetype/chunk storage plus Enableable and Sparse kinds · batched structural flush · per-module `Register` functions and a `TypeRegistry` / `Scheduler` built from them · ECS telemetry and budgets · tags · singletons · queries with Read/Write/Optional/With/Without/Changed · Commands (deferred) and Spawner (immediate) · lifetime hooks (OnAdd/OnRemove only) · `Requires<>` dependencies · reflection with metadata · JSON and binary archives · EntityGuid remapping · component versioning and upgraders · prefabs · hierarchy and TransformSystem · phases, fixed step, ordering, ambiguity detection, stages · run conditions (edit/play) · deferment queue singletons · world clone/snapshot · editor inspector auto-generated from reflection, undo/redo, schedule panel · per-system profiling · Catch2 tests and benchmarks.

**Deliberately out of scope for v1:** running stages in parallel (the metadata is ready for it), networking replication, and C++26 static reflection. We can switch to C++26 reflection later behind the same builder API.

## 1.10 How it scales: many systems adding and removing components

Archetype storage is fastest to iterate, but it has a known weakness: when many systems add and remove components every frame, entities keep moving between archetypes and the archetype count explodes. This section defines the rules and the machinery that keep that cost under control as the game grows.

**Where does this data go? (decision table for gameplay programmers)**

| The data is... | Put it in | Why |
|---|---|---|
| What the entity *is*, set at spawn or by a prefab, rarely changed (`Transform`, `Health`, `MeshRenderer`) | **Table** component (default) | Fastest iteration; structural changes are rare |
| A state flag that flips often (`Grounded`, `Stunned`, `Selected`, `Visible`) | **Enableable** component | Flipping a bit costs nothing and doesn't move the entity. Queries skip disabled rows (`With<Stunned>` checks the bit) |
| Present on few entities and added/removed often (`StatusEffectStack`, `AiTarget`, `Burning`) | **Sparse** component (`.Storage(Sparse)`) | O(1) add/remove that doesn't move the entity and doesn't create archetypes |
| "X happened this frame" (damage, hit, spawn request, sound cue) | **Not a component.** A queue in a singleton (`ModifyHealthQueue`, `ContactQueue`) | Ford's deferment rule. Components used as events are the main cause of archetype churn |
| State owned by one system that others read (`InputState`, `CameraState`) | **Singleton** component | Rule 2 |
| A specialist subsystem's internals (GPU buffers, physics body, audio voice) | **Proxy** component holding a handle | Rule 6, the "iceberg" |

The storage kind is `type.Storage(...)` on the builder. Changing it later does not affect any system code, because queries look the same for all three kinds.

**Batched structural changes.** `Commands.Add/Remove` don't move anything right away. At a sync point, the flush does this:
1. It groups pending changes by entity and works out each entity's **final** archetype once. If five systems add five components to one entity in one frame, the entity moves **once**, not five times.
2. It sorts the moves by (source archetype, destination archetype) and copies rows in bulk, which is cache-friendly.
3. It applies the changes in schedule order, then in the order they were emitted. The result is deterministic, which rollback needs. `Add` on a component that already exists replaces it, and the later system in the schedule wins. Debug builds warn when two systems add the same component to the same entity in one flush with *different* values.

**Archetype growth budget.**
- Archetype lookup uses a hash of the component bitset, and add/remove edges are cached.
- A new archetype is tested against every cached query once, with a bitset-subset test. This stays cheap even with thousands of archetypes.
- Chunks start small (1 KiB) and double up to 16 KiB, so rare archetypes with only a few entities don't waste memory.
- Empty archetypes are freed lazily. Their ID and their place in the graph stay.

**Telemetry, so problems show up early** (Ford's "perf budget" idea, applied to structure). The ECS stats panel and `--ecs-stats` show:
- archetype count,
- chunk occupancy,
- structural moves per frame, **attributed to the system that issued them**,
- and the components behind the most moves, with a suggestion to make them Enableable or Sparse.

CI runs the Sandbox for a fixed number of frames headless and fails if the archetype count or the moves per frame go over a threshold set in the repo.

**Ownership and review rules that scale.**
- Each component has one *owning feature*: the `.cpp` where its `TypeBuilder` lives.
- Other features may read it, and may write it only through that feature's `*Util` functions or queues. This is Ford's "every mutation of `PlayerCommand` in one file" (14:06).
- The schedule panel and `--dump-schedule` list every writer of each component. A component with many writers is flagged in review, the same way a long tuple is.

**What adding the 200th component costs:** one header, one builder lambda, and one line in that module's `Register`. That feature's translation units rebuild. The engine does not. A type that is not nothrow-movable fails at the `modules.Component<T>` call.

## 1.11 Where it lives

`Scheduler`, `Schedule`, `World`, `System`, `SystemDecl`, `SystemContext`, `ModuleRegistration`, and `TypeBuilder` live in the Engine module, under `FlingEngine/Engine/inc/Ecs/` and `src/Ecs/` (flat includes, `#include "Ecs/Scheduler.h"`). The world layer already lives there (locked decision 8), and `Scheduler::RunFrame` takes a `World&`. Putting it in Core would make Core include the world.

Graphics, Physics, Editor, and gameplay modules already depend on Engine, so their systems include `Ecs/System.h` from there. Core stays free of ECS types. We don't add a new module. `Transform`, `Name`, `EntityGuid`, hierarchy, and `TransformSystem` stay in the Engine module beside the world. Game-specific components stay in the game module that registers them.

---

# Part 2 — Migration plan: replacing EnTT step by step

**Strategy: strangler facade.** First, move every caller onto the *new Fling API* while EnTT still backs it. Then swap the backend underneath. Every phase is one or more PRs off `main` that keep Sandbox and FlingTests green on all CI platforms. Coordinate with `module-split-plan.md`, so that ECS code lands in the module targets that plan defines.

### Phase A — Remove the EnTT usage that is not ECS (small, low risk)
- Add `Fling::HashedString` (constexpr FNV-1a, same algorithm as EnTT 3.1) in Core. Make `Guid`/`Guid_Handle`/`HS()`/`_hs` use it (`Core/inc/FlingTypes.h`). Callers in Resources and Graphics don't change.
- Replace `entt::delegate` in `Core/inc/Input/Input.h/.inl` with a small `Fling::Delegate` (to be replaced in Phase D by `InputState`).
- Remove `<entt/...>` from `FlingTypes.h`. Delete the dead cereal include `Core/inc/Serilization.h` and the `FlingExt::cereal` link.
- After this phase, EnTT appears only in ECS code.

### Phase B — `Fling::World` facade, still backed by EnTT
- Add `Ecs/Entity.h`, `Ecs/World.h`, `Ecs/Query.h`, and `Ecs/Commands.h` with the final API (§1.2, §1.5 query types). The implementation wraps `entt::registry` in `Ecs/src/EnttBackend.cpp` and keeps EnTT out of the public headers.
- `World` **owns** its storage. `Engine::g_Registry` is removed, and `entt::registry&` parameters become `World&` / `const World&` in `Game`, `VulkanApp`, `RenderPipeline`, `Subpass`, the editor panels, Sandbox, and the tests. This is mechanical and can go in several PRs, one per area.
- Replace the `entt::tag<"Default"_hs>` / `"Debug"_hs` tags with tag structs (`DeferredRenderTag`).

### Phase C — `ModuleRegistration`, `TypeRegistry`, and reflection (replaces `ComponentTypeRegistry`)
- Add `ModuleRegistration`, `TypeBuilder<T>`, and `TypeRegistry::Build` (§1.3). `fling_add_game` generates `RegisterLinkedModules` from the target's link line. Port `Transform`, `NameComponent`, the lights, `MeshRenderer`, `Mover`, and `Rotator` onto `modules.Component` / `modules.Singleton`. During the move, `.Serialize(fn)` adapts existing `Serialize(JsonArchive&)` methods.
- Make `World::LoadLevelFile` and `OutputLevelFile` use reflection, and add `EntityGuid`. Existing `Assets/Levels/*.json` must load without changes.
- The editor inspector is generated from reflection. The hand-written widgets in `BaseEditor.cpp` become `EditorWidgets::Override<T>`.
- Delete `ComponentTypeRegistry`, `IComponentRegistrar`, `Game::RegisterComponents`, `RegisterGameplayComponents`, `RegisterGraphicsComponents`, and `ResetForTests`, because the TypeRegistry is an object that tests create themselves.
- Move Sandbox's code into feature folders (`Sandbox/Gameplay/Features/Movement/`, ...) as each feature is ported.

### Phase D — `Scheduler`, systems, and the tick loop
- Add `Fling::System`, `SystemDecl`, `SystemContext`, `Schedule`, and `Scheduler` (§1.5). `modules.System<T>()` records each system. `Scheduler::Build` orders them. `--validate-schedule` plus `fling_validate_schedule()` (POST_BUILD) fails the build on a cycle, a write/write conflict, or an unknown `After` name. Add the batched `Commands` flush (§1.10).
- `Engine::Tick` calls `world.RunFrame(tickClock, now)` (§1.5). That frame applies completed loads, replays a resim when one is pending, then runs the due ticks, each with its own `TickInput`. The tick clock and the time types come from [`improve-time.md`](improve-time.md). It stops calling `Game::Update`. Move Sandbox's Rotator and Mover logic into registered systems. `Game` keeps start/stop and quit.
- Add `SimTime`, `RealFrameTime`, and `InputState` (from the `Input` key maps), plus `SpawnQueue` and `ResolveSpawnSystem` for `OnTestSpawn`.
- Add `TransformSystem` (computes the world matrix out of the renderer). `Transform` loses its methods, which move to `TransformUtil`.

### Phase E — Renderer to proxy components
- Subpasses become read-only. `Extract` systems handle mesh proxy creation and write the `RenderSnapshot` (transforms filtered by `Changed<Transform>`, light data). The renderer updates its UBOs from the `SnapshotRing`, not from the world.
- Remove both `on_construct` connections. `OnRemove` hooks release the GPU resources (fixes the leak). The editor moves to queued edits in the `Editor` phase.
- Add a test that destroys an entity with a `MeshRenderer` and checks that the proxy pool released its slot.

### Phase F — Native backend (can be built in parallel from Phase B on)
- Implement the archetypes, chunks (growing 1→16 KiB), Enableable and Sparse storage, the query cache, change versions, snapshot, and ECS telemetry (§1.2, §1.4, §1.10) in `Core/src/Ecs/`. The EnTT backend maps Enableable to add/remove and Sparse to its own storage, so both backends behave the same.
- Add an archetype/move budget check: a headless run of a fixed number of Sandbox frames.
- Add a CMake option `FLING_ECS_BACKEND=entt|native` for the transition. CI runs FlingTests on both backends.
- Add Catch2 `BENCHMARK` cases (tag `[!benchmark]`, not in the default CI run) for iterating 1M entities with 1, 2, and 4 components, create/destroy, and add/remove. They must be at least as fast as EnTT views for the multi-component queries.
- Switch the default to `native` once the tests pass and the benchmarks hold.

### Phase G — Delete EnTT
- Remove `EnttBackend.cpp`, the backend option, `fling_add_external(entt ...)` (`CMake/FlingExternals.cmake:29`), the link at `FlingEngine/CMakeLists.txt`, the EnTT `NOMINMAX` workaround (`FlingTargets.cmake`), and the `external/entt` submodule.
- Revisit the GCC 13 / Clang 16 floor that EnTT forced (root `CMakeLists.txt:42-52`).
- Update `module-split-plan.md` (its module table lists entt), `architecture.md`, and the `ecs-architecture` skill's notes about Fling.

### Phase H — Features built on the new core (after EnTT is gone)
Play-in-editor through world clone · undo/redo · hierarchy and outliner tree · prefabs · schedule panel and profiling · the parallel stage executor on a job system · binary cooked levels · prediction-set restore and input replay for rollback (consult the `rollback-netcode-expert` agent). Full-world snapshot stays the edit-to-play and replay-playback path.

---

## Critical files

- `FlingEngine/Engine/inc/Engine.h`, `Engine/src/Engine.cpp`: registry ownership and the tick loop
- `FlingEngine/Core/inc/FlingTypes.h`, `Core/inc/Input/Input.h/.inl`: Phase A
- `FlingEngine/Gameplay/inc/World.h`, `src/World.cpp`, `inc/Game.h`, `inc/ComponentTypeRegistry.h`, `src/ComponentTypeRegistry.cpp`
- `FlingEngine/Resources/inc/JsonArchive.h`: field serialization folds into the reflection field types
- `FlingEngine/Graphics/src/OffscreenSubpass.cpp`, `GeometrySubpass.cpp`, `VulkanApp.cpp`, `RenderPipeline.cpp`, `inc/Subpass.h`, `inc/MeshRenderer.h`, `src/RegisterGraphicsComponents.cpp`
- `FlingEngine/Editor/src/BaseEditor.cpp`, `EntityEditorPanel.cpp`, `WorldOutlinePanel.cpp`
- `Sandbox/Gameplay/src/SandboxGame.cpp`, `inc/Mover.h`
- `FlingTests/Engine/JsonSerializationTests.cpp`, `FlingTests/Resources/JsonSerializationTests.cpp`, plus new `FlingTests/Engine/Ecs*Tests.cpp`
- `CMake/FlingExternals.cmake`, `FlingEngine/CMakeLists.txt`, `CMake/FlingTargets.cmake`

## Verification (each phase)

- `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && ctest --test-dir build`, plus a `-DDEFINE_SHIPPING=ON` build and `python3 scripts/check_comment_style.py`.
- Run Sandbox: the deferred demo renders, Play/Stop works, a level saves and reloads with the same contents, and `Assets/Levels/Level_1.json` loads unchanged.
- New ECS unit tests:
  - entity generations and stale handles
  - add/remove moves between archetypes
  - query matching with With/Without/Optional/Changed
  - Commands flush at sync points
  - immediate spawn during iteration (not seen this tick, seen by later systems)
  - a snapshot taken before play restores into the same world on stop
  - a resim pass runs only `ReplayOnResim` systems, and only over the prediction set
  - schedule ordering, cycle detection, and ambiguity warnings
  - fixed-step accumulator
  - reflection round-trip (JSON and binary)
  - EntityGuid reference remapping
  - world clone equals its source
  - five systems each adding a component to one entity cause exactly one archetype move per flush
  - Enableable toggles and Sparse add/remove don't create archetypes
  - a TypeRegistry built from a subset of modules rejects types outside that subset
  - two TypeRegistries built in one test stay separate, and the engine tick still owns one world
- Ergonomics check: add a brand-new component and system by adding one feature `.cpp` and one line in that module's `Register`. It shows up in the editor's Add menu, saves and loads, and runs in Play.
- Phase F: both backends pass the full suite. The benchmarks are recorded in the PR.
- Compile-isolation check: touch `MoverSystem.cpp` → 1 TU rebuilt; touch `Mover.h` → only the files that include it (Ninja `-d explain` / build log).
- Phase G: `grep -r entt FlingEngine Sandbox FlingTests CMake` returns nothing, and CI is green on Linux GCC/Clang and Windows MSVC/MinGW.
