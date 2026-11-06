# Core sync primitives plan

Status: not started. This is Phase 1 of [`audio.md`](audio.md).

This brief is for an agent with no prior context. Fling Engine is C++20 with CMake. Read
`AGENTS.md`, `Skills/threading.md`, and `Skills/coding-style.md` first.

## Goal
Add thin engine wrappers for the synchronization primitives in Core: mutex, scoped lock,
shared mutex, spin lock, condition variable, semaphore, and event. Annotate them for Clang's
`-Wthread-safety` analysis. Move Core's threading code off raw `std::mutex` and
`std::condition_variable`, and onto the wrappers.

The wrappers are built on the std types. They do not use native OS primitives.

Why we want them:
1. Compile-time lock checking. Clang's `-Wthread-safety` reports access to `FLING_GUARDED_BY`
   data without the lock held. libstdc++ std types have no annotations, so the analysis cannot see them.
2. One place to add lock profiling and contention stats later.
3. Spin lock, semaphore, and event types the coming audio subsystem needs.

Out of scope:
- lock-free queues (see `Plans/lockless-threading.md`)
- atomics wrappers (keep `std::atomic`)
- native OS primitives
- an SPSC ring buffer
- `PlatformTime`
- any audio code

## Current state (read these)
- `FlingEngine/Core/inc/Threading/ThreadQueue.h`: `std::mutex`, `std::lock_guard`,
  `std::unique_lock`, and `std::condition_variable::wait_for` with a predicate. The mutex is `mutable`.
- `FlingEngine/Core/inc/Threading/Thread.h:112-119` and `src/Threading/Thread.cpp:64-75, 108-125, 150, 160`:
  `m_JoinMutex`, `m_StartMutex`, and `m_StartCondition`, with an explicit `Lock.unlock()` before the end of the scope.
- `FlingEngine/Core/inc/Threading/TickLimiter.h:83-86` and `src/Threading/TickLimiter.cpp:150, 206`:
  a mutex and condition variable for interruptible waits.
  - `TickLimiter.cpp:30-46` has a private `CpuRelax()` spin hint: `_mm_pause` / `__yield` / `yield`.
  - `TickLimiter.cpp:59, 187`: a Windows waitable timer. Leave it as it is.
- `FlingEngine/Core/src/Threading/ThreadManager.cpp:11-60`: a static registry with
  `std::mutex Mutex` and a `std::map`, locked by `lock_guard`.
- Other modules: no Graphics, Engine, Resources, Editor, or Sandbox code uses sync primitives.
- Tests: `FlingTests/Core/ThreadTests.cpp` and `TickLimiterTests.cpp`, tag `[Threading]`.
  They synchronize with `std::latch` and atomics, never sleeps.
- Warnings: `CMake/FlingTargets.cmake:21-37` (`fling_set_warnings`).
- CI: a "Linux Clang Release" job in `.github/workflows/build.yml`.

## Deliverables

### 1. `Core/inc/Threading/ThreadSafety.h`
Annotation macros. Use `__has_attribute` / `__clang__`, so the macros expand to nothing on GCC and MSVC:
- `FLING_CAPABILITY(x)`
- `FLING_SCOPED_CAPABILITY`
- `FLING_GUARDED_BY(x)`, `FLING_PT_GUARDED_BY(x)`
- `FLING_ACQUIRE(...)`, `FLING_ACQUIRE_SHARED(...)`
- `FLING_RELEASE(...)`, `FLING_RELEASE_SHARED(...)`, `FLING_RELEASE_GENERIC(...)`
- `FLING_TRY_ACQUIRE(...)`, `FLING_TRY_ACQUIRE_SHARED(...)`
- `FLING_REQUIRES(...)`, `FLING_REQUIRES_SHARED(...)`
- `FLING_EXCLUDES(...)`
- `FLING_ASSERT_CAPABILITY(x)`
- `FLING_RETURN_CAPABILITY(x)`
- `FLING_NO_THREAD_SAFETY_ANALYSIS`

### 2. `Core/inc/Threading/Mutex.h`
- All types are non-copyable and non-movable. Use `NonCopyable.hpp`, or delete the copy and move operations.
- All methods are short enough to be inline in the header.

