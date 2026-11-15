# Update order, time, input and threads: design notes for a port

This page is the how-we-did-it record of the experiment. It is for someone who
wants to build the same ideas in another engine, in particular a large engine
with a dynamic per-object tick model. It is the one page that tells history:
the other pages describe only what the code is now.

The references are [`architecture-overview.md`](architecture-overview.md)
(start there), [`system-order-api.md`](system-order-api.md),
[`time-and-ticks.md`](time-and-ticks.md),
[`threading-model.md`](threading-model.md) and
[`input-two-sided.md`](input-two-sided.md).

The experiment ran in steps, each with working notes that are not part of this
repository: the order API (`NOTES-phase1.md`,
`NOTES-phase2.md`), two-sided input (`NOTES-input.md`, `NOTES-followup.md`),
time (`NOTES-time.md`), the threads (`DESIGN-threads.md`, `NOTES-mirror.md`,
`NOTES-threads.md`, `NOTES-merge.md`), three reviews (`REVIEW-order.md`,
`REVIEW-time-input-publish.md`, `REVIEW-threads.md`) with their fixes
(`NOTES-review-order-fixes.md`, `NOTES-cleanup-runtime.md`,
`NOTES-cleanup-types.md`), and an audit (`AUDIT-overwatch-rollback.md`).
Numbers below come from those notes. Old names are marked "(old)".

## 1. The problem

Before the experiment, Fling ordered systems with string names and flat phases:

```cpp
// before
.Name = "Sandbox.Mover", .Phase = Fling::Phases::Simulation, .After = "Sandbox.Rotator"};
// after, with #include "SandboxSystemsFwd.h"
.Name = "Sandbox.Mover", .In = Fling::Order::Simulation, .After = Fling::SystemOf<RotatorSystem>};
```

| Weak point | Effect |
| --- | --- |
| Order names are strings | A typo or a rename is found when the schedule is built, not when the code compiles. Rename tools do not see them. |
| One name space for systems and phases | `"Engine.Input"` was both a system and a phase. Every lookup had to guess which kind it had. |
| Phases are flat | A game could not put work inside `Physics`, before the physics step. |
| A system order names one system | A game had to know an engine system by its string. A later split of that system broke the game. |
| Two graphs | Phase order and system order were separate structures, joined by a quadratic loop in the conflict check. |

The time and thread work had the same kind of problem: a tick had five
half-overlapping identities, the input cut lived in seven places with three
names, and "first tick that runs takes everything" made per-tick input depend
on the frame rate.

## 2. Design goals

1. A game runs work before or after an engine system with no change to engine code.
2. A wrong name fails at compile time.
3. No system header includes another system's header.
4. The order is the same on every run and in every build.
5. The order between two systems that share data is always stated, or follows
   from declared access. It is never an accident.
6. A tick's result does not depend on the frame rate, the thread split or the
   timing of the threads.
7. The frame side can show input before a tick consumes it, with no loss and no
   double count.
8. Room for rollback: every tick has one identity, and the state that a rollback
   restores is a checked set.

Not goals: a parallel executor, networking, macros.

## 3. The shape, in one paragraph

A **group** is a named span with a begin and an end node. Groups nest under two
roots: `Order::Tick` (fixed, the game thread) and `Order::Frame` (variable, the
main thread). A system names its group with `.In`, orders itself with typed
references (a group, or `SystemOf<T>` with only a forward declaration), and
declares its access. The builder puts every group and system of one rate in
**one graph**, adds **data edges** only between pairs that the graph leaves
unordered, makes two unordered writers an error, and breaks ties by **name**.
The frame reads of Simulation types are the **published set**. Each tick has
one **`TickStamp`**, which goes into the ledger, the published packet and the
frame. Input is **one stream** with a fixed reader that consumes and a frame
reader that does not; both run **one pure function**. The **simulation steers
the frame side only through data** in a published component. What a tick sends
out is **stamped with the tick**.

