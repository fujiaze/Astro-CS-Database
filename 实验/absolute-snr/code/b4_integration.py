#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B / B4：逆方差集成对拍。

假说：
  H1 独立帧逆方差组合满足 SNR_combined² = Σ SNR_k²（定义式恒等，机器精度）。
  H2 MC 下逆方差组合的实际散度 = 1/Σw_k（w_k=1/σ_F,k²）；等权与 w∝SNR 两种
     替代权重严格劣于它，劣化幅度与解析预言一致。
  H3 point information（Q/W）集成：F̂=ΣQ/ΣW、Var=1/ΣW 对拍解析解；逐帧各自 PSF
     的 matched filter 最优（Zackay & Ofek 2017 I），公共 PSF/等权堆叠次优。
  H4 拟合权重（1/σ²）与堆叠权重（残差制造者方差 σ²(1−h_ii)，含拟合协方差）分离：
     对残差用 1/σ² 会低估方差（χ²/dof ≠ 1）；用正确权重恢复 χ²/dof ≈ 1；
     Huber 权重在离群注入下限制偏差。
  H5 F_ref 锚定 m_ref=6.0：w_k = SNR_k(F_ref,k)²/F_ref,k² = 1/σ_F,k²（恒等）；
     组合 SNR(m)² = 10^(−0.8(m−m_ref))·ΣSNR_k(F_ref,k)²；组内公共 F_ref（已作废
     写法）在 ZP 散度非零时给出有偏组合（负例，ZP 散度=0 时偏差归零）。
输出：results/b4_integration.json
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np
from scipy import stats

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

GAIN, RN, DARK, HALF = 1.3, 10.0, 0.5, 30
N_MC = 4000
R_IN, R_OUT = 10.0, 30.0
# 五帧：不同天光/读出/PSF（a_k 全部 =1：已归一到公共通量尺度）
FRAMES = [
    dict(name="k1", F_e=1500.0, sky=100.0, rn=8.0, sig=1.4, zp=24.00),
    dict(name="k2", F_e=1500.0, sky=300.0, rn=10.0, sig=1.6, zp=24.15),
    dict(name="k3", F_e=1500.0, sky=1000.0, rn=12.0, sig=1.8, zp=24.30),
    dict(name="k4", F_e=1500.0, sky=3000.0, rn=15.0, sig=2.0, zp=24.10),
    dict(name="k5", F_e=1500.0, sky=10000.0, rn=20.0, sig=2.2, zp=24.45),
]
M_REF = C.M_REF


def annulus_mask(shape, r_in, r_out):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    cy = (shape[0] - 1) / 2.0; cx = (shape[1] - 1) / 2.0
    return (np.hypot(yy - cy, xx - cx) >= r_in) & (np.hypot(yy - cy, xx - cx) <= r_out)


def simulate_frame(cfg, n, seed_off):
    P, _, _, _ = C.moffat4_grid(cfg["sig"], HALF)
    shape = P.shape
    r = C.rng(seed_off)
    lam = cfg["F_e"] * P + cfg["sky"] + DARK
    e = r.poisson(lam, size=(n,) + shape).astype(np.float64)
    if cfg["rn"] > 0:
        e += r.normal(0.0, cfg["rn"], size=(n,) + shape)
    return e / GAIN, P


def optimal_flux(d, P, var_i, bg_adu=0.0):
    """固定已知方差的 Horne 最优提取（线性估计量，Var=1/Σ(P²/σ²)）。

    bg_adu：独立已知的局部背景电平（ADU），必须先扣除（信号只计真实源能量）。
    """
    Pf = P.ravel(); w = 1.0 / var_i
    den = float((Pf * Pf * w).sum())
    return ((d - bg_adu).reshape(d.shape[0], -1) @ (Pf * w)) / den, 1.0 / den


