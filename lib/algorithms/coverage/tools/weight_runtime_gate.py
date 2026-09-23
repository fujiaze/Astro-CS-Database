# lib/algorithms/coverage/tools/weight_runtime_gate.py — G7 运行时权重 gate
#
# 验证（真实 Phase1 SNR Catalogue + stage2 diagnostics）：
#   1. 同一帧内不同空间区域（control cell 级）存在不同局部 SNR；
#   2. stage2 diagnostics 显示**单一权重口径**（逐帧 ivar 逆方差）真的被使用：
#      `local_ivar_used > 0`，且 `ivar_product_missing` 登记在位。
#
# §9.73 裁决 A44 订正（WEIGHTMODE-CLEANUP-01）：
#   本工具原第 2 项断言 `local_snr_used > 0` **且** `frame_snr_median_fallback > 0`。
#   这两个诊断键随 legacy 整数权重模式域一并**删除**（stage2.cpp 只保留
#   `local_ivar_used` / `ivar_product_missing`）⇒ 原断言**恒不可满足**（必然 FAIL），
#   即「门本身是错的」（AGENTS §9：检查器静默退化/恒假判据都算未完成）。
#   现改为正向断言单一 ivar 口径 + **已删键出现即判红**（防止降级面悄悄复活），
#   并加 `--self-test`（能红能绿 + fail-closed）。
#
# 用法：
#   python3 lib/algorithms/coverage/tools/weight_runtime_gate.py \
#       --hips <phase1 .hips> --diag <mosaic .hips>/diagnostics.json
#   python3 lib/algorithms/coverage/tools/weight_runtime_gate.py --self-test
import argparse
import json
import os
import sys
import tempfile

import numpy as np

# §9.73 裁决 A44：这些键在 diagnostics 里**出现即判红**（已删除的权重模式域
# 与已删除的降级面；复活 = 单一权重口径被破坏）。
DELETED_DIAG_KEYS = (
    "weight_mode",
    "legacy_allow_weight_fallback",
    "local_snr_used",
    "frame_snr_median_fallback",
    "ivar_tile_read_fallback_pixels",
)


def check_snr_spread(hips):
    """1. 同一帧 SNR catalogue 空间差异（按 ra 分箱比较局部 SNR）。"""
    snr_dir = os.path.join(hips, "snr", "Norder7")
    if not os.path.isdir(snr_dir):
        print("FAIL: 无 SNR catalogue 目录 %s" % snr_dir)
        return 1
    tiles = []
    for d in sorted(os.listdir(snr_dir)):
        p = os.path.join(snr_dir, d)
        if not os.path.isdir(p):
            continue
        for f in sorted(os.listdir(p)):
            if f.startswith("Npix") and f.endswith(".tsv"):
                tiles.append(os.path.join(p, f))
    if not tiles:
        print("no snr tiles")
        return 1
    rows = []
    for t in tiles:
        with open(t, encoding="utf-8") as fh:
            for line in fh:
                parts = line.split()
                if len(parts) >= 4 and parts[0].lstrip("-").isdigit():
                    rows.append((float(parts[1]), float(parts[3])))  # ra, snr
    if len(rows) < 20:
        print("FAIL: 不足两个 tile 有 SNR")
        return 1
    ras = np.array([r[0] for r in rows])
    snrs = np.array([r[1] for r in rows])
    half = np.median(ras)
    low = snrs[ras < half]
    high = snrs[ras >= half]
    if low.size == 0 or high.size == 0:
        print("FAIL: ra 分箱后一侧为空，判据退化")
        return 1
    spread = float(np.median(high) - np.median(low))
    print("[weight-gate] frame-local SNR spread (ra bins) = %.3f "
          "(n_stars=%d, low_med=%.2f, high_med=%.2f)"
          % (spread, len(rows), float(np.median(low)), float(np.median(high))))
    if abs(spread) < 1e-6:
        print("FAIL: 同一帧不同区域无 SNR 差异")
        return 1
    return 0