| Type | Wraps | API |
| --- | --- | --- |
| `Mutex` (`FLING_CAPABILITY("mutex")`) | `std::mutex` | `Lock()` ACQUIRE, `Unlock()` RELEASE, `TryLock()` TRY_ACQUIRE(true) |
| `ScopedLock` (`FLING_SCOPED_CAPABILITY`) | holds `Mutex&` | ctor ACQUIRE, dtor RELEASE, `Unlock()` RELEASE (early unlock, for `Thread.cpp:75/125`), `Lock()` ACQUIRE (relock) |
| `SharedMutex` (`FLING_CAPABILITY("shared_mutex")`) | `std::shared_mutex` | `Lock/Unlock/TryLock` + `LockShared/UnlockShared/TryLockShared` |
| `ReadScopedLock` / `WriteScopedLock` | hold `SharedMutex&` | shared or exclusive scoped capability |
| `SpinLock` (`FLING_CAPABILITY("spinlock")`) | `std::atomic_flag` | `Lock` (test_and_set acquire; spin on `test()` relaxed + `CpuRelax()`), `Unlock` (clear release), `TryLock` |
| `ScopedSpinLock` | holds `SpinLock&` | scoped capability |

- Move `CpuRelax()` from `TickLimiter.cpp:30-46` into a public `Core/inc/Threading/CpuRelax.h`
  as `FORCEINLINE void CpuRelax()`, with `<immintrin.h>` / `<intrin.h>` behind the arch checks.
  `TickLimiter.cpp` then includes it. Keep the existing comment about why it must not yield.
- Document on `SpinLock` that it is for critical sections of a few instructions only, never
  around allocation, IO, or a wait.

### 3. `Core/inc/Threading/ConditionVariable.h`
- `ConditionVariable` wraps `std::condition_variable`.
- It waits on a `ScopedLock`. To get a `std::unique_lock<std::mutex>`, the wait reaches the
  `Mutex`'s inner `std::mutex` through a `friend`. Adopt the lock, wait, then `release()`, so
  ownership stays with `ScopedLock`. Do **not** use `std::condition_variable_any`.
- API:
  - `NotifyOne()`, `NotifyAll()`
  - `Wait(ScopedLock&)`
  - `Wait(ScopedLock&, Pred)`
  - `WaitFor(ScopedLock&, std::chrono::nanoseconds, Pred) -> bool`
  - `WaitUntil(ScopedLock&, time_point, Pred) -> bool`
- Mark the wait functions `FLING_NO_THREAD_SAFETY_ANALYSIS` inside, where needed. The caller still holds the lock.

### 4. `Core/inc/Threading/Semaphore.h` and `Event.h`
- `Semaphore` wraps `std::counting_semaphore<>`.
  - API: `explicit Semaphore(uint32 Initial = 0)`, `Release(uint32 Count = 1)`, `Acquire()`,
    `TryAcquire()`, and `TryAcquireFor(std::chrono::nanoseconds) -> bool`.
- `Event` uses `enum class EventReset { Auto, Manual }`. Build it on `Mutex`, `ConditionVariable`,
  and a `bool FLING_GUARDED_BY(m_Mutex)`.
  - API: `explicit Event(EventReset, bool InitiallySet = false)`, `Signal()`, `Reset()`, `Wait()`,
    `WaitFor(std::chrono::nanoseconds) -> bool`, and `IsSet()`.
  - Auto mode: a wait that succeeds clears the flag and lets exactly one waiter through.
    Manual mode: the flag stays set until `Reset`.
  - These can stay header-only, or go in `src/Threading/Event.cpp` if the header gets large.

### 5. Move Core's threading code onto the wrappers (no API changes)
- `ThreadQueue.h`: `mutable Mutex`, `ConditionVariable`, and `std::vector<T> m_Items FLING_GUARDED_BY(m_Mutex)`.
  `m_WakeRequested` is guarded the same way.
- `Thread.h` / `Thread.cpp`: the start mutex and condition, the join mutex, and the guarded members.
  The two early `Lock.unlock()` calls become `ScopedLock::Unlock()`.
