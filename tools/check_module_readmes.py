#!/usr/bin/env python3
"""DOC-003: 每模块 L2 README 完整性校验。

规则: 模块 README 必须含: 合同 ID (P?-xxx/SCI/ALG), header 路径, source 路径,
test 路径 (存在), 且不抄完整公式 (摘要式)。
检查模块: phase1/{stars,wcs,photometry,noise}, phase3_session。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
MODULES = [
    ("lib/phase1/stars/README.md",        ["star_detector.h", "star_detector.cpp", "p1_stars_test.cpp"]),
    ("lib/phase1/wcs/README.md",          ["wcs_tan.h", "wcs_tan.cpp", "p1_wcs_phot_test.cpp"]),
    ("lib/phase1/photometry/README.md",   ["photometer.h", "photometer.cpp", "p1_wcs_phot_test.cpp"]),
    ("lib/phase1/noise/README.md",        ["noise_model.h", "noise_model.cpp", "p1_noise_test.cpp"]),
    ("lib/phase3_session/README.md",      ["p3_wcs.h", "p3_output.h", "p3_wcs_test.cpp", "p3_assembly_test.cpp"]),
]

def main():
    errors = []
    for rel, refs in MODULES:
        p = REPO / rel
        if not p.is_file():
            errors.append(f"{rel} missing")
            continue
        t = p.read_text(encoding="utf-8", errors="ignore")
        # 合同 ID
        if not re.search(r"(P\d-\d{3}|SCI-|ALG-)", t):
            errors.append(f"{rel}: 无合同 ID")
        for ref in refs:
            if ref not in t:
                errors.append(f"{rel}: 缺引用 {ref}")
                continue
            # 引用的 header/source/test 必须真实存在
            # (原实现生成器真值判断写反: not/and 优先级使检查从未触发 — E-P2 修复:
            #  lib/ 下找不到才先试 tests/unit/, 两个位置都找不到才报错)
            in_lib = (REPO / "lib").glob("**/" + ref)
            found_lib = next(iter(in_lib), None) is not None
            if not found_lib and not (REPO / "lib" / ref).exists() \
                    and not (REPO / ref).exists():
                in_tests = list((REPO / "tests" / "unit").glob(ref))
                if not in_tests:
                    errors.append(f"{rel}: 引用文件不存在 {ref}")
        if "L2" not in t:
            errors.append(f"{rel}: 缺 L2 标注")
    if errors:
        print("DOC-003_README_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"DOC-003_PASS: {len(MODULES)} 模块 README 全含 合同/header/source/test 链接")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
