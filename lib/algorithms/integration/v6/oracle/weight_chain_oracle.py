#!/usr/bin/env python3
# weight_chain_oracle.py — Phase2 权重链独立 FP64 Oracle（Python 侧）
#
# 独立性边界:
#   - 本脚本不 import/link/exec 任何被测 C++ 实现；
#   - 从权威公式独立复算: w = SNR^2/F_ref^2 = 1/sigma_F^2 (Horne 1986 通量型口径)，
#     稀疏层 实际 SNR = 帧级 × 帧内，双线性重建独立手写；
#   - C++ 侧对拍由 oracle/weight_chain_selfcheck.cpp 用其自身独立复算路径完成；
#     两者用同一组合成常量，便于交叉核对。
#
# 正例: 注入已知 SNR ⇒ 权重可复算（rtol 1e-12）。
# 负例: SNR 缺失/非有限/非正、F_ref 非法、稀疏层空/越界、SNR 语义冒充、
#       legacy 静默降级 ⇒ 参考合同必须 fail-closed（不得退化为等权）。
#
# 用法: python3 weight_chain_oracle.py [--out run/RELEASE-02/weight-chain/oracle_result.json]
import argparse
import json
import math
import os
import sys

RTOL = 1e-12
NODE_TOL = 1e-9

# ── 独立复算路径（不调用被测实现） ──────────────────────────────────────
def oracle_weight_from_snr(snr, fref):
    """w = 1/sigma_F^2, sigma_F = F_ref/SNR  (通量型口径)."""
    sigma_f = fref / snr
    return 1.0 / (sigma_f * sigma_f)

def oracle_bilinear(layer, x, y):
    nx, ny = layer["nx"], layer["ny"]
    gx = (x - layer["x0"]) / layer["dx"]
    gy = (y - layer["y0"]) / layer["dy"]
    i0 = min(max(int(math.floor(gx)), 0), nx - 2)
    j0 = min(max(int(math.floor(gy)), 0), ny - 2)
    fx, fy = gx - i0, gy - j0
    pts = layer["points"]  # row-major j*nx+i
    v00 = pts[j0 * nx + i0]["snr"]
    v10 = pts[j0 * nx + i0 + 1]["snr"]
    v01 = pts[(j0 + 1) * nx + i0]["snr"]
    v11 = pts[(j0 + 1) * nx + i0 + 1]["snr"]
    return (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10 + (1 - fx) * fy * v01 + fx * fy * v11

# ── 参考合同（描述必须行为，用于负例） ────────────────────────────────
class Unclosed(Exception):
    def __init__(self, code, why):
        super().__init__(why)
        self.code = code

def contract_compute(frames, fref, legacy_allow_weight_fallback=False):
    """参考合同实现。任何退化 -> Unclosed；legacy 请求仍 Unclosed。"""
    if not frames:
        raise Unclosed("unclosed_empty_input", "no input frames")
    if not (math.isfinite(fref) and fref > 0):
        raise Unclosed("unclosed_invalid_reference_flux", "F_ref invalid")
    out = []
    any_sparse = False
    for k, f in enumerate(frames):
        tag = f.get("frame_id") or ("frame#%d" % k)
        if f.get("kind") != "flux_type_unweighted_snr":
            raise Unclosed("unclosed_wrong_snr_semantics", tag + ": wrong SNR semantics")
        if not f.get("has_frame_snr", False):
            raise Unclosed("unclosed_missing_frame_snr", tag + ": frame SNR missing")
        s = f.get("frame_snr")
        if not (isinstance(s, (int, float)) and math.isfinite(s) and s > 0):
            raise Unclosed("unclosed_invalid_frame_snr", tag + ": frame SNR invalid")
        intra = 1.0
        if f.get("sparse") is not None:
            L = f["sparse"]
            if not L.get("present", False):
                raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": layer absent")
            pts = L.get("points") or []
            if not pts:
                raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": layer empty")
            if L.get("regular_grid"):
                nx, ny = L["nx"], L["ny"]
                if nx < 2 or ny < 2 or len(pts) != nx * ny:
                    raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": grid corrupt")
                for j in range(ny):
                    for i in range(nx):
                        p = pts[j * nx + i]
                        ex = L["x0"] + i * L["dx"]
                        ey = L["y0"] + j * L["dy"]
                        if abs(p["x"] - ex) > L.get("grid_tol", 1e-6) or abs(p["y"] - ey) > L.get("grid_tol", 1e-6):
                            raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": grid mismatch")
                        if not (math.isfinite(p["snr"]) and p["snr"] > 0):
                            raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": bad node")
                gx = (f["x"] - L["x0"]) / L["dx"]
                gy = (f["y"] - L["y0"]) / L["dy"]
                tol = L.get("grid_tol", 1e-6)
                if gx < -tol or gx > (nx - 1) + tol or gy < -tol or gy > (ny - 1) + tol:
                    raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": out of domain")
                intra = oracle_bilinear(L, f["x"], f["y"])
                any_sparse = True
            else:
                if not (isinstance(L.get("max_radius_px"), (int, float)) and L["max_radius_px"] > 0):
                    raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": no radius")
                d2 = min((f["x"] - p["x"]) ** 2 + (f["y"] - p["y"]) ** 2 for p in pts)
                if d2 > L["max_radius_px"] ** 2 * (1 + 1e-12):
                    raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": out of radius")
                nearest = min(pts, key=lambda p: (f["x"] - p["x"]) ** 2 + (f["y"] - p["y"]) ** 2)
                intra = nearest["snr"]
                any_sparse = True
        if not (math.isfinite(intra) and intra > 0):
            raise Unclosed("unclosed_invalid_intra_snr", tag + ": intra invalid")
        actual = s * intra
        if not (math.isfinite(actual) and actual > 0):
            raise Unclosed("unclosed_invalid_intra_snr", tag + ": composed invalid")
        out.append({"frame_id": tag, "intra_snr": intra, "actual_snr": actual,
                    "weight": oracle_weight_from_snr(actual, fref)})
    # 显式 legacy 请求：底层失败时仍必须 Unclosed（此处只有成功路径到达）
    if legacy_allow_weight_fallback:
        pass  # 成功路径无需降级；失败路径在调用点已 Unclosed
    return {"ok": True, "weight_chain_closed": True, "production_allowed": True,
            "closure": "closed",
            "weight_source": "frame_snr_x_sparse_snr" if any_sparse else "frame_snr",
            "reference_flux": fref, "frames": out}

