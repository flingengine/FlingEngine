# Strings and localization

The string types of the engine, the rules for each one, and how text for the
player is translated. Work that is not done yet is in
[`Plans/strings.md`](../Plans/strings.md).

## The types

| Type | Header | What it is | Use it for |
| --- | --- | --- | --- |
| `Name` | `Strings/Name.h` | An immutable 64-bit hash of UTF-8 text, the same in every process and on every platform. Compare, order, and hash are `O(1)`. | **The default** for symbols that people write: tags, keys, component types, material types, entity labels. |
| `String` | `Strings/String.h` | `std::string`: mutable, owning, UTF-8, with the small string optimization. | Building, editing, parsing, and formatting text. |
| `StringView` | `Strings/String.h` | `std::string_view`: a non-owning view of UTF-8 text. | **Every read-only text parameter.** |
| `DisplayText` | `Strings/DisplayText.h` | Localizable text for the player. It keeps its source and resolves in the current language. | Every string that the player can read. |
| `AssetPath` / `AssetId` | `AssetPath.h` (Resources) | A canonical asset path, and the opaque id of that path. | Assets and resources. |

All text is UTF-8 in `char`. There is no other character type in engine code.

## `Name`

- Make one from a literal with `"Jump"_name` (compile time, no runtime cost) or
  from runtime text with `Name(Text)`. The empty text is `Name()`, which is None.
- The hash is xxHash64 with seed 0 (`HashText` in `Strings/Hash.h`). It is
  computed at compile time for literals. **Never change the algorithm**: saved
  files hold its values.
- A `Name` holds only the hash, so it can go as it is into saved files,
  snapshots, desync checksums, and network messages.
- A process-wide table from hash to text makes names readable. `ToView()` gives
  the text, and `fmt` logs a `Name` as its text. Literal names are in the table
  before `main`. A `Name` made from runtime text adds its text.
- Two different texts with the same hash are an error: builds without
  `FLING_SHIPPING` stop.
- `Name::WithoutText(Text)` makes the `Name` without adding the text. Use it to
  look up text that may not be a name (user input, a key from a file), so that
  text that is not found does not stay in the table.
- Names are case-sensitive and compare the exact bytes. `operator<` orders by
  hash: the same on every machine, but not alphabetical. To sort for display,
  compare `ToView()`.
- JSON stores a `Name` as its text (`JsonArchive` does this), so files stay
  readable and give the same hash when they are loaded.
- Do not:
  - Show a `Name` to the player. Use `DisplayText`.
  - Use a `Name` as the identity of an object. Use `Entity` or `EntityGuid`.
    `NameComponent` is a label.
  - Make a `Name` from text that has no limit (chat, player names). The table
    keeps every text until the process exits.
  - Make a `Name` from runtime text in a tick, or on each key press in the
    editor. Make it when the edit ends (see `EntityEditorPanel`).

## `String` and `StringView`

- Spell them `String` and `StringView`, not `std::string` and
  `std::string_view`. Outside `namespace Fling`, write `Fling::String`, or add
  `using Fling::String;` in a test file. `python3 scripts/check_string_types.py`
  checks this, and CI runs it.

- A read-only parameter is `StringView`. Not `const std::string&`, and not
  `const char*`.
- A parameter that the function keeps is `String` by value, then `std::move`.
- Return a `String` by value, or a `const String&` to a member that lives longer
  than the call. Never return a `StringView` to a temporary.
- `const char*` only at a C API call (Win32, GLFW, Vulkan, ImGui ids and labels,
  `argv`) and in `constexpr` tables of literals.
- When a body needs a terminating null, make a `String` from the view at that
  call.
- **Files:** open with `PathFromUtf8(Path)` and read paths back with
  `PathToUtf8`. On Windows, a path made from a `std::string` reads the ANSI code
  page.
- **Wide strings** only in Windows `.cpp` files at Win32 calls. Always call the
  `W` form of a Win32 function. Convert with `Utf8ToWide` and `WideToUtf8`
  (`Core/src/Strings/WindowsStrings.h`).
- **No `u8"..."` literals.** In C++20 they are `char8_t`. Plain `"..."` literals
  are UTF-8: the source files are UTF-8, and MSVC builds with `/utf-8`.
- **Numbers:** parse with `StringUtils::ParseAs`, which uses `std::from_chars`
  and does not use the locale. Do not use `atof`, `strtod`, or `printf("%f")` for
  data that goes into state.

## Assets

- `AssetPath(Text)` makes the canonical form of a path: forward slashes, no `.`
  segments, `..` resolved, case kept. `./Materials\Default.mat` and
  `Materials/Default.mat` are the same asset.
- `AssetId` is an opaque wrapper around the `Name` of the canonical path. Only
  `AssetPath` makes one. Do not compute it another way: it may later become an id
  that the tools assign.
- `Create` functions of resources take an `AssetPath`. `ResourceManager` keys on
  `AssetId` and loads each asset once.
- Builds without `FLING_SHIPPING` warn when the case of a path does not match the
  file on disk (`AssetPath::MatchesCaseOnDisk`). Such a path loads on Windows and
  fails on Linux.

## `DisplayText` and localization

### Making text for the player

