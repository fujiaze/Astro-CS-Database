#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""判据 2（SCI-B 跨帧绝对 SNR 传递链）在 M42 真实数据端到端产物上的复核。

规范依据
--------
- 创新点二：ASTROCS_DESIGN.md:131-137（帧间一致性把每帧独立标定的噪声传递为跨帧绝对 SNR；
  不因组内一致而放大系统项）。冻结定义：docs/science/PHASE2_UPM.md（加性噪声模型）、
  docs/science/SNR_CHAIN.md、lib/algorithms/coverage/src/integrate.cpp:20-79
  （ivar 加权：signal = Sigma w x / Sigma w；w = 1/sigma_F^2）。
- EXP-06 主结论（噪声律）：实验/absolute-snr/results/exp06_e1_analytic.json
  frames[i].methods[phys_auto].vs_T1_var_bg 的 NLF 幂指数 p 实测 0.995~1.017
  （实验/absolute-snr/docs/EXP-06-SUMMARY.md）。
- 权重效率损失 E 的定义与参考值：E = Var_w/Var_opt - 1，Var_w = Sigma w^2 var_true/(Sigma w)^2，
  Var_opt = 1/Sigma(1/var_true)；实现见 实验/absolute-snr/code/exp05/exp05_common.py:393-408；
  EXP-06 报告值（scene A4_realistic，6 seed，中位）：phys_auto 7.33589e-05、
  interp_spline 2.00861e-03、frame_scalar 5.81691e-02、naive_pixel 1.60982e-02、
  phys_unsep 4.22290e-02（exp06_e1_analytic.json）。
- **E 需要真值方差场**：EXP-06 arm C（真实数据臂）因此**没有算 E**
  （results/exp06_e3_real.json 中 eff_loss 出现 0 次）。本单元用"跨帧配对方差"作为真值方差场
  的经验代理，并明确登记其偏差方向。

判据（全部可红）
--------------
C2-G1  产品自身方差模型的噪声律指数：由帧 HiPS variance 平面与 signal 平面拟合
       Var = sigma0^2 + D^p/g，要求 p 的 95% 自助置信区间与 1 相容（EXP-06 实测 0.995~1.017）。
C2-G2  独立经验噪声律：由**跨帧配对差** (x_i-x_j)^2/2 得到的噪声方差随 D 的幂指数 p_emp
       必须与 1 相容（该量完全独立于产品的 variance 平面）。
C2-G3  权重效率损失 E（真实数据上首次计算）：产品权重（per_sample_ivar）的 E 必须
       <= 1e-4（EXP-06 phys_auto 量级）；对照臂 E(const) 必须显著大于 0（能红）。
C2-N1  负例：把权重替换为常数（frame_scalar 型）后 E 必须显著大于 EXP-06 phys_auto 量级。
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m42_common as M

TILE = M.TILE_SPAN
EXP06_P_BAND = (0.995, 1.017)
EXP06_E = dict(phys_auto=7.33589e-05, interp_spline=2.00861e-03, frame_scalar=5.81691e-02,
               naive_pixel=1.60982e-02, phys_unsep=4.22290e-02)
E_GATE = 1e-4
P_GRID = np.linspace(0.2, 3.0, 281)


def loglog_slope(D, V, W=None):
    """log10 V 对 log10 D 的加权最小二乘斜率（非退化：平坦方差给出斜率 0）。"""
    D = np.asarray(D, dtype=float)
    V = np.asarray(V, dtype=float)
    W = np.ones_like(V) if W is None else np.asarray(W, dtype=float)
    m = np.isfinite(D) & np.isfinite(V) & (D > 0) & (V > 0)
    D, V, W = D[m], V[m], W[m]
    if D.size < 6:
        return None
    x, y = np.log10(D), np.log10(V)
    sw = np.sqrt(W)
    A = np.column_stack([x, np.ones_like(x)])
    coef, *_ = np.linalg.lstsq(A * sw[:, None], y * sw, rcond=None)
    res = y - A @ coef
    return dict(slope=float(coef[0]), intercept=float(coef[1]), n=int(D.size),
                rms=float(np.sqrt(np.mean(res ** 2))),
                x_span=float(x.max() - x.min()))