## 4. Alternatives that were rejected

| Alternative | Why not |
| --- | --- |
| A closed `enum class Stage` | A game cannot add a stage to the engine's enum. |
| String names | Found late; no help from rename tools; a shared name space for two kinds of thing. |
| Priority numbers | Two modules cannot agree on numbers; a number says nothing about why. |
| Registration order as the order or the tie-break | Depends on link order and `Add` order. |
| Include the other system's header to name it | Every order edge becomes an include edge. |
| Systems only, no groups | A game must name each engine system; a split breaks every caller. |
| Phases and system order as two graphs | Two reachability tests and a quadratic join. One graph replaced both. |
| Position implies resimulation membership | Implicit. Membership is a field (`Resim`) and is checked. |
| A per-system opt-out of resimulation (`ReplayOnResim`, old) | A resimulation must be "the same systems in the same order". Side effects need a dedupe by tick, not a schedule flag. |
| Per-type double-buffered columns in the simulation storage | A frame query joins a published type and a Presentation type on one row, so both must be in one storage: the mirror. |
| Deltas in the packet | Latest-wins drops packets, and a dropped structural delta is lost. A full state reconciles after any number of drops. |
| A summed mouse rule | Frame-rate dependent (section 7). |

## 5. What the experiment proved

| Measure | Before | Order API | Input | Time | Threads | After reviews |
| --- | --- | --- | --- | --- | --- | --- |
| EngineTests cases | 137 | 150 | 167 | 175 | 203 | 207 (runtime branch) |
| ECS compile-fail checks | 20 | 24 | 29 | 29 | 29 | 30 (types branch) |
| Full ctest | not run | 408 | 437 | 453 | 502 | 507 (runtime), 504 (types) |

- **Typed references work in `constexpr` aggregates on MSVC**, as one item and
  as mixed braced lists. `OrderList` needs one constructor per single-item form,
  because C++ does not chain two user conversions.
- **`SystemOf<T>` works with a forward declaration only**, also inside an
  `inline constexpr GroupInfo` before the class exists.
- **One graph replaced the phase graph.** The 26 original schedule tests passed
  on it with renames only.
- **A game group inside an engine group, before an engine system, needs one
  edge and no engine change** (`FlingTests/Physics/PhysicsOrderTests.cpp`).
- **Wrong names fail to compile** (`OrderGroupTypo`, `OrderSystemTypo`).
- **The schedule is reviewable as a file**
  (`FlingTests/Physics/Schedules/EnginePhysics.schedule.txt`).
- **The dump finds real problems**: `Sandbox.AppKeys` was in the resimulation
  set (now the schedule error `ResimAppendsOutput`), and `InputState` is the one
  thing a rollback must restore for the Sandbox.
- **The order API was enough for input and threads.** The frame-side look
  system needed no `.After`. The thread split needed no schedule change: the
  root of a group is its thread, and the published set falls out of the access
  lists.
- **The ledger is the same in every exec mode and at any frame rate**:
  `--dump-ticks` files are byte-identical for `direct`, `inline`, `threaded`, 4
  render rates and 10 jitter seeds; the headless checksum is 44871.195499957 in
  every mode. (It was 45211.912278667 until the merge of the input thread work,
  which also moved the Sandbox setup draws to `SimRandom`; the notes give no
  other cause.)

## 6. Claims of the original order design that turned out wrong