```cpp
// A string table entry, with its source-language text as the fallback
DisplayText Start = DisplayText::FromTable("UI"_name, "Menu.Start"_name, "Start Game");

// Text that must not be translated
DisplayText Player = DisplayText::AsInvariant(PlayerName);

// A pattern with arguments; numbers and dates format in the current language
DisplayText Left = DisplayText::Format(DisplayText::FromTable("UI"_name, "Hud.EnemiesLeft"_name),
                                       {{"Count"_name, EnemyCount}});
```

- `DisplayText` has no conversion from `String` or a literal, and no
  `operator==`. Compare the `Name` keys.
- It keeps its source and resolves on each `Resolve()`, so text changes when the
  language changes. `Localization::GetRevision()` changes with each change, for
  UI that keeps resolved text.
- `AsNumber` and `AsDate` format in the current language: 12345 is "12,345" in
  English and "12.345" in German.

### Patterns

- `{Key}` is replaced by the argument with that key. `{{` and `}}` give `{` and
  `}`. Text that is not a known argument or form is kept as it is.
- Plural forms (ICU MessageFormat style):
  `{Count, plural, =0 {No enemies} one {# enemy left} other {# enemies left}}`.
  - An `=N` case is used first, then the case of the number's plural category in
    the current language (`zero`, `one`, `two`, `few`, `many`, `other`; see
    `GetPluralCategory`), then `other`.
  - `#` is the number, formatted in the current language.
  - Each translation lists the categories of its own language: Russian uses
    `one`, `few`, `many`; Arabic uses all six; Japanese only `other`.
- Select forms: `{Gender, select, female {She won} male {He won} other {They won}}`.
  The case that matches the argument's text is used, then `other`.
- Forms can be nested.

### String tables

- `Assets/Localization/<language>/<Table>.json`: one JSON object of key to text
  per table. The folder name is the language code (`en`, `fr`, `pt-BR`).
- `[Localization]` in `EngineConf.ini` sets `Folder`, `SourceLanguage` (the
  language with every key), and `Language`. `--language <code>` overrides
  `Language`.
- A lookup uses the current language, then the source language, then the source
  text given to `FromTable`, then the key.
- An optional `_language.json` in a language folder is not a table. It can set
  `DisplayName`, `Direction` (`LeftToRight` or `RightToLeft`),
  `DecimalSeparator`, `GroupSeparator`, `MinimumGroupingDigits`, and
  `DatePattern`. Anything it does not set comes from
  `DefaultDisplayLanguageFor(code)`.
- Core does not depend on Resources: `Localization::SetActive` sets a table
  resolver and the current `DisplayLanguage` in Core.

### Tools

All of these run with no window, do their work, and exit.

| Flag | What it does |
| --- | --- |
| `--loc-gather <folder>` | Finds `DisplayText::FromTable("T"_name, "K"_name, "Source")` calls with literal arguments in C++ source, and adds the missing entries to the source-language tables. Warns when code and table disagree. |
| `--loc-export <file.csv>` | Writes every entry of every language for translators: UTF-8 with a byte order mark, `Table,Key,<language>...`, sorted rows. An empty cell is no entry. |
| `--loc-import <file.csv>` | Reads a CSV file in that form (a new column is a new language), writes each language's JSON tables, and logs how many entries each one is missing. A file that is not valid changes nothing. |

### Right-to-left languages

- Arabic, Hebrew, Persian, Urdu, and some others are right to left (by code, or
  `Direction` in `_language.json`). `GetDisplayTextDirection()` gives the
  current direction, so UI can mirror its layout.
- Text is always stored and resolved in logical order (the order in which it is
  read). Never reverse text.
- In a right-to-left language, `Format` puts each argument between U+2068 and
  U+2069 (direction isolates), so a left-to-right name or number does not
  reorder the sentence. No marks are added in left-to-right languages.
- Reordering for display (the Unicode bidirectional algorithm) and Arabic
  shaping are the job of a text renderer, which the engine does not have yet.
  The developer UI (ImGui) shows right-to-left text in the wrong order.

## Logging

- Format strings are plain UTF-8 literals with fmt `{}` placeholders. `Name`,
  `AssetId`, `AssetPath`, and `DisplayText` log as their text.
- Each `F_LOG_*` macro is one statement, so it is safe in an `if` with no
  braces.
- On Windows the console uses the UTF-8 code page, so text that is not ASCII
  shows correctly.

## Rules for simulation code

These hold in code that runs during a tick, and most of all in code that runs
again in a rollback resimulation.

- Identifiers in state are `Name`. A `Name` is stable, so snapshots, checksums,
  and saves hold it as it is.
- Do not allocate a `String` in a tick, and do not make a `Name` from runtime
  text in a tick. Literal names cost nothing.
- Do not hold a `String` in a component that is snapshotted. It allocates on
  copy and stops the column from being copied with `memcpy`.
- Do not iterate an `unordered_map` or `unordered_set` in an order that changes
  results.
- `std::hash<std::string>` differs between standard libraries. Never save it or
  send it. Use `Name`.
- Do not call functions that use the locale on data that goes into state.
- On the network, a tick's input carries action ordinals or bits, not names.