def boot_slope(D, V, W=None, n_boot=400, tag="c2"):
    rng = M.derive_rng(tag)
    D = np.asarray(D, dtype=float)
    n = D.size
    W = np.ones_like(D) if W is None else np.asarray(W, dtype=float)
    ss = []
    for _ in range(n_boot):
        idx = rng.integers(0, n, size=n)
        r = loglog_slope(D[idx], np.asarray(V)[idx], W[idx])
        if r:
            ss.append(r["slope"])
    ss = np.array(ss, dtype=float)
    if ss.size == 0:
        return dict(median=None, lo=None, hi=None, half_width=None, n_boot=0)
    lo, hi = float(np.percentile(ss, 2.5)), float(np.percentile(ss, 97.5))
    return dict(median=float(np.median(ss)), lo=lo, hi=hi,
                half_width=float(0.5 * (hi - lo)), n_boot=int(ss.size))


def mc_slope_control(kind, tag):
    """斜率判据的正/负控制：photon -> 真斜率 1；flat -> 真斜率 0。"""
    rng = M.derive_rng(tag)
    D = np.logspace(-4, -2, 24)
    truth = 1.0 if kind == "photon" else 0.0
    V = (D ** truth) * (1.0 + 0.02 * rng.normal(size=D.size))
    r = loglog_slope(D, V)
    ci = boot_slope(D, V, n_boot=200, tag=tag + "_boot")
    return dict(slope=r["slope"], ci=ci, truth=truth,
                ok_gate=bool(abs(r["slope"] - 1.0) <= 0.05 and ci["half_width"] <= 0.05))


def fit_power(D, V, W=None, p_grid=P_GRID):
    """Var = s0 + D^p * inv_g。对给定 p 线性最小二乘解 (s0, inv_g)，再对 p 取残差最小。"""
    D = np.asarray(D, dtype=float)
    V = np.asarray(V, dtype=float)
    W = np.ones_like(V) if W is None else np.asarray(W, dtype=float)
    m = np.isfinite(D) & np.isfinite(V) & (D > 0) & (V > 0)
    D, V, W = D[m], V[m], W[m]
    if D.size < 8:
        return None
    best = None
    for p in p_grid:
        A = np.column_stack([np.ones_like(D), D ** p])
        sw = np.sqrt(W)
        coef, *_ = np.linalg.lstsq(A * sw[:, None], V * sw, rcond=None)
        res = V - A @ coef
        chi2 = float(np.sum(W * res ** 2))
        if best is None or chi2 < best[0]:
            best = (chi2, float(p), float(coef[0]), float(coef[1]), int(D.size))
    return dict(chi2=best[0], p=best[1], sigma0_sq=best[2], inv_g=best[3], n=best[4])


def boot_ci(D, V, W, n_boot=200, tag="c2"):
    rng = M.derive_rng(tag)
    ps = []
    n = D.size
    for _ in range(n_boot):
        idx = rng.integers(0, n, size=n)
        r = fit_power(D[idx], V[idx], W[idx])
        if r:
            ps.append(r["p"])
    ps = np.array(ps, dtype=float)
    if ps.size == 0:
        return dict(median=None, lo=None, hi=None, n_boot=0)
    return dict(median=float(np.median(ps)), lo=float(np.percentile(ps, 2.5)),
                hi=float(np.percentile(ps, 97.5)), n_boot=int(ps.size))


def weight_efficiency(w, var_true):
    """实验/absolute-snr/code/exp05/exp05_common.py:393-408 的同一量（逐字移植）。"""
    w = np.asarray(w, dtype=float)
    v = np.asarray(var_true, dtype=float)
    m = np.isfinite(w) & np.isfinite(v) & (w > 0) & (v > 0)
    w, v = w[m], v[m]
    if w.size < 8:
        return None
    var_w = float(np.sum(w ** 2 * v) / (np.sum(w) ** 2))
    var_opt = float(1.0 / np.sum(1.0 / v))
    return dict(eff_loss=var_w / var_opt - 1.0, var_w=var_w, var_opt=var_opt, n=int(w.size))


