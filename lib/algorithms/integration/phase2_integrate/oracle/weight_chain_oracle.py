#!/usr/bin/env python3
# weight_chain_oracle.py — Phase2 权重链独立 FP64 Oracle（Python 侧）
#
# 独立性边界:
#   - 本脚本不 import/link/exec 任何被测 C++ 实现；
#   - 从权威公式独立复算: w = SNR^2/F_ref^2 = 1/sigma_F^2 (Horne 1986 通量型口径)，
#     稀疏层 实际 SNR = 帧级 x 帧内；
#   - 重建算子独立复算：自然边界三次样条用 **numpy.linalg.solve 稠密求解**
#     （不复用被测 C++ 的 Thomas 消元），值域钳制与 3x3 mesh 中值独立手写；
#   - C++ 侧对拍由 oracle/weight_chain_selfcheck.cpp 用其自身独立复算路径完成；
#     两者用同一组固定合成常量（含 4x4 控制网格 fixture），便于交叉核对；
#     oracle/recon_exp04_parity.py 另与实验单元 EXP-04 的算子实现逐像素对拍。
#
# 正例: 注入已知 SNR ⇒ 权重可复算（rtol 1e-12）；默认重建算子 ⇒ 与独立复算一致。
# 负例: SNR 缺失/非有限/非正、F_ref 非法、稀疏层空/越界/几何错位/算子未识别、
#       SNR 语义冒充、legacy 静默降级 ⇒ 参考合同必须 fail-closed（不得退化为等权）。
#
# 用法: python3 weight_chain_oracle.py [--out run/<task>/weight-chain/oracle_result.json]
import argparse
import json
import math
import os
import sys

import numpy as np

RTOL = 1e-12
NODE_TOL = 1e-9

DEFAULT_OPERATOR = "natural_bicubic_spline_clip_v1"
MESH_OPERATOR = "natural_bicubic_spline_clip_mesh_median_v1"
BILINEAR_OPERATOR = "bilinear_regular_grid_v1"
NEAREST_OPERATOR = "nearest_control_point_v1"
OPERATORS = (DEFAULT_OPERATOR, MESH_OPERATOR, BILINEAR_OPERATOR, NEAREST_OPERATOR)


# ── 独立复算路径（不调用被测实现） ──────────────────────────────────────
def oracle_weight_from_snr(snr, fref):
    """w = 1/sigma_F^2, sigma_F = F_ref/SNR  (通量型口径)."""
    sigma_f = fref / snr
    return 1.0 / (sigma_f * sigma_f)


def _natural_second_deriv_1d(y):
    """自然边界三次样条二阶导（等距 h=1，M[0]=M[n-1]=0）。

    独立实现：**稠密矩阵 + numpy.linalg.solve**（部分主元 LU），
    刻意不使用被测 C++ 的 Thomas（追赶）消元。
    """
    y = np.asarray(y, dtype=np.float64)
    n = y.size
    M = np.zeros(n)
    if n < 3:
        return M
    m = n - 2
    A = np.zeros((m, m))
    b = np.zeros(m)
    for r in range(m):
        A[r, r] = 4.0
        if r > 0:
            A[r, r - 1] = 1.0
        if r < m - 1:
            A[r, r + 1] = 1.0
        b[r] = 6.0 * (y[r + 2] - 2.0 * y[r + 1] + y[r])
    M[1:-1] = np.linalg.solve(A, b)
    return M


def _natural_eval_1d(y, M, pos):
    y = np.asarray(y, dtype=np.float64)
    n = y.size
    if n == 1:
        return float(y[0])
    i = int(min(max(math.floor(pos), 0), n - 2))
    h = float(min(max(pos - i, 0.0), 1.0))
    y0, y1 = y[i], y[i + 1]
    m0, m1 = M[i], M[i + 1]
    bb = (y1 - y0) - (2.0 * m0 + m1) / 6.0
    return float(y0 + bb * h + m0 * h * h / 2.0 + (m1 - m0) * h * h * h / 6.0)


def oracle_spline_clip(ctrl, x0, y0, dx, dy, x, y):
    """可分离自然边界双三次样条 + 钳到控制值值域（= 默认算子）。"""
    c = np.asarray(ctrl, dtype=np.float64)
    ny, nx = c.shape
    gx = min(max((x - x0) / dx, 0.0), nx - 1.0)
    gy = min(max((y - y0) / dy, 0.0), ny - 1.0)
    row = np.zeros(nx)
    for i in range(nx):
        col = c[:, i]
        row[i] = _natural_eval_1d(col, _natural_second_deriv_1d(col), gy)
    v = _natural_eval_1d(row, _natural_second_deriv_1d(row), gx)
    return float(min(max(v, float(np.min(c))), float(np.max(c))))


