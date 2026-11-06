# Lock-free thread queues and thread registry

Status: not started. The first version of the thread layer
([`Skills/threading.md`](../Skills/threading.md)) uses a mutex in two places.
This plan replaces both with lock-free code. The public API does not change.

## 1. `ThreadQueue<T>`: lock-free MPSC queue

Today: `Push` locks a mutex and appends to a `std::vector`. `Drain` swaps the
vector out under the lock. `WaitForItems` uses a condition variable.
(`FlingEngine/Core/inc/Threading/ThreadQueue.h`)

Target:

- An intrusive multi-producer, single-consumer queue with nodes (Dmitry
  Vyukov's MPSC node queue). `Push` is one atomic exchange. No producer waits for
  another producer or for the consumer.
- Nodes come from a pool (a free list per queue), so `Push` does not call
  `malloc` in the normal case. The consumer returns nodes to the pool after
  `Drain`.
- `WaitForItems` and `Wake` use a C++20 `std::atomic::wait` / `notify_one` on a
  counter instead of a condition variable. A producer notifies only when the
  consumer is waiting (a "sleeping" flag), so a push with no waiter has no
  system call.
- The order stays the same as today: one producer's items come out in push
  order, and an item pushed during `Drain` goes to the next `Drain`.

Keep `Push`, `Drain`, `WaitForItems`, `Wake`, and `IsEmpty` with the same
meaning. `CommandQueue` and `CommandRunnable` then need no change.

## 2. `ThreadManager`: lock-free registry

Today: a mutex and a `std::map<ThreadId, ThreadInfo>`.
(`FlingEngine/Core/src/Threading/ThreadManager.cpp`)

The main reason to change it is the crash handler. It must be able to print the
name of the thread that crashed, and a signal handler must not lock a mutex or
allocate. The profiler also reads names often.

Target:

- A fixed table of slots (for example 256). Each slot has an atomic `ThreadId`,
  the OS ID, and a fixed `char Name[64]` (the full engine name is kept in
  `Thread`).
- `Register` claims a free slot with a compare-and-swap, writes the name, then
  publishes the ID with a release store. `Unregister` clears the ID.
- `GetThreadName` and `GetThreads` scan the table with acquire loads. They do not
  lock and do not allocate, so a crash handler can call a variant that writes
  into a caller buffer.
- When the table is full, `Register` returns false and the thread runs with no
  registry entry. Log that one time.

## Tests

- Keep every `[Threading]` case in `FlingTests/Core/ThreadTests.cpp`.
- Add a stress case: 8 producers × 100 000 items, with every item seen one time
  and in producer order.
- Run the ThreadSanitizer build (see `Skills/threading.md`) on the new code
  before the merge.
- Add a benchmark of `Push` with 1, 4, and 8 producers, before and after the change, so that the change is
  measured and not assumed.

## When to do it

Do it when one of these is true:

- A profile shows lock contention on a `ThreadQueue` (for example, many job
  workers push to one queue).
- The crash handler prints thread names (part 2 is then required).
- The job system for parallel ECS stages starts (`Plans/ecs-plan.md`). Its
  worker queues need this.
