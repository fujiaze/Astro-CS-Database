#!/usr/bin/env python3
"""#80 P0-A1 — 独立数值 oracle（NumPy/SciPy），对拍生产罚法方程求解器。

输入 = C++ 单元测试（eng/tests/unit/sky_plane_deflation_test.cpp）导出的**同一批算子**
（A, H, b, Q, D, alpha, lambda, gate）。本脚本只读这些算子，独立计算：

  · 谱/秩：A 的对称特征值、τ 口径数值秩、Π 是否为 ker(D) 的正确候选基（D·Q、QᵀQ）
  · 锚矩阵 A_α = A + αQQᵀ 与校正矩阵 M = I − α Qᵀ A_α⁻¹ Q 的谱（M 奇异 ⇔ A 在 span(Q) 上奇异）
  · 独立求解器：直接 lu/solve、SciPy **真实 pivoted QR**、SciPy **gelsd SVD**、
    增广设计 [chol(H); sqrt(λ)D] 的 QR/SVD（仅 H ≻ 0 时）、**精确 Woodbury（生产 ★ 式）**、
    以及**修复前 Q-only 公式**（反例）
  · 每个解的原方程残差 ‖A x − b‖∞/(‖A‖∞‖x‖∞+‖b‖∞)、KKT 残差、参数偏差、数值秩

并把它与 C++ 生产读数逐项比较 → evidence/oracle_comparison.json。

用法:
    python3 a1_penalized_oracle.py --cases <cxx_readings.json> --out <oracle.json>
"""
from __future__ import annotations

import argparse
import json
import math
import sys

import numpy as np
import scipy
from scipy.linalg import cho_factor, cho_solve, lstsq, qr, solve_triangular

TOL_RANK = 1e-10


def f(x):
    return None if (x is None or not math.isfinite(float(x))) else float(x)


def resid_inf(A, b, x):
    r = b - A @ x
    den = np.linalg.norm(A, np.inf) * np.linalg.norm(x, np.inf) + np.linalg.norm(b, np.inf)
    return float(np.linalg.norm(r, np.inf) / den) if den > 0 else float("inf")


def kkt_inf(A, b, x):
    return float(np.linalg.norm(b - A @ x, np.inf))


def pivoted_qr_solve(A, b, rtol=1e-12):
    """真实 scipy CPQR；秩亏时显式拒绝给唯一解（S3 审稿要求正名求解器身份）。"""
    Q, R, piv = qr(A, mode="economic", pivoting=True, check_finite=True)
    d = np.abs(np.diag(R))
    tol = float(rtol * d[0]) if d.size else 0.0
    rank = int(np.count_nonzero(d > tol)) if d.size else 0
    if rank < A.shape[1]:
        return None, {"driver": "scipy.linalg.qr(economic,pivoting=True)",
                      "status": "RANK_DEFICIENT_NO_UNIQUE_QR", "rank": rank,
                      "rank_threshold": tol, "pivot_indices": piv.tolist(),
                      "R_diagonal_abs": d.tolist()}
    y = solve_triangular(R[:rank, :rank], Q[:, :rank].T @ b, lower=False)
    x = np.empty_like(y)
    x[piv] = y
    return x, {"driver": "scipy.linalg.qr(economic,pivoting=True)+solve_triangular",
               "status": "OK", "rank": rank, "rank_threshold": tol,
               "pivot_indices": piv.tolist(), "R_diagonal_abs": d.tolist()}


def gelsd_solve(A, b, rtol=1e-12):
    x, _res, rank, s = lstsq(A, b, cond=rtol, lapack_driver="gelsd")
    return x, {"driver": "scipy.linalg.lstsq(lapack_driver=gelsd; SVD rank-revealing)",
               "status": "OK" if rank == A.shape[1] else "RANK_DEFICIENT",
               "rank": int(rank), "singular_values": s.tolist()}


