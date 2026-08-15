#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_v19r2_package.py — 组装 AstroCS_Review_TraceableFoundation_V19R2.zip。

打包内容（MASTER_CONTROL_SPEC §22）：
  README.md SHA256SUMS.txt reports/ evidence/ self_review/
  source/full_first_party_after.zip + source_manifest.csv + changed_only/
  docs_snapshot/
不含 build/vendor/data。
"""

from __future__ import annotations

import csv
import hashlib
import os
import shutil
import subprocess
import zipfile


ROOT = r"F:\Astro dev\Astro CS Normalization Database"
REV = os.path.join(ROOT, "reports", "v19r2")
PKG_NAME = "AstroCS_Review_TraceableFoundation_V19R2"
PKG = os.path.join(ROOT, PKG_NAME + ".zip")
TMP = os.path.join(ROOT, "run", "temp", "v19r2_pkg")

SKIP_PARTS = {"build", "build2", "_deps", "CMakeFiles", "archive",
              "__pycache__", ".git", "worktrees", "third_party"}
SKIP_EXT = {".dll", ".exe", ".o", ".a", ".pyc", ".bak", ".obj", ".log"}


def git(args: list[str]) -> str:
    r = subprocess.run(["git"] + args, cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace",
                       timeout=180)
    return r.stdout


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def collect_first_party() -> list[str]:
    out = []
    for p in git(["ls-files"]).splitlines():
        parts = p.split("/")
        if any(x in SKIP_PARTS for x in parts):
            continue
        if parts[0] in ("BASS DR3", "testdata", "GaiaDR3", "GaiaDR3SP",
                        "siril-1.4.3", "工程控制", "AstroCS.wiki"):
            continue
        ext = os.path.splitext(p)[1].lower()
        if ext in SKIP_EXT:
            continue
        out.append(p)
    return sorted(out)


def main() -> int:
    if os.path.exists(TMP):
        shutil.rmtree(TMP)
    ev = os.path.join(TMP, "evidence", "git")
    os.makedirs(ev)
    head = git(["rev-parse", "HEAD"])
    open(os.path.join(ev, "head.txt"), "w", encoding="utf-8").write(
        head + git(["log", "-1", "--format=%s"]) + "\n")
    open(os.path.join(ev, "status.txt"), "w", encoding="utf-8").write(
        git(["status", "--short", "--branch"]))
    diff = git(["diff", "c0753e33add3022e756c7bb57088414d3acf822f", "HEAD"])
    open(os.path.join(ev, "diff.patch"), "w", encoding="utf-8").write(diff)

    cmds = [
        ["git", "fetch", "origin", "refs/pull/1/head"],
        ["git", "checkout", "fix/upm-frame-order"],
        ["py -3.12", "tools/quality/check_source_inventory.py"],
        ["py -3.12", "tools/quality/check_comment_hygiene.py"],
        ["py -3.12", "tools/quality/check_traceability.py"],
        ["py -3.12", "tools/docs_machine_consistency.py"],
        [".\\toolchain.ps1", "build"],
        ["ninja -C lib/phase2/build", "phase2_synthetic_gate"],
        ["phase2_synthetic_gate.exe"],
    ]
    with open(os.path.join(ev, "exact_commands.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["command", "purpose"])
        for c in cmds:
            w.writerow([" ".join(c), "V19R2 evidence"])

    # quality evidence
    qsrc = os.path.join(REV, "evidence", "quality")
    qdst = os.path.join(TMP, "evidence", "quality")
    if os.path.isdir(qsrc):
        shutil.copytree(qsrc, qdst)
    prsrc = os.path.join(REV, "evidence", "pr1")
    if os.path.isdir(prsrc):
        shutil.copytree(prsrc, os.path.join(TMP, "evidence", "pr1"))

    # reports
    shutil.copytree(REV, os.path.join(TMP, "reports"),
                    ignore=shutil.ignore_patterns("evidence"))
    shutil.copytree(os.path.join(REV, "evidence"),
                    os.path.join(TMP, "reports", "evidence"))

    # source snapshot
    src = os.path.join(TMP, "source")
    os.makedirs(src)
    files = collect_first_party()
    with zipfile.ZipFile(os.path.join(src, "full_first_party_after.zip"),
                         "w", zipfile.ZIP_DEFLATED) as z:
        for p in files:
            z.write(os.path.join(ROOT, p), p)
    with open(os.path.join(src, "source_manifest.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["path", "size_bytes", "sha256"])
        for p in files:
            w.writerow([p, os.path.getsize(os.path.join(ROOT, p)),
                        sha256_file(os.path.join(ROOT, p))])
    # changed_only
    co = os.path.join(src, "changed_only")
    for p in git(["diff", "--name-only",
                  "c0753e33add3022e756c7bb57088414d3acf822f", "HEAD"]).splitlines():
        if not p or not os.path.exists(os.path.join(ROOT, p)):
            continue
        d = os.path.dirname(os.path.join(co, p))
        os.makedirs(d, exist_ok=True)
        shutil.copy2(os.path.join(ROOT, p), os.path.join(co, p))

    # docs_snapshot
    ds = os.path.join(TMP, "docs_snapshot")
    os.makedirs(ds)
    for f in ("README.md", "CHANGELOG.md"):
        shutil.copy2(os.path.join(ROOT, f), os.path.join(ds, f))
    shutil.copytree(os.path.join(ROOT, "docs"), os.path.join(ds, "docs"),
                    ignore=shutil.ignore_patterns("history"))

    # README
    readme = [
        "# AstroCS Review — TraceableFoundation V19R2",
        "",
        "状态：PRE_RELEASE_ENGINEERING_FOUNDATION=PASS；",
        "FINAL_REAL_DATA_VALIDATION=PENDING",
        "",
        "- 入口：reports/final_status.md、reports/full_repo_audit_summary.md",
        "- PR#1 门禁：evidence/pr1/gate_summary.md",
        "- 追溯：docs/TRACEABILITY.csv（docs_snapshot/docs/）",
        "- 自审：self_review/round0..6",
    ]
    open(os.path.join(TMP, "README.md"), "w", encoding="utf-8").write(
        "\n".join(readme) + "\n")

    if os.path.exists(PKG):
        os.remove(PKG)
    with zipfile.ZipFile(PKG, "w", zipfile.ZIP_DEFLATED) as z:
        for dp, _dn, fn in os.walk(TMP):
            for f in fn:
                full = os.path.join(dp, f)
                rel = os.path.relpath(full, TMP).replace(os.sep, "/")
                z.write(full, rel)
    # SHA256SUMS.txt（包内）
    entries = {}
    with zipfile.ZipFile(PKG) as z:
        for n in z.namelist():
            if n == "SHA256SUMS.txt":
                continue
            entries[n] = hashlib.sha256(z.read(n)).hexdigest()
    with zipfile.ZipFile(PKG, "a") as z:
        z.writestr("SHA256SUMS.txt",
                   "".join(f"{h}  {n}\n" for n, h in sorted(entries.items())))
    print(f"package: {PKG}")
    print(f"sha256: {sha256_file(PKG)}")
    print(f"entries: {len(entries)+1}, first_party_files: {len(files)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
