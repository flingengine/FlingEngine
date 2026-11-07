# Strings: remaining work

The string types, assets, and localization are built. Their rules are in
[`Skills/strings.md`](../Skills/strings.md). This file keeps only the work that is
not done, and the decisions behind the design.

## Not done

- **Text rendering for right-to-left and complex scripts.** The Unicode
  bidirectional algorithm (UAX #9) to reorder each line, shaping (for example
  HarfBuzz) to join Arabic letters, mirrored layout, and cursor movement and
  selection in text fields. These belong to a text renderer and a game UI
  module, which do not exist yet. ImGui cannot do them.
- **Fonts for other scripts.** Font fallback for scripts that the UI font does
  not have (Arabic, Hebrew, CJK, Thai, Devanagari). Also part of the text
  renderer.
- **More number formats.** Indian digit grouping (12,34,567), and native digits
  (Arabic-Indic, Devanagari). `NumberFormat` uses groups of three and Latin
  digits.
- **Dates with month names.** `FormatDate` writes numeric dates only. Month and
  day names need a table per language.
- **`TypeHash` uses the `Name` hash.** The ECS code (`Ecs/TypeHash.h`, FNV-1a) is
  on its own branch. When it reaches `main`, `TypeHash` becomes the hash of a
  `Name` and `HashName` is deleted, so there is one hash function. Saved data
  that holds a `TypeHash` must move with it.
- **Input actions and contexts as `Name`.** Input has no actions or contexts yet
  (see [`improve-input.md`](improve-input.md)). When they exist, they are `Name`.
- **Gathering with macros.** `--loc-gather` finds `DisplayText::FromTable` calls
  with literal arguments only. A macro for game code (for example
  `FLING_TEXT(Table, Key, "Source")`) would make the form fixed.

## Decisions

- **`Name` is a stable hash, not an index into a name table.** An interned index
  is 4 bytes and has no collisions, but it depends on the order in which text
  was added, so it differs between processes and machines. Every save, snapshot,
  checksum, and network message would need a translation, and a `Name` could
  not be made at compile time. A 64-bit hash with a reverse table keeps the text
  and removes those problems. 32 bits is not enough: about a 69% chance of a
  collision at 100,000 names.
- **xxHash64.** Comparing names costs the same with any 64-bit hash. xxHash64
  passes SMHasher (FNV-1a does not), hashes runtime text about eight times
  faster than FNV-1a, has a fixed specification, and can be `constexpr`. XXH3 is
  faster for short text, but its specification and code are much larger.
- **No switch to drop name text in shipping builds.** It was planned to save the
  memory of the reverse table. But `Localization` finds a language folder from
  the text of its code, CSV export and sorting need key text, and saving a level
  writes names as text. Without the text those break, and keeping copies of the
  text in each system would cost most of the memory that the switch saves.
- **`String` is `std::string`, not an engine class.** Every library that the
  engine uses takes `std::string`, and it has the small string optimization.
- **UTF-8 in `char`, not `char8_t`.** No library that the engine uses takes
  `char8_t`, and the engine has no code-page text to keep apart from UTF-8.
- **Case-sensitive names.** A case-insensitive `Name` would need the hash to
  fold case, which puts locale rules into saved ids.