def woodbury_solve(A, b, Q, alpha):
    """生产 ★ 式：B_a = A_α⁻¹b；X = A_α⁻¹Q；M = I − αQᵀX；w = M⁻¹αQᵀB_a；B = B_a + Xw。"""
    n, mq = Q.shape
    Aa = A + alpha * (Q @ Q.T)
    Ba = np.linalg.solve(Aa, b)
    X = np.linalg.solve(Aa, Q)
    M = np.eye(mq) - alpha * (Q.T @ X)
    w = np.linalg.solve(M, alpha * (Q.T @ Ba))
    return Ba + X @ w, {"M": M, "A_alpha": Aa}


def old_q_only_solve(A, H, b, Q, alpha):
    """修复前公式：B_a + Q G⁻¹(Qᵀb − QᵀH B_a)，G = QᵀH Q。"""
    Aa = A + alpha * (Q @ Q.T)
    Ba = np.linalg.solve(Aa, b)
    G = Q.T @ H @ Q
    t = np.linalg.solve(G, Q.T @ b - Q.T @ H @ Ba)
    return Ba + Q @ t


def augmented_design(H, D, lam, b, report):
    """[chol(H); sqrt(λ)D] x ≈ [chol(H)^{-T} b; 0]（等价于 H x = ... 的罚法方程）。"""
    w = np.linalg.eigvalsh((H + H.T) / 2)
    report["H_min_eig"] = float(w.min())
    report["H_max_eig"] = float(w.max())
    if w.min() <= 0:
        report["augmented_design"] = "SKIPPED_H_NOT_SPD"
        return None, None
    c, low = cho_factor(H, lower=True)
    Hc = np.tril(c) if low else np.triu(c).T
    f = solve_triangular(Hc, b, lower=True)
    R = np.vstack([Hc.T, math.sqrt(lam) * D])
    tgt = np.concatenate([f, np.zeros(D.shape[0])])
    report["augmented_design"] = "OK"
    report["augmented_shape"] = list(R.shape)
    return R, tgt


