# Coding Style

Conventions for first-party Fling Engine C++, plus the pitfalls an AI assistant
is most likely to hit.

Scope: first-party code only — `FlingEngine/`, `Sandbox/`, `FlingTests/`.
**Never reformat, refactor, or "clean up" anything under `external/`** — it's
vendored third-party code and is left alone, full stop.

## Documentation comments

- Modern Doxygen-style blocks, plain description first, no `@brief` line.
- Never use `/*!` — always `/**`.
- Only add `@param` / `@return` / `@see` when they say something the signature
  doesn't already make obvious. Don't generate `@return float GetTimef`-style noise.

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

Trivial members/accessors can use a one-line `/** ... */`.

## Enforcement — check before considering work done

- **Comment content**: `python3 scripts/check_comment_style.py` — rejects leftover
  `@brief` / `/*!` in first-party sources. Run this after adding or editing any
  doc comment.
- **Layout/formatting**: `.clang-format` at the repo root. Run `clang-format`
  rather than hand-formatting:

  ```bash
  find FlingEngine Sandbox FlingTests -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) \
    | xargs clang-format -i
  ```

  clang-format handles layout only — it does not check comment *content*, that's
  what `check_comment_style.py` is for. Both checks run in CI
  (`.github/workflows/build.yml`, `comment-style` job runs the Python script;
  formatting is expected to already be clean going in).

## Naming / conventions to preserve

- Flat include names (`#include "Logger.h"`, not `#include "Core/Logger.h"`) are
  intentional, even across the in-progress module split described in
  [`architecture.md`](architecture.md). Don't "fix" these to path-qualified includes.
- `pch.h` is being deleted per the module-split plan — don't add new `#include "pch.h"`
  to public headers, and don't rely on it as a kitchen-sink include.
