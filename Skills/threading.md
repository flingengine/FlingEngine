# Threading

The thread layer in Core (`FlingEngine/Core/inc/Threading/`). It is below the
ECS. Subsystems (audio, render, streaming) use it for their own threads.
Gameplay code does not use it directly.

## Types

| Type | Header | What it does |
| --- | --- | --- |
| `Runnable` | `Threading/Runnable.h` | The work of a thread: `Init`, `Run`, `Stop`, `Exit` |
| `Thread` | `Threading/Thread.h` | One OS thread with an engine name and an engine `ThreadId` |
| `ThreadManager` | `Threading/ThreadManager.h` | Registry of the running threads (ID, name, OS ID). Diagnostics only |
| `ThreadQueue<T>` | `Threading/ThreadQueue.h` | Queue from many producer threads to one consumer thread |
| `CommandQueue` | `Threading/CommandQueue.h` | `ThreadQueue` of `std::function<void()>`, with `Execute` |
| `CommandRunnable` | `Threading/CommandRunnable.h` | A `Runnable` that runs the commands sent to it, then calls `Tick` |
| `TickLimiter` | `Threading/TickLimiter.h` | Waits for each tick of a fixed `TickRate` (for example 60 or 120 Hz) |

The OS code is in `src/Threading/NativeThreadPosix.cpp` and
`src/Threading/NativeThreadWindows.cpp`. No public header includes
`<pthread.h>` or `<Windows.h>`.

## Threads

```cpp
auto Audio = std::make_shared<AudioRunnable>();   // : public CommandRunnable
std::shared_ptr<Thread> AudioThread =
    Thread::Create(Audio, {.Name = "Audio Thread", .Priority = ThreadPriority::AboveNormal});

Audio->Enqueue([Request]() { PlayOneShot(Request); });

AudioThread->Kill(); // Stop, run the commands that are left, join
```

- `Thread::Create` waits until `Runnable::Init` returns. It returns null when
  the OS cannot start the thread or when `Init` returns false. `Run` is not
  called in those cases.
- `Thread::Create(desc, lambda)` runs a lambda one time. `Kill` cannot stop the
  lambda. Give it a flag of its own when it loops.
- `Kill` calls `Runnable::Stop`, then joins. The destructor joins a thread that
  is still running. Do not destroy a `Thread` on its own thread.
- The owner keeps the `std::shared_ptr<Thread>`. `ThreadManager` cannot find a
  `Thread`, so it is not a global path to a thread.

## Fixed tick rate

A thread can tick at a fixed rate. `TickRate::Hz(120)` gives a period of
8.33 ms.

```cpp
// A CommandRunnable at a fixed rate: commands, then Tick, 120 times each second
class MixerRunnable : public CommandRunnable
{
public:
    MixerRunnable() : CommandRunnable(TickRate::Hz(120)) {}
    void Tick() override { MixNextBuffer(); }
};

// Or only a lambda, with no commands
std::shared_ptr<Thread> Worker = Thread::Create({.Name = "Ticker"}, TickRate::Hz(60), []() { Step(); });
```

- Tick N is due at start + N × period. A late wake does not move the next
  ticks, so the rate does not drift.
- A wake is never early. It can be late by the OS timer resolution: about
  0.1 ms on Linux (a condition variable on the monotonic clock), and about
  0.5 ms on Windows 10 1803 and later (a high-resolution waitable timer). Older
  Windows falls back to a condition variable, with about 15.6 ms resolution.
- `TickRate::Hz(120, TickPrecision::Precise)` sleeps until `SpinWindow`
  (default 1 ms) before each tick, then spins until the tick. A wake is then
  late by only a few microseconds, but the thread uses a full CPU core for that
  window of every tick. The window is capped at half a period, so a high rate
  does not spin all the time. Without the Windows high-resolution timer, the
  window is at least 16 ms. `TickPrecision::Normal` is the default. On one
  Linux machine at 100 Hz, `Normal` woke a median of 66 µs late (max 395 µs)
  and `Precise` 0 µs late (max 1 µs). Measure your machine with
  `./build/FlingTests/bin/CoreTests "[TickPrecision]"`.