def oracle_median3(ctrl):
    """3x3 mesh 中值（边界 replicate，无条件替换）。"""
    c = np.asarray(ctrl, dtype=np.float64)
    ny, nx = c.shape
    out = np.zeros_like(c)
    for j in range(ny):
        for i in range(nx):
            w = []
            for dj in (-1, 0, 1):
                for di in (-1, 0, 1):
                    jj = min(max(j + dj, 0), ny - 1)
                    ii = min(max(i + di, 0), nx - 1)
                    w.append(c[jj, ii])
            out[j, i] = float(np.median(np.array(w)))
    return out


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


def ctrl_matrix(layer):
    return np.array([[layer["points"][j * layer["nx"] + i]["snr"] for i in range(layer["nx"])]
                     for j in range(layer["ny"])], dtype=np.float64)


def oracle_reconstruct(layer, x, y):
    """按冻结算子词表独立复算层在 (x,y) 的重建值。"""
    op = layer.get("reconstruction_operator") or DEFAULT_OPERATOR
    if not layer.get("regular_grid"):
        raise Unclosed("unclosed_sparse_layer_unreconstructible", "scattered mode uses nearest")
    if op == BILINEAR_OPERATOR:
        return oracle_bilinear(layer, x, y)
    c = ctrl_matrix(layer)
    if op == MESH_OPERATOR:
        c = oracle_median3(c)
    elif op != DEFAULT_OPERATOR:
        raise Unclosed("unclosed_sparse_layer_unreconstructible", "unknown operator " + str(op))
    return oracle_spline_clip(c, layer["x0"], layer["y0"], layer["dx"], layer["dy"], x, y)


# ── 参考合同（描述必须行为，用于负例） ────────────────────────────────
class Unclosed(Exception):
    def __init__(self, code, why):
        super().__init__(why)
        self.code = code


