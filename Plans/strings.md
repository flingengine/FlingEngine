# Strings: remaining work

The string types, assets, and localization are built. Their rules are in
[`Skills/strings.md`](../Skills/strings.md). This file lists only the work that is
not done, and the reasons for the design.

## Not done

- **Text rendering for right-to-left and complex scripts.** This work needs:
  - The Unicode bidirectional algorithm (UAX #9) to reorder each line.
  - Shaping (for example HarfBuzz) to join Arabic letters.
  - Mirrored layout.
  - Cursor movement and selection in text fields.

  These belong to a text renderer and a game UI module. They do not exist yet.
  ImGui cannot do this work.
- **Fonts for other scripts.** The UI font does not have some scripts (Arabic,
  Hebrew, CJK, Thai, Devanagari). The engine needs font fallback for them. This
  work is also part of the text renderer.
- **More number formats.** Indian digit grouping (12,34,567) and native digits
  (Arabic-Indic, Devanagari). `NumberFormat` now uses groups of three and Latin
  digits.
- **Dates with month names.** `FormatDate` writes numeric dates only. Month names
  and day names need a table for each language.
- **`TypeHash` uses the `Name` hash.** The ECS code (`Ecs/TypeHash.h`, FNV-1a) is
  on its own branch. When that branch reaches `main`, `TypeHash` becomes the hash
  of a `Name`, and `HashName` is deleted. Then the engine has one hash function.
  Saved data that holds a `TypeHash` must change with it.
- **Input actions and contexts as `Name`.** Input has no actions or contexts yet
  (see [`improve-input.md`](improve-input.md)). When they exist, they are `Name`.
- **Gathering with macros.** `--loc-gather` finds only `DisplayText::FromTable`
  calls that have literal arguments. A macro for game code (for example
  `FLING_TEXT(Table, Key, "Source")`) gives the call a fixed form.

## Decisions

- **`Name` is a stable hash. It is not an index into a name table.** An interned
  index is 4 bytes and has no collisions. But it depends on the order in which the
  engine adds text, so it is different in different processes and on different
  machines. Every save, snapshot, checksum, and network message needs a
  translation for it. Also, the engine cannot make the index at compile time. A
  64-bit hash with a reverse table keeps the text and has none of these problems.
  A 32-bit hash is not enough: at 100,000 names, the chance of a collision is
  about 69%.
- **xxHash64.** The cost to compare names is the same with any 64-bit hash.
  xxHash64 passes SMHasher (FNV-1a does not). It hashes runtime text about eight
  times faster than FNV-1a. It has a fixed specification and can be `constexpr`.
  XXH3 is faster for short text, but its specification and code are much larger.
- **No switch to drop name text in shipping builds.** The plan was to save the
  memory of the reverse table. But these parts need the text:
  - `Localization` finds a language folder from the text of its code.
  - CSV export and sorting need key text.
  - A level save writes names as text.

  Without the text, these parts do not work. If each system keeps its own copy of
  the text, the copies use most of the memory that the switch saves.
- **`String` is `std::string`. It is not an engine class.** Every library that the
  engine uses takes `std::string`, and it has the small string optimization.
- **UTF-8 in `char`, not `char8_t`.** No library that the engine uses takes
  `char8_t`. The engine has no code-page text to keep apart from UTF-8.
- **Case-sensitive names.** A case-insensitive `Name` needs the hash to fold case.
  This puts locale rules into saved ids.
