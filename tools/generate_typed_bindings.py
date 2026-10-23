#!/usr/bin/env python3
"""Generate the typed operation bindings, TypeIds, schemas and codecs.

The single declaration source is include/acsd/model.hpp (read through the
Clang AST); the process/operation whitelist is
docs/engineering/rebuild/processes/README.md; probe ids come from each process
document; static module dependencies come from
docs/engineering/rebuild/SYSTEM.md.

Generated files:
  include/acsd/modules/generated/inputs.hpp
  include/acsd/modules/generated/codecs.hpp
  include/acsd/modules/generated/status.hpp
  src/runtime/generated/generated_hooks.hpp
  src/runtime/generated/codecs.cpp
  src/runtime/generated/typed_operations.cpp
  src/runtime/generated/builtin_processes.cpp
  src/runtime/generated/status.cpp

Usage:
  tools/generate_typed_bindings.py            # write the files
  tools/generate_typed_bindings.py --check    # fail if a file is stale
"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from codec_gen.emit import generate  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="do not write; fail if a generated file is stale")
    parser.add_argument("--root", default=str(ROOT),
                        help="repository root (defaults to this script's parent)")
    args = parser.parse_args()
    changed = generate(Path(args.root).resolve(), check_only=args.check)
    if args.check:
        if changed:
            print("typed bindings are stale; rerun "
                  "tools/generate_typed_bindings.py")
            for path in changed:
                print(f"  - {path}")
            return 1
        print("typed bindings are up to date")
        return 0
    for path in changed:
        print(f"wrote {path}")
    if not changed:
        print("typed bindings are up to date")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
