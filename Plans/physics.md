# Physics plan

Status: v1 is done. Gameplay uses one physics API, and the physics library behind
it is a **provider** that can be swapped. Jolt is the default, and Bullet is
built in CI too. The Jolt step can use worker threads (fork and join on the
Core `Thread` layer). The next steps are at the end.

## Goals

- Gameplay code is the same whatever physics library is in use.
- A game can bring its own library (for example a commercial one) by writing one
  provider, with no engine changes.
- The simulation gives the same result on every platform and compiler, so that
  rollback netcode and replays work.
- Strict ECS rules: data-only components, state in singletons, deferred
  requests, and one ordered physics phase.

## Layers

```
Gameplay (Sandbox, game modules)
  │  uses only: RigidBody, Collider, ContactEvents,
  │             Physics::ApplyForce / ApplyImpulse / ApplyTorque / SetLinearVelocity
  ▼
Physics module (FlingEngine/Physics, no physics library)
  │  components, singletons, the fixed step clock, Transform <-> pose,
  │  entity <-> body mapping, the physics systems, PhysicsProvider.h
  ▼
PhysicsProvider / PhysicsScene interface (uses no EnTT and no library types)
  ├── PhysicsJolt module    (external/JoltPhysics, the default)
  ├── PhysicsBullet module  (external/bullet3)
  ├── the null provider     (in Physics: bodies exist but never move)
  └── a game's own provider (Game::CreatePhysicsProvider)
```

## The Physics module

- **Saved components** (`PhysicsComponents.h`):
  - `RigidBody`: mass (0 is static), friction, rolling friction, restitution,
    damping, and velocity. The velocity is the start velocity, and physics
    writes the current velocity back to it.
  - `Collider`: a box or a sphere, in local units multiplied by the Transform
    scale.
- **Runtime types** (`PhysicsTypes.h`, in the registry context, so each registry
  has its own scene):
  - `PhysicsBody` is the proxy component: the provider's body ID.
  - `PhysicsSettings`: gravity, the fixed step, and the most steps per tick.
  - `PhysicsCommands`: the queue of gameplay requests.
  - `ContactEvents`: the touching pairs of the last tick, one time for each pair.
  - `PhysicsWorld`: the provider, its scene, the tracked bodies, and the clock.
- **The phase**: `Engine::Tick` calls `Physics::Tick(reg, dt)` after the game
  update. It runs stateless systems in a fixed order:
  1. CreateBodies
  2. RemoveStaleBodies
  3. QueueCommands
  4. Step: zero or more fixed steps
  5. ContactEvents
  6. WriteBack
- **Fixed step**: the module owns the clock. The provider runs exactly one step
  of `PhysicsSettings::m_FixedStep` each time it is called, so a provider cannot
  bring its own clock. A frame shorter than a step runs no step. A long stall
  runs at most `m_MaxStepsPerTick` steps, and the rest of the time is dropped.
- **Forces**: `ApplyForce` and `ApplyTorque` become impulses of
  `value * tick delta time`. The momentum that they add is then the same at any
  frame rate and with any number of steps.
- **Rollback**: `Physics::SaveState` and `Physics::RestoreState` save and restore
  the scene and the clock. After a restore, every body is written back,
  including sleeping bodies. The entities and their components are saved by the
  caller (the ECS snapshot), not by physics.

## Threading

The physics phase runs on the game thread, inside the game tick, after the game
update. Physics never runs one tick behind on a thread of its own
([improve-time.md](improve-time.md) §Physics).

```
Game thread:  ... World::Update → Physics::Tick ──────────────────────────→ render
                                   │ ECS side: create / remove bodies,
                                   │ requests, contacts, write-back
                                   └ Jolt step: push jobs ─┬─ run jobs while waiting ─┐ join
Physics Worker 1..N:                                       └─ run jobs ───────────────┘
```

- **Fork and join.** `JoltJobSystem` (`PhysicsJolt/src/JoltJobSystem.h`) is
  Jolt's job system on `Fling::Thread` workers named "Physics Worker N". They
  have engine thread IDs and are in `ThreadManager`. The game thread pushes the
  jobs of a step, then runs jobs itself while it waits. When `Physics::Tick`
  returns, all physics work is done.
- **Workers only run Jolt jobs.** Everything that touches the registry (body
  creation, requests, contacts, write-back) stays on the game thread
  ([Skills/threading.md](../Skills/threading.md) rules 3 and 6).
