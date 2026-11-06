# Strings

Status: Phases 1 and 2 are built (Core string types, and assets), and the string
tables of Phase 5. Phases 3, 4, and 6, and the rest of Phase 5, are not.

This plan sets out the string types of the engine, the rules for each one, and
the order in which we move the code onto them. When a phase is done, move its
rules into `Skills/strings.md` (see the last phase).

## Summary

The engine has three string types, plus one parameter type:

| Type | What it is | Use it for |
| --- | --- | --- |
| `Name` | An immutable symbol. A 64-bit hash of UTF-8 text, made at compile time, the same in every process and on every platform. 8 bytes; compare and hash are `O(1)`. A reverse table gives the text for logs and the editor. | **The default** for symbols that people write: actions, systems, component types, tags, config keys, bones, sockets, properties, entity labels. |
| `String` | A mutable, owning UTF-8 byte buffer with the small string optimization. | Building, editing, parsing, and formatting text. |
| `DisplayText` | Localizable text that the player sees. | Every string the player can read. |
| `StringView` | A non-owning view of UTF-8 text. | **The universal parameter type** for read-only text. |

All text is UTF-8 in `char`, from day one.

## Today

Counts are from `FlingEngine/`, `Sandbox/`, and `FlingTests/` (November 2026).

| What | Uses | Where |
| --- | --- | --- |
| `std::string` | ~590 | everywhere; 67 `const std::string&` parameters in headers |
| `std::string_view` | ~100 | newer code: `LaunchOptions`, `CommandLine`, input, `Ecs/Schedule` |
| `const char*` | ~180 | static name tables (`InputKey`, `ComponentInfo`, `SystemDecl`, `PhaseName`), C APIs, `ComponentTypeRegistry` |
| `wchar_t` / `std::wstring` | ~50 | Win32 calls only (`DynamicLibrary.cpp`, `NativeThreadWindows.cpp`, `WindowsStrings.h`) |
| `entt::hashed_string` | ~95 | `Fling::Guid`, `Guid_Handle`, `HS()`, `_hs`, `entt::tag<"Default"_hs>` |
| 64-bit FNV-1a | 1 type | `Engine/inc/Ecs/TypeHash.h` (`TypeHash`, `HashName`) |

The problems:

1. **`Guid` is not a GUID and does not own its text.** `Fling::Guid` is an
   `entt::hashed_string`: a 32-bit hash plus a non-owning `const char*`. Callers
   write `Guid{Path.c_str()}` with a local `std::string`, so every stored
   `Resource::m_Guid` keeps a dangling `data()` pointer. `Resource` copies the
   text in its constructor, so it works today, but any new call to
   `m_Guid.data()` is a use-after-free. A 32-bit hash also has a 69% chance of a
   collision at 100,000 strings.
2. **Two hash algorithms for the same job.** Asset ids are 32-bit (EnTT's
   FNV-1a). Component type ids are 64-bit (`TypeHash`). Neither can give back its
   text for a log message.
3. **No cheap identifier.** Input actions, system names, config keys, material
   types, and `NameComponent` use `std::string` or `const char*`. Each compare is
   a `strcmp`, and each copy can allocate.
4. **No parameter rule.** The same kind of function takes `const std::string&`
   in one file, `std::string_view` in the next, and `const char*` in a third.
5. **No line between identifiers and text the player sees.** Nothing stops an
   identifier from being shown to the player, or player-facing text from being
   written as a literal in code.

## `Name`

```cpp
namespace Fling
{
    class Name
    {
    public:
        constexpr Name() = default;                       // Name::None, hash 0
        constexpr explicit Name(StringView t_Text);       // hashes; registers the text when run at runtime

        constexpr uint64 GetHash() const;
        constexpr bool IsNone() const;

        StringView ToView() const;                        // text from the reverse table; for logs and the editor
        String ToString() const;

        // ==, <=> (by hash), std::hash (the hash itself)
    private:
        uint64 m_Hash = 0;
    };

    // "Jump"_name: made at compile time, and its text is registered before main.
    template <FixedLiteral Text>
    consteval Name operator""_name();
}
```

