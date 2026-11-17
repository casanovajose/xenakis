#!/usr/bin/env python3
"""Fail if the kernel names a feature xPlugin."""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN = (
    "xen.groups",
    "xen.paths",
    "xen.transport",
    "xen.osc",
    "GroupsXPlugin",
    "PathsXPlugin",
    "xplugins/",
    "xplugins\\",
)


def main() -> int:
    failures = []
    for path in (ROOT / "kernel").rglob("*"):
        if path.suffix.lower() not in {".h", ".hpp", ".cpp", ".cxx", ".cc"}:
            continue
        text = path.read_text(encoding="utf-8")
        for token in FORBIDDEN:
            if token in text:
                failures.append(f"{path.relative_to(ROOT)} contains {token}")
    if failures:
        print("kernel boundary check failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print("kernel boundary check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
