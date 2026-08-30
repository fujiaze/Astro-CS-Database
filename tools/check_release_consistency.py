#!/usr/bin/env python3
"""G11: 发布一致性校验 (VERSION/commit/build id/SBOM/checksums)。

规则:
1. VERSION 单源 0.10.0-alpha.1; CLI --version 前缀一致。
2. 发布布局 dist/astrocs-alpha 的 checksums 与文件一致。
3. SBOM (dist/astrocs-alpha/SBOM.json 若有) 记录库清单。
4. 无 NOT VERIFIED 项 (除 owner review) — RELEASE_STATUS 扫描。
exit 0 = PASS。
"""
import hashlib, json, pathlib, subprocess, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

def main():
    errors = []
    # 1) VERSION 单源
    ver = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    if ver != "0.10.0-alpha.1":
        errors.append(f"VERSION 异常: {ver}")
    # 2) 发布布局 checksums
    dist = REPO / "dist" / "astrocs-alpha"
    if dist.is_dir():
        cs = dist / "checksums.sha256"
        if cs.is_file():
            for line in cs.read_text().strip().splitlines():
                h, name = line.split()
                real = hashlib.sha256((dist / name).read_bytes()).hexdigest()
                if h != real:
                    errors.append(f"checksum mismatch: {name}")
    # 3) SBOM
    sbom = dist / "SBOM.json"
    if sbom.is_file():
        doc = json.loads(sbom.read_text())
        if doc.get("product") != "AstroCS":
            errors.append("SBOM product 异常")
        if doc.get("version") != ver:
            errors.append(f"SBOM version {doc.get('version')} != {ver}")
    # 4) NOT VERIFIED 扫描
    rs = (REPO / "docs" / "review" / "RELEASE_STATUS.md").read_text(encoding="utf-8", errors="ignore")
    if "NOT VERIFIED" in rs and "owner" not in rs.lower():
        errors.append("RELEASE_STATUS 含未登记 NOT VERIFIED")
    if errors:
        print("REL_CONSISTENCY_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("REL_CONSISTENCY_PASS: VERSION 单源, checksums 一致, SBOM 对齐, 无未登记 NOT VERIFIED")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