### What a `Name` is

- **The hash is the identity.** A `Name` holds only its 64-bit hash. It does not
  hold an index into a table and does not hold a pointer to text. So it is the
  same value in every process, on every platform, and in every build. It can go
  as it is into saved files, cooked data, rollback snapshots, desync checksums,
  and network messages. No translation is needed at any boundary.
- **Immutable.** There is no way to change the text of a `Name`. Make a new one.
- **`O(1)`.** Compare, copy, order, and hash are integer operations. `std::hash`
  returns the hash itself.
- **Made at compile time.** `"Jump"_name` and `constexpr Name{"Jump"}` are
  constant expressions, so a `Name` can be a `case` label, a template argument,
  or a `static constexpr` member.
- **The hash is never shown to users.** Not to the player, and not in the editor
  UI. Logs and the editor show `ToView()`. Only a missing text entry (see below)
  shows `#1a2b3c4d5e6f7081` in a log.
- **Case-sensitive, byte-exact.** `"Jump"_name != "jump"_name`. The hash is over
  the UTF-8 bytes (each `char` cast to `uint8`), with no case folding, no Unicode
  normalization, and no locale. Any canonical form (for example asset paths)
  belongs to the code that makes the text, never to the hash.
- **Ordering.** `operator<` compares hashes. That order is the same on every
  machine, so it is safe for simulation. It is not alphabetical. To sort for
  display, compare `ToView()`.

### The hash

- 64-bit. The chance of any collision:

  | Distinct names | 32-bit | 64-bit |
  | --- | --- | --- |
  | 100,000 | ~0.7 | ~3e-10 |
  | 1,000,000 | ~1 | ~3e-8 |
  | 100,000,000 | ~1 | ~3e-4 |

  A 32-bit hash reaches 50% at about 77,000 names, and a 64-bit hash at about
  5 billion.
- A 64-bit hash compares and hashes at the same cost as a 32-bit table index plus
  a 32-bit number. The hash gives nothing on the hot path. What it gives is a
  value that is the same everywhere, construction at compile time, and no shared
  state when a `Name` is made from a literal.
- **The algorithm is xxHash64, seed 0.** Reasons:
  - Runtime cost does not depend on it. A `Name` compares and hashes as one
    integer, and literals are hashed at compile time. Only text made at runtime
    (loading, the editor) is hashed while the game runs, and xxHash64 reads 8
    bytes per round where FNV-1a reads 1.
  - It passes SMHasher. FNV-1a does not, and spreads very similar strings
    (`Enemy_000001`, `Enemy_000002`) less well.
  - Its specification is fixed and has implementations in most languages, so
    tools outside the engine can make the same ids.
  - It can be `constexpr`. XXH3 is faster for short strings, but its
    specification and code are much larger.
  **After the first saved file uses it, the algorithm can never change.** Known
  text and hash pairs from the reference implementation are tested on every CI
  toolchain.
- In binary data, the hash is written as little-endian.

### The reverse table

A process-wide table from hash to text. It makes `Name` readable.

- **Interned.** Each distinct text is stored once. Text is stored in fixed-size
  blocks that never move, so `ToView()` is valid until the process exits. Entries
  are never removed.
- **Sharded.** The table has 64 shards, picked by the hash. A read takes a shared
  lock on one shard and an insert takes its exclusive lock, so threads rarely
  wait. Reads are for logs and the editor, not for the simulation, which compares
  hashes and never reads text.
