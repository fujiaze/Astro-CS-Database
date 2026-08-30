#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""pack_audit_package.py — 打包基线代码(不含数据/第三方/二进制) + 证据, 构成审核包 zip。
产物: artifacts/prerelease_v5/AUDIT_PACKAGE_<c12>.zip(目标 <10MB, 无数据)。
用法: python3 tools/pack_audit_package.py
"""
from __future__ import annotations

import datetime, hashlib, json, os, pathlib, zipfile

REPO = pathlib.Path(__file__).resolve().parent.parent
OUT = REPO / "artifacts/prerelease_v5"

# 顶层贡献: 这些路径作为"代码"或"证据"收录
CODE_TOPS = {"cli", "include", "lib", "tools", "tests", "schemas", "docs", "launch"}
EVIDENCE_TOPS = {"reports", "工程控制/RELEASE_V5", "artifacts/prerelease_v5/AUDIT_REVIEW",
                 "artifacts/prerelease_v5/tables"}
ROOT_FILES = {"VERSION", "README.md", "AGENTS.md", "build.sh", "toolchain.ps1", "CHANGELOG.md",
              "memory.md", "FATDUCK_ACCESS.md", "HANDOVER.md", "VISUAL_CHECK_README.md",
              ".clang-format", ".gitignore", ".gitattributes", ".editorconfig", "CMakeLists.txt"}

# 排除: 第三方/数据/二进制/运行时/大产物
EXCLUDE_SUBSTR = ("/third_party/", "/build/", "/builds/", "/run/", "/testdata/", "/BASS DR3/",
                  "/AstroCS.wiki/", "/artifacts/prerelease_v5/ISA-", "/artifacts/prerelease_v5/capsules/",
                  "/astrocs_run_")
EXCLUDE_EXT = {".fts", ".fit", ".fits", ".xisf", ".zip", ".dll", ".lib", ".a", ".o", ".so", ".exe",
               ".pdb", ".obj", ".exp", ".cache", ".pyc"}


def sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def allowed(rel: str) -> tuple[bool, str]:
    """返回 (是否保留, 包内目标路径 of  code/ | evidence/ | root)."""
    rel = rel.replace("\\", "/")
    if any(s in ("/" + rel.replace("\\", "/")) for s in []):
        pass
    if any(x in rel for x in EXCLUDE_SUBSTR):
        return (False, "")
    ext = os.path.splitext(rel)[1].lower()
    if ext in EXCLUDE_EXT:
        return (False, "")
    top = rel.split("/")[0]
    if rel.startswith("工程控制/"):
        if not rel.startswith("工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828"):
            return (False, "")
        return (True, "evidence/control/" + rel.split("AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/", 1)[-1])
    if rel.startswith("reports/"):
        return (True, "evidence/" + rel)
    if top in CODE_TOPS:
        return (True, "code/" + rel)
    if rel.startswith("artifacts/prerelease_v5/AUDIT_REVIEW/"):
        return (True, "evidence/audit_review/" + rel.split("AUDIT_REVIEW/", 1)[-1])
    if rel.startswith("artifacts/prerelease_v5/tables/"):
        return (True, "evidence/tables/" + rel.split("tables/", 1)[-1])
    if rel in ROOT_FILES or (top in {x for x in ROOT_FILES}):
        return (True, rel)
    return (False, "")


def git(*a):
    import subprocess
    return subprocess.run(["git", "-C", str(REPO), *a], capture_output=True, text=True).stdout


def main() -> int:
    ver = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    commit = git("rev-parse", "HEAD").strip()
    c12 = commit[:12]
    files = [f for f in git("ls-files").splitlines() if f]
    zpath = OUT / f"AUDIT_PACKAGE_{c12}.zip"
    manifest, sums = [], []
    total = 0
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        staged = []
        for rel in files:
            ok, dst = allowed(rel)
            if not ok:
                continue
            src = REPO / rel
            if not src.is_file():
                continue
            data = src.read_bytes()
            total += len(data)
            # 包内以相对路径(不带 AstroCS_V5_audit_package_ 前缀)存, 保持 code/ evidence/ 结构
            z.writestr(dst, data)
            staged.append((dst, rel))
            manifest.append({"path": dst, "source": rel, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        md = f"# AstroCS V5 审核包(基线代码+证据)\n\n- 版本: `{ver}`\n- 基线来源提交: `{commit}` (`{c12}`)\n- 生成: {datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}\n- 主机: vm-bj Linux amd64\n- 范围: AstroCS 自有基线源码(cli/ include/ lib/* 不含 third_party/, tools/ tests/ schemas/ docs/ launch/) + 证据(reports/evidence, 工程控制/RELEASE_V5/V5控制包, artifacts/prerelease_v5/AUDIT_REVIEW, artifacts/prerelease_v5/tables)。不含数据(testdata/, BASS DR3, .fts/.fit/.xisf/.zip)、不含 vendored 第三方(lib/astro_image_io/third_party, 需按各自版本单独获取以可控编译)、不含构建产物/运行时(run/, build/, *.dll/.a/.o/.so/.exe)。\n"
        z.writestr("00_README.md", md.encode("utf-8"))
        manifest.append({"path": "00_README.md", "source": "(generated)", "size": len(md.encode()),
                         "sha256": hashlib.sha256(md.encode()).hexdigest()})
        # MANIFEST.json
        mj = json.dumps({"schema_version": 1, "package": f"AstroCS-audit-{c12}",
                         "version": ver, "commit": commit, "total_bytes": total, "files": manifest},
                        indent=1, ensure_ascii=False)
        z.writestr("MANIFEST.json", mj.encode("utf-8"))
        manifest.append({"path": "MANIFEST.json", "source": "(generated)", "size": len(mj.encode()),
                         "sha256": hashlib.sha256(mj.encode()).hexdigest()})
        # SHA256SUMS (所有已写文件)
        lines = [f"{m['sha256']}  {m['path']}" for m in manifest]
        ss = "\n".join(lines) + "\n"
        z.writestr("SHA256SUMS", ss.encode("utf-8"))
    print(f"[pack] {zpath}  bytes={total}  files={len(manifest)-3}  zip_bytes={zpath.stat().st_size}")
    print(f"[pack] MIB={total/1024/1024:.2f}  zip_MIB={zpath.stat().st_size/1024/1024:.2f}")
    print(f"[pack] <10MB target: {'PASS' if zpath.stat().st_size < 10*1024*1024 else 'FAIL'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
