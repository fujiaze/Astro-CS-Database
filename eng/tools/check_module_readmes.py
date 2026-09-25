#!/usr/bin/env python3
"""DOC-003: 每模块 L2 README 完整性校验。

规则: 模块 README 必须含: 合同 ID (P?-xxx/SCI/ALG), header 路径, source 路径,
test 路径 (存在), 且不抄完整公式 (摘要式)。
检查模块: phase1/{stars,wcs,photometry,noise}, phase3_session。
路径为 ARCH-001 迁移后位置（迁移前 = lib/phase1/{stars,wcs,photometry,noise}/README.md，
逐条映射见 eng/cmake/ARCH-001-migration-manifest.md §1 条目 25-28；lib/phase3_session 属
INT-001 拟删的 Session 型模块，本门只核其 README 完整性）。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[2]
MODULES = [
    ("lib/algorithms/star_detection/wrapper_phase1/README.md", ["star_detector.h", "star_detector.cpp", "p1_stars_test.cpp"]),
    ("lib/algorithms/platesolve/wrapper_phase1/README.md",     ["wcs_tan.h", "wcs_tan.cpp", "p1_wcs_phot_test.cpp"]),
    ("lib/algorithms/photometry/wrapper_phase1/README.md",     ["photometer.h", "photometer.cpp", "p1_wcs_phot_test.cpp"]),
    # 2026-09-20 订正（CHK-MODULE-MANIFEST / MODULE-READMES 悬空引用）：
    # 期望引用原为 noise_model.{h,cpp}，该二文件已不存在（NOISE-MODEL-CANON-001，
    # 负责人 §9.67 定案 3「选对的那一套」：旧 noise_model.cpp 是
    # lib/algorithms/noise_snr/cpp/src/noise_model.cpp 的退化子集，已退役）。
    # 现行文件为 snr_frame_science.{h,cpp}（CMakeLists.txt:618-640 同口径）。
    ("lib/algorithms/noise_snr/wrapper_phase1/README.md",      ["snr_frame_science.h", "snr_frame_science.cpp", "p1_noise_test.cpp"]),
    ("lib/phase3_session/README.md",                           ["p3_wcs.h", "p3_output.h", "p3_wcs_test.cpp", "p3_assembly_test.cpp"]),
]

def check_registry_anchors(errors):
    """DOC-003 增补（一页纸 S1-2「红灯被改写成绿灯」）：

    registry 生成页的生成依据锚 + 声明面内的无锚结论字面量 + 产物脚本里写死的绿结论。
    判据全部落在 eng/tools/quality/check_conclusion_anchors.py（带 --self-test 正负例）。
    **载不进来也判红**：不允许因"门没跑起来"而静默放过。
    """
    import importlib.util
    gate = REPO / "eng/tools/quality/check_conclusion_anchors.py"
    if not gate.is_file():
        errors.append("CONCLUSION_ANCHORS_GATE_MISSING: %s" % gate)
        return
    try:
        spec = importlib.util.spec_from_file_location("_concl_anchors", gate)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        report, err = mod.run(REPO, mod.DEFAULT_SURFACE)
    except Exception as exc:                                    # noqa: BLE001
        errors.append("CONCLUSION_ANCHORS_GATE_LOAD_FAIL: %r" % exc)
        return
    if err:
        errors.append("CONCLUSION_ANCHORS_INPUT: %s" % err)
        return
    for v in report["violations"]:
        errors.append("CONCLUSION_ANCHOR_%s %s:%s %s"
                      % (v["rule"], v["file"], v.get("line", "-"), v["detail"]))


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
            #  lib/ 下找不到才先试 eng/tests/unit/, 两个位置都找不到才报错)
            in_lib = (REPO / "lib").glob("**/" + ref)
            found_lib = next(iter(in_lib), None) is not None
            if not found_lib and not (REPO / "lib" / ref).exists() \
                    and not (REPO / ref).exists():
                # 2026-09-21 根目录整合：tests/ → eng/tests/（引用面同步平移）。
                in_tests = list((REPO / "eng" / "tests" / "unit").glob(ref))
                if not in_tests:
                    errors.append(f"{rel}: 引用文件不存在 {ref}")
        if "L2" not in t:
            errors.append(f"{rel}: 缺 L2 标注")
    check_registry_anchors(errors)
    if errors:
        print("DOC-003_README_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"DOC-003_PASS: {len(MODULES)} 模块 README 全含 合同/header/source/test 链接")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
