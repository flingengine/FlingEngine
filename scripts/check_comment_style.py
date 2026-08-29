#!/usr/bin/env python3
"""
Check that first-party C++ sources follow the FlingEngine doc-comment style
(see Skills/coding-style.md and issue #166).

Fails if any of these legacy patterns remain:
  - @brief tags (prefer a plain description line)
  - /*! doc comments (prefer /**)
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOTS = ("FlingEngine", "Sandbox", "FlingTests")
EXTENSIONS = {".h", ".hpp", ".cpp", ".cc", ".cxx"}

PATTERNS = [
    (re.compile(r"@brief\b"), "@brief tag (use a plain description instead)"),
    (re.compile(r"/\*!"), "/*! doc comment (use /** instead)"),
]


def find_sources(repo_root: Path) -> list[Path]:
    files: list[Path] = []
    for root_name in ROOTS:
        root = repo_root / root_name
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if path.suffix in EXTENSIONS and path.is_file():
                files.append(path)
    return sorted(files)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
    )
    args = parser.parse_args()

    violations = 0
    for path in find_sources(args.repo_root):
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(args.repo_root)
        for lineno, line in enumerate(text.splitlines(), start=1):
            for pattern, message in PATTERNS:
                if pattern.search(line):
                    print(f"{rel}:{lineno}: {message}")
                    print(f"  {line.strip()}")
                    violations += 1

    if violations:
        print(f"\nFound {violations} comment-style violation(s).")
        return 1

    print("Comment style check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
