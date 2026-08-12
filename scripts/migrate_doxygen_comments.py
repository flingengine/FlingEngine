#!/usr/bin/env python3
"""
One-time migration: convert verbose XML/Doxygen-wizard comments to the
project's preferred style (issue #166).

Transforms (first-party sources only):
  - /*! ... */  ->  /** ... */
  - Remove @brief tags; keep the description text
  - Drop empty @brief-only lines
  - Drop useless auto-generated @return lines like "@return float GetTimef"
  - Collapse runs of blank comment lines inside a doc block
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOTS = ("FlingEngine", "Sandbox", "FlingTests")
EXTENSIONS = {".h", ".hpp", ".cpp", ".cc", ".cxx"}

# Match a doc-comment line that is only "@brief" (optional whitespace).
EMPTY_BRIEF = re.compile(r"^(\s*\*)\s*@brief\s*$")
# Match "@brief <text>" and keep the text.
BRIEF_WITH_TEXT = re.compile(r"^(\s*\*)\s*@brief\s+(.*)$")
# Auto-generated "@return <type> <name>" with no real description
# (e.g. "@return float GetTimef", "@return const float& m_nearPlane").
USELESS_RETURN = re.compile(
    r"^(\s*\*)\s*@return\s+"
    r"(?:const\s+)?"
    r"(?:unsigned\s+)?"
    r"(?:[\w:]+(?:\s*<*[\w:\s,]*>)?(?:\s*[*&])?)\s+"
    r"([A-Za-z_][\w]*)\s*$"
)
# Blank interior comment line: " *" or " * " with only whitespace after *.
BLANK_COMMENT_LINE = re.compile(r"^\s*\*\s*$")
# Opening of a C-style doc comment (/** or /*!), possibly with content.
DOC_OPEN = re.compile(r"/\*[\*!]")


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


# Single-line or opening-line forms: "/** @brief text */" / "/** @brief text"
OPENING_BRIEF = re.compile(
    r"^(\s*/\*\*)\s*@brief\s*(.*?)(\s*\*/\s*)?$"
)


def _is_interior_blank(stripped: str) -> bool:
    """True for a middle comment line that is only '*' / '* '."""
    if stripped.lstrip().startswith("/**") or stripped.rstrip().endswith("*/"):
        return False
    return bool(BLANK_COMMENT_LINE.match(stripped))


def transform_doc_block(block: str) -> str:
    """Transform the interior of a /** ... */ block (including delimiters)."""
    # Normalize /*! to /**
    if block.startswith("/*!"):
        block = "/**" + block[3:]

    lines = block.splitlines(keepends=True)
    out: list[str] = []

    for line in lines:
        stripped = line.rstrip("\r\n")
        newline = line[len(stripped) :]

        opening = OPENING_BRIEF.match(stripped)
        if opening:
            prefix, text, closer = opening.group(1), opening.group(2).strip(), opening.group(3)
            if closer:
                body = f" {text} " if text else " "
                out.append(f"{prefix}{body}*/{newline}")
            else:
                out.append(f"{prefix}{newline}" if not text else f"{prefix} {text}{newline}")
            continue

        if EMPTY_BRIEF.match(stripped):
            continue

        m = BRIEF_WITH_TEXT.match(stripped)
        if m:
            prefix, text = m.group(1), m.group(2).rstrip()
            out.append(f"{prefix} {text}{newline}" if text else f"{prefix}{newline}")
            continue

        if USELESS_RETURN.match(stripped):
            continue

        out.append(line)

    if not out:
        return block

    # Drop leading blank interior lines (right after /**).
    while len(out) > 2 and _is_interior_blank(out[1].rstrip("\r\n")):
        del out[1]

    # Drop trailing blank interior lines (right before */).
    while len(out) > 2 and _is_interior_blank(out[-2].rstrip("\r\n")):
        del out[-2]

    # Collapse consecutive blank interior lines.
    collapsed: list[str] = []
    prev_blank = False
    for line in out:
        blank = _is_interior_blank(line.rstrip("\r\n"))
        if blank and prev_blank:
            continue
        collapsed.append(line)
        prev_blank = blank

    return "".join(collapsed)


def transform_file_text(text: str) -> str:
    result: list[str] = []
    i = 0
    n = len(text)

    while i < n:
        m = DOC_OPEN.search(text, i)
        if not m:
            result.append(text[i:])
            break

        start = m.start()
        result.append(text[i:start])

        # Find end of this comment block.
        end = text.find("*/", start + 3)
        if end == -1:
            result.append(text[start:])
            break
        end += 2  # include */

        block = text[start:end]
        result.append(transform_doc_block(block))
        i = end

    return "".join(result)


def process_file(path: Path, dry_run: bool) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = transform_file_text(original)
    if updated == original:
        return False
    if not dry_run:
        path.write_text(updated, encoding="utf-8")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Report files that would change without writing",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository root (default: parent of scripts/)",
    )
    args = parser.parse_args()

    changed = []
    for path in find_sources(args.repo_root):
        if process_file(path, dry_run=args.dry_run):
            changed.append(path.relative_to(args.repo_root))

    action = "Would update" if args.dry_run else "Updated"
    for rel in changed:
        print(f"{action}: {rel}")
    print(f"{action} {len(changed)} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
