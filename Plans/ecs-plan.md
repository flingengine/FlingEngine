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
2. **System state lives in singleton components**, reached through the world and never through a global. Examples: `SingletonInput`, `SingletonTime`, `SingletonContactQueue`. (11:47)
3. **Systems never call systems.** Shared behavior goes in `*Util` free functions. Many call sites → reads few components, no side effects. Many side effects → few call sites. (14:55, 16:32)
4. **Defer big side effects.** Callers push to a queue singleton. One resolver system drains it at one point in the frame. (18:10, 20:35)
5. **Explicit tuples with read/write access.** The scheduler reads them to order systems and, later, to run them in parallel. (43:27)
6. **Proxy ("iceberg") components for specialist subsystems.** The renderer, and later physics and audio, keep their own internals. ECS sees only a handle. (43:27+)
7. **Worlds can be created more than once.** No global world, registry, or system accessors. Edit world, play world, preview, replay, rollback resim, and tests all just work. (10:58)
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
- A dense `ComponentId` (uint16) indexes every runtime table. Static init assigns it from the type catalog (§1.3). It is fixed for the life of the process and never saved.
- A **stable `TypeHash`** (64-bit FNV-1a of the registered name) is what goes into saved files, snapshots, and (later) the network. `.FormerNames("OldName")` keeps renamed types loadable.
- `ComponentIdOf<T>()` is one `inline` variable read. It needs no RTTI. If `T` is not declared as a component, the build fails with a `static_assert`, so a missing registration can't become a runtime crash.

## 1.3 Declaring components: no central lists, no mutable global registry

**The problem with the first draft and with today's code.** `ComponentTypeRegistry::Get()` is a mutable process-wide singleton that needs `Seal()` and `ResetForTests()`. Hand-maintained `RegisterXComponents()` / `RegisterXSystems()` lists grow into one file per module that every feature edits:
- every new component or system means touching it, which causes merge conflicts,
- forgetting it causes a silent runtime bug ("why doesn't my component show up or save?"),
- and the file includes every component header, so it recompiles when any of them changes.

That doesn't scale to hundreds of components (Overwatch's client had about 103 component types and about 46 systems).

**The fix has two parts:**

1. **A type catalog, assembled at link time.** It is immutable, like a vtable. Each component declares itself next to its own code. The declaration adds a constant descriptor node to an intrusive linked list during static init. That step only links a pointer: it allocates nothing, has no init-order dependencies, and does no work. The catalog is never mutated after `main()` starts, so it is not "a registry" in the bad sense.
2. **`TypeRegistry` as an ordinary object.** `Engine` builds it from the catalog. Tests build their own, from the full catalog or a chosen subset. Worlds get a `const TypeRegistry&`. There is no `Get()`, no `Seal()`, and no `ResetForTests()`. Plugins and scripting can add runtime types to *their* registry object later.

**Adding a component means touching only its own files:**

```cpp
// Sandbox/Gameplay/Features/Movement/MoverComponents.h
#include "Ecs/ComponentDecl.h"   // tiny: traits and the macro, no World.h
struct Mover { glm::vec3 TargetPos{}; float Speed = 1.0f; };
FLING_COMPONENT(Mover);          // marks it as a component; queries static_assert on this

// Sandbox/Gameplay/Features/Movement/MoverComponents.cpp
FLING_REFLECT(Mover, "Mover")    // self-registers into the catalog
{
    r.Version(1).Category("Movement");
    r.Field("TargetPos", &Mover::TargetPos);
    r.Field("Speed", &Mover::Speed).Range(0.f, 100.f).Tooltip("Units per second");
}
```

