#!/usr/bin/env python3
"""REL-003: 白名单审核包打包器。

白名单: 只打包本轮审核必要内容 (源码快照/L0-L2 文档/证据/清单+SHA)。
禁止: .git / build / testdata / 大型缓存 / 未声明文件。
用法: python3 scripts/package_audit.py <out_dir>
exit 0 = PASS。
"""
import hashlib, json, pathlib, subprocess, sys, datetime

REPO = pathlib.Path(__file__).resolve().parents[1]

# 白名单 (相对路径; 目录=递归含, 但跳过禁止项)
WHITELIST = [
    "docs/refactor", "docs/review", "docs/contracts", "docs/science", "docs/api",
    "evidence/refactor",
    "lib/phase1", "lib/phase2/src", "lib/phase3_session", "lib/core", "lib/io",
    "include", "cli", "tools", "tests/unit",
    "CMakeLists.txt", "VERSION", "REVIEW.md", "AGENTS.md",
]
FORBIDDEN_DIRS = {".git", "build", "testdata", "__pycache__", "third_party", "node_modules"}
FORBIDDEN_EXT = {".o", ".a", ".so", ".dll", ".exe", ".tmp", ".pyc"}

def main():
    if len(sys.argv) < 2:
        print("usage: package_audit.py <out_dir>"); return 2
    out_dir = pathlib.Path(sys.argv[1])
    out_dir.mkdir(parents=True, exist_ok=True)
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO,
                          capture_output=True, text=True).stdout.strip()
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    manifest = {
        "kind": "astrocs_audit_package",
        "commit": head,
        "created_utc": stamp,
        "files": {},
    }
    n = 0
    for entry in WHITELIST:
        base = REPO / entry
        if base.is_dir():
            for p in sorted(base.rglob("*")):
                if not p.is_file(): continue
                if any(fd in p.parts for fd in FORBIDDEN_DIRS): continue
                if p.suffix in FORBIDDEN_EXT: continue
                rel = str(p.relative_to(REPO))
                manifest["files"][rel] = hashlib.sha256(p.read_bytes()).hexdigest()
                n += 1
        elif base.is_file():
            rel = entry
            manifest["files"][rel] = hashlib.sha256(base.read_bytes()).hexdigest()
            n += 1
    mf_path = out_dir / "audit_manifest.json"
    mf_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    # 打包 tar.gz (不含 .git)
    import tarfile
    tarball = out_dir / f"astrocs_audit_{stamp}.tar.gz"
    with tarfile.open(tarball, "w:gz") as tf:
        for rel in sorted(manifest["files"]):
            tf.add(REPO / rel, arcname=rel)
        tf.add(mf_path, arcname="audit_manifest.json")
    print(f"PACKAGE_OK: {n} files, commit={head[:12]}, -> {tarball.name}")
    print(f"PACKAGE_SHA: {hashlib.sha256(tarball.read_bytes()).hexdigest()}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