- **Filled three ways:**
  1. **Literals.** `operator""_name` is a literal operator template. Using it
     instantiates a registrar variable for that literal, which adds the text at
     static initialization, before `main`. So every literal in the program is in
     the table before any code runs. Phase 1 must prove this works on all four
     CI toolchains, and in static libraries where the linker can drop unused
     objects. If it does not, the fallback is `FLING_NAME("Jump")`, a macro
     that does the same with a function-local static.
  2. **Runtime text.** `Name(StringView)` at runtime hashes and registers. This
     is how text typed in the editor, read from a level file, or read from
     config becomes a `Name`.
  3. **Load.** Cooked data stores hashes, so each cooked file also stores a
     table of the texts that it uses. Loading the file registers them.
- **Collisions are errors.** Registering a second, different text for a hash that
  is already in the table is a fatal error in builds without `FLING_SHIPPING`,
  and an error log in shipping builds. Because the hash is fixed, a collision in
  built content shows up when the content is built or cooked, before it ships.
  The only collisions that can first appear at runtime are from text made at
  runtime (mods, procedurally made ids). Registration catches those too.
- **Content made by players.** The hash is fast, not cryptographic, so a creator
  can make a string on purpose that collides with an engine `Name`. A table of
  indices would not have this problem, and it cannot be fixed at runtime. If the
  engine ever accepts content made by players (mods, workshop uploads), the upload
  pipeline checks every `Name` in the content against the shipped table, and
  engine `Name`s live under prefixes that player content cannot use.
- **Start-up.** The table is `constinit` storage that works before `main` and is
  never destroyed, so names can be made in static initializers and used in
  static destructors.
- **Memory.** The table uses its own block allocator and reports its size in
  memory stats. It is on in every build, so shipping logs and crash reports are
  readable. A game that needs the memory can turn the text off in shipping with
  `FLING_NAME_TEXT=0` (not built yet). Then logs show `#<hash>`, and nothing
  else changes.

### Editor and level files

- The editor shows `ToView()` and edits text. When the user types `Jump`, the
  editor makes `Name("Jump")`, which hashes and registers it.
- JSON level files store the **text**. Loading makes the `Name` from the text,
  which gives the same hash. Files stay readable and diffable, and a renamed
  `Name` is a text change in the file.
- Binary and cooked files store the **hash**, plus the text table from the
  previous section.

### What a `Name` is not for

- **Not the identity of an object instance.** An entity is an `Entity` handle in
  memory and an `EntityGuid` on disk. Two entities can have the same `Name`
  label. `Name` has no number suffix (`Enemy_12`), because nothing should need
  unique names to tell objects apart.
- **Not structured data dressed as a name.** Asset paths, type names, and module
  names each get their own type (`AssetPath`/`AssetId`, `TypeHash`), even when
  that type uses a `Name` inside. A `Name` is a symbol, not a path.
- Not for text that the player sees. Use `DisplayText`.
- Not for text that is edited or built up. Use `String`, then make a `Name` when
  it is finished.
- Not for unbounded user text (chat, player names). Every distinct text stays in
  the table until exit.
- Do not make a `Name` from runtime text in a tick. Hashing and the shard lock
  belong to load and setup time. Literals cost nothing.

## `String` and `StringView`

```cpp
namespace Fling
{
    using String = std::string;
    using StringView = std::string_view;
}
```

- `String` is `std::string`. It already has the small string optimization, and
  spdlog, fmt, nlohmann::json, ImGui, GLFW, and the standard library all take it.
  Our own class would need a conversion at each of those boundaries. The alias
  gives us one place to change if we later need a custom allocator.
- **Read-only parameter:** `StringView`, always. Not `const String&` and not
  `const char*`. A `StringView` takes a literal, a `String`, or a slice without a
  copy. When the function passes the text to a C API that needs a terminating
  null, it makes a `String` at that call.
- **Parameter that the function keeps:** `String` by value, then `std::move`.
- **Return value:** `String` by value, or `const String&` to a member that lives
  longer than the call. Never a `StringView` to a temporary.
- **Member:** `String`, or `Name` when it is an identifier. Never `StringView`.
- **`const char*`** only at a C API call, or in `constexpr` tables of literals
  (`InputKeyDetails`, `ComponentInfo::Name`, `PhaseName`), which become `Name`
  or `StringView` where they can.

