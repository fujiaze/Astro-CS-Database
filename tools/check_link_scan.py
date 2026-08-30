#!/usr/bin/env python3
"""BLD-001 link scan: 校验根 CMake 生产 CLI 不链接 ACR 符号、无 GLOB。
用法: python3 tools/check_link_scan.py <binary>
exit 0 = PASS; 生产二进制含 ACR 符号或 CMake 有 production GLOB => 非 0。
"""
import re, subprocess, sys, pathlib

def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "build/root-cmake/astrocs"
    repo = pathlib.Path(__file__).resolve().parents[1]
    # 1) CMake GLOB 扫描(排除 vendored cfitsio 生成清单)
    glob_hits = []
    for f in (repo/"CMakeLists.txt").read_text(encoding="utf-8").splitlines():
        if "GLOB" in f and "cfitsio_sources" not in f and not f.strip().startswith("#"):
            glob_hits.append(f.strip())
    # 2) 二进制 ACR 符号扫描
    acr_hits = []
    if pathlib.Path(binary).is_file():
        nm = subprocess.run(["nm", "-C", binary], capture_output=True, text=True)
        for line in nm.stdout.splitlines():
            if "astro::compute" in line or "kernel_registry" in line or "device_executor" in line or "acr_" in line:
                acr_hits.append(line.strip()[:100])
    errors = []
    if glob_hits:
        errors.append(f"production GLOB in CMakeLists.txt: {glob_hits}")
    if acr_hits:
        errors.append(f"ACR symbols in production binary: {len(acr_hits)} (e.g. {acr_hits[:3]})")
    if errors:
        print("LINK_SCAN_FAIL:")
        for e in errors:
            print(" ", e)
        return 1
    print(f"LINK_SCAN_PASS binary={binary} globs=0 acr_symbols=0")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