def part_a_b(seed_off):
    rows = []
    P_cache = {}
    for cfg in FRAMES:
        P, _, _, _ = C.moffat4_grid(cfg["sig"], HALF)
        P_cache[cfg["name"]] = P
    for cfg in FRAMES:
        P = P_cache[cfg["name"]]
        var_i = (cfg["sky"] + DARK + cfg["rn"] ** 2 + cfg["F_e"] * P.ravel()) / GAIN ** 2
        sigF = float(np.sqrt(1.0 / (P.ravel() ** 2 / var_i).sum()))
        cfg["sigma_f"] = sigF
        cfg["snr_at_F"] = (cfg["F_e"] / GAIN) / sigF
        cfg["f_ref"] = C.f_ref_from_zeropoint(cfg["zp"], M_REF)
        cfg["snr_at_fref"] = cfg["f_ref"] / sigF
        cfg["w"] = 1.0 / sigF ** 2
        rows.append(dict(**{k: cfg[k] for k in ("name", "F_e", "sky", "rn", "sig", "zp",
                                                "sigma_f", "snr_at_F", "f_ref", "snr_at_fref", "w")}))
    # 定义式恒等：SNR_combined² = Σ SNR_k²（同一真实 F 下）
    F_adu = FRAMES[0]["F_e"] / GAIN
    snr_k = np.array([r["snr_at_F"] for r in rows])
    snr_comb_def = float(np.sqrt(np.sum(snr_k ** 2)))
    sig_comb = float(1.0 / np.sqrt(np.sum([r["w"] for r in rows])))
    snr_comb_from_sigma = F_adu / sig_comb
    ident_rel = abs(snr_comb_def / snr_comb_from_sigma - 1.0)
    # MC：每帧独立实现 → 三种组合
    Fs = {}
    for i, cfg in enumerate(FRAMES):
        d, P = simulate_frame(cfg, N_MC, seed_off + 1000 * i)
        var_i = (cfg["sky"] + DARK + cfg["rn"] ** 2 + cfg["F_e"] * P.ravel()) / GAIN ** 2
        F, _ = optimal_flux(d, P, var_i, bg_adu=(cfg["sky"] + DARK) / GAIN)
        Fs[cfg["name"]] = F
    Fmat = np.vstack([Fs[c["name"]] for c in FRAMES])
    w = np.array([c["w"] for c in FRAMES])[:, None]
    sigF_arr = np.array([c["sigma_f"] for c in FRAMES])[:, None]
    comb_ivar = (Fmat * w).sum(axis=0) / w.sum()
    comb_eq = Fmat.mean(axis=0)
    w_snr = np.array([c["snr_at_F"] for c in FRAMES])[:, None]
    comb_snr_w = (Fmat * w_snr).sum(axis=0) / w_snr.sum()
    out = dict(frames=rows, identity_rel_dev=float(ident_rel),
               snr_comb_def=snr_comb_def, snr_comb_from_sigma=snr_comb_from_sigma,
               mc=dict(
                   n_mc=N_MC,
                   var_ivar=float(np.var(comb_ivar, ddof=1)),
                   var_ivar_pred=float(1.0 / w.sum()),
                   var_equal=float(np.var(comb_eq, ddof=1)),
                   var_equal_pred=float((sigF_arr ** 2).sum() / len(FRAMES) ** 2),
                   var_snr_weight=float(np.var(comb_snr_w, ddof=1)),
                   var_snr_weight_pred=float((w_snr ** 2 * sigF_arr ** 2).sum() / w_snr.sum() ** 2),
                   bias_ivar=float(comb_ivar.mean() - F_adu),
                   bias_equal=float(comb_eq.mean() - F_adu),
                   bias_snr_weight=float(comb_snr_w.mean() - F_adu)))
    return out