| Claim | What happened | Final state |
| --- | --- | --- |
| The builder shrinks by about a sixth (`Schedule.cpp` 1204 lines) | It grew to 2021 lines (one 1535-line class with 30 members) before the order review. | Split into `Schedule.cpp` (530) and five files (`ScheduleGroups`, `ScheduleGraph`, `ScheduleOrders`, `ScheduleAccess`, `ScheduleDump`); the largest class has 6 members. The gain is in the shape, not in size. |
| A missing group is a compile error, so the "missing" error kind goes | A hand-built `SystemDesc` skips `CheckSystem`. `RateOf` on an unset reference hid the readable assert. | `MissingGroup` stays. `CheckSystem` guards `RateOf`. |
| `CheckSystem` changes in one place | Six more rate checks in `SystemContext.h`. | One helper, `Detail::RunsAtRate`, in all seven. |
| Only `Tick` and `Frame` are roots, by convention | A module root ordered inside the tick would be skipped by a resimulation. | A root is a group with no `In` and the name `TickRootName` or `FrameRootName`; anything else is `MissingParent`. The `Root` field and its enum (old) were deleted by the review. |
| A system id is the hash of `TypeNameOf<T>()` | MSVC prints `class` or `struct` inside template arguments as each file declared it: two hashes. | `Detail::SystemTypeHash` drops the keys everywhere. Two classes with one name in two anonymous namespaces are `AmbiguousSystemType`. |
| Field order of `GroupInfo` is free | MSVC needs designators in declaration order (C7560). | The order is API: `Name, In, After, Before, FlushAfter, Resim`. |
| Group edges join `ScheduleEdge`, so the dump shows them | `ScheduleEdge` indexes systems; group edges join begin and end nodes. | The dump shows a group tree. |
| The reference holds the group, so the name lookup goes | Shared libraries make addresses unsafe. | Name lookup with a structural compare. |
| `FlushAfter` only on a direct child of `Tick` | Not needed. | No restriction. |
| A redundant `Resim` inside a resimulated group is harmless | Two rules in two places (compile time allowed it, the text said no). | Strict: only a direct child of `Order::Tick`; `ResimPlacement` elsewhere. |
| "Resim reads from outside" is dump text | It is the most important output for a rollback port. | `Schedule::ResimInputs`, a field that tests assert on. |
| A string fix-it can be pasted | With strings gone there was nothing to paste. | `SystemDesc::TypeName`; the fix prints `SystemOf<Sandbox::LookCameraSystem>`. |

**The behaviour change.** The old builder skipped a data edge only for a pair
ordered directly. The one-graph builder skips it for any pair with a path
between them. `A.Before = B, B.Before = C`, C writes X, A reads X: old, a data
edge C to A and a cycle error; new, run order A, B, C and A's read is one tick
late (test "An order through a third system replaces the inferred edge").
Transitive order counts; nesting is not order.

## 7. What the later work found

### The per-sample input rule

Applying the sum of a tick's mouse counts once makes the result depend on how
input is split into ticks. Pitch 1.4, clamp 1.5, up 30 then down 30: one tick
gives 1.4, two ticks give 1.5 then 1.2. With the real loop (poll once per
frame), 25 moves over 100 ms from pitch 1.2: summed, **1.1001 at 30 fps,
0.9885 at 60 and 240 fps, 0.9420 at 144 fps**; per sample, **0.942000 at every
rate, bit-identical**. Yaw, with no clamp, still differed in the last bits
under the sum. The rule is now per sample on both sides
(`Look(Look(v, a), b) == Look(v, a + b)`), and the summed mode was deleted as a
known-wrong option. Key edges take effect at their place in the stream; that
removed a measured snap of 0.12 rad on a freeze.

### Input by time, and the live input rule

"The first tick that runs takes everything" made per-tick input depend on the
frame rate. Now a tick takes the messages before the end of its window, or the
recorded cut in a replay. Cost: up to one tick of latency for input polled
after the last due window (the frame still shows it). Recorded cuts are stored
as counts of file messages, because sequences include device messages that the
file does not have.

With threads, the main thread is the only input writer, and the game thread
cuts. A live tick could wait for the writer's watermark to pass its window end;
that would make live cuts equal to `direct`, but the watermark moves once per
poll, so the wait costs up to a frame of latency per tick, and it still cannot
remove the poll-time stamps. **The trade that was chosen**: headless and replay
wait with no limit (exact); live windowed play does not wait, takes what is
polled, and records its cuts, so it is reproducible from its recording only. A
test proves that a replay of the live cuts gives the same ledger in every mode.