- **Worker count**: `[Physics] WorkerThreads` in `Config/EngineConf.ini`
  (`PhysicsSettings::m_WorkerThreadCount`, read by `InitWorld`). 0, the default,
  runs every job on the game thread. Each scene owns its workers, and they stop
  when the scene is destroyed.
- **Same result for any count.** Jolt's results do not depend on how its jobs are
  scheduled. Jolt calls the contact listener from its workers in any order, so
  the Physics module sorts the contacts by entity pair before it fills
  `ContactEvents`. The tests check that 0 and 3 workers give the same bits on
  every tick, and the golden hash is checked with both.
- **Sync.** The Jolt provider uses `Fling::Mutex` / `ScopedLock` with
  `FLING_GUARDED_BY` (the job queue, the contact list, the process-wide Jolt
  setup), so Clang's `-Wthread-safety` checks it. The workers share one quit
  flag. One flag per worker can deadlock, because a wake for one worker can be
  taken by another.
- Bullet and the null provider ignore the worker count. They do not have
  `PhysicsCapability::Multithreaded`.

## The provider interface (`PhysicsProvider.h`)

- **`PhysicsProvider`**: `GetName`, `GetCapabilities`, and `CreateScene`.
- **`PhysicsScene`**: one world. Its calls take arrays, so there is one virtual
  call for each phase of a step, not one for each body:
  - `CreateBodies`, `DestroyBodies`, `ApplyCommands`, `SetGravity`
  - `Step` (exactly one step)
  - `ReadBodies` and `ReadContacts`
  - `GetBodyCount`
  - optional: `SaveState` and `RestoreState`
- **Types across the boundary**: plain structs with GLM types and our own enums
  (`PhysicsBodyDesc`, `PhysicsBodyState`, `PhysicsContact`, `PhysicsBodyCommand`).
  Each body carries a `uint64` of user data, which the Physics module sets to the
  entity, so a provider never depends on EnTT.
- **Capabilities** (`PhysicsCapability`): `Deterministic`,
  `CrossPlatformDeterministic`, `SaveRestore`, `ContactImpulse`, and
  `Multithreaded`. Netcode can
  check them at start and refuse a provider that cannot support rollback.
- **`ReadBodies(out, false)`** returns each body that was awake during any step
  since the last call, including a body that went to sleep in a step. Without
  that, the last movement of a body that goes to sleep is lost, and the ECS and
  the scene no longer agree.

| Provider | Deterministic | Cross-platform | Save/restore | Contact impulse | Worker threads | Notes |
|---|---|---|---|---|---|---|
| Jolt (default) | yes | yes (`CROSS_PLATFORM_DETERMINISTIC`) | yes | no | yes | No rolling friction |
| Bullet | yes (same build) | no | no | yes | no | SIMD only on MSVC |
| Null | yes | yes | no | no | no | Nothing moves |

## Selecting a provider

- **Build time**: `-DFLING_PHYSICS_PROVIDER=<Name>`. The default is `Jolt`.
  `CMake/FlingPhysicsProvider.cmake` searches the folders in
  `FLING_PHYSICS_PROVIDER_PATHS` (one level deep) for a `CMakeLists.txt` that
  calls `fling_add_physics_provider(<Name>)`. `FlingEngine/` is always searched,
  and `None` (the null provider) is always available. The search only reads
  files. Only the selected folder is added, so only that provider and its library
  are configured and compiled. An unknown name stops the configure step with the
  list of providers that were found.
- `Fling::DefaultPhysicsProvider` links the selected module and generates
  `DefaultPhysicsProvider.h`, which has `CreateDefaultPhysicsProvider()` and
  `DefaultPhysicsProviderName`. Application and `PhysicsTests` link it.
- **Run time**: a game can also override `Game::CreatePhysicsProvider()` to
  return a provider object. The engine uses the build's provider only when the
  game returns null.

## How to add a provider

No engine file changes. A provider is a folder with two things.

1. A `CMakeLists.txt` that calls the provider function. It makes the module
   `Physics<Name>`, which depends on `Physics`, links `LIBS` PRIVATE, and gets
   `fling_set_deterministic_math()`:

   ```cmake
   include(MyPhysicsLibrary)          # Add the physics library here, so it is
                                      # configured only when this provider is selected
   fling_add_physics_provider(MyPhysics
       LIBS
           MyPhysicsLibrary
   )
   ```

