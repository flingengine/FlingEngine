# Skills

Topic-specific guidance for AI coding assistants working in Fling Engine. Start at
[`AGENTS.md`](../AGENTS.md) in the repo root — it links into the files here for
detail on a given topic. These files are meant to be read on demand, not all at once.

| File | Covers |
|------|--------|
| [`building.md`](building.md) | Init scripts, build modes, CMake flags, per-platform setup |
| [`testing.md`](testing.md) | Running FlingTests, adding new Catch2 tests |
| [`coding-style.md`](coding-style.md) | Doc-comment conventions, formatting, what not to touch |
| [`architecture.md`](architecture.md) | Current folder/module layout, where new code belongs |
| [`adding-modules.md`](adding-modules.md) | How to add a new engine module (`fling_add_module`, UI example) |

These are supplementary to, not a replacement for, the canonical docs they
reference (`docs/CodingStyle.md`, `docs/BuildModules.md`) — when in doubt, the
canonical doc wins.
