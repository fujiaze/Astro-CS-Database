#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""pack_audit_package.py — 打包基线代码(不含数据/第三方/二进制) + 证据, 构成审核包 zip。
产物: artifacts/prerelease_v5/AUDIT_PACKAGE_<c12>.zip(目标 <10MB, 无数据)。
用法: python3 tools/pack_audit_package.py

退役 (RETIRED, 2026-09-16, 负责人裁决) —— **打包入口(main)退役; 白名单/排除函数仍为活动依赖**:
  1. 依据: ASTROCS_DESIGN.md §0(权威链: 旧世代控制包产物不构成判据)+ 负责人裁决
     「历史版本控制包全部作废; artifacts/ 不归档不保留」(artifacts/ 整体删除见 commit b1290525);
     产物内含 VERSION/CHANGELOG 口径, 与 §12「Alpha 前不含任何版本信息」冲突;
  2. 输出已不存在: artifacts/prerelease_v5/(其 AUDIT_PACKAGE_*.zip 与 capsules/ 已随 artifacts/ 删除);
  3. 实测 2026-09-16: 该目录若已被旁路重建则 rc=0 **静默产出 11.2MB / 2709 条目**、
     zip_bytes=10.67 超过 10MB 目标仍 exit 0; 目录不存在则未捕获 FileNotFoundError
     —— 两种形态都属 ENGINEERING_SPEC.md §8 禁止的「静默坏掉」;
  4. **保留不退役的部分(活动替代即其自身)**: allowed()/denied()/EXCLUDE_EXT/DENY_PATHS/DENY_NAME_RE
     是**审计包收录白名单与凭据排除保证的唯一真源**, 被活动门 CHK-SECRET-HYGIENE 直接 import
     (tools/quality/check_secret_hygiene.py:53 DEFAULT_PACKER → pack_files() 调 allowed()),
     并由 tests/quality/test_secret_hygiene.py 的能绿能红证据覆盖 —— 不得删改语义;
  5. 发布候选打包门为 ci/checks.json 的 CHK-PACKAGE; 正式证据落 reports/**。
  复原命令 (内容未丢): git show 01754fab8618:tools/pack_audit_package.py
  退役后行为: 运行本文件打印 PACK_AUDIT_PACKAGE_RETIRED 说明并 exit 2 (fail-closed, 不伪装绿);
     作为模块 import 时不受影响(allowed/denied 语义不变)。
  登记见 docs/ci/01_CHECKS.md §2.2 与 reports/PROJECT-GOVERNANCE-01/retire/RETIREMENT_LEDGER.md。
"""
from __future__ import annotations

import datetime, hashlib, json, os, pathlib, re, sys, zipfile

REPO = pathlib.Path(__file__).resolve().parent.parent
OUT = REPO / "artifacts/prerelease_v5"

# 顶层贡献: 这些路径作为"代码"或"证据"收录
CODE_TOPS = {"cli", "include", "lib", "tools", "tests", "contracts/schemas", "docs"}
EVIDENCE_TOPS = {"reports", "工程控制/RELEASE_V5", "artifacts/prerelease_v5/AUDIT_REVIEW",
                 "artifacts/prerelease_v5/tables"}
ROOT_FILES = {"VERSION", "README.md", "AGENTS.md", "build.sh", "toolchain.ps1", "CHANGELOG.md",
              "memory.md", "HANDOVER.md", "VISUAL_CHECK_README.md",
              ".clang-format", ".gitignore", ".gitattributes", ".editorconfig", "CMakeLists.txt"}

# 排除保证（ROOT-006 / CHK-SECRET-HYGIENE）：凭据/接入类文件**永不入包**——即使被显式塞回
# 白名单也能拦住。判定只看路径形态，不读文件内容；审计包是外发给第三方审核的产物，
# 扩大暴露面没有必要。
#   1) DENY_PATHS    : 路径归一化后的精确条目（与 ROOT_FILES 解耦）；
#   2) DENY_NAME_RE  : 凭据类文件形态（.env/私钥/密钥库/*_ACCESS.md）。
DENY_PATHS = {"FATDUCK_ACCESS.md"}
DENY_NAME_RE = re.compile(
    r"(?i)(^|/)(\.env(\..*)?|\.netrc|id_(?:rsa|dsa|ecdsa|ed25519)[^/]*)$"
    r"|\.(pem|key|ppk|p12|pfx|jks|keystore)$"
    r"|(^|/)[^/]*_access\.md$")


def denied(rel: str) -> bool:
    """凭据类路径排除保证：True = 无论白名单怎么写都不入包。"""
    rel = rel.replace("\\", "/")
    while rel.startswith("./"):
        rel = rel[2:]
    if rel in DENY_PATHS:
        return True
    return bool(DENY_NAME_RE.search(rel))

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
    if denied(rel):                     # 排除保证先于一切白名单判定
        return (False, "")
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


RETIRED_NOTICE = (
    "PACK_AUDIT_PACKAGE_RETIRED: 本打包入口（旧世代 V5 审核包 zip）已于 2026-09-16 按负责人裁决退役。\n"
    "  依据: ASTROCS_DESIGN.md §0（权威链：旧世代控制包产物不构成判据）+ §12（Alpha 前不含版本信息）"
    "+ 负责人裁决（历史版本控制包全部作废；artifacts/ 不归档不保留，commit b1290525）；"
    "ENGINEERING_SPEC.md §8（不允许静默坏掉）。\n"
    "  输出路径已不存在: artifacts/prerelease_v5/AUDIT_PACKAGE_<c12>.zip。\n"
    "  仍在使用（不退役）: 本模块的 allowed()/denied()/EXCLUDE_EXT 是 CHK-SECRET-HYGIENE 的白名单真源，"
    "import 语义不变。\n"
    "  复原命令: git show 01754fab8618:tools/pack_audit_package.py\n"
    "  登记: docs/ci/01_CHECKS.md §2.2 / reports/PROJECT-GOVERNANCE-01/retire/RETIREMENT_LEDGER.md"
)


def main() -> int:
    """退役后入口：显式失败（fail-closed，禁止 traceback 崩溃，不伪装绿）。"""
    print(RETIRED_NOTICE, file=sys.stderr)
    return 2


def legacy_main() -> int:
    """退役保留实现（复用方法：按复原命令取回旧世代控制包布局后可直接复跑）。"""
    ver = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    commit = git("rev-parse", "HEAD").strip()
    c12 = commit[:12]
    files = [f for f in git("ls-files").splitlines() if f]
    zpath = OUT / f"AUDIT_PACKAGE_{c12}.zip"
    manifest, sums = [], []
    total = 0
    denied_hits = []
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        staged = []
        for rel in files:
            ok, dst = allowed(rel)
            if not ok:
                if denied(rel):
                    denied_hits.append(rel)
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
        md = f"# AstroCS V5 审核包(基线代码+证据)\n\n- 版本: `{ver}`\n- 基线来源提交: `{commit}` (`{c12}`)\n- 生成: {datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}\n- 主机: vm-bj Linux amd64\n- 范围: AstroCS 自有基线源码(lib/infrastructure/cli/ include/ lib/* 不含 third_party/, tools/ tests/ schemas/ docs/ launch/) + 证据(reports/evidence, 工程控制/RELEASE_V5/V5控制包, artifacts/prerelease_v5/AUDIT_REVIEW, artifacts/prerelease_v5/tables)。不含数据(testdata/, BASS DR3, .fts/.fit/.xisf/.zip)、不含 vendored 第三方(lib/astro_image_io/third_party, 需按各自版本单独获取以可控编译)、不含构建产物/运行时(run/, build/, *.dll/.a/.o/.so/.exe)。\n"
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
    print(f"[pack] DENY 排除(凭据类, 即使被显式指定也不入包): {len(denied_hits)} {denied_hits}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