### The duplicate lane concept

The order review found that the schedule had a `Lane` type that was
`UpdateRate` under a second name (a 1:1 map, a copied per-system array), and a
"command channel" from the frame into the simulation that the compile rules
made impossible: no real system could reach it. Both were deleted, with three
error kinds that only hand-made descriptions could raise. One check
(`UpdateRateAccess`) stayed for hand-made descriptions, because the published
set is complete only when the rate rules hold. Lesson: add a lane type only
when one rate runs on more than one thread. (`Core/inc/Threading/Lane.h` is a
different thing, the debug thread tag; its name now reads like the old schedule
lanes.)

### The first threaded determinism tests were true by construction

The first threaded tests polled input on the game thread. The frame thread then
had no path into the simulation, so equal ledgers across modes, render rates
and jitter could not fail. The harness also had mutexes that ordered the two
threads on every tick and hid races. The threads review found this. The real
test: the main thread writes the input in every mode, also headless and in the
harness, and the game thread cuts the queue by the timeline; the harness log
has one slot per tick with no mutex; a free game thread runs 30 000 ticks (and
5 000 with 300 µs jitter on both threads) against `direct`; a live-rule run is
replayed from its cuts in every mode. MSVC has no ThreadSanitizer. One
ThreadSanitizer run was made on an earlier revision, on Linux with clang, and
it was not run again on the final code (`threading-model.md`, section 9).

### The three reviews and the audit

| Review | Findings | What it changed |
| --- | --- | --- |
| Order (`REVIEW-order.md`) | 6 High, 17 Medium, 18 Low | Deleted the lane machinery and command channels; system id collisions found (`AmbiguousSystemType`); the strict `Resim` rule; `Schedule::ResimInputs` and `Schedule::Published` as fields; groups as `ScheduleGroup`; `GroupInfo::Root` deleted (`MissingParent`); the unknown-name kind split into `OrderTargetNotAdded` and `EmptyOrderRef`; `SystemRegistry::Order` renamed `RunBefore`; the builder split into units. |
| Time, input, publish (`REVIEW-time-input-publish.md`) | 11 High, 29 Medium, 10 Low | One `InputCut`, one `InputSlice` for both sides, one `TickStamp`, one `TickInputProvider` rule, no wall time in anything a fixed system reads, `TickHistory`, the hash set equal to the restore set, the summed mouse rule and experiment flags deleted, `LookCamera = {View, Policy}`. Skipped: the cut file format v2, the late-poll playback guess, a streaming hash. |
| Threads (`REVIEW-threads.md`) | 10 High, 18 Medium, 10 Low | One wake (`World::RunWake`) and one frame path for every mode; the two policy objects (`TickDriver`, `PresentationLink`); the tick-stamped outbox and clock commands; the legacy per-frame game update deleted; one play-start order; shutdown joins first; the live input rule; the main thread writes input in every mode. Mode branches went from 38 to 14. Skipped: untagged threads as violations, Edit tick numbering. |
| Audit against the Overwatch rules (`AUDIT-overwatch-rollback.md`) | 22 rules, 9 "must fix now" | Done: rollback classes (with `Subsystem`), hash set fixed, explicit classes on engine and game singletons; the legacy update deleted; `ResimSkipsState`, with Finalize run once after the resimulation and never read by it; `TickStamp` and `TickHistory`. `Sandbox.AppKeys` in `Order::Input` (`ResimAppendsOutput`). Not done: per-player input, one physics step per tick, logs in fixed systems. |

### What it costs

| Cost | Size |
| --- | --- |
| Order headers | One per module, groups and forward declarations only. |
| Test ceremony | A hand-built system that an order names needs a tag type. |
| State hash | About 0.7 ms per tick in a Debug Sandbox (754 transforms); off in a windowed game. |
| Publish | 1 000 entities: 64 KB per packet, fill 2.2 µs, apply 12.4 µs (optimized, debug runtime). 10 000: 640 KB, 14.3 µs and 126 µs. |
| Reachability | One walk per system, one bit per node per system. Fine for hundreds. |

