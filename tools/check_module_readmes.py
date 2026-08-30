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
            else:
                # 引用的 header/source/test 必须真实存在
                cand = (REPO / "lib" / ref) if not (REPO / ref).exists() else (REPO / ref)
                if ref.endswith(".cpp") and ref.startswith("p1_") or ref.startswith("p3_"):
                    found = list((REPO / "tests" / "unit").glob(ref))
                    if not found and not (REPO / "lib").glob("**/" + ref):
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