def check_single_weight_path(diag):
    """2. diagnostics：单一 ivar 口径真的被使用；已删键不得出现。"""
    try:
        with open(diag, encoding="utf-8") as f:
            d = json.load(f)
    except OSError as e:
        print("FAIL: diagnostics 不可读（fail-closed）：%s" % e)
        return 1
    except ValueError as e:
        print("FAIL: diagnostics 不是合法 JSON（fail-closed）：%s" % e)
        return 1
    if not isinstance(d, dict):
        print("FAIL: diagnostics 顶层不是对象")
        return 1
    revived = [k for k in DELETED_DIAG_KEYS if k in d]
    if revived:
        print("FAIL: diagnostics 出现已按 §9.73 A44 删除的键 %s "
              "⇒ 单一权重口径被破坏（降级面/模式号复活）" % revived)
        return 1
    if "local_ivar_used" not in d:
        print("FAIL: diagnostics 缺 local_ivar_used ⇒ 无法证明 ivar 口径被使用")
        return 1
    used = d.get("local_ivar_used", 0)
    missing = d.get("ivar_product_missing")
    print("[weight-gate] local_ivar_used=%s ivar_product_missing=%s"
          % (used, missing))
    if not isinstance(used, (int, float)) or used <= 0:
        print("FAIL: local_ivar_used <= 0 ⇒ 逐帧 ivar 逆方差口径未被使用")
        return 1
    if missing is None:
        print("FAIL: diagnostics 缺 ivar_product_missing ⇒ 缺 ivar 面未登记")
        return 1
    return 0


def run(hips, diag):
    rc = check_snr_spread(hips)
    if rc:
        return rc
    rc = check_single_weight_path(diag)
    if rc:
        return rc
    print("WEIGHT_RUNTIME_GATE=PASS")
    return 0


def _mk_hips(root, snrs):
    d = os.path.join(root, "snr", "Norder7", "Npix0")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "Npix0.tsv"), "w", encoding="utf-8") as fh:
        for i, s in enumerate(snrs):
            fh.write("%d %f 0 %f\n" % (i, 100.0 + i, s))


def self_test():
    cases = []
    with tempfile.TemporaryDirectory() as tmp:
        hips_ok = os.path.join(tmp, "ok.hips")
        _mk_hips(hips_ok, [1.0 + 0.1 * i for i in range(40)])
        hips_flat = os.path.join(tmp, "flat.hips")
        _mk_hips(hips_flat, [5.0] * 40)

        def diag(name, obj):
            p = os.path.join(tmp, name)
            with open(p, "w", encoding="utf-8") as fh:
                json.dump(obj, fh)
            return p

        good = diag("good.json", {"local_ivar_used": 1234,
                                  "ivar_product_missing": 0})
        cases.append(("green_clean", run(hips_ok, good), 0))
        cases.append(("red_no_snr_spread", run(hips_flat, good), 1))
        cases.append(("red_weight_mode_revived", run(
            hips_ok, diag("wm.json", {"local_ivar_used": 10,
                                     "ivar_product_missing": 0,
                                     "weight_mode": 2})), 1))
        cases.append(("red_legacy_fallback_revived", run(
            hips_ok, diag("lf.json", {"local_ivar_used": 10,
                                     "ivar_product_missing": 0,
                                     "legacy_allow_weight_fallback": False})), 1))
        cases.append(("red_degradation_counter_revived", run(
            hips_ok, diag("dc.json", {"local_ivar_used": 10,
                                     "ivar_product_missing": 0,
                                     "local_snr_used": 5})), 1))
        cases.append(("red_ivar_not_used", run(
            hips_ok, diag("z.json", {"local_ivar_used": 0,
                                     "ivar_product_missing": 0})), 1))
        cases.append(("red_missing_registration", run(
            hips_ok, diag("m.json", {"local_ivar_used": 10})), 1))
        cases.append(("failclosed_missing_diag", run(
            hips_ok, os.path.join(tmp, "nope.json")), 1))
        cases.append(("failclosed_missing_hips", run(
            os.path.join(tmp, "nope.hips"), good), 1))
    ok = True
    for name, got, want in cases:
        good_case = (got == want)
        ok = ok and good_case
        print("SELFTEST_%s %s (rc=%s want=%s)"
              % ("PASS" if good_case else "FAIL", name, got, want))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hips", default=os.path.join(
        "run", "temp", "phase1_freeze", "t4_full_v3_final.hips"))
    ap.add_argument("--diag", default=os.path.join(
        "run", "phase2", "t4_overlap_sigma.mosaic.hips", "diagnostics.json"))
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    return run(a.hips, a.diag)


if __name__ == "__main__":
    sys.exit(main())