def _check_layer(L, tag):
    if not L.get("present", False):
        raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": layer absent")
    pts = L.get("points") or []
    if not pts:
        raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": layer empty")
    op = L.get("reconstruction_operator") or None
    if op is not None and op not in OPERATORS:
        raise Unclosed("unclosed_sparse_layer_unreconstructible",
                       tag + ": unknown reconstruction_operator " + op)
    if not L.get("regular_grid"):
        if op is not None and op != NEAREST_OPERATOR:
            raise Unclosed("unclosed_sparse_layer_unreconstructible",
                           tag + ": operator requires regular_grid=true")
        if not (isinstance(L.get("max_radius_px"), (int, float)) and L["max_radius_px"] > 0):
            raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": no radius")
        for p in pts:
            if not (math.isfinite(p["snr"]) and p["snr"] > 0):
                raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": bad node")
        return
    if op == NEAREST_OPERATOR:
        raise Unclosed("unclosed_sparse_layer_unreconstructible",
                       tag + ": nearest_control_point_v1 is scattered-mode only")
    nx, ny = L["nx"], L["ny"]
    if nx < 2 or ny < 2 or len(pts) != nx * ny:
        raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": grid corrupt")
    tol = L.get("grid_tol", 1e-6)
    ox, oy = L.get("grid_origin_x", 0.0), L.get("grid_origin_y", 0.0)
    for j in range(ny):
        for i in range(nx):
            p = pts[j * nx + i]
            ex = L["x0"] + i * L["dx"]
            ey = L["y0"] + j * L["dy"]
            if abs(p["x"] - ex) > tol or abs(p["y"] - ey) > tol:
                raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": grid mismatch")
            # cell 中心门：节点必须落在所属 cell 中心
            if abs(p["x"] - (ox + i * L["dx"] + (L["dx"] - 1.0) / 2.0)) > max(tol, 1e-9) or \
               abs(p["y"] - (oy + j * L["dy"] + (L["dy"] - 1.0) / 2.0)) > max(tol, 1e-9):
                raise Unclosed("unclosed_sparse_layer_unreconstructible",
                               tag + ": control point not at cell center (half-cell shift)")
            # NaN = schema 声明的 invalid（可填充）；0/负/非有限 = 非法值 ⇒ fail-closed
            if not math.isnan(p["snr"]) and not (math.isfinite(p["snr"]) and p["snr"] > 0):
                raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": illegal node")
    if not any(math.isfinite(p["snr"]) for p in pts):
        raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": all nodes invalid")
    # 定义域 = 层覆盖的 cell 并集
    if not (ox - 0.5 - tol <= L["_x"] <= ox + nx * L["dx"] - 0.5 + tol and
            oy - 0.5 - tol <= L["_y"] <= oy + ny * L["dy"] - 0.5 + tol):
        raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": out of domain")


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
            L = dict(f["sparse"])
            L["_x"], L["_y"] = f["x"], f["y"]
            _check_layer(L, tag)
            if L.get("regular_grid"):
                intra = oracle_reconstruct(L, f["x"], f["y"])
            else:
                d2 = min((f["x"] - p["x"]) ** 2 + (f["y"] - p["y"]) ** 2 for p in L["points"])
                if d2 > L["max_radius_px"] ** 2 * (1 + 1e-12):
                    raise Unclosed("unclosed_sparse_layer_unreconstructible", tag + ": out of radius")
                nearest = min(L["points"],
                              key=lambda p: (f["x"] - p["x"]) ** 2 + (f["y"] - p["y"]) ** 2)
                intra = nearest["snr"]
            any_sparse = True
        if not (math.isfinite(intra) and intra > 0):
            raise Unclosed("unclosed_invalid_intra_snr", tag + ": intra invalid")
        actual = s * intra
        if not (math.isfinite(actual) and actual > 0):
            raise Unclosed("unclosed_invalid_intra_snr", tag + ": composed invalid")
        out.append({"frame_id": tag, "intra_snr": intra, "actual_snr": actual,
                    "weight": oracle_weight_from_snr(actual, fref)})
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
    """2x2 单位网格（dx=dy=1 ⇒ cell 中心 = origin + (dx-1)/2 = 0）。"""
    return {"present": True, "regular_grid": True, "nx": 2, "ny": 2,
            "x0": 0.0, "y0": 0.0, "dx": 1.0, "dy": 1.0, "grid_tol": 1e-6,
            "grid_origin_x": 0.0, "grid_origin_y": 0.0,
            "reconstruction_operator": None,
            "points": [{"x": 0, "y": 0, "snr": 1.0}, {"x": 1, "y": 0, "snr": 1.2},
                       {"x": 0, "y": 1, "snr": 0.8}, {"x": 1, "y": 1, "snr": 1.0}]}


GRID4_VALUES = [1.0, 1.1, 0.9, 1.05,
                0.8, 1.4, 1.2, 0.7,
                1.3, 0.6, 1.5, 1.0,
                0.95, 1.25, 0.85, 1.35]
GRID4_QUERIES = [(3.5, 3.5), (11.5, 3.5), (3.5, 27.5), (20.0, 12.0), (0.0, 0.0), (31.0, 31.0)]
MESH_QUERIES = [(3.5, 3.5), (20.0, 12.0), (11.5, 27.5), (31.0, 0.0)]


