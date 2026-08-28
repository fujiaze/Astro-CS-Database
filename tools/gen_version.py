#!/usr/bin/env python3
"""VER-001 唯一版本源生成接口。
读取仓库根 VERSION（唯一权威版本源）+ git 状态，输出 --version --json 合同对象。
规则见 docs/VERSIONING.md；版本格式合同见控制包 13_ALPHA_VERSION_AND_PHASE3.md §1。
"""
import hashlib, json, os, re, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEMVER_ALPHA = re.compile(r"^(\d+)\.(\d+)\.(\d+)-alpha\.(\d+)$")
# ABI/schema 版本的唯一定义点: ABI-001/ API-002 冻结后置 1
ABI_VERSION = "0"
CLI_SCHEMA_VERSION = "0"
FORBIDDEN_PRERELEASE = ("stable", "rc", "beta", "release")

def read_base_version(path=None):
    p = path or os.path.join(REPO, "VERSION")
    with open(p, encoding="utf-8") as f:
        raw = f.read().strip()
    if not SEMVER_ALPHA.match(raw):
        raise SystemExit(f"VERSION 源非法(必须 MAJOR.MINOR.PATCH-alpha.N, 拒绝 {FORBIDDEN_PRERELEASE}): {raw!r}")
    low = raw.lower()
    if any(t in low for t in FORBIDDEN_PRERELEASE):
        raise SystemExit(f"VERSION 源含禁止的 prerelease 标记: {raw}")
    return raw

def git(args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True).stdout.strip()

def build_report(base=None, commit=None, dirty=None):
    base = base or read_base_version()
    commit = commit or git(["rev-parse", "HEAD"])
    if dirty is None:
        dirty = bool(git(["status", "--porcelain"]))
    c12 = commit[:12]
    suffix = f"+g{c12}" + (".dirty" if dirty else "")
    return {
        "version": f"{base}{suffix}",
        "prerelease": "alpha",
        "commit": commit,
        "dirty": dirty,
        "build_id": f"g{c12}" + (".dirty" if dirty else ""),
        "abi_version": ABI_VERSION,
        "cli_schema_version": CLI_SCHEMA_VERSION,
    }

def main():
    rep = build_report()
    if "--json" in sys.argv:
        print(json.dumps(rep, ensure_ascii=False, indent=1))
    else:
        print(rep["version"])
    return 0

if __name__ == "__main__":
    sys.exit(main())