## 8. Deliberately out of scope

| Left out | Why, and where it would go |
| --- | --- |
| Player and device mapping | `InputState` is one world input. Needs a per-player command per tick (section 10, input). |
| Physics rollback | `PhysicsWorld` is `Subsystem`; `Physics::SaveState` and `RestoreState` exist but are not registered as a rollback hook. The physics step runs one step of the tick's fixed delta, with no clock of its own. |
| State snapshots and a resimulation runner | The schedule and the class rules are ready; nothing saves or runs. |
| Network ids | Entity handles are local. A shared id from (tick, creator, counter) is the audit's proposal. |
| Remote input, ack, sliding windows, time sync, time dilation | No network. |
| Predicted against authoritative compare | The state hash is for same-build desync detection only. |
| Correction smoothing, extrapolation, teleport flags | Interpolation between the two published ticks is built (`ReadBlend<T>`, `threading-model.md` "The two blend states"); a visible correction after a resimulation needs an epoch in the packet header first. |
| Frame-to-simulation commands through the input stream | Designed (`DESIGN-threads.md` D7), not built. |
| A parallel executor, change tracking in the packet, an editor inspection packet, a render thread off main | Later steps of the thread design. |
| Cross-platform float determinism | One build on one platform only. Needed only for peer-to-peer input-only lockstep. |

## 9. Source honesty

The audit checked the prototype against the rules of the GDC 2017 talk on the
Overwatch gameplay architecture and netcode. **No primary source could be
reached**: the GDC Vault page is for members only, no English transcript or
slides were found, and transcript services refused. The rules were confirmed
only from secondary sources: a Chinese translation of the full talk, read
through a fetch-and-summarize tool (so the wording is a summary of a
translation), and a third-party write-up for some rules. The audit counts 0
rules as primary, 17 as secondary and 5 as unverified. These remain unverified,
as the audit records them:

- Which system turns the input singleton into the networked command (the
  translation names a command system but does not say that it reads the
  singleton).
- The sizes of the client's input and state ring buffers (about 2 s, 1 to 2 s).
- That misprediction detection is "the snapshot for frame N differs from the
  recorded prediction for N".
- Clamping the server's hit-registration rewind, and extrapolation past the clamp.
- How presentation and effects behave during a rollback replay (an inference;
  the talk gives no explicit presentation-domain rule).
- Position quantization (1/1024 m) on both sides.

Treat any statement in these pages that cites the talk as "from secondary
sources".

## 10. Mapping to a large engine with a dynamic per-object tick model

Fling is small: systems are stateless classes, there is one instance per system
class, the whole schedule is known at start-up, and access is declared and
complete. A large engine with a dynamic per-object tick model has objects with
member state that register tick functions at run time, order between objects
set by run-time links, several separate orderers, and access that is not
declared. For each element: what it depends on here, what the equivalent would
have to be there, and what is likely the hardest part.

### Compile-time order references (`SystemOf<T>`, `inline constexpr` groups)

- **Depends on:** one node per system class; a static schedule built once; the
  class name as identity.
- **Equivalent:** references to kinds of work (a component or tick-function
  class, a named group) checked at compile time, with instances resolved inside
  the node at run time. A reference to an object instance stays a run-time link,
  but its kind is still a typed group.
- **Hardest:** per-object order (a parent before its child, an attachment
  before its owner) is a different relation from per-kind order. It must stay a
  local rule inside a node, or the graph grows with the object count.

### Groups (open, nestable, begin and end nodes, resimulation on groups)

- **Depends on:** every piece of per-tick work being a system in one graph.
- **Equivalent:** each existing orderer (per-object tick groups, a physics
  step, an animation pass, a task graph) gets a place in one graph: a group it
  runs in, with edges at its boundaries. Resimulation membership is a group
  property there too, with the contiguity check.
