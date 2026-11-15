# Skills

Topic-specific guidance for AI coding assistants working in Fling Engine. Start at
[`AGENTS.md`](../AGENTS.md) in the repo root: it links into the files here for
detail on a given topic. These files are meant to be read on demand, not all at once.

| File | Covers |
|------|--------|
| [`building.md`](building.md) | Init scripts, build modes, CMake flags, per-platform setup |
| [`testing.md`](testing.md) | Per-module Catch2 executables, adding tests, isolation tests |
| [`coding-style.md`](coding-style.md) | Doc-comment conventions, formatting, what not to touch |
| [`architecture.md`](architecture.md) | Current folder/module layout, where new code belongs |
| [`input-playback.md`](input-playback.md) | Record and replay input (`--record-input`, `--replay-input`), the `.input.jsonl` format |
| [`adding-modules.md`](adding-modules.md) | How to add a new engine module (`fling_add_module`, UI example) |
| [`strings.md`](strings.md) | `Name`, `String`/`StringView`, `DisplayText`, assets, localization (string tables, plurals, translator CSV, right-to-left), strings in simulation code |
| [`ecs.md`](ecs.md) | ECS reference: storage, components (domain, rollback class), systems, hooks, groups, tools |
| [`threading.md`](threading.md) | The Core thread layer: threads, queues, sync types, fixed-rate threads, lane tags, rules with the ECS |
| [`../Docs/architecture-overview.md`](../Docs/architecture-overview.md) | Start here: the whole model on a few pages (declarations, the two roots and threads, the life of a tick and a frame, what crosses, exec modes, determinism rules) |
| [`../Docs/system-order-api.md`](../Docs/system-order-api.md) | System order reference: groups (`.In`), `.After`/`.Before` with `SystemOf<T>`, order headers, the run order rules, every order error, the schedule dump, schedule tests |
| [`../Docs/input-two-sided.md`](../Docs/input-two-sided.md) | One input stream read by the fixed tick (`InputState`, its cut) and the frame (`PendingInput`, late poll); `InputSlice`, the stateless frame side, worked look camera example |
| [`../Docs/time-and-ticks.md`](../Docs/time-and-ticks.md) | The clock, tick windows, hitch and pause, `TickStamp`, `InputCut`, the input rule (live and replay), the ledger and state hash, rollback classes, `--dump-ticks`, rollback hooks |
| [`../Docs/threading-model.md`](../Docs/threading-model.md) | Game thread and main thread: the published set, `TickPacket`, the mirror, `TickDriver` and `PresentationLink`, Play and Stop, lane checks, shared state, determinism tests |
| [`../Docs/update-order-design-notes.md`](../Docs/update-order-design-notes.md) | Why it is built this way: rejected alternatives, what broke, the reviews, costs, out of scope, and the mapping to a large engine with a dynamic per-object tick model |
