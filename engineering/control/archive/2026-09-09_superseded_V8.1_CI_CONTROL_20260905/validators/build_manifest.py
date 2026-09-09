#!/usr/bin/env python3
"""Build deterministic manifest and SHA256SUMS for this package."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    excluded = {"CONTROL_MANIFEST.json", "SHA256SUMS"}
    files = []
    for path in sorted((p for p in root.rglob("*") if p.is_file() and p.name not in excluded), key=lambda p: p.relative_to(root).as_posix()):
        rel = path.relative_to(root).as_posix()
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        files.append({"path": rel, "size": path.stat().st_size, "sha256": digest})
    manifest = {"schema_version": 1, "package": root.name, "files": files}
    (root / "CONTROL_MANIFEST.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (root / "SHA256SUMS").write_text("".join(f"{item['sha256']}  {item['path']}\n" for item in files), encoding="utf-8")
    print(f"MANIFEST_BUILT files={len(files)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
