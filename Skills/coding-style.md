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

## Braces

An opening brace `{` is always on a new line (Allman style). This applies to
namespaces, classes, structs, enums, functions, control statements, lambdas, and
`try`/`catch`. `.clang-format` sets `BreakBeforeBraces: Allman`, so
`clang-format` puts braces in the correct place.

```cpp
namespace Fling
{
    class InputDevice
    {
    public:
        void PushKey(InputKey t_Key, bool t_IsDown)
        {
            if (!m_Connected)
            {
                return;
            }
        }
    };
} // namespace Fling
```

A short function body inside a class can stay on one line, for example
`bool IsValid() const { return Value != 0; }`. A braced initializer
(`InputKey Key{InputKeyId::W};`) is not a block, so it stays on one line too.

## Naming / conventions to preserve

- Flat include names (`#include "Logger.h"`, not `#include "Core/Logger.h"`) are
  intentional, even across the in-progress module split described in
  [`architecture.md`](architecture.md). Don't "fix" these to path-qualified includes.
- There is no `pch.h` or other catch-all include. Each header compiles on its own,
  and each file includes the headers it uses.
- Include `Platform.h` before you test `FLING_WINDOWS`, `FLING_LINUX`, or
  `FLING_DEBUG` with `#if`. If you do not, the macro is undefined and the block is
  silently compiled out. GCC and Clang builds use `-Wundef` to catch this.
- `Platform.h` defines platform macros only. Include `DesktopWindow.h` and
  `Input/Input.h` directly where you use them.