- After that, the component is available to queries, serialization, the editor's "Add Component" menu (grouped by `Category`), prefabs, and snapshots. You edit no other file, and nothing else recompiles.
- **Header-only components** are allowed: you may put `FLING_REFLECT` in any .cpp of the feature, for example the system file that owns the component.
- **Debug check for forgotten fields:** if the reflected field sizes sum to clearly less than `sizeof(T)` minus padding, registration logs a warning. Mark intentionally partial reflection with `.Partial()`.
- **Compile-time checks in `FLING_COMPONENT`:** the type is nothrow-movable, it is not a pointer, and (for Table storage) it fits in a chunk.
- **Linker safety:** static libraries drop object files that nothing references, which would silently drop self-registration (this is why today's code avoids static registration). Fling fixes it once, in CMake:
  - Content modules (Gameplay, Graphics, Sandbox's runtime library, and Editor) link with `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>` through `fling_add_module(... CONTENT)`.
  - This needs CMake 3.24, up from 3.21.
  - A CTest case checks that every catalog type a test expects is present, which catches a dropped TU.
  - This replaces the `IComponentRegistrar` / `Game::RegisterComponents` hooks.

**Feature folders.** Today, gameplay code is split by kind (components here, systems there). Instead, group it by feature:

```
Sandbox/Gameplay/Features/Movement/
    MoverComponents.h / .cpp   (structs + FLING_REFLECT)
    MoverSystem.cpp            (class MoverSystem : Fling::System, no header)
    MoveUtil.h / .cpp          (shared helpers, called by several systems)
```

To add a feature, add a folder. `CONFIGURE_DEPENDS` globs pick it up. To delete a feature, delete the folder.

- **Field types** are registered once (`float`, `int`, `bool`, `std::string`, `glm::vec2/3/4/quat`, enums, `Entity`, `AssetRef<T>`, `std::vector<T>`, nested reflected structs). Each field type has save/load/draw/compare/copy functions. This replaces `JsonArchive`'s `if constexpr` chain, and users can add their own types.
- **Metadata:** `Range`, `Tooltip`, `Hidden`, `ReadOnly`, `Transient` (not saved), and `Category`.
- **Singletons are declared in the header**, with `FLING_SINGLETON(T)` instead of `FLING_COMPONENT(T)`. System files can't see the reflection .cpp, so whether a type is a singleton has to be known at compile time. Then `T&` / `const T&` system parameters are checked with `static_assert`, and so is putting a singleton in a `Query` or on an entity.
- **Type options (runtime, in `FLING_REFLECT`):** `.Transient()` for runtime-only data (proxies, caches, queues): never written to level or prefab files, and read-only in the editor. Also `.Tag()`, and `.Hooks({OnAdd, OnRemove})`, which only manage lifetime as Overwatch allows. Custom `.Serialize(fn)` is an escape hatch, and existing `Serialize(JsonArchive&)` methods adapt to it during migration.
- **No raw resource pointers in saved components.** `MeshRenderer` splits into authoring data (`AssetRef<Model> Mesh; AssetRef<Material> Material;`) and a transient renderer proxy `GpuMeshProxy { uint32_t Handle; }`.
- **Editor widgets** are attached by the Editor module (`EditorWidgets::Override<T>(fn)`), so Editor stays a leaf. The default inspector is generated from reflection, so `Mover`, `Rotator`, and `NameComponent` stop showing "missing widget".

## 1.4 Serialization

- **Level format:** the current JSON shape stays (`{version, title, entities:[{name, <TypeName>:{...}}]}`) so existing levels load. We add:
  - an `EntityGuid` component (stable 64-bit ID, saved), so `Entity` fields in components are remapped on load. This preserves references, which is the gap today.
  - a `version` per component, plus an optional upgrade function per type. Loading by field name already ignores unknown or missing fields.
- **Archive abstraction:** both JSON (authoring) and binary (cooked levels, snapshots) go through the same reflected field visitor.
- **World snapshot / restore:** chunks are copied with `memcpy` for trivially-copyable columns and with reflected copy for the rest. This is used to clone the edit world into a play world, for tests, and later for rollback (`rollback-netcode` plugin).
- **Prefabs = entity definitions** (Overwatch: an entity is an ID plus an optional entity-definition asset). A prefab is a JSON entity template. `Instantiate(world, prefab)` creates the entity immediately. Instances save only their overrides.

## 1.5 Systems and scheduling

**A system is a class that overrides `Fling::System`.** This follows Overwatch's shape: a `System` base class with an update function, and the system loops over component tuples. There are no system macros, and autocomplete works everywhere.

```cpp
// Core/inc/Ecs/System.h (engine)
class System
{
public:
    virtual ~System() = default;

    /** Called once when the schedule is built. Declares the name, the phase, and every access. */
    virtual void Declare(SystemDecl& decl) const = 0;

    /** Runs one tick. It is const because systems have no state. */
    virtual void Update(SystemContext& ctx) const = 0;
};

// Sandbox/Gameplay/Features/Movement/MoverSystem.cpp   (no header; nobody includes it)
class MoverSystem final : public Fling::System
{
public:
    void Declare(Fling::SystemDecl& decl) const override
    {
        decl.Name("Movement.Mover");
        decl.Phase(Fling::Phase::Simulation);
        decl.Write<Fling::Transform>();
        decl.Read<Mover>();
        decl.Read<Fling::SingletonTime>();
    }

    void Update(Fling::SystemContext& ctx) const override
    {
        const float dt = ctx.Singleton<const Fling::SingletonTime>().FixedDelta;

        for (auto [entity, transform, mover] : ctx.Query<Fling::Transform, const Mover>())
        {
            transform.Position = MoveUtil::Toward(transform.Position, mover.TargetPos, mover.Speed * dt);
        }
    }
};

static Fling::RegisterSystem<MoverSystem> s_MoverSystem;   // a plain static object, not a macro
```

**Rules the base class enforces:**
- **No state, checked at compile time.** `RegisterSystem<T>` does `static_assert(sizeof(T) == sizeof(Fling::System))`, so a system with a member variable doesn't compile. `Update` is `const`.
- **What gets declared is what gets touched.** `Declare` is the explicit tuple Ford asks for (43:27).
  - `ctx.Query<T>()` (T means write, `const T` means read), `ctx.Singleton<T>()`, `ctx.Lookup<T>()`, and `ctx.Queue<T>()` check against the declaration. A debug build asserts on the first access that wasn't declared, and the message names the system and the component.
  - A CTest runs every system in the catalog once against an empty world, so a mismatch fails CI even if nobody runs the game.
  - The system lists its access twice: once in `Declare` and once in `Update`. We accept this on purpose, in exchange for code with no template deduction that is easy to read.
- **Per-call compile-time checks:** `decl.Write<T>()` and `ctx.Query<T>()` `static_assert` that `T` is a component. `ctx.Singleton<T>()` and `decl.Read/Write` of a singleton `static_assert` that `T` is a singleton.
- **Error messages point at your code.** Every `decl.*` call captures `std::source_location` (C++20), so schedule errors give `file:line`.

**`SystemDecl` vocabulary:** `Name`, `Phase`, `Read<T>`, `Write<T>`, `Append<Q>` (pushes into queue singleton `Q`; any number of systems can append, each into its own buffer, merged in schedule order), `Consume<Q>` (reads and clears `Q`), `Spawns()` (creates entities immediately), `After("Name")` / `Before("Name")` (explicit, rarely needed), `RunIn(WorldMode)`, `RunIf(fn)`. `Commands` (deferred destroy/add/remove) and `With<>` / `Without<>` filters need no declaration, because archetype membership changes only at sync points.

**`SystemContext` vocabulary:**
- `Query<A, const B, ...>()` returns a range of `[entity, a, b...]` tuples. It has `.With<T>()`, `.Without<T>()`, `.Changed<T>()`, and `Optional<T>` (gives a `T*`). `.ForEachChunk(fn)` hands raw column spans to the hottest loops.
- `Singleton<T>()`, `Lookup<T>().TryGet(entity)` (random access, returns `T*`), `Queue<Q>().Push(...)`, `Commands()`, `Spawn(components...)`.

A small amount of template code exists inside the engine, mainly the query iterator's tuple and the `const` handling. Gameplay code never writes or reads type lists.

**The order comes from the data.** Inside a phase, the scheduler builds the graph from the declarations:

| Declarations | Edge |
|---|---|
| A `Write<X>`, B `Read<X>` | A runs before B (producer before consumer) |
| A `Append<Q>`, B `Consume<Q>` | every appender runs before the consumer |
| A `Read<X>`, B `Read<X>` | no edge; they can run in parallel |
| A `Write<X>`, B `Write<X>` | **error** unless an explicit `After` orders them. This pushes toward one writer per component (Ford, 14:06) |
| explicit `After/Before` | wins over the inferred edge between those two systems |

As a result, `ResolveHealth` (which `Consume`s `ModifyHealthQueue`) runs after every damage source without naming any of them, and a new damage source is ordered correctly just because it declares `Append<ModifyHealthQueue>`. The explicit order constraints that the old design needed sets for are mostly unnecessary now.

**Cycle detection: when is it caught?** The graph is made of declarations spread over many .cpp files. A C++ *compiler* can only see the whole graph if a single TU includes every system, which is the Unreal-style recompile hub we're avoiding. So cycles are caught **at build time, not at C++ compile time**:
- `fling_validate_schedule(<target>)` adds a POST_BUILD step that runs `<target> --validate-schedule`. That command builds every world mode's schedule headless and exits non-zero on a cycle, a write/write conflict with no order, an unknown `After` name, or a declaration that doesn't match the code. **`cmake --build` fails** and prints the cycle path with the `file:line` of each declaration.
- The same validation runs at startup in every build, and as a CTest.
- Checks that *can* happen at compile time do: no system state, component vs singleton misuse, and non-components in queries.

**Registration:** `static Fling::RegisterSystem<T> s_X;` adds a constant node to the system catalog at static init, just like `FLING_REFLECT` does for components (same WHOLE_ARCHIVE linking, §1.3). Each world builds its `Schedule` from the catalog, filtered by run mode or by an explicit list of names (tests). There is no central list.

A long `Declare` is the warning sign Ford describes ("I kind of like the friction").

**Fixed phases**, run in this order every frame:

| Phase | Runs | Examples |
|---|---|---|
| `Input` | once per frame | `InputSystem` reads the OS and fills `SingletonInput` (replaces `entt::delegate` key maps) |
| `Simulation` | **fixed step**, 0..N times per frame from an accumulator | gameplay, movement, AI; later rollback resim |
| `ResolveDeferred` | after each sim step | `ResolveSpawnQueue`, `ResolveContactSystem`, `ModifyHealthQueue`, then the `Commands` flush |
| `PostSimulation` | once per frame | `TransformSystem` (hierarchy → `LocalToWorld`), interpolation |
| `PresentationPrep` | once per frame | render extraction: write the proxy/UBO data the renderer reads (moves the transform and light code out of the subpasses) |
| `Editor` | editor builds only | applies queued editor edits at a sync point (not during the render pass) |

- **Order inside a phase:** the data edges and the explicit edges above, then a topological sort. Systems with no edge between them are tie-broken by name, so the order is always deterministic, which rollback needs.
- **Parallel-ready from day one:** the scheduler groups systems into *stages* of non-conflicting systems (disjoint writes; any number of readers). Version 1 runs the stages one at a time. A job system can later run them in parallel without changing any system code.
- **Sync points:** `Commands` are flushed at the end of each phase (and after each sim step). Destruction is deferred, which is safe.
- **Immediate creation during iteration:** a query captures the chunk count and the last chunk's row count when it starts. New entities are appended past that point, so the current iterator does not see them, but every later system does.
- **Run conditions:** `.RunIn(WorldMode::Edit | WorldMode::Play)` and `.RunIf(fn)`. `TransformSystem` runs in edit mode so the scene view stays correct. Gameplay runs only in play mode. This replaces `World`'s `Playing` check.
- `Game::Update` is removed. A game is the set of features it links.
- **Tools:** an editor "Schedule" panel shows phases, stages, systems, per-system timings, access sets, and *why* each edge exists ("writes Transform, read by X"). `--dump-schedule` writes a DOT graph. Tests run one system with `world.Run<MoverSystem>()`, or a named list with `Schedule::Build(types, {...})`.

## 1.6 Worlds, editor, and play-in-editor

- **`Fling::World`** owns its storage and its singletons, and takes a `const TypeRegistry&` that `Engine` (or a test) built from the catalog (§1.3). `Engine` owns its worlds, and `g_Registry` goes away.
- **Edit world + play world:** pressing Play clones the edit world into a new play world (§1.4 snapshot). Stop throws the play world away. This replaces `OnStopGame`'s "destroy everything with Rotator/Mover" hack, and the systems never know which world they are in (the Kill Cam lesson).
- **Editor edits are commands:** a widget edits a copy of the value, and if it changed, the editor pushes `EditCommand{entity, type, before, after}` (reflected bytes). They apply in the `Editor` phase. This gives **undo/redo** for free and removes the mid-render mutation. Selection becomes editor singleton state, and `EditableComponent` is deleted.
- **Hierarchy:** `Parent` plus `Children` (or first-child/next-sibling) components, `LocalTransform` / `LocalToWorld`, and `TransformSystem` computing them in order. The outliner shows a tree.

## 1.7 Renderer integration (proxy components)

- Subpasses take a **read-only** `const World&` (or the extracted render data) and never mutate it.
- `MeshRenderer` (authoring) gets a `GpuMeshProxy` (transient) from a `PresentationPrep` system that runs on `Query<Read<MeshRenderer>, Without<GpuMeshProxy>>`. This is ordinary ECS, not a construct signal. Its `OnRemove` hook releases the UBO and descriptor set, which **fixes the leak**.
- `PointLight` needs a `Transform` because of `.Requires<Transform>()` in its registration. The editor and the loader add the required component. Light position is extracted into the lighting UBO by a system. `PointLight::Pos` leaves the component.
- The debug gizmo mesh for lights comes from an editor-only extraction system, not from adding a `MeshRenderer` inside a hook.

## 1.8 Compile-time isolation (the anti-Unreal rules)

- **No umbrella header.** Each component is in its own header and includes only its field types. Component headers never include `World.h`.
- **Systems have no headers.** A change to a system's .cpp recompiles **one translation unit**. There is no aggregating registration file to recompile.
- **Reflection lives in .cpp files.** Adding or changing metadata recompiles one TU. `FLING_COMPONENT` in a header pulls in only `Ecs/ComponentDecl.h`.
- **Thin templates over a type-erased core.**
  - `Query<...>` expands to a small array of `ComponentId`s plus an inlined `ForEach` loop.
  - Archetype matching, chunk management, and the scheduler are non-template code in `.cpp` files.
  - There is no EnTT-style variadic metaprogramming tower.
- **Header budget:** `Entity.h`, `Query.h`, and `World.h` include only `<cstdint>`, `<cstddef>`, `<span>`, `<type_traits>`, and `<utility>`. CI keeps an include-what-you-use style check (a grep-based script like `check_comment_style.py`).
- **Measure it:** a `-ftime-trace` check on the ECS headers, plus a documented edit→rebuild test: touch `MoverSystem.cpp` → 1 TU; touch `Mover.h` → only the files that include it.

## 1.9 What "complete" means (feature checklist)

Entities with generations · archetype/chunk storage plus Enableable and Sparse kinds · batched structural flush · link-time component and system catalogs (no central lists) · ECS telemetry and budgets · tags · singletons · queries with Read/Write/Optional/With/Without/Changed · Commands (deferred) and Spawner (immediate) · lifetime hooks (OnAdd/OnRemove only) · `Requires<>` dependencies · reflection with metadata · JSON and binary archives · EntityGuid remapping · component versioning and upgraders · prefabs · hierarchy and TransformSystem · phases, fixed step, ordering, ambiguity detection, stages · run conditions (edit/play) · deferment queue singletons · world clone/snapshot · editor inspector auto-generated from reflection, undo/redo, schedule panel · per-system profiling · Catch2 tests and benchmarks.

**Deliberately out of scope for v1:** running stages in parallel (the metadata is ready for it), networking replication, and C++26 static reflection. We can switch to C++26 reflection later behind the same builder API.

## 1.10 How it scales: many systems adding and removing components

Archetype storage is fastest to iterate, but it has a known weakness: when many systems add and remove components every frame, entities keep moving between archetypes and the archetype count explodes. This section defines the rules and the machinery that keep that cost under control as the game grows.

**Where does this data go? (decision table for gameplay programmers)**

| The data is... | Put it in | Why |
|---|---|---|
| What the entity *is*, set at spawn or by a prefab, rarely changed (`Transform`, `Health`, `MeshRenderer`) | **Table** component (default) | Fastest iteration; structural changes are rare |
| A state flag that flips often (`Grounded`, `Stunned`, `Selected`, `Visible`) | **Enableable** component | Flipping a bit costs nothing and doesn't move the entity. Queries skip disabled rows (`With<Stunned>` checks the bit) |
| Present on few entities and added/removed often (`StatusEffectStack`, `AiTarget`, `Burning`) | **Sparse** component (`.Storage(Sparse)`) | O(1) add/remove that doesn't move the entity and doesn't create archetypes |
| "X happened this frame" (damage, hit, spawn request, sound cue) | **Not a component.** A queue in a singleton (`ModifyHealthQueue`, `SingletonContactQueue`) | Ford's deferment rule. Components used as events are the main cause of archetype churn |
| State owned by one system that others read (`SingletonInput`, `SingletonCamera`) | **Singleton** component | Rule 2 |
| A specialist subsystem's internals (GPU buffers, physics body, audio voice) | **Proxy** component holding a handle | Rule 6, the "iceberg" |

The storage kind is one line in `FLING_REFLECT`. Changing it later doesn't affect any system code, because queries look the same for all three kinds.

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
- Each component has one *owning feature*: the folder where its `FLING_REFLECT` lives.
- Other features may read it, and may write it only through that feature's `*Util` functions or queues. This is Ford's "every mutation of `PlayerCommand` in one file" (14:06).
- The schedule panel and `--dump-schedule` list every writer of each component. A component with many writers is flagged in review, the same way a long tuple is.

**What adding the 200th component costs:** one header, one `FLING_REFLECT` block, and a rebuild of that feature's TUs. No central file. No engine recompile. No forgotten registration (the build fails with a `static_assert` instead).

## 1.11 Where it lives

`FlingEngine/Core/inc/Ecs/` and `FlingEngine/Core/src/Ecs/` (flat include names, per locked decision 2, e.g. `#include "Ecs/World.h"`). Core has no dependencies, and Gameplay, Graphics, Editor, and Physics can all use it. We don't add a new module (`architecture.md` says not to add one on a whim). Gameplay-level components (`Transform`, `Name`, `EntityGuid`, hierarchy) and `TransformSystem` stay in `Gameplay/`.

---

# Part 2 — Migration plan: replacing EnTT step by step

**Strategy: strangler facade.** First, move every caller onto the *new Fling API* while EnTT still backs it. Then swap the backend underneath. Every phase is one or more PRs off `main` that keep Sandbox and FlingTests green on all CI platforms. Coordinate with `module-split-plan.md`, so that ECS code lands in the module targets that plan defines.

### Phase A — Remove the EnTT usage that is not ECS (small, low risk)
- Add `Fling::HashedString` (constexpr FNV-1a, same algorithm as EnTT 3.1) in Core. Make `Guid`/`Guid_Handle`/`HS()`/`_hs` use it (`Core/inc/FlingTypes.h`). Callers in Resources and Graphics don't change.
- Replace `entt::delegate` in `Core/inc/Input/Input.h/.inl` with a small `Fling::Delegate` (to be replaced in Phase D by `SingletonInput`).
- Remove `<entt/...>` from `FlingTypes.h`. Delete the dead cereal include `Core/inc/Serilization.h` and the `FlingExt::cereal` link.
- After this phase, EnTT appears only in ECS code.

### Phase B — `Fling::World` facade, still backed by EnTT
- Add `Ecs/Entity.h`, `Ecs/World.h`, `Ecs/Query.h`, and `Ecs/Commands.h` with the final API (§1.2, §1.5 query types). The implementation wraps `entt::registry` in `Ecs/src/EnttBackend.cpp` and keeps EnTT out of the public headers.
- `World` **owns** its storage. `Engine::g_Registry` is removed, and `entt::registry&` parameters become `World&` / `const World&` in `Game`, `VulkanApp`, `RenderPipeline`, `Subpass`, the editor panels, Sandbox, and the tests. This is mechanical and can go in several PRs, one per area.
- Replace the `entt::tag<"Default"_hs>` / `"Debug"_hs` tags with tag structs (`DeferredRenderTag`).

### Phase C — Type catalog, TypeRegistry, and reflection (replaces `ComponentTypeRegistry`)
- Raise `cmake_minimum_required` to 3.24. Add the `CONTENT` flag to `fling_add_module` (WHOLE_ARCHIVE linking), and while the engine is still one library, put the monolith and Sandbox on it too. Add the CTest check for missing catalog types.
- Add `FLING_COMPONENT`, `FLING_REFLECT`, the catalog, the `TypeRegistry` object, and the field-type table (§1.3). The comment in `ComponentTypeRegistry.h` that rules out static registration is replaced by the WHOLE_ARCHIVE rationale. Port `Transform`, `NameComponent`, the lights, `MeshRenderer`, `Mover`, and `Rotator`. During the move, `.Serialize(fn)` adapts existing `Serialize(JsonArchive&)` methods.
- Make `World::LoadLevelFile` and `OutputLevelFile` use reflection, and add `EntityGuid`. Existing `Assets/Levels/*.json` must load without changes.
- The editor inspector is generated from reflection. The hand-written widgets in `BaseEditor.cpp` become `EditorWidgets::Override<T>`.
- Delete `ComponentTypeRegistry`, `IComponentRegistrar`, `Game::RegisterComponents`, `RegisterGameplayComponents`, `RegisterGraphicsComponents`, and `ResetForTests`, because the TypeRegistry is an object that tests create themselves.
- Move Sandbox's code into feature folders (`Sandbox/Gameplay/Features/Movement/`, ...) as each feature is ported.

### Phase D — Systems, schedule, and singletons
- Add `Fling::System`, `SystemDecl`, `SystemContext`, `RegisterSystem<T>`, the system catalog, the phases, the fixed-step accumulator, ordering derived from data, `--validate-schedule` plus `fling_validate_schedule()` (POST_BUILD), the declaration-vs-access checks, and run conditions (§1.5). Add the batched `Commands` flush (§1.10). `Engine::Tick` calls `world.RunFrame(dt)`.
- Move Sandbox's Rotator and Mover logic into registered systems, and remove `Game::Update`. `Game` keeps only its registration hooks and start/stop.
- Add `SingletonTime` and `SingletonInput` (from the `Input` key maps), plus `SpawnQueue` and `ResolveSpawnSystem` for `OnTestSpawn`.
- Add `TransformSystem` (computes the world matrix out of the renderer). `Transform` loses its methods, which move to `TransformUtil`.

### Phase E — Renderer to proxy components
- Subpasses become read-only. `PresentationPrep` systems handle mesh proxy creation, UBO updates (filtered by `Changed<Transform>`), and the light UBO.
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
Play-in-editor through world clone · undo/redo · hierarchy and outliner tree · prefabs · schedule panel and profiling · the parallel stage executor on a job system · binary cooked levels · snapshot/restore for rollback (consult the `rollback-netcode-expert` agent).

---

## Critical files

- `FlingEngine/Engine/inc/Engine.h`, `Engine/src/Engine.cpp`: registry ownership and the tick loop
- `FlingEngine/Core/inc/FlingTypes.h`, `Core/inc/Input/Input.h/.inl`: Phase A
- `FlingEngine/Gameplay/inc/World.h`, `src/World.cpp`, `inc/Game.h`, `inc/ComponentTypeRegistry.h`, `src/ComponentTypeRegistry.cpp`
- `FlingEngine/Resources/inc/JsonArchive.h`: field serialization folds into the reflection field types
- `FlingEngine/Graphics/src/OffscreenSubpass.cpp`, `GeometrySubpass.cpp`, `VulkanApp.cpp`, `RenderPipeline.cpp`, `inc/Subpass.h`, `inc/MeshRenderer.h`, `src/RegisterGraphicsComponents.cpp`
- `FlingEngine/Editor/src/BaseEditor.cpp`, `EntityEditorPanel.cpp`, `WorldOutlinePanel.cpp`
- `Sandbox/Gameplay/src/SandboxGame.cpp`, `inc/Mover.h`
- `FlingTests/src/JsonSerializationTests.cpp`, plus new `FlingTests/src/Ecs*Tests.cpp`
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
  - singletons are separate per world (two worlds alive at once)
  - schedule ordering, cycle detection, and ambiguity warnings
  - fixed-step accumulator
  - reflection round-trip (JSON and binary)
  - EntityGuid reference remapping
  - world clone equals its source
  - five systems each adding a component to one entity cause exactly one archetype move per flush
  - Enableable toggles and Sparse add/remove don't create archetypes
  - a TypeRegistry built from a subset of the catalog rejects types outside that subset
  - two TypeRegistries and two worlds alive at once in one test
- Ergonomics check: add a brand-new component and system to Sandbox by creating one feature folder only. It shows up in the editor's Add menu, saves and loads, and runs in Play, with no other file edited.
- Phase F: both backends pass the full suite. The benchmarks are recorded in the PR.
- Compile-isolation check: touch `MoverSystem.cpp` → 1 TU rebuilt; touch `Mover.h` → only the files that include it (Ninja `-d explain` / build log).
- Phase G: `grep -r entt FlingEngine Sandbox FlingTests CMake` returns nothing, and CI is green on Linux GCC/Clang and Windows MSVC/MinGW.
