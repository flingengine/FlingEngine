#!/usr/bin/env python3
"""
Check that ECS system headers are included only where the rules allow
(see Plans/ecs-systems.md section 3.3). Systems never call other systems, so
a file may include "Systems/<Name>System.h" only if it is:

  - that system's own .cpp: src/Systems/<Name>System.cpp,
  - a module register file: Register*Systems.cpp,
  - a test under FlingTests/.

Also checks the layout: each file in inc/Systems/ is <Name>System.h, and each
file in src/Systems/ is <Name>System.cpp.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOTS = ("FlingEngine", "Sandbox", "FlingTests")
EXTENSIONS = {".h", ".hpp", ".inl", ".cpp", ".cc", ".cxx"}

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]Systems/([^">]+)[">]')
REGISTER_FILE = re.compile(r"^Register\w*Systems\.cpp$")
SYSTEM_HEADER = re.compile(r"^\w+System\.h$")
SYSTEM_SOURCE = re.compile(r"^\w+System\.cpp$")


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


def include_allowed(rel: Path, included: str) -> bool:
    if rel.parts[0] == "FlingTests":
        return True
    if REGISTER_FILE.match(rel.name):
        return True
    # A system's own .cpp in src/Systems/ may include its own header.
    if rel.parent.name == "Systems" and rel.parent.parent.name == "src":
        return Path(included).stem == rel.stem
    return False


def layout_problem(rel: Path) -> str | None:
    if rel.parts[0] == "FlingTests" or rel.parent.name != "Systems":
        return None
    folder = rel.parent.parent.name
    if folder == "inc" and not SYSTEM_HEADER.match(rel.name):
        return "a file in inc/Systems/ must be one system header named <Name>System.h"
    if folder == "src" and not SYSTEM_SOURCE.match(rel.name):
        return "a file in src/Systems/ must be one system body named <Name>System.cpp"
    return None


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
        rel = path.relative_to(args.repo_root)

        problem = layout_problem(rel)
        if problem:
            print(f"{rel}: {problem}")
            violations += 1

        text = path.read_text(encoding="utf-8", errors="replace")
        for lineno, line in enumerate(text.splitlines(), start=1):
            match = INCLUDE.match(line)
            if match and not include_allowed(rel, match.group(1)):
                print(f"{rel}:{lineno}: includes a system header. Systems never call other systems:")
                print("  move the shared code to a *Util function, or read the data the system writes.")
                print(f"  {line.strip()}")
                violations += 1

    if violations:
        print(f"\nFound {violations} system include violation(s). See Plans/ecs-systems.md section 3.3.")
        return 1

    print("System include check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