def part_c_point_information(seed_off):
    """Q/W 集成：逐帧各自 PSF 的 matched filter + 解析权重。"""
    K = 3
    cfgs = [dict(name="p1", F_e=2000.0, sky=50.0, rn=6.0, sig=1.2),
            dict(name="p2", F_e=2000.0, sky=300.0, rn=10.0, sig=1.7),
            dict(name="p3", F_e=2000.0, sky=1500.0, rn=14.0, sig=2.3)]
    F_adu = cfgs[0]["F_e"] / GAIN
    Fs, Ps, Ws, varis = {}, {}, {}, {}
    for i, cfg in enumerate(cfgs):
        P, _, _, _ = C.moffat4_grid(cfg["sig"], HALF)
        Ps[cfg["name"]] = P
        var_i = (cfg["sky"] + DARK + cfg["rn"] ** 2 + cfg["F_e"] * P.ravel()) / GAIN ** 2
        varis[cfg["name"]] = var_i
        W = float((P.ravel() ** 2 / var_i).sum())
        Ws[cfg["name"]] = W
        d, _ = simulate_frame(cfg, N_MC, seed_off + 1000 * i)
        F, _ = optimal_flux(d, P, var_i, bg_adu=(cfg["sky"] + DARK) / GAIN)
        Fs[cfg["name"]] = F
    Fmat = np.vstack([Fs[c["name"]] for c in cfgs])
    W = np.array([Ws[c["name"]] for c in cfgs])[:, None]
    # (i) Q/W 最优
    qw = (Fmat * W).sum(axis=0) / W.sum()
    # (ii) 等权平均
    eq = Fmat.mean(axis=0)
    # (iii) 只用最窄 PSF 的帧（PSF 失配/单帧）
    single = Fmat[0]
    # (iv) 公共 PSF（最宽）对等权堆叠做 matched filter：近似为"用最宽 PSF 提取最窄帧"
    P_wide = Ps["p3"]
    d_wide, _ = simulate_frame(cfgs[2], N_MC, seed_off + 9999)   # 独立实现，避免共用噪声
    # 真实"公共 PSF"流程需要重采样/卷积；此处给出保守对照 = 对最窄帧用最宽 PSF 提取
    d_narrow, _ = simulate_frame(cfgs[0], N_MC, seed_off + 8888)
    var_narrow = varis["p1"]
    Pf_w = P_wide.ravel(); w_w = 1.0 / var_narrow
    den_w = float((Pf_w * Pf_w * w_w).sum())
    mismatch = ((d_narrow - (cfgs[0]["sky"] + DARK) / GAIN).reshape(N_MC, -1) @ (Pf_w * w_w)) / den_w
    res = dict(frames=[dict(name=c["name"], sigma_psf=c["sig"], sky=c["sky"], rn=c["rn"],
                            W=Ws[c["name"]], sigma_f=float(1 / np.sqrt(Ws[c["name"]])),
                            snr=float(F_adu * np.sqrt(Ws[c["name"]]))) for c in cfgs],
               mc=dict(n_mc=N_MC,
                       var_qw=float(np.var(qw, ddof=1)), var_qw_pred=float(1.0 / W.sum()),
                       var_equal=float(np.var(eq, ddof=1)),
                       var_equal_pred=float(np.mean([1 / Ws[c["name"]] for c in cfgs]) / len(cfgs)),  # K 帧等权平均 ⇒ mean(σ²)/K
                       var_single=float(np.var(single, ddof=1)),
                       var_single_pred=float(1.0 / Ws["p1"]),
                       var_psf_mismatch=float(np.var(mismatch, ddof=1)),
                       bias_qw=float(qw.mean() - F_adu), bias_equal=float(eq.mean() - F_adu),
                       bias_mismatch=float(mismatch.mean() - F_adu)),
               identity_snr_comb_sq=float((F_adu ** 2 * W.sum()) / sum(
                   (F_adu * np.sqrt(Ws[c["name"]])) ** 2 for c in cfgs)))
    return res