def analyse_case(case):
    ops = case["ops"]
    n = int(case["n_free"]) if "n_free" in case else int(case["n"])
    A = np.asarray(ops["A"], dtype=float).reshape(n, n)
    H = np.asarray(ops["H"], dtype=float).reshape(n, n)
    b = np.asarray(ops["b"], dtype=float).reshape(n)
    Q = np.asarray(ops["Q"], dtype=float).reshape(n, ops.get("mq", 0)) if ops.get("Q") else np.zeros((n, 0))
    mq = Q.shape[1]
    alpha = float(ops["alpha"])
    lam = float(ops["lambda"])
    gate = float(ops["gate"])
    rep = {"case": case["case"], "n": n, "mq": mq, "alpha": alpha, "lambda": lam, "gate": gate}

    # D（可重算索引）；真实 dump 给 stencil_rows，合成给稠密 D
    if "D" in ops:
        D = np.asarray(ops["D"], dtype=float).reshape(ops["D_rows"], n)
    elif "stencil_rows" in ops:
        rows = []
        for (i0, i1, i2) in ops["stencil_rows"]:
            r = np.zeros(n)
            r[i0] += 1.0
            r[i1] += -2.0
            r[i2] += 1.0
            rows.append(r)
        D = np.stack(rows) if rows else np.zeros((0, n))
    else:
        D = None
    rep["D_shape"] = list(D.shape) if D is not None else None
    if D is not None:
        sD = np.linalg.svd(D, compute_uv=False)
        rep["D_rank"] = int(np.count_nonzero(sD > TOL_RANK * sD[0])) if sD.size and sD[0] > 0 else 0
        rep["D_nullity"] = int(n - rep["D_rank"])
        rep["D_singular_values"] = sD.tolist()
        P = D.T @ D
        rep["A_minus_H_plus_lambdaP_max_abs"] = float(np.max(np.abs(A - (H + lam * P))))

    # Q 是惩罚零空间候选基？
    if mq:
        rep["DQ_inf"] = float(np.linalg.norm(D @ Q, np.inf)) if D is not None else None
        rep["QtQ_minus_I_inf"] = float(np.linalg.norm(Q.T @ Q - np.eye(mq), np.inf))

    # 谱 / 数值秩（生产同一 τ）
    eA = np.linalg.eigvalsh((A + A.T) / 2)
    rep["A_eig_min"] = float(eA.min())
    rep["A_eig_max"] = float(eA.max())
    rep["A_negative_eig_count"] = int(np.count_nonzero(eA < -1e-14 * max(1.0, abs(eA.max()))))
    rep["A_rank_tau1e-10"] = int(np.count_nonzero(eA > TOL_RANK * max(abs(eA.max()), 1e-300)))
    eH = np.linalg.eigvalsh((H + H.T) / 2)
    rep["H_red_eig_min"] = float(eH.min())
    rep["H_red_eig_max"] = float(eH.max())
    rep["H_red_rank_tau1e-10"] = int(np.count_nonzero(eH > TOL_RANK * max(abs(eH.max()), 1e-300)))

    # 锚与校正矩阵
    if mq:
        Aa = A + alpha * (Q @ Q.T)
        Aq = np.linalg.solve(Aa, Q)
        M = np.eye(mq) - alpha * (Q.T @ Aq)
        eM = np.linalg.eigvalsh((M + M.T) / 2)
        rep["M_eig_min"] = float(eM.min())
        rep["M_eig_max"] = float(eM.max())
        rep["M_singular_abs_min"] = float(np.min(np.abs(eM)))
        rep["A_alpha_min_eig"] = float(np.linalg.eigvalsh((Aa + Aa.T) / 2).min())

    # 独立求解器
    sols = {}
    try:
        sols["direct_numpy_solve"] = np.linalg.solve(A, b)
    except np.linalg.LinAlgError:
        rep["direct_numpy_solve"] = "SINGULAR"
    x, info = pivoted_qr_solve(A, b)
    rep["pivoted_qr_info"] = info
    if x is not None:
        sols["scipy_pivoted_qr"] = x
    x, info = gelsd_solve(A, b)
    rep["gelsd_info"] = info
    sols["scipy_gelsd_svd"] = x
    sols["numpy_lstsq_svd"] = np.linalg.lstsq(A, b, rcond=1e-12)[0]
    if mq:
        xw, aux = woodbury_solve(A, b, Q, alpha)
        sols["woodbury_production_star"] = xw
        rep["M_from_woodbury_eig_min"] = float(np.linalg.eigvalsh((aux["M"] + aux["M"].T) / 2).min())
        sols["old_q_only_formula"] = old_q_only_solve(A, H, b, Q, alpha)
    # 增广设计（H ≻ 0 时）：真实 QR 与 gelsd SVD
    if D is not None:
        R, tgt = augmented_design(H, D, lam, b, rep)
        if R is not None:
            xa, ia = pivoted_qr_solve(R, tgt)
            rep["augmented_qr_info"] = ia
            if xa is not None:
                sols["augmented_pivoted_qr"] = xa
            xa, ia = gelsd_solve(R, tgt)
            rep["augmented_gelsd_info"] = ia
            sols["augmented_gelsd_svd"] = xa
            rep["augmented_cond"] = float(np.linalg.cond(R))
            rep["augmented_rank_gelsd"] = ia.get("rank")

    # 读数
    rep["solutions"] = {}
    for k, xv in sols.items():
        rep["solutions"][k] = {
            "residual_inf_over_denom": resid_inf(A, b, xv),
            "kkt_inf_norm": kkt_inf(A, b, xv),
            "x_inf": float(np.linalg.norm(xv, np.inf)),
        }
    # 参数偏差（对直接解/最小范数解）
    if "direct_numpy_solve" in sols:
        xr = sols["direct_numpy_solve"]
        for k, xv in sols.items():
            d = float(np.linalg.norm(xv - xr, np.inf))
            s = float(np.linalg.norm(xr, np.inf))
            rep["solutions"][k]["param_rel_diff_vs_direct"] = d / s if s > 0 else d
    # 生产读数（C++）与 oracle 的对照
    rep["production"] = case.get("solve_forced_anchor")
    rep["production_direct"] = case.get("solve_direct")
    rep["independent_gauss_residual"] = case.get("independent_gauss_residual_inf_over_denom")
    if "woodbury_production_star" in sols and case.get("solve_forced_anchor", {}).get("B"):
        Bp = np.asarray(case["solve_forced_anchor"]["B"], dtype=float)
        d = float(np.linalg.norm(Bp - sols["woodbury_production_star"], np.inf))
        s = float(np.linalg.norm(sols["woodbury_production_star"], np.inf))
        rep["cxx_forced_anchor_vs_oracle_woodbury_rel"] = d / s if s > 0 else d
    if "direct_numpy_solve" in sols and case.get("solve_direct", {}).get("B"):
        Bp = np.asarray(case["solve_direct"]["B"], dtype=float)
        d = float(np.linalg.norm(Bp - sols["direct_numpy_solve"], np.inf))
        s = float(np.linalg.norm(sols["direct_numpy_solve"], np.inf))
        rep["cxx_direct_vs_oracle_direct_rel"] = d / s if s > 0 else d
    return rep


