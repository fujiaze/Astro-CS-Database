#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B / B5：传递到 Phase3 —— C_out = R C_in Rᵀ 与按输出 PSF 重算点源信息量。

假说：
  H1 重采样后方差/协方差满足 C_out = R C_in Rᵀ（MC 逐元素对拍）。
  H2 只取对角 Σc_k²u_k（C_in 对角假设）系统性低估输出方差；低估因子与解析
     预言 1+0.75ρ（近邻近似）同量级。
  H3 点源信息量必须按**输出 PSF** 与**完整 C_out** 重算：
     Var(F̂_out)=1/(P_outᵀ C_out⁻¹ P_out) 与 MC 散度一致；用对角近似或输入 PSF
     会高估信息量（低估方差）。
输出：results/b5_phase3_transfer.json
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

N = 24               # 补丁边长
SIGMA_CORR = 0.6     # 输入噪声相关长度 [px]（drizzle 后相关噪声的等效表征）
SIGMA_PSF = 1.2      # 输入 PSF sigma [px]
SHIFT = 0.3          # 输出网格相对输入的整体平移 [px]
N_MC = 2000


def build_R(n=N, shift=SHIFT):
    """双线性重采样矩阵：输出像素 (i,j) 中心 = 输入坐标 (i+shift, j+shift)。"""
    rows, cols, vals = [], [], []
    for i in range(n):
        for j in range(n):
            p = i * n + j
            y = i + shift; x = j + shift
            y0 = int(np.floor(y)); x0 = int(np.floor(x))
            wy = y - y0; wx = x - x0
            for (yy, xx, w) in ((y0, x0, (1 - wy) * (1 - wx)), (y0, x0 + 1, (1 - wy) * wx),
                                (y0 + 1, x0, wy * (1 - wx)), (y0 + 1, x0 + 1, wy * wx)):
                if 0 <= yy < n and 0 <= xx < n and w != 0.0:
                    rows.append(p); cols.append(yy * n + xx); vals.append(w)
    R = np.zeros((n * n, n * n))
    R[rows, cols] = vals
    return R


def build_cin(n=N, sigma=SIGMA_CORR, amp=1.0):
    yy, xx = np.mgrid[0:n, 0:n]
    idx = np.stack([yy.ravel(), xx.ravel()], axis=1)
    d2 = ((idx[:, None, :] - idx[None, :, :]) ** 2).sum(axis=2)
    return amp ** 2 * np.exp(-d2 / (2.0 * sigma ** 2))


