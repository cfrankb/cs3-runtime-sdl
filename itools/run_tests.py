#!/usr/bin/env python3
"""Build and run the cs3-tests suite from the repo root.

Usage: run_tests.py [--no-build] [test-name-filter]
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build", action="store_true", help="skip cmake build")
    ap.add_argument("filter", nargs="?", help="only run tests whose name contains this")
    args = ap.parse_args()

    build = ROOT / "build"
    if not args.no_build:
        if not (build / "CMakeCache.txt").exists():
            subprocess.run(["cmake", "-B", "build"], cwd=ROOT, check=True)
        subprocess.run(["cmake", "--build", "build", "--target", "cs3-tests"],
                       cwd=ROOT, check=True)

    exe = build / "tests" / "cs3-tests"
    if not exe.exists():
        sys.exit(f"missing {exe} (build it first)")

    # the suite must run from the repo root (relative tests/in, tests/out paths)
    cmd = [str(exe)]
    if not args.filter:
        return subprocess.run(cmd, cwd=ROOT).returncode

    # filter: keep headers plus blocks whose name matches
    proc = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)
    keep, capture = [], False
    for line in proc.stdout.splitlines():
        if "==> " in line:
            capture = args.filter in line
        if capture:
            keep.append(line)
    print("\n".join(keep))
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
