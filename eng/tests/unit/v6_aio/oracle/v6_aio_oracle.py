#!/usr/bin/env python3
"""IMPL-AIO-001 独立 Oracle 驱动（正例）。"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from v6_aio_oracle_lib import run_checks  # noqa: E402


def main(argv):
    if len(argv) != 3:
        print("usage: v6_aio_oracle.py <artifacts_dir> <repo_root>")
        return 2
    failures = run_checks(argv[1], argv[2])
    if failures:
        print("ORACLE FAIL (%d)" % len(failures))
        for f in failures:
            print("  - " + f)
        return 1
    print("ORACLE PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