def moffat_patch(n=N, sigma=SIGMA_PSF):
    P, _, _, _ = C.moffat4_grid(sigma, n // 2)
    return P[:n, :n] / P[:n, :n].sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "b5_phase3_transfer.json"))
    a = ap.parse_args()
    t0 = time.time()
    R = build_R()
    Cin = build_cin()
    Cout = R @ Cin @ R.T
    L = np.linalg.cholesky(Cin + 1e-12 * np.eye(N * N))
    r = C.rng(4242)
    Z = r.normal(size=(N * N, N_MC))
    X = L @ Z
    Y = R @ X
    Cout_mc = np.cov(Y)
    # 只在对角带内比较（远离对角元的真值 ~0，MC 噪声主导 Frobenius 范数）
    idx = np.arange(N * N).reshape(N, N)
    band = np.zeros((N * N, N * N), bool)
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            yy = idx[max(0, -dy):N - max(0, dy), max(0, -dx):N - max(0, dx)]
            xx = idx[max(0, dy):N - max(0, -dy), max(0, dx):N - max(0, -dx)]
            band[yy.ravel(), xx.ravel()] = True
    rel_frob = float(np.linalg.norm((Cout_mc - Cout)[band]) / np.linalg.norm(Cout[band]))
    diag_ratio = float(np.mean(np.diag(Cout_mc) / np.diag(Cout)))
    # 相邻像素相关系数
    rho_in = float(Cin[0, 1] / Cin[0, 0])
    rho_out = float(Cout[0, 1] / np.sqrt(Cout[0, 0] * Cout[1, 1]))
    # 对角近似 vs 完整
    var_full = np.diag(Cout)
    var_diag_approx = np.diag(R @ np.diag(np.diag(Cin)) @ R.T)
    var_diag_ratio = float(np.mean(var_diag_approx / var_full))
    # 点源信息量
    Pin = moffat_patch().ravel()
    Pout = R @ Pin
    Pout = Pout / Pout.sum()
    Cin_inv = np.linalg.inv(Cin + 1e-10 * np.eye(N * N))
    W_full = float(Pout @ np.linalg.solve(Cout + 1e-10 * np.eye(N * N), Pout))
    var_ps_full = 1.0 / W_full
    # 对角近似（输出逐像素方差）
    w_diag = 1.0 / np.diag(Cout)
    var_ps_diag = float(1.0 / (Pout ** 2 * w_diag).sum())
    # 输入 PSF + 输入方差（完全忽略重采样）
    w_in = 1.0 / np.diag(Cin)
    var_ps_in = float(1.0 / (Pin ** 2 * w_in).sum())
    # MC：注入点源，按输出 PSF + 完整 C_out 提取
    F_true = 100.0
    src = F_true * Pin
    Ys = Y + (R @ src)[:, None]
    Finv = np.linalg.inv(Cout + 1e-10 * np.eye(N * N))
    num = Pout @ (Finv @ Ys)
    den = float(Pout @ Finv @ Pout)
    Fhat = num / den
    var_mc = float(np.var(Fhat, ddof=1))
    # 对照：对角近似提取（忽略协方差）
    Fhat_diag = (Pout * w_diag) @ Ys / float((Pout ** 2 * w_diag).sum())
    var_mc_diag = float(np.var(Fhat_diag, ddof=1))
    gates = dict(
        H1_rel_frobenius_band=rel_frob, H1_band_lt_0p10=bool(rel_frob < 0.10),
        H1_band_note="5x5 对角带内 Frobenius；元素级 MC 噪声 sqrt((CiiCjj+Cij^2)/(n-1)) 主导" ,
        H1_diag_ratio_mc_over_analytic=diag_ratio,
        H1_diag_within_2pct=bool(abs(diag_ratio - 1) < 0.02),
        H2_diag_approx_underestimates=bool(var_diag_ratio < 0.99),
        H2_full_over_diag_measured=float(1.0 / var_diag_ratio),
        H2_pred_1_plus_0p75rho=float(1 + 0.75 * rho_out),
        H2_pred_matches_measured_10pct=bool(abs((1.0 / var_diag_ratio) / (1 + 0.75 * rho_out) - 1) < 0.10),
        H3_var_full_pred=float(var_ps_full), H3_var_full_mc=var_mc,
        H3_full_matches_mc=bool(abs(var_mc / var_ps_full - 1) < 0.10),
        H3_diag_pred=float(var_ps_diag), H3_diag_underestimates=bool(var_ps_diag < var_ps_full * 0.99),
        H3_input_only_pred=float(var_ps_in), H3_input_only_underestimates=bool(var_ps_in < var_ps_full * 0.99),
        H3_mc_diag_extractor_var=var_mc_diag,
        H3_diag_extractor_suboptimal=bool(var_mc_diag > var_mc * 1.01),
        H3_diag_claim_underestimates_actual=bool(var_ps_diag < var_mc_diag * 0.99),
        H3_diag_claim_over_actual=float(var_ps_diag / var_mc_diag),
        H3_flux_unbiased=bool(abs(Fhat.mean() / F_true - 1) < 0.02),
    )
    obj = dict(experiment="SCI-B / B5 Phase3 variance transfer C_out=R C_in R^T and output-PSF point information",
               frozen_config=dict(patch=N, sigma_corr_px=SIGMA_CORR, sigma_psf_px=SIGMA_PSF,
                                  shift_px=SHIFT, n_mc=N_MC, F_true_adu=F_true, seed_base=C.SEED_BASE),
               rho_in_neighbor=rho_in, rho_out_neighbor=rho_out,
               gates=gates, generated_at=C.now(), wall_s=time.time() - t0)
    C.save_json(a.out, obj)
    print("wrote", a.out)
    for k, v in gates.items():
        print("  %-42s %s" % (k, v))


if __name__ == "__main__":
    main()