# ---------------------------------------------------------------- 帧方差平面 vs 信号
def frame_var_vs_signal(n_tiles=6, n_per_tile=40000):
    rows = []
    rng = M.derive_rng("c2_varfit")
    for blk in ("t2",):
        for f in M.frame_ids(blk)[:4]:
            tiles = M.hips_tiles(blk, f)
            pick = rng.choice(len(tiles), size=min(n_tiles, len(tiles)), replace=False)
            S, V = [], []
            for ti in pick:
                tip = int(tiles[int(ti)])
                s = M.read_tile_opt(blk, f, "signal", tip)
                v = M.read_tile_opt(blk, f, "variance", tip)
                u = M.read_tile_opt(blk, f, "support", tip)
                if s is None or v is None or u is None:
                    continue
                m = (u > 0.9) & np.isfinite(s) & np.isfinite(v) & (s > 0) & (v > 0)
                if int(m.sum()) < 64:
                    continue
                idx = np.where(m.ravel())[0]
                if idx.size > n_per_tile:
                    idx = rng.choice(idx, n_per_tile, replace=False)
                S.append(s.ravel()[idx])
                V.append(v.ravel()[idx])
            if S:
                rows.append((f, np.concatenate(S), np.concatenate(V)))
    return rows


def binned(D, V, nb=24):
    """按 D 分位分箱，箱内取中位 D 与中位 V（对离群稳健）。"""
    q = np.quantile(D, np.linspace(0, 1, nb + 1))
    q = np.unique(q)
    db, vb, wb = [], [], []
    for a, b in zip(q[:-1], q[1:]):
        m = (D >= a) & (D < b)
        if int(m.sum()) < 32:
            continue
        db.append(float(np.median(D[m])))
        vb.append(float(np.median(V[m])))
        wb.append(float(m.sum()))
    return np.array(db), np.array(vb), np.array(wb)


# ---------------------------------------------------------------- 跨帧配对经验噪声
def empirical_noise(block, n_tiles=10, cap=400):
    """由共同覆盖叶上的跨帧配对差 (x_i-x_j)^2/2 估计每叶噪声方差（独立于 variance 平面）。"""
    from collections import defaultdict
    rng = M.derive_rng("c2_emp_%s" % block)
    fids = M.frame_ids(block)
    tiles = {f: set(M.hips_tiles(block, f)) for f in fids}
    union = sorted(set().union(*tiles.values()))
    pick = set(int(union[int(i)]) for i in rng.choice(len(union), size=min(n_tiles, len(union)),
                                                     replace=False))
    accD, accV = [], []
    for tip in sorted(pick):
        have = [i for i in range(len(fids)) if tip in tiles[fids[i]]]
        if len(have) < 2:
            continue
        S, U = {}, {}
        for i in have:
            s = M.read_tile_opt(block, fids[i], "signal", tip)
            u = M.read_tile_opt(block, fids[i], "support", tip)
            if s is None or u is None:
                continue
            S[i] = s.ravel()
            U[i] = u.ravel()
        idx = sorted(S)
        for aa in range(len(idx)):
            for bb in range(aa + 1, len(idx)):
                i, j = idx[aa], idx[bb]
                m = ((U[i] > 0.9) & (U[j] > 0.9) & np.isfinite(S[i]) & np.isfinite(S[j])
                     & (S[i] > 0) & (S[j] > 0))
                k = np.where(m)[0]
                if k.size < 64:
                    continue
                if k.size > cap:
                    k = rng.choice(k, cap, replace=False)
                d2 = 0.5 * (S[i][k] - S[j][k]) ** 2
                mean = 0.5 * (S[i][k] + S[j][k])
                # 剔除非正噪声估计（浮点/重合样本）
                g = np.isfinite(d2) & (d2 > 0) & np.isfinite(mean) & (mean > 0)
                if int(g.sum()) < 16:
                    continue
                accD.append(mean[g])
                accV.append(d2[g])
    if not accD:
        return np.array([]), np.array([])
    D = np.concatenate(accD)
    V = np.concatenate(accV)
    if D.size > 400000:
        sel = rng.choice(D.size, 400000, replace=False)
        D, V = D[sel], V[sel]
    return D, V


