#!/usr/bin/env python3
"""Generate MANIFEST/SHA256SUMS, validate, then create a size-capped audit ZIP."""

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
        print("usage: package_audit.py EVIDENCE_DIR OUTPUT.zip", file=sys.stderr)
        return 2
    root = pathlib.Path(sys.argv[1]).resolve()
    output = pathlib.Path(sys.argv[2]).resolve()
    if not root.is_dir() or output.suffix.lower() != ".zip":
        print("invalid input directory or output suffix", file=sys.stderr)
        return 2

    # Do not allow an old ZIP to become part of a newly generated package.
    if output == root or root in output.parents:
        print("output ZIP must be outside evidence directory", file=sys.stderr)
        return 2

    manifest_path = root / "MANIFEST.json"
    sums_path = root / "SHA256SUMS"
    content = sorted(
        p for p in root.rglob("*")
        if p.is_file() and p.name not in {"MANIFEST.json", "SHA256SUMS"}
    )
    entries = [
        {
            "path": p.relative_to(root).as_posix(),
            "size_bytes": p.stat().st_size,
            "sha256": sha(p),
        }
        for p in content
    ]
    manifest = {
        "schema": "AstroCS-audit-manifest-v3",
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "file_count": len(entries),
        "total_bytes": sum(item["size_bytes"] for item in entries),
        "files": entries,
    }
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    sum_files = sorted(p for p in root.rglob("*") if p.is_file() and p.name != "SHA256SUMS")
    sums_path.write_text(
        "".join(f"{sha(p)}  {p.relative_to(root).as_posix()}\n" for p in sum_files),
        encoding="utf-8",
    )

    validator = pathlib.Path(__file__).with_name("validate_audit_package.py")
    check = subprocess.run([sys.executable, str(validator), str(root)], check=False)
    if check.returncode:
        return check.returncode

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output.name + ".", suffix=".tmp", dir=output.parent, delete=False) as tmp:
        tmp_path = pathlib.Path(tmp.name)
    try:
        with zipfile.ZipFile(tmp_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(p for p in root.rglob("*") if p.is_file()):
                archive.write(path, arcname=f"{root.name}/{path.relative_to(root).as_posix()}")
        if tmp_path.stat().st_size > MAX_ZIP:
            print(f"ZIP exceeds 25MiB: {tmp_path.stat().st_size}", file=sys.stderr)
            return 1
        tmp_path.replace(output)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()
    print(f"PACKAGE_PASS path={output} bytes={output.stat().st_size} sha256={sha(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