### UTF-8 everywhere

- **All text is UTF-8 in `char`.** We do not use `char8_t` or `std::u8string`.
  A separate UTF-8 character type would stop UTF-8 from mixing with code-page
  text, but we have no code-page text, and every library we use takes `char`.
- **No `u8"..."` literals.** In C++20 they are `char8_t` arrays, which do not
  convert to `String` or `StringView`. Plain `"..."` literals are already UTF-8:
  source files are UTF-8, and MSVC builds with `/utf-8` (root
  `CMakeLists.txt`). Keep that flag.
- **No text macro.** `char` is UTF-8 on every platform, so no macro needs to
  change the type of a literal per platform.
- **Decide this now.** Changing the character type of an engine after the code
  base has grown is very expensive: every overload, literal, and library
  boundary changes at once. UTF-8 in `char` is fixed from Phase 1.
- **Wide strings** only in Windows `.cpp` files, at Win32 calls. Always call the
  `W` form of a Win32 function, never the `A` form, which reads the ANSI code
  page. Convert at the boundary with `Utf8ToWide` and `WideToUtf8`
  (`Core/src/Misc/WindowsStrings.h`).
- **Paths.** On Windows, `std::filesystem::path(std::string)` reads the ANSI code
  page. Make paths with `PathFromUtf8` and read them back with `PathToUtf8`.
- **Parsing.** Parse numbers with `std::from_chars`, which does not use the
  locale. `StringUtils::ParseAs` moves to it in Phase 4.

## `DisplayText`

Text that the player sees. It is the only type that UI code takes for visible
text.

```cpp
namespace Fling
{
    class DisplayText
    {
    public:
        // From a string table entry: the table and the key are Names. The source text
        // is the fallback when no table has the entry.
        static DisplayText FromTable(Name t_Table, Name t_Key, StringView t_SourceText = {});

        // Text that must not be translated (a player's name, a number already formatted).
        static DisplayText AsInvariant(String t_Text);

        // Text with arguments: "{Count} enemies left".
        static DisplayText Format(const DisplayText& t_Pattern, std::initializer_list<FormatArg> t_Args);

        String Resolve() const;   // the text in the current language
    };
}
```

- **It keeps its source, not its result.** A `DisplayText` holds its table and
  key, or its pattern and its arguments, separately from the resolved text. When
  the language changes, it resolves again and gives the new text. A
  `DisplayText` made by `Format` keeps the arguments, not the formatted string.
- **No implicit conversion** from `String`, `StringView`, or a literal. A literal
  in code cannot become text that the player sees by accident.
- **Not for logic.** `DisplayText` has no `operator==`. Its text changes with the
  language. Compare the `Name` keys instead.
- **String tables** hold the text per language, keyed by `Name`. The game loads
  the tables for the current language. Changing the language at runtime
  re-resolves the visible text. See Localization below.
- Localization is wider than text (fonts, text direction, number and date
  formats, audio). Those are designed when the string tables are, in Phase 5.
  The type and the rule come first, so that new UI code is right from the start.

## Localization

`Localization` (Resources module) holds the string tables of every language and
the current language.

- **Files.** `Assets/Localization/<language>/<Table>.json`, one JSON object of key
  to text per table. The folder name is the language code (`en`, `fr`). The
  `[Localization]` section of `EngineConf.ini` sets the folder, the source
  language, and the language. `--language <code>` overrides the language.
- **Lookup.** A table entry resolves in the current language, then in the source
  language, then to the source text given to `FromTable`, then to the key. Core
  does not depend on Resources: `Localization::SetActive` sets a resolver
  function in `DisplayText`.
- **Language change.** `SetLanguage` loads the language if it is not loaded. Every
  `DisplayText` shows the new language the next time it resolves. `GetRevision`
  changes with each change, so UI that keeps resolved text knows when to resolve
  again.