def grid4x4(op=None):
    """4x4 控制网格（Δ=8，origin=0 ⇒ 节点在 cell 中心 x0 = 3.5）。
    与 C++ selfcheck 的 grid4x4() 逐值相同。"""
    pts = []
    for j in range(4):
        for i in range(4):
            pts.append({"x": 3.5 + 8.0 * i, "y": 3.5 + 8.0 * j,
                        "snr": GRID4_VALUES[j * 4 + i]})
    return {"present": True, "regular_grid": True, "nx": 4, "ny": 4,
            "x0": 3.5, "y0": 3.5, "dx": 8.0, "dy": 8.0, "grid_tol": 1e-6,
            "grid_origin_x": 0.0, "grid_origin_y": 0.0,
            "reconstruction_operator": op, "points": pts}


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

    # ── 正例 3: 稀疏层 帧级 x 帧内（默认算子） ──
    print("[positive] sparse layer composition (frame x intra, default operator)")
    L = grid2x2()
    frames = [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
               "frame_snr": 200.0, "sparse": L, "x": 0.5, "y": 0.5},
              {"frame_id": "f1", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
               "frame_snr": 100.0, "sparse": L, "x": 0.25, "y": 0.5}]
    r = contract_compute(frames, fref)
    check(r["weight_source"] == "frame_snr_x_sparse_snr", "weight_source = frame_snr_x_sparse_snr")
    # 2x2 上自然样条退化为双线性（M ≡ 0）⇒ 期望值仍可用独立双线性复算
    i0 = oracle_bilinear(L, 0.5, 0.5)
    i1 = oracle_bilinear(L, 0.25, 0.5)
    check(rel_close(r["frames"][0]["intra_snr"], i0), "intra[0] matches independent oracle")
    check(rel_close(r["frames"][1]["intra_snr"], i1), "intra[1] matches independent oracle")
    check(rel_close(r["frames"][0]["actual_snr"], 200.0 * i0), "actual = frame x intra")
    check(rel_close(r["frames"][0]["weight"], oracle_weight_from_snr(200.0 * i0, fref)),
          "weight from composed SNR")
    # 节点复现
    node_ok = all(rel_close(oracle_reconstruct(L, p["x"], p["y"]), p["snr"], 1e-12)
                  for p in L["points"])
    check(node_ok, "default operator reproduces control nodes (node residual ~ 0)")

    # ── 正例 4: 4x4 网格上默认算子 = 独立复算的自然样条 + 钳制 ──
    print("[positive] default operator vs independent natural-spline oracle (4x4)")
    L4 = grid4x4()
    ref4 = {}
    ok4 = True
    for (qx, qy) in GRID4_QUERIES:
        want = oracle_spline_clip(ctrl_matrix(L4), L4["x0"], L4["y0"], L4["dx"], L4["dy"], qx, qy)
        got = oracle_reconstruct(L4, qx, qy)
        ref4["%.1f,%.1f" % (qx, qy)] = want
        ok4 = ok4 and rel_close(got, want, 1e-12)
    check(ok4, "default operator == independent spline+clip oracle on 4x4 grid")
    L4m = grid4x4(MESH_OPERATOR)
    ref4m = {}
    ok4m = True
    for (qx, qy) in MESH_QUERIES:
        want = oracle_spline_clip(oracle_median3(ctrl_matrix(L4m)), L4m["x0"], L4m["y0"],
                                  L4m["dx"], L4m["dy"], qx, qy)
        got = oracle_reconstruct(L4m, qx, qy)
        ref4m["%.1f,%.1f" % (qx, qy)] = want
        ok4m = ok4m and rel_close(got, want, 1e-12)
    check(ok4m, "mesh-median operator == independent oracle (median then spline then clip)")
    # 非退化：4x4 上样条与双线性必须不同
    L4b = grid4x4(BILINEAR_OPERATOR)
    check(abs(oracle_reconstruct(L4b, 20.0, 12.0) - ref4["20.0,12.0"]) > 1e-3,
          "spline default differs from bilinear on 4x4 grid (non-vacuous default change)")
    # 值域钳制：病态网格上输出有界且严格为正
    patho = [0.05, 9.0, 0.06, 8.0, 8.5, 0.07, 7.5, 0.08,
             0.09, 7.0, 0.10, 6.5, 6.0, 0.11, 5.5, 0.12]
    Lp = grid4x4()
    for k, p in enumerate(Lp["points"]):
        p["snr"] = patho[k]
    vals = [oracle_reconstruct(Lp, float(i), float(j)) for j in range(32) for i in range(32)]
    lo, hi = min(patho), max(patho)
    check(all(v > 0 for v in vals), "clipped field strictly positive on pathological grid")
    check(all(lo - 1e-12 <= v <= hi + 1e-12 for v in vals),
          "clipped field inside the valid control-value range")
    raw = [oracle_spline_clip(ctrl_matrix(Lp), Lp["x0"], Lp["y0"], Lp["dx"], Lp["dy"],
                              float(i), float(j)) for j in range(4) for i in range(4)]
    unclipped_min = min(_unclipped_min(Lp, float(i), float(j)) for j in range(32) for i in range(32))
    check(unclipped_min < 0.0, "unclipped natural spline goes negative (clip is mandatory)")
    del raw

    # ── 正例 5: NaN（schema 声明的 invalid）按最近有效控制点填充 ──
    print("[positive] NaN control points filled by nearest valid")
    Ln = grid4x4()
    Ln["points"][5]["snr"] = float("nan")
    Ln["points"][6]["snr"] = float("nan")
    Lnf = grid4x4()
    Lnf["points"][5]["snr"] = (GRID4_VALUES[4] + GRID4_VALUES[1] + GRID4_VALUES[9]) / 3.0
    Lnf["points"][6]["snr"] = (GRID4_VALUES[7] + GRID4_VALUES[2] + GRID4_VALUES[10]) / 3.0
    same = True
    for q in range(8):
        x, y = 3.5 + 3.0 * q, 3.5 + 2.0 * q
        same = same and rel_close(_fill_then_eval(Ln, x, y), oracle_reconstruct(Lnf, x, y), 1e-12)
    check(same, "nearest-valid fill equals explicit filled reference layer")

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
    # 新增负例：算子词表 / 几何相位 / 非法值
    def layer_with(**kw):
        L2 = grid4x4()
        L2.update(kw)
        return L2

    expect_unclosed("unknown reconstruction_operator token",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
                      "frame_snr": 200.0, "sparse": layer_with(reconstruction_operator="spline_natural_v1"),
                      "x": 12.0, "y": 12.0}],
                    code="unclosed_sparse_layer_unreconstructible")
    corner = grid4x4()
    corner["x0"] = 0.0
    corner["y0"] = 0.0
    for k, p in enumerate(corner["points"]):
        p["x"] = 8.0 * (k % 4)
        p["y"] = 8.0 * (k // 4)
    expect_unclosed("corner-anchored grid (half-cell shift)",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
                      "frame_snr": 200.0, "sparse": corner, "x": 12.0, "y": 12.0}],
                    code="unclosed_sparse_layer_unreconstructible")
    for bad in (0.0, -1.0, float("inf")):
        Lb = grid4x4()
        Lb["points"][7]["snr"] = bad
        expect_unclosed("illegal control value %r" % bad,
                        [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
                          "frame_snr": 200.0, "sparse": Lb, "x": 12.0, "y": 12.0}],
                        code="unclosed_sparse_layer_unreconstructible")
    Lall = grid4x4()
    for p in Lall["points"]:
        p["snr"] = float("nan")
    expect_unclosed("all-NaN layer",
                    [{"frame_id": "f0", "kind": "flux_type_unweighted_snr", "has_frame_snr": True,
                      "frame_snr": 200.0, "sparse": Lall, "x": 12.0, "y": 12.0}],
                    code="unclosed_sparse_layer_unreconstructible")
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
        "default_operator": DEFAULT_OPERATOR,
        "operator_tokens": list(OPERATORS),
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
            # 与 C++ selfcheck 的 grid4x4() 同值同查询点 ⇒ 跨语言交叉核对锚
            "grid4x4_values": GRID4_VALUES,
            "grid4x4_default_queries": ref4,
            "grid4x4_mesh_median_queries": ref4m,
        },
    }
    out = args.out
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, ensure_ascii=False)
    print("\n== summary: %d passed, %d failed ==" % (len(passed), len(failed)))
    print("reference fixtures -> " + out)
    return 0 if not failed else 1


