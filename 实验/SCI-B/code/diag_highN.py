#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B 高统计量锚定：N=20000 帧，检验 Var(F_hat)=1/sum(P^2/sigma_i^2)（线性 oracle 权重）。"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C

GAIN, RN, DARK, SIG, F_E, HALF, N = 1.3, 10.0, 0.5, 1.5, 3000.0, 30, 20000
P, sum_p2, pc, half = C.moffat4_grid(SIG, HALF)
shape = P.shape
Pf = P.ravel()
F_ADU = F_E/GAIN
for B in [0.0, 1000.0]:
    var_i = (B + DARK + RN**2 + F_E*Pf)/GAIN**2
    w = 1.0/var_i
    den = float((Pf*Pf*w).sum())
    sigF_def = float(np.sqrt(1.0/den))
    rr = C.rng(11)
    lam = F_E*P + B + DARK
    e = rr.poisson(lam, size=(N,)+shape).astype(float) + rr.normal(0, RN, size=(N,)+shape)
    d = e/GAIN
    # 已知真值方差的固定权重线性估计量（无背景扣除；背景=0 常数不影响）
    F = (d.reshape(N, -1) @ (Pf*w))/den
    m, s = float(F.mean()), float(F.std(ddof=1))
    sm = s/np.sqrt(2*(N-1))
    print("B=%8.1f  sigmaF_def=%.4f  sigmaF_mc=%.4f +- %.4f  dev=%+.3f%% (%.2f sigma)  SNR_def=%.4f SNR_mc=%.4f"
          % (B, sigF_def, s, sm, 100*(s/sigF_def-1), (s-sigF_def)/sm, F_ADU/sigF_def, m/s))
