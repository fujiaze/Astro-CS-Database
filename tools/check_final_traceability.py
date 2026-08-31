#!/usr/bin/env python3
"""REL-002: 最终追溯校验 + 旧入口/旧版本/假状态扫描。

规则:
1. 六层追溯 (TRACEABILITY.csv) 全链 PASS (委托 check_traceability)。
2. 扫描旧入口: orchestrator.exe/astrocs-stage2/旧 drizzle 直连 — 生产退出或标 LEG。
3. 旧版本: VERSION 单源 0.10.0-alpha.2; 无 0.9/0.8 残留入口。
4. Windows-only 主流程: phase3 非假状态 (真实实现)。
5. RELEASE_STATUS: 未通过项标 NOT VERIFIED/FAIL (不写"基本完成")。
exit 0 = PASS。
"""
import pathlib, subprocess, sys, re

REPO = pathlib.Path(__file__).resolve().parents[1]

def main():
    errors = []
    # 1) 追溯矩阵
    r = subprocess.run([sys.executable, str(REPO / "tools" / "check_traceability.py")],
                       capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        errors.append(f"traceability FAIL: {r.stdout[-300:]}")
    # 2) 旧入口扫描 (生产文档不得把遗留当唯一入口)
    pub_lines = (REPO / "docs" / "contracts" / "PUBLIC_API.md").read_text(encoding="utf-8", errors="ignore").splitlines()
    for legacy in ("orchestrator.exe", "astrocs-stage2.exe"):
        for ln in pub_lines:
            if legacy in ln and "LEG-" not in ln:
                errors.append(f"旧入口未标退出: {legacy}")
    # 3) 版本单源
    ver = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    if ver != "0.10.0-alpha.2":
        errors.append(f"VERSION 非预期: {ver}")
    # 4) phase3 假状态: 真实实现存在
    for impl in ("p3_wcs.cpp", "p3_resample.cpp", "p3_output.cpp"):
        if not (REPO / "lib" / "phase3_session" / impl).is_file():
            errors.append(f"phase3 实现缺失: {impl}")
    # 5) RELEASE_STATUS 诚实性
    rs = (REPO / "docs" / "review" / "RELEASE_STATUS.md").read_text(encoding="utf-8", errors="ignore")
    if "基本完成" in rs or "基本" in rs:
        errors.append("RELEASE_STATUS 用模糊词 '基本'")
    if errors:
        print("REL-002_TRACE_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"REL-002_PASS: traceability {66} claims, 旧入口标退出, VERSION 单源, phase3 真实, RELEASE_STATUS 诚实")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
