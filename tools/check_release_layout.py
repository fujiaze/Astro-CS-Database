#!/usr/bin/env python3
"""REL-001: 发布布局校验 (单一用户入口 + 必含文件 + 不含 build/testdata/history)。

规则:
1. 布局内只有一个可执行用户入口 (astrocs)。
2. 必含: VERSION, LICENSE, README, checksums.sha256。
3. 必不含: build/, testdata/, .git/ (history)。
4. checksums.sha256 与实际文件哈希一致。
exit 0 = PASS。
"""
import hashlib, pathlib, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
DIST = REPO / "dist" / "astrocs-alpha"

def main():
    errors = []
    if not DIST.is_dir():
        errors.append("dist/astrocs-alpha missing"); return 1
    exes = [p for p in DIST.iterdir() if p.is_file() and p.stat().st_mode & 0o111]
    if len(exes) != 1 or exes[0].name != "astrocs":
        errors.append(f"非单一入口: {[p.name for p in exes]}")
    for req in ("VERSION", "LICENSE", "README.txt", "checksums.sha256"):
        if not (DIST / req).is_file():
            errors.append(f"missing {req}")
    for banned in ("build", "testdata", ".git", "history"):
        for p in DIST.rglob("*"):
            if p.is_dir() and p.name == banned:
                errors.append(f"不应含 {banned}")
    # checksum 校验
    cs = (DIST / "checksums.sha256").read_text(encoding="utf-8").strip()
    for line in cs.splitlines():
        h, name = line.split()
        real = hashlib.sha256((DIST / name).read_bytes()).hexdigest()
        if h != real:
            errors.append(f"checksum mismatch {name}")
    if errors:
        print("REL-001_LAYOUT_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("REL-001_PASS: 单一入口 astrocs, 必含文件齐, checksum 一致, 无 build/testdata/history")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