def part_d_fit_vs_stack(seed_off):
    """拟合权重（1/σ²）与堆叠权重（残差制造者方差 σ²(1−h_ii)）分离 + Huber 稳健拟合。

    拟合（估计背景模型参数）用 1/σ²；但残差 r_i = y_i − ŷ_i 的方差是 σ²(1−h_ii)
    （h_ii 为杠杆，含拟合参数协方差），对残差做堆叠必须用 1/(σ²(1−h_ii))。
    """
    r = C.rng(seed_off)
    N = 24
    x = np.cos(np.pi * (np.arange(N) + 0.5) / N)          # Chebyshev 设计点（杠杆不均）
    A = np.vstack([np.ones(N), x, x ** 2 - np.mean(x ** 2)]).T   # 二次背景模型
    H = A @ np.linalg.inv(A.T @ A) @ A.T
    h = np.diag(H)
    factor = 1.0 - h
    sigma = np.ones(N)
    n_trial = 4000
    Y = r.normal(0.0, 1.0, size=(n_trial, N))
    B = np.linalg.lstsq(A, Y.T, rcond=None)[0]
    R = Y.T - A @ B                                        # (N, n_trial)
    emp_res_var = R.var(axis=1, ddof=0)                    # 每样本的残差方差（对 trials）
    # 堆叠场景：K 个"帧"各自用不同采样设计拟合同一目标点的背景电平，
    # 拟合值 ŷ_t 的方差 = σ²·h_k（含拟合参数协方差），不是 σ²。
    designs = []
    x_t = np.array([1.0, 0.5, 0.25 - np.mean(np.cos(np.pi * (np.arange(24) + 0.5) / 24) ** 2)])
    for (nn, lo, hi, power) in [(24, -1.0, 1.0, 1.0), (12, -1.0, 1.0, 3.0),
                                (40, -1.0, 1.0, 0.5), (8, -1.0, 1.0, 6.0)]:
        xs = np.sign(np.linspace(-1, 1, nn)) * (np.abs(np.linspace(-1, 1, nn)) ** power)
        Ak = np.vstack([np.ones(nn), xs, xs ** 2 - np.mean(xs ** 2)]).T
        hk = float(x_t @ np.linalg.inv(Ak.T @ Ak) @ x_t)
        designs.append(dict(n=nn, A=Ak, h=hk))
    n_trial2 = 2000
    Yd = r.normal(0.0, 1.0, size=(n_trial2, sum(d["n"] for d in designs)))
    off = 0
    yhat = []
    for d in designs:
        Yk = Yd[:, off:off + d["n"]]; off += d["n"]
        bk = np.linalg.lstsq(d["A"], Yk.T, rcond=None)[0]
        yhat.append(x_t @ bk)
        d["emp_var"] = float(np.var(yhat[-1], ddof=1))
        d["pred_var"] = d["h"]
    yhat = np.vstack(yhat)
    h_arr = np.array([d["h"] for d in designs])[:, None]
    w_naive = np.ones_like(h_arr)
    w_correct = 1.0 / h_arr
    stack_naive = (yhat * w_naive).sum(axis=0) / w_naive.sum()
    stack_correct = (yhat * w_correct).sum(axis=0) / w_correct.sum()
    pred_naive = float((w_naive ** 2 * h_arr).sum() / w_naive.sum() ** 2)
    pred_correct = float((w_correct ** 2 * h_arr).sum() / w_correct.sum() ** 2)
    # 等杠杆对照（真值无效应 ⇒ 损失归零）
    h_eq = np.full_like(h_arr, h_arr.mean())
    w_eq = 1.0 / h_eq
    pred_naive_eq = float((np.ones_like(h_eq) ** 2 * h_eq).sum() / np.ones_like(h_eq).sum() ** 2)
    pred_correct_eq = float((w_eq ** 2 * h_eq).sum() / w_eq.sum() ** 2)
    # 独立测量（非残差）对照：1/σ² 最优
    indep = Y.T                                   # (N, n_trial)：独立测量（非残差）
    stack_indep = indep.mean(axis=0)              # 1/σ² 等权 ⇒ 样本均值
    # Huber 稳健拟合（IRLS）对离群：拦截项偏差对比
    def huber_irls(A, y, k=1.345, n_iter=30):
        beta = np.linalg.lstsq(A, y, rcond=None)[0]
        for _ in range(n_iter):
            res = y - A @ beta
            s = np.median(np.abs(res)) / 0.6745
            z = res / max(s, 1e-12)
            w = np.where(np.abs(z) <= k, 1.0, k / np.maximum(np.abs(z), 1e-12))
            Aw = A * np.sqrt(w)[:, None]
            beta = np.linalg.lstsq(Aw, y * np.sqrt(w), rcond=None)[0]
        return beta
    n_out = int(0.05 * N)
    out_rows = []
    for tag, amp in [("clean", 0.0), ("outliers_10sigma", 10.0)]:
        Y2 = r.normal(0.0, 1.0, size=(n_trial, N))
        if amp > 0:
            for t in range(n_trial):
                idx = r.choice(N, n_out, replace=False)
                Y2[t, idx] += amp
        b_plain = np.array([np.linalg.lstsq(A, Y2[t], rcond=None)[0][0] for t in range(0, n_trial, 4)])
        b_huber = np.array([huber_irls(A, Y2[t])[0] for t in range(0, n_trial, 4)])
        out_rows.append(dict(case=tag, amp=amp, n_used=len(b_plain),
                             plain_bias=float(b_plain.mean()), plain_std=float(b_plain.std(ddof=1)),
                             huber_bias=float(b_huber.mean()), huber_std=float(b_huber.std(ddof=1))))
    return dict(n=N, n_trial=n_trial, model="quadratic background fit (3 params)",
                leverage_min=float(h.min()), leverage_max=float(h.max()),
                residual_var_factor_min=float(factor.min()), residual_var_factor_max=float(factor.max()),
                emp_res_var_over_sigma2_min=float((emp_res_var / sigma ** 2).min()),
                emp_res_var_over_sigma2_max=float((emp_res_var / sigma ** 2).max()),
                emp_res_var_pred_min=float(factor.min()), emp_res_var_pred_max=float(factor.max()),
                designs=[dict(n=d["n"], leverage=d["h"], emp_var=d["emp_var"], pred_var=d["pred_var"])
                         for d in designs],
                stack_naive_var=float(np.var(stack_naive, ddof=1)), stack_naive_pred=pred_naive,
                stack_correct_var=float(np.var(stack_correct, ddof=1)), stack_correct_pred=pred_correct,
                naive_over_optimal_measured=float(np.var(stack_naive, ddof=1) / np.var(stack_correct, ddof=1)),
                naive_over_optimal_pred=float(pred_naive / pred_correct),
                equal_leverage_naive_over_optimal=float(pred_naive_eq / pred_correct_eq),
                indep_var=float(np.var(stack_indep, ddof=1)),
                indep_pred=float(1.0 / N),
                robust=out_rows)


