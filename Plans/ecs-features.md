# ECS worlds, features, and the editor

Status: proposal for review. Nothing here is implemented yet.

This plan builds on [`ecs-systems.md`](ecs-systems.md) (systems, components,
and the schedule) and changes parts of [`ecs-plan.md`](ecs-plan.md). It
answers these questions:

- Who owns which data?
- What is a world, and do we need one?
- Should the `Game` class exist?
- How do features (plugins) add and remove systems at runtime?
- How does level streaming fit?
- How does the editor work: panel edits, edit and play worlds, and
  editor-only systems?

Parallel execution and parallel loading are in
[`ecs-parallel.md`](ecs-parallel.md).

The ECS rules come from Timothy Ford's GDC 2017 talk, *'Overwatch' Gameplay
Architecture and Netcode*
(https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and).

## What changes in `ecs-plan.md`

| `ecs-plan.md` | This plan |
|---|---|
| Rule 7: "The engine runs one `World`. Edit and play are modes of that object" | Worlds are instantiable. A game runs one play world. The editor runs an edit world, and clones it into a play world when you press Play (§6.2) |
| §1.6: Play snapshots the world, Stop restores it into the same object | Stop destroys the play world. The edit world is never touched during play |
| The system set is fixed after startup | Features add and remove systems at runtime. The schedule is rebuilt and swapped at a frame boundary (§4) |
| `Game` keeps start, stop, and quit (Phase D) | `Game` goes away. A game is a feature that is always active (§3) |

---

## 1. Who owns what

**Rule: mutable state lives only in components and singletons.** Anything
else that holds state is an engine service behind a proxy, or it is a bug.

| Thing | Owner | Lifetime |
|---|---|---|
| Entities, components, singletons | The world's storage | Until destroyed. A clone or snapshot copies all of it |
| Systems | The world's active system set, grouped by the feature that added them | While that feature is active in that world |
| System code, component type descriptions | The process: the executable, or a loaded feature library | While the library is loaded |
| Schedule | The world. Built from its active systems. Never changed in place | Until the active systems change |
| Renderer, physics internals, resource cache, threads | Engine services outside the world. The world sees them only through proxy components and command queues | Process |

State that breaks the rule today, and where it goes:

| Today | Becomes |
|---|---|
| `World::m_CurrentState`, `m_PendingRequest`, `m_PlaySnapshot` | `PlaySession` singleton, plus the edit/play world split (§6.2) |
| `Game::m_WantsToQuit` | `AppRequests` singleton. `Engine` reads it after each frame |
| Members on `Sandbox`'s `Game` | Singletons that the game's systems own |
| `Input::Get()` | `InputSnapshot` singleton, written by `InputSystem` |
| `Timing::Get()` | `SimTime` and `RealFrameTime` singletons, written by the runner |

---

## 2. What a world is

A world is **one set of data and the systems that run on it**. It is a
container. It has no gameplay logic, no level logic, and no play state
machine.

```cpp
// Engine/inc/Ecs/World.h
namespace Fling
{
    enum class WorldRole : uint8
    {
        Edit    = 1 << 0,   // The editor's authoring world. No sim ticks run.
        Play    = 1 << 1,   // A running game, or a play session in the editor.
        Preview = 1 << 2,   // An editor preview (asset thumbnail, prefab view). No sim ticks.
    };

    class World
    {
    public:
        World(WorldRole role, const TypeTable& types);

        WorldRole Role() const;

        Entity Create();
        void Destroy(Entity e);                        // deferred to the next flush
        template<Component T> T& Singleton();

        /** One frame: apply pending feature changes, then run the schedule. */
        void RunFrame(float frameDelta);

        /** Applied at the start of the next RunFrame, never during a phase. */
        void RequestActivate(const FeatureDesc& feature);
        void RequestDeactivate(FeatureId id);

        /** A new world of `role` with a copy of every entity and singleton. */
        std::unique_ptr<World> Clone(WorldRole role) const;

    private:
        EntityStorage m_Storage;      // EnTT now, native storage later
        ActiveSystems m_Systems;      // Systems grouped by FeatureId
        Schedule m_Schedule;          // Built from m_Systems
        SystemRunner m_Runner;        // Fixed-tick accumulator and phase loop
        WorldRole m_Role;
    };
}
```

**Do we need `World`?** Without it, the loop is:

```cpp
EntityStorage storage;
ActiveSystems systems;
Schedule schedule = BuildSchedule(systems);
while (running)
{
    runner.RunFrame(storage, schedule, dt);
}
```

That works. But every API then takes three objects that must always agree.
A clone must copy storage and know which systems it gets. A test must build
all three by hand. A `World` struct comes back anyway. So we keep the name,
but only as the container above.

