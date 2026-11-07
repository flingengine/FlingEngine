#!/usr/bin/env python3
"""
Checks that first-party C++ uses the engine string types (Skills/strings.md):

- String and StringView, not std::string and std::string_view. Only the headers
  that define the aliases may name the std types.
- A read-only text parameter is StringView, not const String&.

Comments are not checked. Run from the repository root. Exits with 1 and lists
each line that breaks a rule.
"""

import re
import subprocess
import sys

ROOTS = ["FlingEngine", "Sandbox", "FlingTests"]
EXTENSIONS = (".h", ".hpp", ".cpp", ".inl", ".cpp.in")
# These headers define String and StringView, so they name the std types.
ALLOWED = {
    "FlingEngine/Core/inc/Strings/String.h",
    "FlingEngine/Core/inc/Strings/Hash.h",
}

STD_STRING = re.compile(r"\bstd::(string|string_view)\b")
# "(const String& Name," or ", const String& Name)" or "... = default". A range-for
# ("for (const String& Item : Items)") has a ':' after the name, so it is not matched.
CONST_REF_PARAM = re.compile(r"[(,]\s*const\s+(Fling::)?String\s*&\s*\w+\s*[,)=]")


def code_part(line, in_block_comment):
    """The part of a line that is code, and whether a block comment is still open."""
    code = ""
    index = 0
    while index < len(line):
        if in_block_comment:
            end = line.find("*/", index)
            if end < 0:
                return code, True
            in_block_comment = False
            index = end + 2
            continue
        if line.startswith("//", index):
            break
        if line.startswith("/*", index):
            in_block_comment = True
            index += 2
            continue
        code += line[index]
        index += 1
    return code, in_block_comment


def main():
    files = subprocess.check_output(["git", "ls-files", *ROOTS], text=True).split()
    problems = []
    for path in files:
        if not path.endswith(EXTENSIONS) or path in ALLOWED:
            continue
        in_block_comment = False
        with open(path, encoding="utf-8", errors="replace") as source:
            for number, line in enumerate(source, 1):
                code, in_block_comment = code_part(line, in_block_comment)
                # Ignore the text of string literals
                code = re.sub(r'"(\\.|[^"\\])*"', '""', code)
                if STD_STRING.search(code):
                    problems.append(f"{path}:{number}: use String or StringView, not std::string or std::string_view")
                if CONST_REF_PARAM.search(code):
                    problems.append(f"{path}:{number}: a read-only text parameter is StringView, not const String&")

    if problems:
        print("\n".join(problems))
        print(f"\n{len(problems)} problem(s). See Skills/strings.md.")
        return 1
    print("String type check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
