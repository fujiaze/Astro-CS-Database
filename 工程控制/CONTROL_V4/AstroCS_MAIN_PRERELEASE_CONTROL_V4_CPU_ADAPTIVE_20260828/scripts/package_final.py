#!/usr/bin/env python3
from __future__ import annotations

import datetime as dt
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import zipfile

MAX_ZIP = 25 * 1024 * 1024


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: package_final.py EVIDENCE_DIR OUTPUT.zip", file=sys.stderr)
        return 2
    root = pathlib.Path(sys.argv[1]).resolve()
    output = pathlib.Path(sys.argv[2]).resolve()
    if not root.is_dir() or output.suffix.lower() != ".zip" or root in output.parents:
        print("invalid paths", file=sys.stderr)
        return 2
    manifest_path = root / "MANIFEST.json"
    sums_path = root / "SHA256SUMS"
    content = sorted(p for p in root.rglob("*") if p.is_file() and p.name not in {"MANIFEST.json", "SHA256SUMS"})
    entries = [{"path": p.relative_to(root).as_posix(), "size_bytes": p.stat().st_size, "sha256": sha(p)} for p in content]
    manifest_path.write_text(json.dumps({
        "schema": "AstroCS-V4-final-audit",
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "file_count": len(entries),
        "total_bytes": sum(x["size_bytes"] for x in entries),
        "files": entries,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    sum_files = sorted(p for p in root.rglob("*") if p.is_file() and p.name != "SHA256SUMS")
    sums_path.write_text("".join(f"{sha(p)}  {p.relative_to(root).as_posix()}\n" for p in sum_files), encoding="utf-8")
    validator = pathlib.Path(__file__).with_name("validate_final_package.py")
    result = subprocess.run([sys.executable, str(validator), str(root)], check=False)
    if result.returncode:
        return result.returncode
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output.name, suffix=".tmp", dir=output.parent, delete=False) as handle:
        temp = pathlib.Path(handle.name)
    try:
        with zipfile.ZipFile(temp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(p for p in root.rglob("*") if p.is_file()):
                archive.write(path, f"{root.name}/{path.relative_to(root).as_posix()}")
        if temp.stat().st_size > MAX_ZIP:
            print(f"ZIP >25MiB: {temp.stat().st_size}", file=sys.stderr)
            return 1
        temp.replace(output)
    finally:
        if temp.exists():
            temp.unlink()
    print(f"PACKAGE_PASS bytes={output.stat().st_size} sha256={sha(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