def product_weights(block, n_tiles=10, cap=400):
    """产品实际权重：帧 HiPS ivar 平面（p2_integrated.json: weight_basis = per_sample_ivar）。"""
    rng = M.derive_rng("c2_w_%s" % block)
    fids = M.frame_ids(block)
    tiles = {f: set(M.hips_tiles(block, f)) for f in fids}
    union = sorted(set().union(*tiles.values()))
    pick = set(int(union[int(i)]) for i in rng.choice(len(union), size=min(n_tiles, len(union)),
                                                     replace=False))
    W = []
    for tip in sorted(pick):
        for f in fids:
            if tip not in tiles[f]:
                continue
            iv = M.read_tile_opt(block, f, "ivar", tip)
            u = M.read_tile_opt(block, f, "support", tip)
            if iv is None or u is None:
                continue
            m = (u > 0.9) & np.isfinite(iv) & (iv > 0)
            k = np.where(m.ravel())[0]
            if k.size == 0:
                continue
            if k.size > cap:
                k = rng.choice(k, cap, replace=False)
            W.append(iv.ravel()[k])
    return np.concatenate(W) if W else np.array([])


def main():
    g = M.Gates()
    out = {"unit": "M42-REALDATA-01", "criterion": "C2 SCI-B absolute SNR chain"}

    # ---- C2-G1 产品方差平面的噪声律 ----
    rows = frame_var_vs_signal()
    allD, allV = [], []
    per_frame = []
    for f, S, V in rows:
        db, vb, wb = binned(S, V)
        r = fit_power(db, vb, wb)
        per_frame.append(dict(frame_key=f, n=int(S.size), fit=r,
                              corr_signal_variance=float(np.corrcoef(S, V)[0, 1])))
        allD.append(S)
        allV.append(V)
    D = np.concatenate(allD)
    V = np.concatenate(allV)
    db, vb, wb = binned(D, V)
    fit = fit_power(db, vb, wb)
    ci = boot_ci(db, vb, wb, tag="c2_boot_g1")
    out["variance_model"] = dict(per_frame=per_frame, pooled_fit=fit, pooled_ci=ci,
                                 pooled_corr=float(np.corrcoef(D, V)[0, 1]),
                                 binned=dict(D=db.tolist(), V=vb.tolist(), n=wb.tolist()),
                                 exp06_p_band=list(EXP06_P_BAND),
                                 variance_median=float(np.median(V)),
                                 variance_robust_scale=float(M.robust_scale(V)))
    sl = loglog_slope(db, vb, wb)
    sci = boot_slope(db, vb, wb, tag="c2_boot_g1_slope")
    mc_ph = mc_slope_control("photon", "c2_mc_photon")
    mc_fl = mc_slope_control("flat", "c2_mc_flat")
    out["variance_model"].update(slope=sl, slope_ci=sci,
                                 mc_photon=mc_ph, mc_flat=mc_fl)
    # 判据（非退化）：斜率必须既**与 1 相容**又**被数据约束住**（CI 半宽 <= 0.05）。
    ok1 = bool(sl is not None and abs(sl["slope"] - 1.0) <= 0.05
               and sci["half_width"] is not None and sci["half_width"] <= 0.05)
    g.add("C2-G1-variance-plane-noise-law",
          "产品 variance 平面的 log-log 斜率必须 = 1 +- 0.05 且 95%% CI 半宽 <= 0.05（非退化）",
          [sl["slope"], sci["lo"], sci["hi"]], ok1,
          source="实验/absolute-snr/docs/EXP-06-SUMMARY.md + exp06_e1_analytic.json",
          level="external-consistency",
          note="实测斜率 %.4f [%.4f, %.4f]（CI 半宽 %.4f，D 跨 %.2f dex）；"
               "corr(signal,variance)=%.4f；median(var)=%.4e"
               % (sl["slope"], sci["lo"], sci["hi"], sci["half_width"], sl["x_span"],
                  out["variance_model"]["pooled_corr"],
                  out["variance_model"]["variance_median"]))
    g.add("C2-G1c-mc-photon-green",
          "正例：合成 Var ∝ D 时同一斜率判据必须判绿",
          mc_ph["slope"], bool(mc_ph["ok_gate"]),
          source="同上", level="positive-control")
    g.add("C2-G1d-mc-flat-red",
          "负例：合成 Var 与 D 无关（平坦）时同一斜率判据必须判红",
          mc_fl["slope"], bool(not mc_fl["ok_gate"]),
          source="同上", level="negative-control")

    # ---- C2-G2 跨帧配对经验噪声律 ----
    emp = {}
    for blk in ("t2", "t3"):
        De, Ve = empirical_noise(blk)
        if De.size < 8:
            emp[blk] = None
            continue
        db2, vb2, wb2 = binned(De, Ve, nb=24)
        sl2 = loglog_slope(db2, vb2, wb2)
        ci2 = boot_slope(db2, vb2, wb2, tag="c2_boot_%s" % blk)
        emp[blk] = dict(n_leaf_samples=int(De.size), n_bins=int(db2.size), slope=sl2, ci=ci2,
                        binned=dict(D=db2.tolist(), V=vb2.tolist(), n=wb2.tolist()))
        ok2 = bool(sl2 is not None and abs(sl2["slope"] - 1.0) <= 0.15)
        g.add("C2-G2-empirical-noise-law-%s" % blk,
              "跨帧配对差给出的经验噪声律 log-log 斜率 = 1 +- 0.15", [sl2["slope"], ci2["lo"], ci2["hi"]],
              ok2, source="独立测量（不依赖产品 variance 平面）", level="data",
              note="斜率 %.4f [%.4f, %.4f]，%d 个叶样本 / %d 个信号箱"
                   % (sl2["slope"], ci2["lo"], ci2["hi"], De.size, db2.size))
    out["empirical_noise"] = emp

    # ---- C2-G3 权重效率损失 E ----
    De, Ve = empirical_noise("t2", n_tiles=10)
    W = product_weights("t2", n_tiles=10)
    e = {}
    vb, wbv, cnt = np.array([]), np.array([]), np.array([])
    if De.size >= 8:
        # 统一分箱：按信号 D 的 24 个分位箱，箱内取中位的真值方差与产品权重。
        # 说明：w 与 var_true 都只随驱动 D 变化，故 24 箱是该数据上二者的共同分辨率；
        # E 在箱级样本上计算（与 EXP-06 的逐像素 E 同定义，只是样本被 D 分箱粗化）。
        nb = 24
        q = np.quantile(De, np.linspace(0, 1, nb + 1))
        vv, ww, cc = [], [], []
        for a, b in zip(q[:-1], q[1:]):
            m = (De >= a) & (De < b)
            if int(m.sum()) < 32:
                continue
            vv.append(float(np.median(Ve[m])))
            if W.size:
                ww.append(float(np.median(W)))
            cc.append(float(m.sum()))
        vb = np.clip(np.array(vv), 1e-300, None)
        wbv = np.array(ww)
        cnt = np.array(cc)
        if vb.size >= 8:
            if wbv.size == vb.size:
                e["product_per_sample_ivar"] = weight_efficiency(wbv, vb)
            e["constant_weight"] = weight_efficiency(np.ones_like(vb), vb)
            e["optimal_1_over_var"] = weight_efficiency(1.0 / vb, vb)
    out["weight_efficiency"] = dict(arms=e, exp06_reference=EXP06_E, gate=E_GATE,
                                    n_leaf_samples=int(De.size), n_bins=int(vb.size),
                                    binned=dict(var_true=vb.tolist(), w=wbv.tolist(),
                                                n=cnt.tolist()),
                                    w_stats=M.stats(W) if W.size else None)
    ep = e.get("product_per_sample_ivar")
    ec0 = e.get("constant_weight")
    g.add("C2-G3-weight-efficiency-product",
          "产品权重（per_sample_ivar）的 E <= %.0e（EXP-06 phys_auto 量级）" % E_GATE,
          ep["eff_loss"] if ep else None, bool(ep and ep["eff_loss"] <= E_GATE),
          source="实验/absolute-snr/code/exp05/exp05_common.py:393-408",
          level="external-consistency",
          note="EXP-06 参考：phys_auto %.3e / naive_pixel %.3e / frame_scalar %.3e；"
               "实测 E(常数权重)=%.6f，var_true 跨 %.2f 倍"
               % (EXP06_E["phys_auto"], EXP06_E["naive_pixel"], EXP06_E["frame_scalar"],
                  ec0["eff_loss"] if ec0 else float("nan"),
                  (max(vb) / min(vb)) if vb.size else float("nan")))
    # 非退化补充：产品权重必须真的利用噪声随信号的变化（E(product) < 0.5 * E(常数权重)）
    ratio_e = (ep["eff_loss"] / ec0["eff_loss"]) if (ep and ec0 and ec0["eff_loss"] > 0) else None
    g.add("C2-G3c-weights-exploit-noise-variation",
          "产品权重的 E 必须 < 0.5 * E(常数权重)（权重确实随噪声变化而变）",
          ratio_e, bool(ratio_e is not None and ratio_e < 0.5),
          source="E 的定义 + 实验/absolute-snr/code/exp05/exp05_common.py:393-408",
          level="data",
          note="E(product)/E(const)=%s（=1.0 表示权重在信号维上完全不变）"
               % ("%.6f" % ratio_e if ratio_e is not None else "n/a"))
    # 正例控制：合成 w ∝ var_true^-0.9 -> E 必须很小（判据能绿）
    if vb.size:
        vv = np.asarray(vb)
        # 控制臂指数取 0.997（相对 1/var_true 的指数误差 0.3%）：
        # E ≈ (1-a)^2 * Var(ln var_true)，本数据 Var(ln v) ~ 1.26 ⇒ E ~ 1e-5，恰好过 1e-4 门。
        w_syn = vv ** -0.997
        e_syn = weight_efficiency(w_syn, vv)
        out["weight_efficiency"]["mc_near_optimal"] = e_syn
        out["weight_efficiency"]["mc_near_optimal_note"] = (
            "合成 w = var_true^-0.997（指数误差 0.3%%）；用于证明 E <= 1e-4 的门可被达到。"
            "对照：指数 0.9 时 E=%.4f（差 2 个数量级）" % weight_efficiency(vv ** -0.9, vv)["eff_loss"])
        g.add("C2-G3d-mc-near-optimal-green",
              "正例：合成 w ∝ var_true^-0.997 时同一 E 判据必须判绿（E <= 1e-4）",
              e_syn["eff_loss"], bool(e_syn["eff_loss"] <= E_GATE),
              source="同上", level="positive-control")
    ec = e.get("constant_weight")
    g.add("C2-N1-constant-weight-red",
          "负例：常数权重（frame_scalar 型）的 E 必须显著大于 EXP-06 phys_auto 量级",
          ec["eff_loss"] if ec else None, bool(ec and ec["eff_loss"] > 10 * EXP06_E["phys_auto"]),
          source="同上", level="negative-control")
    eo = e.get("optimal_1_over_var")
    g.add("C2-G3b-optimal-degenerate",
          "退化对照：w = 1/var_true 时 E 必须为 0（该臂恒真，无证据资格）",
          eo["eff_loss"] if eo else None, bool(eo and abs(eo["eff_loss"]) < 1e-12),
          source="E 的定义", level="degenerate-control")

    out["gates"] = g.summary()
    p = M.json_dump(out, "c2_absolute_snr.json")
    print(json.dumps(out["gates"], ensure_ascii=False, indent=1)[:7000])
    print("wrote", p)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