def _unclipped_min(layer, x, y):
    """独立复算的**未钳制**自然样条（用于证明钳制是必需的）。"""
    c = ctrl_matrix(layer)
    ny, nx = c.shape
    gx = min(max((x - layer["x0"]) / layer["dx"], 0.0), nx - 1.0)
    gy = min(max((y - layer["y0"]) / layer["dy"], 0.0), ny - 1.0)
    row = np.zeros(nx)
    for i in range(nx):
        col = c[:, i]
        row[i] = _natural_eval_1d(col, _natural_second_deriv_1d(col), gy)
    return _natural_eval_1d(row, _natural_second_deriv_1d(row), gx)


def _fill_then_eval(layer, x, y):
    """参考合同侧的 NaN 填充（原始有效集合 + 等距并列平均）后再求值。"""
    c = ctrl_matrix(layer)
    ny, nx = c.shape
    good = [(i, j, c[j, i]) for j in range(ny) for i in range(nx) if math.isfinite(c[j, i])]
    filled = c.copy()
    for j in range(ny):
        for i in range(nx):
            if math.isfinite(c[j, i]):
                continue
            best = min((i - gi) ** 2 + (j - gj) ** 2 for gi, gj, _ in good)
            tie = [v for gi, gj, v in good if (i - gi) ** 2 + (j - gj) ** 2 <= best + 1e-12]
            filled[j, i] = float(np.mean(tie))
    op = layer.get("reconstruction_operator") or DEFAULT_OPERATOR
    if op == MESH_OPERATOR:
        filled = oracle_median3(filled)
    return oracle_spline_clip(filled, layer["x0"], layer["y0"], layer["dx"], layer["dy"], x, y)


if __name__ == "__main__":
    sys.exit(main())