Its value is that you can make more than one (Ford's rule 8):

- tests, one world per test case
- the editor's edit world and play world
- previews and replays
- a client world and a server world in one process. This is the best
  netcode test setup.

`World` stays in the Engine module.

---

## 3. The `Game` class goes away

| `Game` today | Becomes |
|---|---|
| `Update(reg, dt)` | Systems |
| `Init`, `OnStartGame` | The start level or a prefab, plus `OnPlayBegin` on a game system that sets up its singletons |
| `OnStopGame`, `ResetPlayState` | Mostly nothing: Stop destroys the play world. What is left goes in `OnPlayEnd` |
| `RegisterComponents`, `RegisterSystems` | The game's feature register function |
| `m_WantsToQuit` | `AppRequests` singleton |
| `Shutdown` | `OnDestroy` on the game's systems |
| `GetWorld()` | Not needed. Systems get the world through their context |

What is left of "the game" is two things:

1. **A feature** that registers the game's components and systems.
2. **A project descriptor** (see [`project-model.md`](project-model.md)): the
   start level, the features that are on at startup, and the tick rate.

**The game is the first feature, and it is always active.** Game code,
engine features, and runtime plugins all go through one path.

```cpp
// Sandbox/Gameplay/src/SandboxFeature.cpp
#include "Ecs/Feature.h"
#include "Mover.h"
#include "Systems/MoverSystem.h"
#include "Systems/RotatorSystem.h"

namespace
{
    void Register(Fling::FeatureRegistration& feature)
    {
        feature.Component<Mover>();
        feature.Component<Rotator>();
        feature.System<Sandbox::MoverSystem>();
        feature.System<Sandbox::RotatorSystem>();
    }
}

extern const Fling::FeatureDesc SandboxFeature {
    .Name = "Sandbox",
    .Requires = {},
    .Register = &Register };
```

```cpp
// Application/src/Engine.cpp
void Engine::Startup(const ProjectDesc& project)
{
    m_World = std::make_unique<World>(WorldRole::Play, m_Types);
    m_World->RequestActivate(EngineFeature);      // InputSystem, ResolveSpawn, TransformSystem
    m_World->RequestActivate(GraphicsFeature);    // extraction systems
    for (const FeatureDesc* feature : project.StartupFeatures)
    {
        m_World->RequestActivate(*feature);       // SandboxFeature
    }
    m_World->RequestLoadLevel(project.StartLevel);
}
```

`Application` makes these calls because it is the only module that sees every
other module. `World` (Engine) cannot name `GraphicsFeature`.

---

## 4. Features: adding and removing systems at runtime

A **feature** is a unit of code that can be on or off in a world. It adds
component types, systems, and optional data. Engine modules, the game, and
dynamic plugins are all features.

```cpp
// Engine/inc/Ecs/Feature.h
namespace Fling
{
    struct FeatureDesc
    {
        const char* Name = nullptr;
        std::span<const char* const> Requires;          // Features that must be active first
        void (*Register)(FeatureRegistration&) = nullptr;
    };

    class FeatureRegistration
    {
    public:
        template<Component T> void Component();
        template<typename T> void System();             // Same checks as SystemRegistry::Add
    };
}
```

### 4.1 Activation

At the start of `RunFrame`, before any phase runs:

1. Check that every name in `Requires` is active. If not, reject.
2. Call `Register` into a temporary `FeatureRegistration`.
3. Add its component types to the world's type table. Make the storage pools
   for them.
4. Build a **new** schedule from the active systems plus the new ones.
5. If the build fails (two writers, a cycle, an unknown `After`), reject the
   activation. Log each error with its `file:line`. Keep the old schedule.
   Activation is all or nothing.
6. Call `OnCreate` on the new systems, in schedule order. Their singletons
   now exist.
7. Swap in the new schedule.

### 4.2 Deactivation

Also at the start of `RunFrame`:

1. Reject it if another active feature `Requires` this one.
2. Call `OnDestroy` on its systems, in reverse schedule order. This removes
   their singletons.
3. Destroy every entity that has `OwnedByFeature{id}`. Entities that the
   feature's systems spawn get that tag automatically.
4. Remove the feature's component types from every remaining entity, then
   from the type table.
5. Build the schedule without its systems, and swap it in.

### 4.3 Unloading a library

A feature in a shared library can be unloaded only after it is deactivated
in **every** world. These things live in the library: system vtables, the
component save and load functions, and component destructors. If one
instance is still alive when the library is unloaded, the process crashes.

- Each loaded library keeps a count of live objects that came from it:
  system instances, component pools, and singletons.
- `FeatureLibrary::Unload` asserts that the count is 0, then calls
  `DynamicLibrary` to close the library.
- Unloading is a request, like everything else. It runs after the frame.

### 4.4 Why this is cheap

Systems have no state (Ford 01:00). Nothing must be moved or saved when one
is added or removed. Adding a system means "rebuild the order". Removing one
means "drop its singletons and its components". Kahn's sort over a few
hundred systems takes microseconds, so rebuilding at a frame boundary costs
nothing that matters.

The same path gives **hot reload** of a feature library: deactivate, unload,
load the new build, activate. Entity data that the feature does not own
stays in the world.

### 4.5 Validation

The build cannot check every combination of features. `--validate-schedule`
checks:

- the base features alone,
- the base plus each feature, with what it `Requires`,
- every feature on together.

Runtime activation still runs the full check, so a bad combination is
rejected. It is never run.

### 4.6 Determinism

- An activation or deactivation in a play world is applied at a tick
  boundary, and recorded as a tick command. A replay applies it at the same
  tick.
- A resim never crosses an activation. The activation tick counts as
  confirmed state.
- A dedicated server and its clients must agree on the active features. The
  server sends the list when a client joins (later, with networking).

---

## 5. Level streaming

**Levels are data, not code, and not worlds.** A streamed level is a cell of
entities in the one play world. A level does not bring systems. A feature
does. A level can say "requires feature X", and loading the level activates
X first.

```
StreamingSystem (Simulation)
    reads the camera or player Transform, and the StreamingState singleton
    appends LoadCellRequest / UnloadCellRequest to StreamingQueue

ResolveStreamingSystem (ResolveDeferred, the only consumer)
    load:   sends the cell to the loader (a command queue, see Skills/threading.md)
    unload: destroys every entity with StreamingCell{id} (deferred)

ApplyCompletedLoads (start of RunFrame, time-budgeted)
    turns finished cell data into entities tagged StreamingCell{id}
```

- `StreamingState` holds which cells are wanted, loading, and loaded. It is a
  singleton, so the editor can show it and a clone copies it.
- Loader threads never create entities (`Skills/threading.md` rule 3).
- A cell is a tag on its entities, not a container. Unloading a cell is one
  query. Systems do not care which cell an entity came from.
- How the loader decodes cells in parallel is in
  [`ecs-parallel.md`](ecs-parallel.md) §4.

---

## 6. The editor

### 6.1 Panel edits are commands

Today `ImGuiSubpass::Draw` changes the registry in the middle of the render
pass. In this model a panel **reads** the world and **queues** each change:

```cpp
// Editor/src/EntityEditorPanel.cpp, while it draws
Fling::LocalTransform edited = *world.TryGet<const Fling::LocalTransform>(selected);
if (DrawReflectedFields(edited))
{
    edits.Push(EditCommand::SetComponent(selected, edited));    // before and after bytes
}
if (ImGui::Button("Delete"))
{
    edits.Push(EditCommand::Destroy(selected));
}
if (ImGui::Button("Add Child"))
{
    edits.Push(EditCommand::Create(prefab, /* parent */ selected));
}
```

One system, `Editor.ApplyEdits`, applies the queue at one point in the frame:

- **In an edit world** no sim ticks run. It applies in the `Editor` phase.
  This is the one place where a frame phase may write simulation
  components, and only in a world with role `Edit`. It answers question 2 in
  `ecs-systems.md`.
- **In a play world** (a value tweak during play), the edit becomes a tick
  command. It applies at the start of the next tick. The sim stays
  deterministic, and a recording reproduces the tweak.
- **Undo and redo:** each command keeps its before and after values. The
  command history is the undo stack. `EditableComponent` is deleted.

To the user, creating and deleting entities from panels works as it does
today.

### 6.2 Edit world and play world

| | One world plus snapshot (old plan) | Edit world plus cloned play world (this plan) |
|---|---|---|
| Play | Snapshot, then change the mode | `editWorld.Clone(WorldRole::Play)` |
| Stop | Restore the snapshot into the same object | Destroy the play world |
| Play state leaks into edit state | Possible. Commits `38df60f` (reset on stop) and `534fb36` (free descriptor sets) fix bugs of this kind | Not possible |
| Edit the level while it plays | No | Possible later |
| Client and server sessions in one editor | No | One play world each |
| Cost | Snapshot memory | Clone time and memory, and one set of render proxies per world |

The clone is cheap for our levels. With streaming, it copies only the loaded
cells. The standalone game is a play world with no edit world.

**Rendering:** each viewport shows one world. The renderer keeps proxies
(GPU buffers, descriptor sets) per world. Destroying a play world releases
its proxies, through the same `OnRemove` path that fixes the mesh leak in
`ecs-plan.md` Phase E.

### 6.3 Editor-only systems

There are three levels of control. Use the strongest one that fits.

**a. Compile time: editor systems live in the Editor module.** Editor is a
leaf, linked only in editor builds. Its systems are in `EditorFeature`, and
`Application` activates it only `#ifdef WITH_EDITOR`. A shipping game has
no editor systems at all.

**b. Per world: `RunIn` names the world roles.** A system whose `RunIn`
does not include the world's role is never added to that world's schedule.
It is not skipped each frame. It is not there.

```cpp
// Editor/inc/Systems/TransformGizmoSystem.h
class TransformGizmoSystem final : public Fling::System<TransformGizmoSystem, Fling::Access<
    Fling::Read<EditorSelection>,
    Fling::Read<Fling::LocalToWorld>,
    Fling::Read<EditorSettings>,
    Fling::Write<GizmoDrawList>,
    Fling::Append<EditCommandQueue>>>       // A drag pushes an edit. It never writes LocalTransform.
{
public:
    static constexpr Fling::SystemInfo Info {
        .Name = "Editor.TransformGizmo",
        .Phase = Fling::Phases::Editor,
        .RunIn = Fling::WorldRole::Edit | Fling::WorldRole::Play };   // Remove Play to hide it during play

    void Update(Context& ctx) const;
};
```

**c. Runtime toggle: `RunIf`.** For a setting such as "show gizmos during
play", `RunIf` reads `EditorSettings` and skips the call when it is off. The
schedule does not change.

Editor data is ordinary singletons that `EditorFeature` registers:
`EditorSelection`, `EditorSettings`, `GizmoDrawList`, `EditCommandQueue`.
They are `Transient` and `Presentation`. They are never saved to levels.
`Clone` does not copy them into a play world unless the play-tools feature
is active there. Game systems cannot name them, because game modules do not
include Editor headers.

So "editor" is a feature too. It is active in the edit world. A smaller
`EditorPlayTools` feature (gizmos, debug draw, the inspector) can be active
in a play world.

### 6.4 Threads

Editor systems run on the game thread in the `Editor` phase, as every other
system does. This includes building the ImGui frame. They do not draw. They
fill draw lists (`GizmoDrawList`, the ImGui draw data) that the renderer
reads. When the renderer moves to its own thread, it touches no world, so
the editor cannot race it.

---

## 7. Work items

These come after `ecs-systems.md` work item 6 and `ecs-plan.md` Phase B
(the `World` facade owns its storage).

1. **Singletons for world state.** `PlaySession`, `AppRequests`. `Engine`
   reads `AppRequests` for quit. `Game::m_WantsToQuit` is deleted.
2. **`FeatureDesc`, `FeatureRegistration`, `World::RequestActivate`.** The
   engine, Graphics, and Sandbox become features. Activation at startup
   only. `Game::RegisterComponents` and `Game::RegisterSystems` are deleted.
3. **Runtime activation and deactivation** with schedule rebuild and swap,
   `OwnedByFeature`, and the all-or-nothing rule. Tests: activate a feature
   mid-run, check its systems run on the next frame; deactivate it, check
   its singletons, components, and entities are gone; activate a feature
   that makes a write conflict, check it is rejected and the old schedule
   still runs.
4. **`World::Clone` and the edit/play split.** Play clones, Stop destroys.
   Delete the play snapshot code in `World`. Per-world render proxies.
5. **Edit commands and undo.** `EditCommandQueue`, `Editor.ApplyEdits`, and
   undo/redo. Panels stop writing the registry. `ImGuiSubpass` gets
   `const` access only.
6. **Editor-only systems.** `EditorFeature`, `RunIn` by role, gizmo system.
7. **`Game` deleted.** What is left moves into the Sandbox feature and the
   project descriptor.
8. **Feature libraries.** Load, unload with the live-object count, and hot
   reload. Wait for [`project-model.md`](project-model.md) to define plugin
   layout.
9. **Level streaming** (`StreamingSystem`, cells, budgeted apply), with the
   parallel decode from `ecs-parallel.md`.

## 8. Questions for you

1. **Where component types live.** This plan keeps one type table per world
   (a feature adds types to the worlds it is active in). The other choice
   is one process-wide table that every loaded library adds to, with
   activation only controlling systems. Per-world is stricter. Process-wide
   is simpler for the editor, which wants to list every type.
2. **Edit while playing.** The split makes it possible to edit the edit
   world while a play world runs. Do you want that, or should the edit
   world be read-only during play?
