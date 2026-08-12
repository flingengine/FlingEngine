# Coding Style

Conventions for first-party Fling Engine C++ (`FlingEngine/`, `Sandbox/`, `FlingTests/`).
Third-party code under `external/` is left alone.

## Documentation comments

Prefer modern Doxygen-style blocks with a plain description. Do **not** use
Visual Studio / XML-wizard tags like `@brief`, and do not use `/*!`.

```cpp
/**
 * Holds onto the command line arguments passed to this application.
 *
 * @param Argc  Number of arguments
 * @param ArgV  Argument values
 * @return True if successfully initialized
 */
bool Init(const int32 Argc, const char* ArgV[]);
```

Guidelines:

- Lead with a short description; skip `@brief`.
- Use `@param` / `@return` / `@see` only when they add information the signature does not already make obvious.
- Prefer `/** ... */` for API docs. One-line members can use `/** ... */` on a single line.
- Do not write auto-generated noise such as `@return float GetTimef` or empty `@brief` lines.

Member fields and trivial accessors can use a short one-liner:

```cpp
/** The time that the program started */
double m_startTime = 0.0;
```

## Enforcement

- **Comment content**: `python3 scripts/check_comment_style.py`  
  Rejects leftover `@brief` and `/*!` in first-party sources.
- **Layout**: `.clang-format` at the repo root.  
  clang-format does **not** validate documentation tags; it only formats code/comment layout.

Format first-party files (example):

```bash
find FlingEngine Sandbox FlingTests -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) \
  | xargs clang-format -i
```

A one-time migration from the old `@brief`-heavy style lives in
`scripts/migrate_doxygen_comments.py` (see issue
[#166](https://github.com/flingengine/FlingEngine/issues/166)).