- **Translators work in CSV.**
  - `Sandbox --loc-export Strings.csv` writes every entry of every language on
    disk: UTF-8 with a byte order mark, a `Table,Key,<language>...` header, and
    rows sorted by table and key. An empty cell is no entry.
  - `Sandbox --loc-import Strings.csv` reads the file back (a new column is a new
    language), writes the JSON tables of each language in it, and logs how many
    source entries each one is missing.
  - Both run with no window and exit.
- **Not built yet:** gathering keys and source text from code, plural rules,
  number and date formats per language, fonts for other scripts, and right-to-left
  text.

## Assets

Assets use `Name` and `String`. They do not add a string type.

- **`AssetPath`** (Resources module) makes a canonical path from text: forward
  slashes, relative to its root folder, no `.` segments, and a `..` removes the
  segment before it. Case is kept. Later, builds without `FLING_SHIPPING` should
  warn when the case does not match the file on disk, because Windows does not
  care about case and Linux does.
- **`AssetId`** is a typed wrapper around the `Name` of the canonical path. Only
  `AssetPath` and the asset database make one. Code must not depend on how it is
  made, so it can later become an id that the tools assign, which survives a
  rename.
- `ResourceManager` keys on `AssetId`. `AssetRef<T>` (`ecs-plan.md`) holds one.
- These replace `Guid`, `Guid_Handle`, `INVALID_GUID`, and `HS()`. The word
  `Guid` is left for real GUIDs (`EntityGuid`).

## Logging

Log format strings are plain `"..."` literals in UTF-8, with fmt `{}`
placeholders. There is never a wide or per-platform log format. Format strings
are stored in the binary, so a 1-byte encoding keeps them small.

- `Name`, `AssetId`, and `DisplayText` have `fmt::formatter` specializations, so
  they go straight into `F_LOG_*` calls. A `Name` logs as its text.
- **Windows console.** The console reads bytes in its code page, not UTF-8, so
  non-ASCII text shows as garbage. At start-up on Windows, call
  `SetConsoleOutputCP(CP_UTF8)`. Do not use `wprintf` or `%ls` to work around
  this.
- **The `F_LOG_*` macros are each two statements** (console, then file), with no
  `do { } while (0)`. In `if (x) F_LOG_TRACE(...);` the file log runs even when
  `x` is false. Phase 1 wraps each one in `do { } while (false)`. Later, one
  logger with two sinks would format each message once; the file log is
  asynchronous and the console is not, so that is a separate change.

## Rules for simulation code

These hold in code that runs during a tick, and most of all in code that runs
again in a rollback resimulation.

- Identifiers in state are `Name`. A `Name` is stable, so a snapshot, a desync
  checksum, and a save hold it as it is.
- Do not allocate a `String` in a tick, and do not make a `Name` from runtime
  text in a tick. Literals are free.
- Do not hold a `String` in a component that is snapshotted. It allocates on copy
  and stops the column from being copied with `memcpy`.
- Do not iterate an `unordered_map` or `unordered_set` in an order that changes
  results. Lookups are fine.
- `std::hash<String>` differs between standard libraries. Never save it or send
  it. Use `Name`.
- Do not call functions that use the locale (`tolower`, `atof`, `strtod`,
  `printf("%f")`) on data that goes into state.
- **On the network**, a tick's input carries action ordinals or bits, not
  `Name`s. At connect, peers compare one content hash made from the registered
  type `Name`s, the action `Name`s, and the protocol version. If they differ, the
  connection is refused, before any desync can happen.

## Phases

Each phase is one or more PRs off `main`. Each one keeps all CI platforms green.

### Phase 1 — Core types

- `Core/inc/Strings/String.h`: `String`, `StringView`, `PathFromUtf8`,
  `PathToUtf8`.
- `Core/inc/Strings/Name.h` and `src/Strings/NameTable.cpp`: `Name`,
  `operator""_name`, the reverse table, `fmt::formatter<Name>`.
