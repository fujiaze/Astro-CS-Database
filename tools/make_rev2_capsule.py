#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_rev2_capsule.py — REV-002: 完整 ARCH/API/headers/core-source/oracles/checkers/Linux-reports 审阅胶囊。
覆盖(03 L144): 索引/hash/全文件完整; REVIEW_PENDING 后继续 Windows 探测。
范围: docs/architecture + docs/api + 头文件 + 核心源码 + oracle 测试 + checkers + Linux reports
      + TRACEABILITY + 审阅胶囊索引 + 本包控制快照(复制版)。
禁止: 二进制/真实数据/FITS/HiPS/build/.git/大日志。
输出: artifacts/prerelease_v5/capsules/REV-002_<commit12>.zip
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import time
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "artifacts", "prerelease_v5", "capsules")
MAX_BYTES = 5 * 1024 * 1024
MAX_TOTAL = 25 * 1024 * 1024


def git(*a):
    return subprocess.run(["git", "-C", REPO, *a], capture_output=True, text=True,
                          check=True).stdout.strip()


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def collect_files() -> list[tuple[str, str]]:
    """返回 (rel_dest, rel_source) 需打包的相对路径。"""
    out: list[tuple[str, str]] = []

    def add(pattern: str, src_dir: str, dest_dir: str = None) -> None:
        dest_dir = dest_dir or os.path.basename(src_dir)
        for dp, _dn, fns in os.walk(src_dir):
            for fn in fns:
                if fn.endswith(".pyc"):
                    continue
                if fn == "__init__.py":
                    continue
                full = os.path.join(dp, fn)
                rel = os.path.relpath(full, src_dir)
                # 排除 archive/legacy/third_party/build
                if any(seg in ("archive", "legacy", "third_party", "build", "run") for seg in rel.split(os.sep)):
                    continue
                out.append((os.path.join(dest_dir, rel), os.path.relpath(full, REPO)))

    add("*.md", os.path.join(REPO, "docs", "architecture"), "docs/architecture")
    add("*.md", os.path.join(REPO, "docs", "api"), "docs/api")

    # 头文件(lib/**/include + cli + include)
    for root in ["lib", "cli"]:
        rd = os.path.join(REPO, root)
        for dp, _dn, fns in os.walk(rd):
            if any(seg in ("archive", "legacy", "third_party", "build", "run") for seg in dp.split(os.sep)):
                continue
            if "include" in dp.split(os.sep) or root == "cli":
                for fn in fns:
                    if fn.endswith((".h", ".hpp", ".h.in")):
                        full = os.path.join(dp, fn)
                        out.append((os.path.join("headers", root, os.path.relpath(full, rd)),
                                    os.path.relpath(full, REPO)))
    # 头文件(Root 的 include/)
    idir = os.path.join(REPO, "include")
    if os.path.isdir(idir):
        for dp, _dn, fns in os.walk(idir):
            for fn in fns:
                if fn.endswith((".h", ".hpp")):
                    full = os.path.join(dp, fn)
                    out.append((os.path.join("headers", "_root_include", fn),
                                os.path.relpath(full, REPO)))

    # 核心源码(lib/**/src + cli/*.cpp)
    for root in ["lib", "cli"]:
        rd = os.path.join(REPO, root)
        for dp, _dn, fns in os.walk(rd):
            if any(seg in ("archive", "legacy", "third_party", "build", "run") for seg in dp.split(os.sep)):
                continue
            for fn in fns:
                if fn.endswith((".cpp", ".c", ".cc")):
                    full = os.path.join(dp, fn)
                    out.append((os.path.join("source", root, os.path.relpath(full, rd)),
                                os.path.relpath(full, REPO)))

    # oracle 测试
    for f in ["tests/backend/test_calibration_oracle.py", "tests/backend/test_wcs_psf_oracle.py",
              "tests/backend/test_noise_model_oracle.py", "tests/backend/test_drizzle_oracle.py",
              "tests/backend/test_phase3_reproject_oracle.py", "tests/api/test_upm_recovery_oracle.py",
              "tests/api/test_reject_integration_oracle.py"]:
        full = os.path.join(REPO, f)
        if os.path.isfile(full):
            out.append((os.path.join("oracles", f), f))

    # checkers
    for dp, _dn, fns in os.walk(os.path.join(REPO, "tools")):
        if any(seg in ("__pycache__", "archive") for seg in dp.split(os.sep)):
            continue
        for fn in fns:
            if fn.startswith("check_") and fn.endswith(".py"):
                full = os.path.join(dp, fn)
                out.append((os.path.join("checkers", os.path.relpath(full, os.path.join(REPO, "tools"))),
                            os.path.relpath(full, REPO)))

    # Linux reports
    for fn in sorted(os.listdir(os.path.join(REPO, "reports", "evidence"))):
        if fn.startswith("LNX") and fn.endswith(".md"):
            out.append((os.path.join("reports", fn), os.path.join("reports", "evidence", fn)))

    # traceability + 审阅胶囊索引 + 本包控制快照
    for f in ["artifacts/prerelease_v5/tables/TRACEABILITY.csv",
              "artifacts/prerelease_v5/tables/COMMITS.csv",
              "artifacts/prerelease_v5/tables/REVIEW_CAPSULE_INDEX.csv"]:
        full = os.path.join(REPO, f)
        if os.path.isfile(full):
            out.append((os.path.join("tables", os.path.basename(f)), f))
    return out


def main() -> int:
    commit = git("rev-parse", "HEAD")
    c12 = commit[:12]
    items = sorted(set(collect_files()))
    zpath = os.path.join(OUT, f"REV-002_{c12}.zip")
    index = {"task_id": "REV-002", "commit": commit,
             "generated_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
             "host": "vm-bj Linux amd64", "files": [], "total_bytes": 0}
    total = 0
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for dest, src in items:
            full = os.path.join(REPO, src)
            if not os.path.isfile(full):
                continue
            data = open(full, "rb").read()
            if len(data) > MAX_BYTES:
                z.writestr(dest + ".OVERSIZED_REGISTERED_ONLY", "")
                index["files"].append({"path": dest, "sha256": sha(data), "size": len(data), "included": False})
                continue
            z.writestr(dest, data)
            index["files"].append({"path": dest, "sha256": sha(data), "size": len(data), "included": True})
            total += len(data)
        index["total_bytes"] = total
        # 索引自身
        idx_bytes = json.dumps(index, ensure_ascii=False, indent=1).encode()
        z.writestr("INDEX.json", idx_bytes)
        z.writestr("SHA256SUMS", "".join(
            f"{e['sha256']}  {e['path']}\n" for e in index["files"]) +
            sha(idx_bytes) + "  INDEX.json\n")
    if total > MAX_TOTAL:
        print(f"WARN: total {total} > {MAX_TOTAL}", file=sys.stderr)
    print(zpath, total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