def rel_close(a, b, rtol=RTOL):
    if not (math.isfinite(a) and math.isfinite(b)):
        return False
    return abs(a - b) <= rtol * max(1.0, abs(a), abs(b))

def grid2x2():
    return {"present": True, "regular_grid": True, "nx": 2, "ny": 2,
            "x0": 0.0, "y0": 0.0, "dx": 1.0, "dy": 1.0, "grid_tol": 1e-6,
            "points": [{"x": 0, "y": 0, "snr": 1.0}, {"x": 1, "y": 0, "snr": 1.2},
                       {"x": 0, "y": 1, "snr": 0.8}, {"x": 1, "y": 1, "snr": 1.0}]}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="run/RELEASE-02/weight-chain/oracle_result.json")
    args = ap.parse_args()

    passed, failed = [], []
    def check(cond, what):
        (passed if cond else failed).append(what)
        print(("  [PASS] " if cond else "  [FAIL] ") + what)

    print("== weight_chain oracle (independent Python FP64) ==")

    # ── 正例 1: 标量 w = SNR^2/F_ref^2 ──
    print("[positive] scalar w = SNR^2/F_ref^2")
    fref = 1000.0
    for s in (200.0, 100.0, 50.0):
        w = oracle_weight_from_snr(s, fref)
        check(rel_close(w, s * s / (fref * fref)), "w == SNR^2/F_ref^2 for SNR=%.1f" % s)

    # ── 正例 2: 多帧（无稀疏层）+ 独立帧恒等式 ──
    print("[positive] multi-frame without sparse layer")
    frames = [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True, "frame_snr": 200.0},
              {"frame_id": "f1", "kind": "flux_type_unweighted_snr", "has_frame_snr": True, "frame_snr": 100.0},
              {"frame_id": "f2", "kind": "flux_type_unweighted_snr", "has_frame_snr": True, "frame_snr": 50.0}]
    r = contract_compute(frames, fref)
    check(r["closure"] == "closed" and r["production_allowed"], "chain closed")
    check(r["weight_source"] == "frame_snr", "weight_source = frame_snr")
    sum_w = sum(fr["weight"] for fr in r["frames"])
    sum_snr2 = sum(f["frame_snr"] ** 2 for f in frames)
    check(rel_close(sum_w * fref * fref, sum_snr2), "SNR_combined^2 = Sum SNR_k^2")

    # ── 正例 3: 稀疏层 帧级×帧内（双线性） ──
    print("[positive] sparse layer composition (frame x intra)")
    L = grid2x2()
    frames = [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
               "frame_snr": 200.0, "sparse": L, "x": 0.5, "y": 0.5},
              {"frame_id": "f1", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
               "frame_snr": 100.0, "sparse": L, "x": 0.25, "y": 0.5}]
    r = contract_compute(frames, fref)
    check(r["weight_source"] == "frame_snr_x_sparse_snr", "weight_source = frame_snr_x_sparse_snr")
    i0 = oracle_bilinear(L, 0.5, 0.5)
    i1 = oracle_bilinear(L, 0.25, 0.5)
    check(rel_close(r["frames"][0]["intra_snr"], i0), "intra[0] bilinear")
    check(rel_close(r["frames"][1]["intra_snr"], i1), "intra[1] bilinear")
    check(rel_close(r["frames"][0]["actual_snr"], 200.0 * i0), "actual = frame x intra")
    check(rel_close(r["frames"][0]["weight"], oracle_weight_from_snr(200.0 * i0, fref)),
          "weight from composed SNR")
    # 节点复现
    node_ok = all(rel_close(oracle_bilinear(L, p["x"], p["y"]), p["snr"], 1e-12) for p in L["points"])
    check(node_ok, "bilinear reproduces control nodes (node residual ~ 0)")

    # ── 负例: 合同必须 fail-closed ──
    print("[negative] contract fail-closed cases")
    def expect_unclosed(name, frames_, fref_=1000.0, code=None, legacy=False):
        try:
            contract_compute(frames_, fref_, legacy_allow_weight_fallback=legacy)
            check(False, name + " (unexpectedly succeeded)")
        except Unclosed as e:
            check(code is None or e.code == code, name + " -> " + e.code)

    expect_unclosed("missing frame SNR",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": False}],
                    code="unclosed_missing_frame_snr")
    for bad in (float("nan"), float("inf"), -3.0, 0.0):
        expect_unclosed("invalid SNR %r" % bad,
                        [{"frame_id": "f0", "kind": "flux_type_unweighted_snr",
                          "has_frame_snr": True, "frame_snr": bad}],
                        code="unclosed_invalid_frame_snr")
    for bad in (0.0, -1.0, float("nan")):
        expect_unclosed("invalid F_ref %r" % bad,
                        [{"frame_id": "f0", "kind": "flux_type_unweighted_snr",
                          "has_frame_snr": True, "frame_snr": 200.0}], fref_=bad,
                        code="unclosed_invalid_reference_flux")
    expect_unclosed("present-but-empty sparse layer",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
                      "frame_snr": 200.0, "sparse": {"present": True, "regular_grid": True,
                                                     "nx": 2, "ny": 2, "x0": 0, "y0": 0,
                                                     "dx": 1, "dy": 1, "points": []},
                      "x": 0.5, "y": 0.5}],
                    code="unclosed_sparse_layer_unreconstructible")
    expect_unclosed("sparse layer out of domain",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
                      "frame_snr": 200.0, "sparse": grid2x2(), "x": 5.0, "y": 0.5}],
                    code="unclosed_sparse_layer_unreconstructible")
    expect_unclosed("wrong SNR semantics",
                    [{"frame_id": "f0", "kind": "relative_quality_weight",
                      "has_frame_snr": True, "frame_snr": 200.0}],
                    code="unclosed_wrong_snr_semantics")
    # legacy 请求不改变失败语义
    expect_unclosed("legacy fallback request still fail-closed",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": False}],
                    code="unclosed_missing_frame_snr", legacy=True)

    # ── 非空真: 等权与逆方差不同 ──
    print("[non-vacuity] equal weight differs from inverse-variance weight")
    r = contract_compute(frames[:1] + [{"frame_id": "f2", "kind": "flux_type_unweighted_snr",
                                        "has_frame_snr": True, "frame_snr": 50.0}], fref)
    check(any(not rel_close(fr["weight"], 1.0, 1e-9) for fr in r["frames"]),
          "inverse-variance weights not all 1.0")

    result = {
        "oracle": "weight_chain_oracle.py",
        "formula": "w = SNR^2 / F_ref^2 = 1/sigma_F^2; sparse: actual = frame_snr * intra_snr",
        "passed": passed, "failed": failed,
        "n_passed": len(passed), "n_failed": len(failed),
        "reference_values": {
            "F_ref": fref,
            "snr": [200.0, 100.0, 50.0],
            "weights": [oracle_weight_from_snr(s, fref) for s in (200.0, 100.0, 50.0)],
            "sparse_grid_2x2": grid2x2(),
            "intra_0.5_0.5": i0,
            "intra_0.25_0.5": i1,
            "weight_f0_composed": oracle_weight_from_snr(200.0 * i0, fref),
        },
    }
    out = args.out
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, ensure_ascii=False)
    print("\n== summary: %d passed, %d failed ==" % (len(passed), len(failed)))
    print("reference fixtures -> " + out)
    return 0 if not failed else 1

if __name__ == "__main__":
    sys.exit(main())
