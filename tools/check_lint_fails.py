#!/usr/bin/env python3
"""The cross-include sample must fail lint, or this check fails CI."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    sample = ROOT / "tools" / "testdata" / "cross_include"
    completed = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "xplugin"), "lint", "--root", str(sample)],
        text=True,
        capture_output=True,
    )
    if completed.returncode == 0:
        print("expected lint to fail on tools/testdata/cross_include", file=sys.stderr)
        print(completed.stdout)
        return 1
    if "includes" not in completed.stdout:
        print(completed.stdout)
        print(completed.stderr)
        print("lint failed, but not because of a cross-include", file=sys.stderr)
        return 1
    print("cross-include sample failed lint, as required")
    return 0


if __name__ == "__main__":
    sys.exit(main())