- `Core/inc/Strings/DisplayText.h`: the type with `FromTable`, `AsInvariant`, and
  `Format`.
  `FromTable` returns the key's text until Phase 5 adds string tables.
- `Core/inc/Strings/Hash.h`: `constexpr` xxHash64. When the ECS types reach
  `main`, `Ecs/TypeHash.h` uses it: `TypeHash` becomes the hash of a `Name`, and
  `HashName` is deleted, so there is one hash function.
- Logging: the `do { } while (false)` fix and `SetConsoleOutputCP(CP_UTF8)` on
  Windows.
- Tests in `FlingTests/Core/`:
  - Known text and hash pairs, run on all four CI toolchains. This proves that
    the hash is the same on every compiler and platform.
  - `constexpr` and `case` label use.
  - Literals registered before `main`, including from a static library.
  - Runtime registration, collision reporting, and inserts from many threads
    with reads while they run.
  - `ToView()` of a `Name` made from runtime text.

### Phase 2 — Assets (replaces `Guid`)

- Add `AssetPath` and `AssetId`. Port `Resource`, `ResourceManager`, and every
  `Create(Guid)` in Resources and Graphics. Remove `m_HumanReadableName`.
- During the move, `Guid` and `Guid_Handle` are `[[deprecated]]` aliases. Then
  delete them, with `INVALID_GUID` and `HS()`, and remove the
  `<entt/core/hashed_string.hpp>` include from `FlingTypes.h`. This is the first
  bullet of `ecs-plan.md` Phase A.
- The `entt::tag<"Default"_hs>` render tags go away in `ecs-plan.md` Phase B
  (tag structs). They do not need `Name`.

### Phase 3 — Identifiers

- Input actions and contexts, system and phase names, `Material::TypeMap`, and
  config sections and keys move to `Name`.
- `NameComponent` holds a `Name`. JSON stores the text.
- `ComponentTypeRegistry` keys on `Name`. That removes its dangling `const char*`
  comment. It goes away in `ecs-plan.md` Phase C anyway.

### Phase 4 — Parameters

- Change read-only `const std::string&` and `const char*` parameters to
  `StringView`, one module at a time (67 `const std::string&` in headers today).
- `StringUtils::ParseAs` uses `std::from_chars`.
- Add the parameter rules to `Skills/coding-style.md`.

### Phase 5 — Localization

- Done: string tables per language, keyed by `Name`, loaded for the current
  language, with a language change at runtime. `DisplayText::FromTable` resolves
  through them. CSV export and import for translators.
- The editor and the UI module take only `DisplayText` for visible text.
- Fonts, text direction, and number and date formats.

### Phase 6 — Move the rules into Skills

- Write `Skills/strings.md` with the summary table, the rules, and the
  simulation rules. Link it from `AGENTS.md`. Delete this plan.

## Rejected

- **`Name` as an index into a name table.** An interned index is 4 bytes, has no
  collisions, and has its text in every build. But the index depends on the order
  in which text was added, so it differs between processes and machines. Every
  save, snapshot, checksum, and network message would need a translation, a sort
  by index would change between runs, and a `Name` could not be made at compile
  time. A stable hash with a reverse table keeps the readable text and removes
  those problems.
- **A 32-bit hash.** About 69% chance of a collision at 100,000 names.
- **Our own string class.** It needs a conversion at every library boundary, and
  `std::string` already has the small string optimization.
- **Keeping `entt::hashed_string`.** It does not own its text, it is 32-bit, and
  we are moving off EnTT (`ecs-plan.md`).
- **A `Name` that compares without case.** The hash would have to fold case,
  which puts locale rules into saved ids.
- **`std::u8string` and `char8_t`.** No library that we use takes them.
- **More string types** (a null-terminated view, an inline fixed-size string).
  Three types cover the uses. A `Name` covers identifiers in simulation state.
  Add one only for a real need that the three cannot meet.