- **Hardest:** several orderers that meet only at fixed points cannot check a
  cycle or a conflict across each other until they share one graph. Moving
  them in is the main cost, and the main gain.

### Declared access (data edges, conflicts, the published set)

- **Depends on:** a complete, compile-time access list per system, and
  stateless systems.
- **Equivalent:** declared access per tick function or per component class for
  new work, with runtime recording of reads in a debug pass to find the rest.
  Data edges and the conflict check then hold only for declared work.
- **Hardest:** undeclared legacy access. The "never an accident" goal holds only
  where access is declared; everywhere else the order must be stated by hand.

### Two roots, where the root is the thread

- **Depends on:** the compile rules that a fixed system cannot touch
  Presentation data and a frame system cannot write Simulation data.
- **Equivalent:** a domain tag per property or per component class, checked by
  runtime asserts, and the rule "the tick group decides the thread".
- **Hardest:** code with member state that does both simulation and
  presentation in one object. It must be split, with its presentation state in a
  separate proxy object; it cannot be moved by lane as it is.

### The published set

- **Depends on:** the set derived from declared frame reads; full-state copies
  that are cheap; one entity handle space shared by both storages.
- **Equivalent:** explicit registration of published state per component class
  (a proxy or state-stream producer per class), or a debug pass that records
  frame reads. Object graphs with pointers need an extract step per object.
- **Hardest:** completeness. Without a full read list, a missed field shows as
  stale data on screen, not as an error.

### The tick stamp and the ledger

- **Depends on:** a single tick counter that only goes up; a state hash over
  reflected component data (0.7 ms per tick at Fling's size).
- **Equivalent:** one tick key on everything that names a tick (input, packets,
  events, histories); the hash over the replicated or restore set only, by an
  archetype or class walk, sampled when it is too costly.
- **Hardest:** making the restore set exact: subsystem state (physics),
  allocator state, and state in objects that is not reflected.

### Two-sided input with one pure function

- **Depends on:** one input stream with sequences; a rule that is a pure
  function of input and published data; per-sample folding.
- **Equivalent:** a sequenced, timestamped input stream with per-reader
  cursors; the frame-side prediction runs the same function as the tick,
  stateless, from the published state plus the input after the published cut.
- **Hardest:** finding gameplay rules that are pure functions of input. Most
  are not; those reach the frame only through the published state, one tick late.

### Policy through data for simulation control of presentation

- **Depends on:** the policy being plain data in a published component, and the
  non-input writer ordered after the input fold.
- **Equivalent:** the same rule, enforced by review: presentation code reads
  policy, never decides it; a policy change that is not from input is written
  after the input fold, or the frame snaps.
- **Hardest:** copying the extra state a rule needs ("not while in a vehicle")
  into the published data, for every rule that the frame side predicts.

### Tick-stamped events

- **Depends on:** one outbox drained on the main thread, and ticks that only go
  up.
- **Equivalent:** every side effect leaves the simulation through a queue
  stamped with the tick (and later an epoch), with one consumer that drops ticks
  that are not newer than the last it sent; clock commands stamped with the tick
  they go before.
- **Hardest:** side effects made directly from object code (sounds, effects,
  logs). Each needs to become an event, or a resimulation repeats it.

## 11. Open problems

- No runner executes a resimulation; no state snapshots.
- Side effects in resimulated groups: logs in fixed systems.
- Two appenders to one queue push in name order; no rule checks it.
- An order against your own ancestor is a `Cycle` with no specific message.
- Hand-built `SystemDesc`s skip `CheckSystem`.
- The checked-in dump covers Engine and Physics only.
- Untagged threads pass the lane checks; some read entry points have no check.
- Edit ticks share the play tick numbers.
- A recorder start or stop racing a tick decides only whether that tick's cut is
  in the file.
- The `--dump-ticks` `cut` column is a raw queue sequence.