- When a `Tick` takes too long, the tick that is due runs at once. When a tick
  starts more than half a period late, the schedule starts again one period
  after that tick, so ticks do not run back to back to catch up. The ticks of
  the old schedule are counted in `GetMissedTicks()`.
- In fixed-rate mode, commands run only at the start of a tick. Every command
  sent before a tick is seen by that tick.
- `Kill` does not wait for the next tick. `Stop` interrupts the wait.
- A fixed-rate thread is not the simulation clock. Gameplay and physics tick on
  `SimTick` inside the game loop (see `Plans/improve-time.md`). Use a fixed-rate
  thread for work that has its own clock, for example audio mixing or device
  polling.

## IDs and names

- `ThreadId` is an engine ID, given in creation order. `InvalidThreadId` is 0.
  `GameThreadId` is 1. `Engine::Startup` calls `Thread::InitGameThread` first.
- `Thread::CurrentId()` and `Thread::CurrentName()` read thread-local values.
  They return `InvalidThreadId` and an empty name on a thread that the engine
  did not start.
- The engine keeps the full name. Linux keeps only 15 bytes of the OS name.
  Windows uses `SetThreadDescription` when it is available.
- `GetNativeId()` is the OS ID, for debuggers and profilers.
- Priority: Linux can only lower a priority without `CAP_SYS_NICE`.
  `SetPriority` returns false when the OS refuses.

## Queues

- Any thread can `Push`. Only the consumer calls `Drain`, `Execute`, and
  `WaitForItems`. Items come out in push order. An item that is pushed during a
  drain goes to the next drain.
- Use a data item (`ThreadQueue<PlaySoundRequest>`) when the work crosses into
  a subsystem. Data can be recorded and replayed. A lambda cannot.
- A command copies values. It must not capture a `World`, a registry, a
  component pointer, or a query.
- The queue uses a mutex and a vector swap. The API does not show the lock, so
  a lock-free queue can replace it later. `ThreadManager` also uses a mutex. See
  [`Plans/lockless-threading.md`](../Plans/lockless-threading.md) for both.

## Rules with the ECS

These rules follow `Plans/ecs-plan.md` and the `gameplay-architecture` skill.

1. **Threads are subsystem internals.** A `Runnable` can have member state. It
   is never a component. ECS systems do not create or own threads.
2. **Gameplay does not send commands to a thread.** A gameplay system appends a
   request (for example `SoundCue`) to a queue singleton. One resolver system in
   `ResolveDeferred` drains it and pushes the requests to the subsystem thread.
   The resolver does not set `ReplayOnResim`, so a rollback resim does not send
   the requests again. The resolver gets the subsystem from a transient proxy
   singleton, not from a global.
3. **Other threads do not touch the world.** Results that come back (a voice
   ended, a load is done) go through a `ThreadQueue` that a main-thread system
   drains, with a tick stamp (see `Plans/improve-time.md`).
4. **Determinism.** Systems do not branch on `Thread::CurrentId()` or on thread
   timing. Thread IDs are not simulation state.
5. **No-thread mode.** Without a thread (tests, `--null-render`), the owner calls
   `CommandRunnable::ExecuteCommands` and `Tick` from its own loop. The same
   code then runs on one thread.
6. **ECS stages do not run on named threads.** A later job system (a pool of
   worker `Thread`s) runs parallel stages. It is a separate type.

## Tests

`FlingTests/Core/ThreadTests.cpp` and `FlingTests/Core/TickLimiterTests.cpp`, tag
`[Threading]` (`[TickRate]` for the fixed-rate cases). Timing tests check only
that a tick is never early. Their upper bounds are large, so that a busy CI
machine does not make them fail. Synchronize with
`std::latch` and atomics, not with sleeps. To run them with ThreadSanitizer,
configure a separate tree with `-fsanitize=thread`. On kernels with high ASLR
entropy, TSan stops with "unexpected memory mapping". Run the test with
`setarch -R` in that case.
