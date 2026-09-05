#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""V8-CI-009 可重放注入脚本：N11（validate_candidate 上传内容注入，离线）。

用法：python3 inject_n11_validate_candidate.py
覆盖：FITS / header 成员 → 期望 exit 1 excluded_entry_in_zip；
      绝对路径 / '..' 段成员（SHA256SUMS 完备）→ 现状 exit 0（GAP-G3）。
"""
import hashlib
import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[5]          # logs → …/evidence → 仓库根
VALIDATOR = REPO / "ci" / "validate_candidate.py"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def make_zip(entries: dict, path: Path) -> Path:
    with zipfile.ZipFile(path, "w") as zf:
        for name, blob in sorted(entries.items()):
            zf.writestr(name, blob)
    return path


def build(tag: str, extra: dict, tmp: Path) -> Path:
    readme = b"readme\n"
    dll = b"MZ" + b"\x00" * 32
    provenance = json.dumps({"schema_version": 1, "source_sha": "a" * 40,
                             "built_utc": "2026-01-01T00:00:00Z",
                             "preset": {"x": "y"}, "acr_enabled": False}).encode()
    manifest = json.dumps({
        "schema_version": 1, "file_count": 2,
        "files": [{"path": "README.txt", "sha256": sha256(readme)},
                  {"path": "lib/a.dll", "sha256": sha256(dll)}]}).encode()
    entries = {"README.txt": readme, "lib/a.dll": dll,
               "BUILD_PROVENANCE.json": provenance, "SOURCE_MANIFEST.json": manifest}
    for name, blob in extra.items():
        entries[name] = blob
    entries["SHA256SUMS"] = "".join(
        f"{sha256(blob)}  {name}\n" for name, blob in sorted(entries.items())).encode()
    return make_zip(entries, tmp / f"cand_{tag}.zip")


def run(zip_path: Path):
    proc = subprocess.run([sys.executable, str(VALIDATOR), str(zip_path), "--json"],
                          cwd=str(REPO), capture_output=True, text=True, timeout=60)
    return proc.returncode, proc.stdout


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for tag, extra, expect in (
                ("fits", {"data/frame.fits": b"SIMPLE  = T"}, "exit 1 excluded_entry_in_zip"),
                ("header", {"include/extra.h": b"#pragma once\n"}, "exit 1 excluded_entry_in_zip"),
                ("escape", {"/abs/escape.dll": b"UP", "../escape.dll": b"UP"},
                 "现状 exit 0（GAP-G3：路径逃逸无守卫）")):
            rc, out = run(build(tag, extra, tmp))
            signal = ""
            if tag == "escape":
                signal = "（无拒绝信号）"
            else:
                try:
                    errs = json.loads(out).get("errors") or []
                    signal = "; ".join(str(e.get("code", e)) if isinstance(e, dict) else str(e)
                                       for e in errs)
                except Exception:
                    signal = out[:120]
            print(f"N11[{tag}] exit={rc}  期望: {expect}\n      信号: {signal}")


if __name__ == "__main__":
    main()