def verdict(out):
    """独立判读：不引用生产状态，只用本脚本的谱/残差读数 + C++ 导出读数做交叉核对。"""
    checks = []

    def add(ok, name, detail=""):
        checks.append({"check": name, "ok": bool(ok), "detail": str(detail)})

    for c in out["cases"]:
        name = c["case"]
        s = c["solutions"]
        pi = c.get("production", {}) or {}
        pd = c.get("production_direct", {}) or {}
        mq = c["mq"]
        # 1) 生产报告的算子身份：A == H + λDᵀD
        d = c.get("A_minus_H_plus_lambdaP_max_abs")
        if d is not None:
            scale = max(abs(c.get("A_eig_max", 1.0)), 1e-300)
            add(d <= 1e-12 * scale, name + ": A == H + λDᵀD", d)
        # 2) D 的零空间与候选基一致（Q 是合法的惩罚零空间候选基）
        if mq:
            add((c.get("DQ_inf") or 0.0) <= 1e-12, name + ": D Q == 0", c.get("DQ_inf"))
            add((c.get("QtQ_minus_I_inf") or 0.0) <= 1e-12, name + ": QᵀQ == I",
                c.get("QtQ_minus_I_inf"))
            add(mq >= min(4 * 1, 4), name + ": mq ≥ 4", mq)
        # 3) M 的正定性 ⟺ A 的正定性（生产回退判据的数学依据）
        a_min = c.get("A_eig_min", 0.0)
        astol = 1e-12 * max(abs(c.get("A_eig_max", 1.0)), 1e-300)
        m_min = c.get("M_eig_min")
        if m_min is not None:
            a_pd = a_min > astol
            m_pd = m_min > 1e-12
            add(a_pd == m_pd, name + ": (A≻0) ⟺ (M≻0)", f"Amin={a_min:.3e} Mmin={m_min:.3e}")
        # 4) 独立求解器：A 满秩时直接/QR/SVD/增广/Woodbury 残差全部在算术地板上
        if c.get("A_rank_tau1e-10") == c["n"]:
            for k in ("direct_numpy_solve", "scipy_pivoted_qr", "scipy_gelsd_svd",
                      "woodbury_production_star", "augmented_pivoted_qr",
                      "augmented_gelsd_svd"):
                if k in s:
                    add(s[k]["residual_inf_over_denom"] <= 1e-11,
                        name + f": {k} 原方程后向误差 ≤ 1e-11",
                        s[k]["residual_inf_over_denom"])
            # 5) 修复前 Q-only 公式的反例：残差显著大于所有正确解法
            if "old_q_only_formula" in s:
                old_r = s["old_q_only_formula"]["residual_inf_over_denom"]
                new_r = min(s[k]["residual_inf_over_denom"] for k in s if k != "old_q_only_formula")
                add(old_r > 1e-4 and old_r > 1e4 * max(new_r, 1e-300),
                    name + ": 旧 Q-only 公式残差 ≫ 正确解（反例成立）",
                    f"old={old_r:.3e} best_new={new_r:.3e}")
        else:
            # A 数值秩亏：独立 pivoted QR 必须拒绝给唯一解；生产不得报 OK
            add(c.get("pivoted_qr_info", {}).get("status") != "OK",
                name + ": A 秩亏 ⇒ pivoted QR 拒绝唯一解",
                c.get("pivoted_qr_info", {}).get("status"))
            add(pi.get("status") not in (0, 1) or pi.get("rc") == 1,
                name + ": A 秩亏 ⇒ 生产不产生可信解",
                f"status={pi.get('status')} rc={pi.get('rc')}")
        # 6) C++ 生产读数与 oracle 独立解一致
        if "cxx_forced_anchor_vs_oracle_woodbury_rel" in c:
            add(c["cxx_forced_anchor_vs_oracle_woodbury_rel"] <= 1e-10,
                name + ": C++ 强制锚定 == oracle Woodbury",
                c["cxx_forced_anchor_vs_oracle_woodbury_rel"])
        if "cxx_direct_vs_oracle_direct_rel" in c:
            add(c["cxx_direct_vs_oracle_direct_rel"] <= 1e-10,
                name + ": C++ 直接解 == oracle 直接解",
                c["cxx_direct_vs_oracle_direct_rel"])
        if pi.get("rc") == 0:
            add(pi.get("reported_backward_error", 1.0) <= c["gate"] * 1.000001,
                name + ": 生产报 OK 时其自报后向误差 ≤ 门",
                pi.get("reported_backward_error"))
            add(pd.get("rc") == 0, name + ": 生产直接分支亦成功（常规档）", pd.get("rc"))
    n_fail = sum(1 for x in checks if not x["ok"])
    out["checks"] = checks
    out["n_checks"] = len(checks)
    out["n_failed_checks"] = n_fail
    out["verdict"] = "PASS" if n_fail == 0 else "FAIL"
    return n_fail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    doc = json.load(open(a.cases))
    cases = doc["cases"] if isinstance(doc, dict) and "cases" in doc else doc
    out = {"schema": "acsd.sky-plane-deflation-oracle/v1",
           "numpy": np.__version__, "scipy": scipy.__version__,
           "rank_rtol": TOL_RANK,
           "driver_identity": {
               "pivoted_qr": "scipy.linalg.qr(mode=economic,pivoting=True)+solve_triangular",
               "svd": "scipy.linalg.lstsq(lapack_driver=gelsd)",
               "woodbury": "production fixed-point form B=Ba+Xw, M=I-alpha*Q^T A_alpha^-1 Q",
               "old_formula": "B=Ba+Q*(Q^T H Q)^-1(Q^T b-Q^T H Ba) [retired, counterexample]"},
           "cases": []}
    for c in cases:
        out["cases"].append(analyse_case(c))
    nfail = verdict(out)
    with open(a.out, "w") as fh:
        json.dump(out, fh, indent=1, allow_nan=False, default=f)
    for x in out["checks"]:
        if not x["ok"]:
            print("FAIL %-58s %s" % (x["check"], x["detail"]))
    print("verdict=%s  checks=%d failed=%d" % (out["verdict"], out["n_checks"], nfail))
    for c in out["cases"]:
        print("== %s n=%d mq=%d A_rank=%s M_min=%s" %
              (c["case"], c["n"], c["mq"], c.get("A_rank_tau1e-10"), c.get("M_eig_min")))
    print("wrote", a.out)
    return nfail


if __name__ == "__main__":
    sys.exit(main())