2. A `src/*.cpp` that implements `PhysicsScene` and `PhysicsProvider`, and
   registers the provider at global scope:

   ```cpp
   FLING_IMPLEMENT_PHYSICS_PROVIDER(MyPhysics, MyGame::MyPhysicsProvider)
   ```

   The macro defines `FlingCreatePhysicsProvider_MyPhysics()`. The engine knows
   that name from the provider name alone, so it needs no header or class name
   from the provider. The class must be default-constructible. Report the
   capabilities honestly.

Then point the engine at the folder that contains the provider folder and select
it. A game does this in its own `CMakeLists.txt`, before it adds the engine:

```cmake
list(APPEND FLING_PHYSICS_PROVIDER_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(FLING_PHYSICS_PROVIDER "MyPhysics" CACHE STRING "" FORCE)
```

or on the command line: `-DFLING_PHYSICS_PROVIDER=MyPhysics -DFLING_PHYSICS_PROVIDER_PATHS=<folder>`.

Run `FlingTests/Physics/PhysicsProviderTests.cpp` against it: build with the
provider selected and run `PhysicsTests`. Tests for capabilities that the
provider does not claim are skipped.

`FlingTests/PhysicsProviderExample/Example` is a complete small provider outside
the engine (bodies fall and take impulses, with no collisions). The test
"Physics: providers are found" checks that the search finds it. A build with
`FLING_PHYSICS_PROVIDER=Example` builds and runs Sandbox, and `PhysicsTests`
then fails only the collision tests, as it should for that provider.

Rules for the registration:

- Use explicit factory functions, not self-registering static objects. The
  linker drops a static object in a static library that nothing calls
  ([ecs-plan.md](ecs-plan.md) §1.3). The generated header calls the factory, so
  the linker keeps it.
- Provider modules are static libraries.
- The name `None` is reserved. Two providers with one name stop the configure
  step.

## Determinism

- **What is done:**
  - Jolt is built with `CROSS_PLATFORM_DETERMINISTIC`, which also turns off FMA
    in Jolt.
  - `fling_set_deterministic_math()` builds the Physics module and the provider
    modules with `-ffp-contract=off -fno-fast-math` (GCC/Clang) or `/fp:precise`
    (MSVC).
  - The physics step is fixed, and `ReadBodies` output is sorted.
- **What is tested** (`PhysicsProviderTests.cpp`):
  - Two scenes given the same inputs give bit-identical states.
  - 0 and 3 worker threads give bit-identical states and the same contacts.
  - Physics workers are in `ThreadManager` while the scene exists, and are
    gone after `ShutdownWorld`.
  - A restored state gives a bit-identical future.
  - A golden hash of the pyramid scene after 240 ticks must be the same on every
    platform, compiler, and worker count. The value was measured on Linux with
    Clang Debug and GCC Release, with 0 and 3 workers (all match), and CI
    checks it on MSVC and MinGW. If a change
    moves it on purpose, update the value in the same change.
- **What is not covered yet:**
  - The Euler-degrees conversion in the Physics module uses the platform `sin`,
    `cos`, and `atan2`. Rotations that come from Transform can therefore differ
    in the last bit across platforms. The golden scene uses zero rotations for
    this reason. The fix is a quaternion in `Transform`, or deterministic trig
    functions.
  - Gameplay code that computes forces must follow the same floating point
    rules. The fixed tick of [improve-time.md](improve-time.md) is where that
    gets enforced.

## Next steps

1. Step on `SimTick` with `FixedDelta()` when the tick loop of
   [improve-time.md](improve-time.md) lands, in place of the physics clock.
2. Move the physics workers to the engine job system when it exists (a pool
   shared by every subsystem, Skills/threading.md rule 6), and pick a default
   worker count from the CPU count.
3. Kinematic bodies, and more shapes: capsule, convex hull, mesh, compound.
4. Collision layers and masks, triggers, and raycast, sweep, and overlap queries
   as utility functions, each added to the provider interface.
5. A rollback test that saves the ECS state and the physics state together and
   resimulates with the rollback-netcode tools.
6. When a system scheduler exists ([ecs-plan.md](ecs-plan.md)), register the
   physics systems with it, with read/write sets, in place of `Physics::Tick`.
