#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B 诊断：σ_F 噪声项组成与单位一致性（写实验代码前的定位脚本，结论并入 B1/B2）。"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C

GAIN, RN, DARK, SIG, F_E, HALF, N = 1.3, 10.0, 0.5, 1.5, 3000.0, 30, 2000
P, sum_p2, pc, half = C.moffat4_grid(SIG, HALF)
shape = P.shape
yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
cy = cx = (shape[0]-1)/2.0
r = np.hypot(yy-cy, xx-cx)
mask_bg = (r >= 10) & (r <= 30)
Pf = P.ravel()
F_ADU = F_E / GAIN

for B in [0.0, 1000.0, 100000.0]:
    rr = C.rng(7)
    lam = F_E*P + B + DARK
    e = rr.poisson(lam, size=(N,)+shape).astype(float) + rr.normal(0, RN, size=(N,)+shape)
    d = e/GAIN
    b = np.median(d[:, mask_bg], axis=1)
    s_tot = C.K_MAD_TO_SIGMA*np.median(np.abs(d[:, mask_bg]-b[:,None]), axis=1)  # 经验总 rms
    s_sky_only = np.full(N, np.sqrt((B+DARK)/GAIN**2))                          # 天光+暗流散粒（不含 RN）
    dsub = d - b[:,None,None]
    def extract(var_mode):
        F = np.empty(N)
        for k in range(N):
            if var_mode == "oracle":     # 真值方差（固定权重，线性估计量）
                var_i = (B+DARK+RN**2+F_E*Pf)/GAIN**2
                w = 1.0/var_i; den=(Pf*Pf*w).sum(); F[k]=(Pf*w*dsub[k].ravel()).sum()/den
            elif var_mode == "total":    # 经验总 σ + 源泊松（不另加 RN）—— ACSD 正确口径
                Fk=0.0
                for _ in range(2):
                    var_i = s_tot[k]**2 + np.maximum(Fk*Pf,0)/GAIN
                    w=1.0/var_i; den=(Pf*Pf*w).sum(); Fk=(Pf*w*dsub[k].ravel()).sum()/den
                F[k]=Fk
            elif var_mode == "emp_plus_rn":  # 调度器路径：经验总 σ 再 + (RN/g)² ⇒ 双计
                Fk=0.0
                for _ in range(2):
                    var_i = s_tot[k]**2 + (RN/GAIN)**2 + np.maximum(Fk*Pf,0)/GAIN
                    w=1.0/var_i; den=(Pf*Pf*w).sum(); Fk=(Pf*w*dsub[k].ravel()).sum()/den
                F[k]=Fk
            elif var_mode == "skyonly_plus_rn":  # 设计意图：σ 只含天光散粒 + RN 项
                Fk=0.0
                for _ in range(2):
                    var_i = s_sky_only[k]**2 + (RN/GAIN)**2 + np.maximum(Fk*Pf,0)/GAIN
                    w=1.0/var_i; den=(Pf*Pf*w).sum(); Fk=(Pf*w*dsub[k].ravel()).sum()/den
                F[k]=Fk
        return float(np.mean(F)/np.std(F, ddof=1))
    sigF = C.horne_sigma_f_theory(F_E, B, DARK, RN, GAIN, P)     # ADU
    snr_def = F_ADU/sigF                                          # 单位修正后定义式
    print("B=%9.1f  sigF_def=%9.3f ADU  SNR_def=%8.4f | oracle=%8.4f total=%8.4f emp+RN=%8.4f skyonly+RN=%8.4f"
          % (B, sigF, snr_def, extract("oracle"), extract("total"), extract("emp_plus_rn"), extract("skyonly_plus_rn")))
    print("           s_tot_med=%.4f ADU  s_skyonly=%.4f ADU  RN/g=%.4f ADU"
          % (np.median(s_tot), np.median(s_sky_only), RN/GAIN))