def part_e_fref(seed_off):
    """F_ref 锚定 m_ref=6.0 的跨帧可比性 + "定义/换算 F_ref 不同源"负例。"""
    cfgs = [dict(name="k1", sky=100.0, rn=8.0, sig=1.5, zp=24.00),
            dict(name="k2", sky=800.0, rn=12.0, sig=1.8, zp=24.30),
            dict(name="k3", sky=5000.0, rn=18.0, sig=2.1, zp=24.60)]
    for cfg in cfgs:
        P, _, _, _ = C.moffat4_grid(cfg["sig"], HALF)
        cfg["P"] = P
        cfg["sigma_f"] = None   # 依赖源通量（源泊松项）；用 m_ref 电平定义参考 σ_F
        # 参考电平 F_ref 处的 σ_F：σ_i² = (sky+dark+RN²+F_ref·P_i)/g²
        cfg["f_ref"] = C.f_ref_from_zeropoint(cfg["zp"], M_REF)
        # f_ref 为 ADU；源泊松项在 ADU² 中 = F_e·P/g² = f_ref·P/g
        var_i = (cfg["sky"] + DARK + cfg["rn"] ** 2 + cfg["f_ref"] * GAIN * P.ravel()) / GAIN ** 2
        cfg["sigma_f"] = float(np.sqrt(1.0 / (P.ravel() ** 2 / var_i).sum()))
        cfg["snr_fref"] = cfg["f_ref"] / cfg["sigma_f"]
        cfg["w"] = 1.0 / cfg["sigma_f"] ** 2              # 未归一通量空间的逆方差
        cfg["w_norm"] = cfg["f_ref"] ** 2 / cfg["sigma_f"] ** 2   # 归一空间 = SNR(F_ref)²
    ident = max(abs((c["snr_fref"] ** 2 / c["f_ref"] ** 2) * c["sigma_f"] ** 2 - 1.0) for c in cfgs)
    w_sum = float(sum(c["w_norm"] for c in cfgs))
    snr_comb_fref = float(np.sqrt(sum(c["snr_fref"] ** 2 for c in cfgs)))
    # 跨帧可比性：物理源 m 在各帧的通量 F_k = Φ(m)·F_ref,k（Φ(m)=10^(−0.4(m−m_ref))）
    mag_rows = []
    for m in [4.0, 6.0, 8.0, 10.0, 12.0]:
        Phi = 10.0 ** (-0.4 * (m - M_REF))
        Fs = []
        for i, c in enumerate(cfgs):
            Fk = Phi * c["f_ref"]
            cfg2 = dict(c); cfg2["F_e"] = Fk * GAIN
            d, P = simulate_frame(cfg2, 2000, seed_off + 100 * i + int(10 * m))
            var_i = (c["sky"] + DARK + c["rn"] ** 2 + cfg2["F_e"] * P.ravel()) / GAIN ** 2
            F, _ = optimal_flux(d, P, var_i, bg_adu=(c["sky"] + DARK) / GAIN)
            Fs.append(F / c["f_ref"])                       # 归一到公共通量尺度
        Fmat = np.vstack(Fs)
        w = np.array([c["w_norm"] for c in cfgs])[:, None]
        comb = (Fmat * w).sum(axis=0) / w.sum()
        sigF_m = []
        for c in cfgs:
            var_i = (c["sky"] + DARK + c["rn"] ** 2 + Phi * c["f_ref"] * GAIN * c["P"].ravel()) / GAIN ** 2
            sigF_m.append(float(np.sqrt(1.0 / (c["P"].ravel() ** 2 / var_i).sum())))
        w_or = np.array([c["f_ref"] ** 2 / s ** 2 for c, s in zip(cfgs, sigF_m)])[:, None]  # 归一空间
        comb_or = (Fmat * w_or).sum(axis=0) / w_or.sum()
        mag_rows.append(dict(mag=m, Phi=Phi,
                             snr_k_at_source=[float(Phi * c["snr_fref"]) for c in cfgs],
                             snr_comb_pred=float(Phi * snr_comb_fref),
                             comb_mean=float(comb.mean()), comb_rel_bias=float(comb.mean() / Phi - 1.0),
                             comb_scatter=float(comb.std(ddof=1)),
                             comb_scatter_pred=float(1.0 / np.sqrt(w_sum)),
                             scatter_ratio=float(comb.std(ddof=1) * np.sqrt(w_sum)),
                             sigma_f_at_fref=[c["sigma_f"] for c in cfgs], sigma_f_at_m=sigF_m,
                             scatter_oracle=float(comb_or.std(ddof=1)),
                             scatter_oracle_pred=float(1.0 / np.sqrt(w_or.sum())),
                             anchored_over_oracle=float(comb.std(ddof=1) / comb_or.std(ddof=1))))
    # 负例：定义用 F_common、换算用逐帧 F_ref,k（不同源）⇒ 权重错 (F_common/F_ref,k)²
    neg = []
    for spread in [0.0, 0.1, 0.3, 0.6, 1.0]:
        cs = []
        for i, c in enumerate(cfgs):
            cc = dict(c); cc["zp"] = 24.0 + spread * i / 2.0
            cc["f_ref"] = C.f_ref_from_zeropoint(cc["zp"], M_REF)
            var_i = (cc["sky"] + DARK + cc["rn"] ** 2 + cc["f_ref"] * GAIN * cc["P"].ravel()) / GAIN ** 2
            cc["sigma_f"] = float(np.sqrt(1.0 / (cc["P"].ravel() ** 2 / var_i).sum()))
            cc["snr_fref"] = cc["f_ref"] / cc["sigma_f"]
            cc["w"] = 1.0 / cc["sigma_f"] ** 2
            cs.append(cc)
        f_common = float(np.mean([c["f_ref"] for c in cs]))
        c_k = np.array([(f_common / c["f_ref"]) ** 2 for c in cs])       # 权重偏差因子
        w = np.array([c["w"] for c in cs])
        w_bad = c_k * w
        var_opt = 1.0 / w.sum()
        var_bad = float((w_bad ** 2 / w).sum() / w_bad.sum() ** 2)
        implied_mag = [M_REF - 2.5 * np.log10(f_common / c["f_ref"]) for c in cs]
        neg.append(dict(zp_spread_mag=spread, f_common=f_common,
                        weight_bias_factor=[float(v) for v in c_k],
                        var_opt=float(var_opt), var_mismatch=float(var_bad),
                        eff_loss=float(var_bad / var_opt - 1.0),
                        implied_mag_spread=float(max(implied_mag) - min(implied_mag))))
    return dict(frames=[dict(name=c["name"], zp=c["zp"], f_ref=c["f_ref"], sigma_f=c["sigma_f"],
                             snr_fref=c["snr_fref"], w=c["w"]) for c in cfgs],
                identity_w_vs_snr_rel_dev=float(ident),
                snr_comb_at_fref=snr_comb_fref, w_sum=w_sum,
                sigma_comb_pred=float(1.0 / np.sqrt(w_sum)), mag_rows=mag_rows,
                mismatch_negative=neg)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "b4_integration.json"))
    a = ap.parse_args()
    t0 = time.time()
    ab = part_a_b(0)
    c = part_c_point_information(50000)
    d = part_d_fit_vs_stack(70000)
    e = part_e_fref(90000)
    gates = {}
    gates["H1_identity_rel_dev"] = ab["identity_rel_dev"]
    gates["H1_identity_lt_1e-12"] = bool(ab["identity_rel_dev"] < 1e-12)
    mc = ab["mc"]
    gates["H2_ivar_var_matches_pred"] = bool(abs(mc["var_ivar"] / mc["var_ivar_pred"] - 1) < 0.05)
    gates["H2_equal_var_matches_pred"] = bool(abs(mc["var_equal"] / mc["var_equal_pred"] - 1) < 0.05)
    gates["H2_snr_weight_var_matches_pred"] = bool(abs(mc["var_snr_weight"] / mc["var_snr_weight_pred"] - 1) < 0.05)
    gates["H2_ivar_better_than_equal"] = bool(mc["var_ivar"] < mc["var_equal"])
    gates["H2_ivar_better_than_snr_weight"] = bool(mc["var_ivar"] < mc["var_snr_weight"])
    gates["H2_bias_ivar_lt_1pct"] = bool(abs(mc["bias_ivar"]) / (1500 / GAIN) < 0.01)
    cmc = c["mc"]
    gates["H3_qw_var_matches_pred"] = bool(abs(cmc["var_qw"] / cmc["var_qw_pred"] - 1) < 0.05)
    gates["H3_qw_better_than_equal"] = bool(cmc["var_qw"] < cmc["var_equal"])
    gates["H3_qw_better_than_single"] = bool(cmc["var_qw"] < cmc["var_single"])
    gates["H3_psf_mismatch_penalized"] = bool(cmc["var_psf_mismatch"] > cmc["var_qw"])
    gates["H4_residual_var_factor_matches_pred"] = bool(
        abs(d["emp_res_var_over_sigma2_min"] / d["emp_res_var_pred_min"] - 1) < 0.05 and
        abs(d["emp_res_var_over_sigma2_max"] / d["emp_res_var_pred_max"] - 1) < 0.05)
    gates["H4_fit_var_matches_leverage"] = bool(all(
        abs(x["emp_var"] / x["pred_var"] - 1) < 0.08 for x in d["designs"]))
    gates["H4_naive_stack_var_matches_pred"] = bool(abs(d["stack_naive_var"] / d["stack_naive_pred"] - 1) < 0.05)
    gates["H4_correct_stack_var_matches_pred"] = bool(abs(d["stack_correct_var"] / d["stack_correct_pred"] - 1) < 0.05)
    gates["H4_equal_leverage_loss_zero"] = bool(abs(d["equal_leverage_naive_over_optimal"] - 1) < 1e-12)
    gates["H4_naive_suboptimal_ratio_measured"] = d["naive_over_optimal_measured"]
    gates["H4_naive_suboptimal_ratio_pred"] = d["naive_over_optimal_pred"]
    gates["H4_naive_suboptimal_gt_5pct"] = bool(d["naive_over_optimal_measured"] > 1.05)
    def z_var(measured, pred, n):
        return float((measured - pred) / (pred * np.sqrt(2.0 / max(n - 1, 1))))
    gates["H4_indep_z"] = z_var(d["indep_var"], d["indep_pred"], d["n_trial"])
    gates["H4_indep_matches_pred"] = bool(abs(gates["H4_indep_z"]) <= 3.0)
    rob = {x["case"]: x for x in d["robust"]}
    gates["H4_huber_clean_no_loss"] = bool(abs(rob["clean"]["huber_bias"] - rob["clean"]["plain_bias"]) < 0.05)
    gates["H4_huber_limits_outlier_bias"] = bool(abs(rob["outliers_10sigma"]["huber_bias"]) <
                                                 0.5 * abs(rob["outliers_10sigma"]["plain_bias"]))
    gates["H4_plain_outlier_bias"] = float(rob["outliers_10sigma"]["plain_bias"])
    gates["H4_huber_outlier_bias"] = float(rob["outliers_10sigma"]["huber_bias"])
    gates["H5_identity_rel_dev"] = e["identity_w_vs_snr_rel_dev"]
    gates["H5_identity_lt_1e-12"] = bool(e["identity_w_vs_snr_rel_dev"] < 1e-12)
    gates["H5_oracle_weights_optimal"] = bool(all(
        abs(r["scatter_oracle"] / r["scatter_oracle_pred"] - 1) < 0.15 for r in e["mag_rows"]))
    gates["H5_oracle_scatter_ratio_by_mag"] = {str(r["mag"]): float(r["scatter_oracle"] / r["scatter_oracle_pred"])
                                               for r in e["mag_rows"]}
    gates["H5_anchored_ratio_by_mag"] = {str(r["mag"]): r["anchored_over_oracle"] for r in e["mag_rows"]}
    gates["H5_anchored_near_optimal_at_mref"] = bool(
        abs(next(r for r in e["mag_rows"] if r["mag"] == 6.0)["anchored_over_oracle"] - 1) < 0.08)
    gates["H5_anchored_loss_at_m4_and_m12"] = [float(next(r for r in e["mag_rows"] if r["mag"] == mm)["anchored_over_oracle"] - 1)
                                              for mm in (4.0, 12.0)]
    gates["H5_mag_bias_max_abs"] = float(max(abs(r["comb_rel_bias"]) for r in e["mag_rows"]))
    gates["H5_mag_unbiased_lt_1pct"] = bool(gates["H5_mag_bias_max_abs"] < 0.01)
    neg = e["mismatch_negative"]
    gates["H5_mismatch_zero_spread_zero_loss"] = bool(abs(neg[0]["eff_loss"]) < 1e-12)
    gates["H5_mismatch_loss_grows_with_spread"] = bool(neg[-1]["eff_loss"] > 0.05)
    gates["H5_mismatch_eff_loss_at_max_spread"] = float(neg[-1]["eff_loss"])
    gates["H5_mismatch_implied_mag_spread_at_max"] = float(neg[-1]["implied_mag_spread"])
    obj = dict(experiment="SCI-B / B4 inverse-variance integration, Q/W point information, fit vs stack weights, F_ref anchoring",
               frozen_config=dict(gain=GAIN, rn=RN, dark=DARK, half=HALF, n_mc=N_MC,
                                  frames=FRAMES, m_ref=M_REF, seed_base=C.SEED_BASE),
               part_ab=ab, part_c=c, part_d=d, part_e=e, gates=gates,
               generated_at=C.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote", a.out)
    print("GATES:", gates)


if __name__ == "__main__":
    main()