- `TickLimiter.h` / `.cpp`: the mutex and condition variable. `m_Interrupted` and `m_MissedTicks` stay `std::atomic`.
- `ThreadManager.cpp`: the registry mutex, with the map `FLING_GUARDED_BY`.
- Keep the existing `#TODO` comments that point to `Plans/lockless-threading.md`.
- Afterwards, `grep -rn "std::mutex\|std::condition_variable\|lock_guard\|unique_lock" FlingEngine/`
  must match only the new wrapper headers.

### 6. Build flags
In `fling_set_warnings` (`CMake/FlingTargets.cmake`), when `CMAKE_CXX_COMPILER_ID MATCHES "Clang"`,
add `-Wthread-safety`.
- It only applies to first-party code, because externals are SYSTEM includes.
- Fix every warning it reports. Do not suppress them.
- Do not add `-Werror` for it unless the repo already uses `-Werror`.

### 7. Docs
- `Skills/threading.md`:
  - Add the new types to the **Types** table.
  - In **Queues**: the queue now uses `Fling::Mutex`, not "a mutex and a vector swap".
  - New rule: first-party code uses the `Fling::` sync types, not raw std types. Guard shared
    members with `FLING_GUARDED_BY`. Use `SpinLock` only for a few instructions.
- `Skills/architecture.md` Core bullet: mention the sync types.
- `Plans/lockless-threading.md`: note that `ThreadQueue` and `ThreadManager` now use the wrappers.
  The lock-free plan replaces the wrapper use inside them, not the wrappers.

## Coding rules (repo-specific)
- Style:
  - Run `clang-format` on every changed file.
  - Doc comments are `/** ... */`, with no `@brief` and no `/*!`.
  - Run `python3 scripts/check_comment_style.py`.
  - Comment wording follows ASD-STE100 Simplified Technical English (see `Skills/coding-style.md`).
  - Project `"..."` includes come first, then `<...>` includes, each group alphabetical.
- Names: PascalCase types and methods, `m_` members, `t_` parameters (see `ThreadQueue.h`).
  Use `uint32` and the other types from `FlingTypes.h`.
- No mention of other engines in comments ("like Unreal's X").
- Never edit `external/`.
- Work on a feature branch off `main`. Do not commit or push unless asked.

## Tests: `FlingTests/Core/SyncTests.cpp`, tag `[Threading]`
The CMake glob picks the file up. Synchronize with `std::latch` and atomics, never with sleeps.
- `Mutex`: 8 threads × 100k increments of a guarded counter give an exact total.
  `TryLock` fails while another thread holds the lock.
- `ScopedLock`: early `Unlock`, then a relock.
- `SharedMutex`: two readers hold shared locks at the same time; check with a latch inside the
  read section. A writer excludes the readers.
- `SpinLock`: the same counter test under contention. `TryLock`.
- `ConditionVariable`: the predicate wait wakes on notify. `WaitFor` returns false on timeout,
  for example after 1 ms with no notify.
- `Semaphore`: the initial count. `TryAcquire` when the count is 0. `Release(3)` lets exactly 3 acquires through.
- `Event`: Auto mode lets one of two waiters through for each `Signal`. Manual mode lets every
  waiter through until `Reset`. `WaitFor` times out. With `InitiallySet`, the first wait does not block.
- The existing `ThreadTests.cpp` and `TickLimiterTests.cpp` must still pass unchanged.

## Verification
1. `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel`, then `ctest --test-dir build`. All tests pass.
2. A Clang build (`-DCMAKE_CXX_COMPILER=clang++`) with `-Wthread-safety` compiles with no thread-safety warnings.
3. Negative check, local only, do not commit: remove one lock around a `FLING_GUARDED_BY` access
   and confirm Clang warns. Then put the lock back.
4. ThreadSanitizer: configure a separate tree with `-fsanitize=thread`. Run
   `./build-tsan/FlingTests/bin/CoreTests "[Threading]"`. Use `setarch -R` if TSan reports
   "unexpected memory mapping". There must be no reports.
5. `./build/Sandbox --null-render --unattended --frames 120 --seed 1` exits 0.
6. `python3 scripts/check_comment_style.py` passes.
7. The grep from deliverable 5 is clean.
