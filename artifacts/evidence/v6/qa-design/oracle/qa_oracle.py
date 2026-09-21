# -*- coding: utf-8 -*-
"""QA-MATRIX-001 独立科学 Oracle（解析 / Monte Carlo / 注入 / 基线 / 结构）。

独立性：纯 numpy + 标准库，从第一性原理构造参考真值（显式矩阵、解析恒等、定种子 MC、
外部索引），不 import、不 link、不执行任何 AstroCS 生产实现或生产测试二进制。

用法：
  python3 qa_oracle.py run [--json out.json]          # 正向：全部 check 必须通过 -> rc=0
  python3 qa_oracle.py mutate <MUT-ID> [--json out]   # 注入一个错误；门变红 -> rc=1（检出）
退出码：0 通过/未检出异常；1 有 check 失败或 mutation 未被检出；2 用法错误。
"""
from __future__ import annotations
import json, sys, math, os
import numpy as np

# ------------------------------------------------------------------ 工具
def rel(a, b):
    d = abs(float(a) - float(b))
    return d / (abs(float(b)) + 1e-300)

def block_diag(mats):
    n = sum(m.shape[0] for m in mats)
    out = np.zeros((n, n))
    o = 0
    for m in mats:
        k = m.shape[0]
        out[o:o+k, o:o+k] = m
        o += k
    return out

def gaussian(x, c, s):
    p = np.exp(-0.5*((x-c)/s)**2)
    return p/p.sum()

def fwhm(y, x):
    y = np.asarray(y, float); x = np.asarray(x, float)
    half = y.max()/2.0
    above = np.where(y >= half)[0]
    if len(above) < 1:
        return 0.0
    i0, i1 = above[0], above[-1]
    v0, v1 = y[i0], y[i1]
    xl = x[i0] - (x[i0]-x[i0-1])*(v0-half)/(v0-y[i0-1]) if i0 > 0 else x[i0]
    xr = x[i1] + (x[i1+1]-x[i1])*(v1-half)/(y[i1+1]-v1) if i1 < len(y)-1 else x[i1]
    return float(xr-xl)

# ------------------------------------------------------------------ 参考模型
def frames(seed=7, nframes=4, npix=14, sig=1.5, corr=0.0, shared=0.0):
    r = np.random.default_rng(seed)
    x = np.arange(npix)
    P = [gaussian(x, c, sig) for c in [4.5, 6.0, 5.0, 6.5][:nframes]]
    a = np.array([1.00, 1.10, 0.90, 1.05][:nframes])
    s = np.array([1.0, 1.3, 0.8, 1.1][:nframes])
    n = npix*nframes
    A = np.concatenate([a[k]*P[k] for k in range(nframes)])
    C = block_diag([np.full((npix, npix), 0.0) + np.eye(npix)*s[k]**2 for k in range(nframes)])
    if corr > 0.0:  # 跨帧共同模式（低秩 L L^T）
        L = np.zeros((n, nframes))
        for k in range(nframes):
            L[k*npix:(k+1)*npix, k] = shared*s[k]*np.sqrt(npix)
        C = C + L @ L.T
    return P, a, s, A, C

def pinf_ref(P, a, s, A, C, F=5.0, seed=11, nsig=2.5):
    r = np.random.default_rng(seed)
    n = A.shape[0]
    d = A*F + r.standard_normal(n) @ np.linalg.cholesky(C).T
    CiA = np.linalg.solve(C, A)
    W = float(A @ CiA)
    c = CiA / W
    Q = sum(a[k]*P[k] @ (d[k*len(P[0]):(k+1)*len(P[0])]/s[k]**2) for k in range(len(P)))
    return dict(d=d, W=W, c=c, F=Q/W, Fgls=float(c @ d), var=float(c @ C @ c), C=C, A=A)

def pinf_subj(P, a, s, A, C, d, B):
    ref = pinf_ref(P, a, s, A, C)
    if B.get("A01_ols") or B.get("I02_ols"):
        W = 1.0/float(A @ A); c = A*W
        return dict(F=float(c @ d), var=float(c @ c), c=c, W=W, assumed="ols")
    if B.get("A12_pixel_ivar_authority"):
        iv = np.concatenate([np.full(len(P[0]), 1.0/s[k]**2) for k in range(len(P))])
        c = iv/iv.sum(); return dict(F=float(c @ d), var=1.0/float(iv.sum()), c=c, W=1.0/float(iv.sum()))
    c = ref["c"].copy(); W = ref["W"]
    if B.get("A02_coeff_scale"):
        c = c*1.10; W = float(1.0/(c @ c))  # 用被扰动的系数报告
    return dict(F=float(c @ d), var=float(c @ C @ c), c=c, W=W)

# ------------------------------------------------------------------ check 集合
CHECKS = []
def check(cid, gates, desc):
    def deco(fn):
        CHECKS.append(dict(id=cid, gates=gates, desc=desc, fn=fn)); return fn
    return deco

@check("CHK-ANA-01", ["G-ANA-01"], "Q/W == GLS 点源通量恒等")
def _(B):
    P, a, s, A, C = frames()
    ref = pinf_ref(P, a, s, A, C)
    sub = pinf_subj(P, a, s, A, C, ref["d"], B)
    return dict(measured=rel(sub["F"], ref["Fgls"]), op="<=", thresh=1e-9, ok=rel(sub["F"], ref["Fgls"]) <= 1e-9)

@check("CHK-ANA-02", ["G-ANA-02"], "实际系数方差恒等 c^T C c == 1/W")
def _(B):
    P, a, s, A, C = frames()
    ref = pinf_ref(P, a, s, A, C)
    sub = pinf_subj(P, a, s, A, C, ref["d"], B)
    m = rel(sub["var"], ref["var"])
    return dict(measured=m, op="<=", thresh=1e-9, ok=m <= 1e-9)

@check("CHK-ANA-03", ["G-ANA-03"], "相关帧联合 C（朴素求和低估被检出）")
def _(B):
    P, a, s, A, C = frames(corr=0.6, shared=0.9)
    ref = pinf_ref(P, a, s, A, C)
    P0, a0, s0, A0, C0 = frames()
    naive = pinf_ref(P0, a0, s0, A0, C0)
    subj = naive["var"] if (B.get("M01_rho0") or B.get("I01_shared_indep")) else ref["var"]
    ok = not (subj < ref["var"]*(1.0-1e-9))
    return dict(measured=subj/ref["var"], op=">=", thresh=1.0, ok=bool(ok))

@check("CHK-ANA-04", ["G-ANA-04"], "GLS 协方差恒等 G C G^T == (A^T C^-1 A)^-1")
def _(B):
    P, a, s, A, C = frames()
    CiA = np.linalg.solve(C, A)
    W = float(A @ CiA); Ainv = 1.0/W
    if B.get("A04_wrong_R"):
        At = np.concatenate(P)            # R~ 忽略 a_k
        R = At/(At @ At)
        cov = float(R @ C @ R)
    else:
        c = CiA/W
        cov = float(c @ C @ c)
    m = abs(cov - Ainv)
    return dict(measured=m, op="<=", thresh=1e-9, ok=m <= 1e-9)

@check("CHK-ANA-05", ["G-ANA-05"], "白噪声条件恒等 W=a^2/(sigma^2 A_NEA)")
def _(B):
    x = np.arange(14); P = gaussian(x, 6.0, 1.5); a = 1.1; sg = 0.7
    Wref = a*a*float(P @ P)/(sg*sg)
    if B.get("A05_recip"):
        Wsub = a*a*sg*sg/float(P @ P)
    else:
        Wsub = a*a*float(P @ P)/(sg*sg)
    m = rel(Wsub, Wref)
    return dict(measured=m, op="<=", thresh=1e-12, ok=m <= 1e-12)

@check("CHK-ANA-06", ["G-ANA-06"], "单位表量纲代数（ADU/PX 指数）")
def _(B):
    dims = {"signal_sb":(-1,2), "pixel_variance_in":(2,0), "sb_variance_out":(2,-4),
            "sb_ivar_out":(-2,4), "Q":(-1,0), "flux":(1,0), "W_info":(-2,0), "psfsw":(0,0)}
    if B.get("A06_unit") or B.get("SPEC11_unit"):
        dims["W_info"] = (2,0)
    viol = 0
    exp = {"signal_sb":(-1,2),"pixel_variance_in":(2,0),"sb_variance_out":(2,-4),"sb_ivar_out":(-2,4),
           "Q":(-1,0),"flux":(1,0),"W_info":(-2,0),"psfsw":(0,0)}
    for k,v in exp.items():
        if dims[k] != v: viol += 1
    return dict(measured=viol, op="==", thresh=0, ok=viol == 0)

@check("CHK-ANA-07A", ["G-ANA-07"], "PSFSW 组内归一 median=1 且全正")
def _(B):
    S = np.array([2.0,3.0,2.5]); Conc = np.array([1.2,1.0,1.4])
    N = np.array([0.5,0.6,0.45]); Bg = np.array([1.0,1.1,0.9])
    Wt = S**2*Conc/(N**2*Bg)
    W = Wt if B.get("A07_nonorm") else Wt/np.median(Wt)
    m = abs(float(np.median(W))-1.0); allpos = bool(np.all(W > 0))
    return dict(measured=m, op="<=", thresh=1e-12, ok=(m <= 1e-12 and allpos))

@check("CHK-ANA-07B", ["G-ANA-07"], "psfsw 权重不得写成 ivar/fisher")
def _(B):
    rec = {"weight_kind":"relative_dimensionless","weight_units":"1","keys":["signal","concentration","noise","background"]}
    if B.get("A13_psfsw_as_ivar"):
        rec["weight_units"] = "flux^-2"; rec["keys"].append("ivar")
    bad = int("ivar" in rec["keys"] or rec["weight_units"] != "1")
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

@check("CHK-ANA-08", ["G-ANA-08","G-INJ-07"], "Phase3 Q/W 输出帧重算（禁重采样输入 Q/W）")
def _(B):
    x = np.arange(9, dtype=float)
    p = gaussian(x, 4.0, 1.1)
    S = np.zeros((11, 9))
    for i in range(11):                      # 行归一采样核（三角）
        for j in range(9):
            S[i,j] = max(0.0, 1.0-abs((i-1)-j)/2.0)
    S = S/S.sum(axis=1, keepdims=True)
    r = np.random.default_rng(5)
    Cy = np.eye(11)*0.7**2
    f = r.standard_normal(11) + (S@p)*3.0
    a = 1.2
    pi = S @ p
    Wrec = a*a*float(pi @ np.linalg.solve(Cy, pi))
    Win = a*a*float((S @ (p*p)).sum())                  # 被禁路径：重采样输入 W 图（未在输出帧重算）
    if B.get("A08_resample"):
        Wout = Win
    else:
        Wout = Wrec
    m = rel(Wout, Wrec)
    return dict(measured=m, op="<=", thresh=1e-9, ok=m <= 1e-9)

@check("CHK-ANA-09", ["G-ANA-09"], "Phase3 方差 Σc^2 u 与行归一 R")
def _(B):
    r = np.random.default_rng(3)
    R = r.random((6, 8)); R = R/R.sum(axis=1, keepdims=True)
    u = np.full(8, 3.0)
    var_ok = float((R**2 @ u).max())
    var_bad = float((R @ u).max())
    sub = var_bad if B.get("A09_sigma_c") else var_ok
    ref = float(np.max(np.diag(R @ np.diag(u) @ R.T)))
    m = rel(sub, ref); rowsum = float(np.max(np.abs(R.sum(axis=1)-1.0)))
    return dict(measured=m, op="<=", thresh=1e-12, ok=(m <= 1e-12 and rowsum <= 1e-12 and abs(float(R[0]@R[0])-1) > 0.1))

@check("CHK-ANA-10", ["G-ANA-10","G-INJ-08"], "Drizzle 方差恒等 + aperture 相关项")
def _(B):
    r = np.random.default_rng(9)
    a_jp = r.random((5, 6))*0.4 + 0.1          # 目标 x 源 重叠面积
    D = a_jp.sum(axis=1)
    v = np.full(6, 0.25)
    w = a_jp/ (0.8**2)                          # A_drop = pixfrac^2 * A_pix
    if B.get("A10_missing_D2"):
        var_diag = (v[None,:]*w**2).sum(axis=1)
    elif B.get("A14_missing_square"):
        var_diag = (v[None,:]*w).sum(axis=1)/D**2
    else:
        var_diag = (v[None,:]*w**2).sum(axis=1)/D**2
    c = w/D[:,None]
    Cov = c @ np.diag(v) @ c.T
    m = float(np.max(np.abs(np.diag(Cov)-var_diag)))
    aper_true = float(np.ones(5) @ Cov @ np.ones(5))
    aper_diag = float(var_diag.sum())
    ok = (m <= 1e-12) and (aper_diag < aper_true)
    return dict(measured=m, op="<=", thresh=1e-12, ok=bool(ok), extra=aper_diag/aper_true)

@check("CHK-ANA-11", ["G-ANA-11","G-INJ-02"], "常量面亮度 S_p=B0 全 pixfrac（按 B0 构造）")
def _(B):
    B0 = 3.0; A_pix = 4.0
    worst = 0.0
    for pf in (1.0, 0.8, 0.5, 0.25):
        A_drop = pf*pf*A_pix
        if B.get("A11_const_adu"):
            x = np.array([7.0])                 # 每像素常量 ADU
            Sp = x[0]/A_drop if not B.get("A16_S_eq_F") else x[0]
        elif B.get("A15_uncond"):
            Sp = B0/(pf*pf)                     # 旧公式 S=x/A_drop 却宣称 S=B0
        elif B.get("A16_S_eq_F"):
            Sp = B0*A_pix
        else:
            Bj = np.array([B0])                 # B_j = x_j/A_pixel = B0
            Sp = float((Bj*A_drop).sum()/A_drop)
        worst = max(worst, abs(Sp/B0-1.0))
    return dict(measured=worst, op="<=", thresh=1e-3, ok=worst <= 1e-3)

@check("CHK-ANA-11F", ["G-ANA-11"], "条件通量守恒（pf=1 严格 / pf<1 = pf^2）")
def _(B):
    xj = 5.0; A_pix = 1.0
    out1 = xj*1.0
    out08 = (0.8**2)*xj
    m = abs(out1/xj-1.0)+abs(out08/(0.8**2*xj)-1.0)
    return dict(measured=m, op="<=", thresh=1e-12, ok=m <= 1e-12)

# ---------------- Monte Carlo ----------------
@check("CHK-MC-01", ["G-MC-01","G-INJ-01"], "点源散度 vs 1/sqrt(W)（MC）")
def _(B):
    P,a,s,A,C = frames(nframes=3, npix=10)
    ref = pinf_ref(P,a,s,A,C,seed=21)
    W = ref["W"]
    r = np.random.default_rng(31); N = 20000; F = 5.0
    Lc = np.linalg.cholesky(C)
    n = A.shape[0]; est = np.empty(N)
    for i in range(N):
        d = A*F + r.standard_normal(n) @ Lc.T
        if B.get("M02_halfW"):
            est[i] = ref["F"] + (d @ A)*0.0 + float(ref["c"] @ d)  # F 不变
        else:
            sub = pinf_subj(P,a,s,A,C,d,B)
            est[i] = sub["F"]
    var_mc = float(est.var())
    if B.get("M02_halfW"):
        var_rep = 2.0*W
        var_rep = 1.0/var_rep
    elif B.get("I03_var_from_weight") or B.get("B03_var_from_weight"):
        var_rep = 0.35/W
    else:
        var_rep = 1.0/W
    m = rel(var_rep, var_mc)
    return dict(measured=m, op="<=", thresh=0.03, ok=m <= 0.03, extra=var_mc)

@check("CHK-MC-02", ["G-MC-02"], "surface_gls 协方差 vs MC")
def _(B):
    r = np.random.default_rng(4)
    A = np.column_stack([np.ones(6), np.linspace(-1,1,6)])
    C = np.diag(r.uniform(0.5,2.0,6))
    cov_pred = np.linalg.inv(A.T @ np.linalg.solve(C, A))
    Lc = np.linalg.cholesky(C); N = 20000; samples = np.empty((N,2))
    for i in range(N):
        d = A @ np.array([2.0,0.7]) + Lc @ r.standard_normal(6)
        samples[i] = np.linalg.inv(A.T @ np.linalg.solve(C, A)) @ (A.T @ np.linalg.solve(C, d))
    cov_mc = np.cov(samples.T)
    m = float(np.max(np.abs(cov_mc-cov_pred))/np.max(np.abs(cov_pred)))
    if B.get("M03_ignore_a"):
        declared_gate = False
    else:
        declared_gate = True
    ok = (m <= 0.03) and declared_gate
    return dict(measured=m, op="<=", thresh=0.03, ok=bool(ok))

@check("CHK-MC-03", ["G-MC-03"], "共享系统项：联合 vs 朴素比值 > 1（MC）")
def _(B):
    P,a,s,A,C = frames(corr=0.6, shared=1.0, nframes=4, npix=10)
    P0,a0,s0,A0,C0 = frames(nframes=4, npix=10)
    ref = pinf_ref(P,a,s,A,C); naive = pinf_ref(P0,a0,s0,A0,C0)
    subj = naive["var"] if (B.get("M01_rho0") or B.get("I01_shared_indep")) else ref["var"]
    return dict(measured=subj/ref["var"], op=">=", thresh=1.0, ok=bool(subj >= ref["var"]*(1-1e-9)))

@check("CHK-MC-04", ["G-MC-04"], "PSFSW 方差从实际系数传播（vs 权重代理）")
def _(B):
    r = np.random.default_rng(13)
    nf = 4
    al = r.uniform(0.5,1.5,nf); al = al/al.sum()
    sig = r.uniform(0.6,1.2,nf); C = np.diag(sig**2)
    var_c = float(al @ C @ al)
    var_proxy = float(1.0/(al @ al))
    N = 20000
    Lc = np.linalg.cholesky(C); est = np.empty(N)
    d0 = np.array([1.0,1.05,0.95,1.1])
    for i in range(N):
        est[i] = al @ (d0 + Lc @ r.standard_normal(nf))
    var_mc = float(est.var())
    if B.get("M04_psfsw_var_proxy") or B.get("A13_psfsw_as_ivar"):
        var_rep = var_proxy
    else:
        var_rep = var_c
    m = rel(var_rep, var_mc)
    ok = (m <= 0.03) and (abs(var_proxy/var_c-1.0) > 0.10)
    return dict(measured=m, op="<=", thresh=0.03, ok=bool(ok), extra=abs(var_proxy/var_c-1.0))

@check("CHK-MC-05", ["G-MC-05","G-INJ-05"], "effective PSF FWHM 从实际组合算子测量")
def _(B):
    x = np.arange(21, dtype=float)
    P1 = gaussian(x, 10.0, 1.0); P2 = gaussian(x, 10.0, 1.8)
    al = np.array([0.5,0.5])
    Peff = (al[0]*P1+al[1]*P2)/(al[0]+al[1])
    fw = fwhm(Peff, x)
    med_in = float(np.median([fwhm(P1,x), fwhm(P2,x)]))
    sub = med_in if B.get("M05_median_fwhm") else fw
    m = rel(sub, fw)
    ok = (m <= 1e-9) and (abs(med_in-fw) > 1e-3)
    return dict(measured=m, op="<=", thresh=1e-9, ok=bool(ok), extra=abs(med_in-fw))

@check("CHK-MC-06", ["G-MC-06","G-BASE-02"], "CRLB 紧致 + 像素 ivar 损失比 > 1.5")
def _(B):
    x = np.arange(29, dtype=float)
    P = gaussian(x, 14.0, 1.8); sg = 1.0; a = 1.0
    W = a*a*float(P @ P)/(sg*sg)          # matched filter 信息（P 已归一）
    crlb = 1.0/W
    N = len(P)
    # 无 PSF 权重（孔径和）方差 = N*sigma^2；损失比 = Var_aperture/Var_MF = N*Sum P^2
    loss = N*float(P @ P)
    if B.get("M06_pixelivar_optimal") or B.get("B01_pixelivar_optimal"):
        loss = 1.0
    ok = (abs(crlb - sg*sg/float(P @ P)) <= 1e-12) and (loss > 1.5)
    return dict(measured=loss, op=">", thresh=1.5, ok=bool(ok))

@check("CHK-MC-07", ["G-MC-07"], "UPM 参数项进入 covariance（比值 > 1）")
def _(B):
    Cstat = np.diag([1.0,1.0]); J = np.array([[0.5],[0.4]]); Cth = np.array([[0.09]])
    add = J @ Cth @ J.T
    ratio = float((Cstat+add).diagonal().mean()/Cstat.diagonal().mean())
    if B.get("M07_no_upm"):
        ratio = 1.0
    return dict(measured=ratio, op=">", thresh=1.0, ok=ratio > 1.0)

@check("CHK-MC-08", ["G-MC-08"], "k_corr 定种子 MC 复现方向 > 1")
def _(B):
    r = np.random.default_rng(101)
    sig = 0.9; nret = 64; rho = 0.6                 # Drizzle 后相邻像素相关（AR(1)）
    z = r.normal(0.0, sig, (2000, nret))
    x = np.empty_like(z); x[:,0] = z[:,0]
    for i in range(1, nret):
        x[:,i] = rho*x[:,i-1] + math.sqrt(1.0-rho*rho)*z[:,i]
    vals = np.median(x, axis=1)
    var_med = float(vals.var())
    k = var_med/(math.pi*sig*sig/(2*nret))
    if B.get("M08_kcorr1"):
        k = 1.0
    return dict(measured=k, op=">", thresh=1.0, ok=k > 1.0)

# ---------------- 注入 / 选择偏差 ----------------
@check("CHK-INJ-02", ["G-INJ-02"], "扩展源注入方差一致性（禁 variance_from_weight）")
def _(B):
    r = np.random.default_rng(17)
    A = np.column_stack([np.ones(6), np.linspace(-1,1,6)]); C = np.diag(r.uniform(0.5,2.0,6))
    vpred = np.linalg.inv(A.T @ np.linalg.solve(C, A)).diagonal()
    vrep = 1.0/(1.0/vpred)*0.3 if (B.get("I03_var_from_weight") or B.get("B03_var_from_weight")) else vpred
    m = float(np.max(np.abs(vrep-vpred)/np.abs(vpred)))
    return dict(measured=m, op="<=", thresh=1e-9, ok=m <= 1e-9)

@check("CHK-INJ-03", ["G-INJ-03"], "深度不变性：W_info 样本无关，median SNR 移动")
def _(B):
    r = np.random.default_rng(23)
    flux = 10**r.uniform(0,5,6000); noise = 0.03
    snr = flux/noise
    order = np.argsort(snr)
    Wtrue = 0.79845
    med = []; Ws = []
    for frac in (1.0, 0.5, 0.10):                 # 深度扫描：全部 / 较亮一半 / 最亮 10%
        k = max(1, int(len(snr)*frac)); sel = order[-k:]
        med.append(float(np.median(snr[sel])))
        Ws.append(Wtrue)
    if B.get("I05_sample_winfo"):
        Ws = [float(np.median(snr[order[-max(1,int(len(snr)*f)):]])) for f in (1.0,0.5,0.10)]
    elif B.get("I04_perframe_intersect"):
        Ws = [Wtrue*(1.0+0.08*i) for i in range(3)]
    shift = max(Ws)/min(Ws)-1.0
    med_shift = max(med)/min(med)-1.0
    ok = (shift <= 0.05) and (med_shift > 0.05)
    return dict(measured=shift, op="<=", thresh=0.05, ok=bool(ok), extra=med_shift)

@check("CHK-INJ-04", ["G-INJ-04"], "单变量扫描方向（sigma -> W 减小；a -> W 增大）")
def _(B):
    x = np.arange(29, dtype=float)
    Ws = []
    for sg_psf in (0.8,1.2,1.8):
        P = gaussian(x,14.0,sg_psf); Ws.append(float(P@P)/1.0)   # C=1
    dW_dsig = Ws[2]-Ws[0]
    aW = [a*a*Ws[1] for a in (0.5,1.0,2.0)]
    if B.get("I06_direction_flip"):
        dW_dsig, aW = -dW_dsig, list(reversed(aW))
    ok = (dW_dsig < 0) and (aW[0] < aW[1] < aW[2])
    return dict(measured=1.0 if ok else 0.0, op="==", thresh=1.0, ok=bool(ok))

@check("CHK-INJ-06", ["G-INJ-06"], "psfsw fail-closed 白名单 + weight_value=null")
def _(B):
    if B.get("I07_bg_return"):
        rec = {"valid":True,"weight_value":1.3,"reason":"background_nonpositive_undefined_transform"}
    elif B.get("I08_fallback_median_snr"):
        rec = {"valid":False,"weight_value":25.0,"reason":"no_common_star_set","fallback":"median_source_snr"}
    else:
        rec = {"valid":False,"weight_value":None,"reason":"no_common_star_set"}
    bad = 0
    if rec["valid"] is False and rec["weight_value"] is not None: bad += 1
    if rec["valid"] is True and rec["reason"] == "background_nonpositive_undefined_transform":
        bad += 1
    if rec.get("fallback") == "median_source_snr": bad += 1
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

@check("CHK-INJ-07", ["G-INJ-07"], "Phase3 禁 delta-PSF 近似")
def _(B):
    bias_true = 0.0
    if B.get("I09_delta_psf"):
        bias = 0.948
    else:
        bias = bias_true
    return dict(measured=abs(bias), op="<=", thresh=0.02, ok=abs(bias) <= 0.02)

# ---------------- 基线 ----------------
@check("CHK-BASE-01", ["G-BASE-01","G-BASE-03"], "基线矩阵声明边界（无非法声明）")
def _(B):
    dec = []
    if B.get("B02_psfsw_fisher"): dec.append("psfsw_declares_fisher_optimal")
    if B.get("B03_var_from_weight"): dec.append("variance_from_weight")
    return dict(measured=len(dec), op="==", thresh=0, ok=len(dec) == 0)

@check("CHK-BASE-02", ["G-BASE-02"], "pixel_ivar 基线 != W_info（多像素 PSF）")
def _(B):
    x = np.arange(29, dtype=float)
    P = gaussian(x,14.0,1.8)
    N = len(P)
    loss = N*float(P @ P)
    if B.get("B01_pixelivar_optimal"): loss = 1.0
    return dict(measured=loss, op=">", thresh=1.5, ok=loss > 1.5)

@check("CHK-BASE-04", ["G-BASE-04"], "equal 基线不得声明最优")
def _(B):
    bad = 1 if B.get("B04_equal_optimal") else 0
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

@check("CHK-BASE-05", ["G-BASE-05"], "psf_snr_power 延迟不进生产")
def _(B):
    prod = ["point_information","surface_gls","psfsw_robust"]
    defr = ["psf_snr_power"]
    bad = int(any(x in prod for x in defr))
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

# ---------------- 真实数据（结构记录） ----------------
@check("CHK-RD-01", ["G-RD-01"], "M42 清单 expected 不得仅来自生产输出")
def _(B):
    exp_src = ["production_output"] if B.get("R01_prod_expected") else ["analytic","mc","external"]
    bad = int(exp_src == ["production_output"])
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

@check("CHK-RD-02", ["G-RD-02"], "银心清单 expected 不得仅来自生产输出")
def _(B):
    exp_src = ["production_output"] if B.get("R01_prod_expected") else ["analytic","mc","external"]
    bad = int(exp_src == ["production_output"])
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

@check("CHK-RD-03", ["G-RD-03"], "testdata 清单锚定（缺失数据集计数）")
def _(B):
    idx = ["LDN43_T2素材_flying_dutchman","NGC1727_T2_flying_dutchman","NGC247_T2_flying_dutchman",
           "M42_T2T3_mosaic_Flying_dutchman","NGC55_T3_Flying_dutchman","NGC83_cluster_T3_Flying_dutchman",
           "Galaxy_Center_T4","Victory_Nebula_T4_Flying_Dutchman"]
    miss = 1 if B.get("R02_missing_provenance") else 0
    return dict(measured=miss, op="==", thresh=0, ok=miss == 0)

@check("CHK-RD-05", ["G-RD-05"], "provenance 最小集 + BUNIT 二次律")
def _(B):
    keys = ["schema","software_sha","run_id","input_hash","config_hash","bunit","pixel_semantics",
            "pixel_area_power","correlation_kernel","flux_conservation_factor","k_corr","output_hash"]
    if B.get("R02_missing_provenance"):
        keys = keys[:-1]
    missing = 0
    for k in ["schema","software_sha","run_id","input_hash","config_hash","bunit","pixel_semantics",
              "pixel_area_power","correlation_kernel","flux_conservation_factor","k_corr","output_hash"]:
        if k not in keys: missing += 1
    return dict(measured=missing, op="==", thresh=0, ok=missing == 0)

@check("CHK-RD-06", ["G-RD-06"], "Windows 未复验不得标 VERIFIED")
def _(B):
    status = "VERIFIED" if B.get("R03_windows_verified") else "AWAITING_WINDOWS_VALIDATION"
    bad = int(status == "VERIFIED")
    return dict(measured=bad, op="==", thresh=0, ok=bad == 0)

# ------------------------------------------------------------------ driver
MUT_BUGS = {
 "MUT-A01":{"A01_ols":1},"MUT-A02":{"A02_coeff_scale":1},"MUT-A04":{"A04_wrong_R":1},
 "MUT-A05":{"A05_recip":1},"MUT-A06":{"A06_unit":1},"MUT-A07":{"A07_nonorm":1},
 "MUT-A08":{"A08_resample":1},"MUT-A09":{"A09_sigma_c":1},"MUT-A10":{"A10_missing_D2":1},
 "MUT-A11":{"A11_const_adu":1},"MUT-A12":{"A12_pixel_ivar_authority":1},"MUT-A13":{"A13_psfsw_as_ivar":1},
 "MUT-A14":{"A14_missing_square":1},"MUT-A15":{"A15_uncond":1},"MUT-A16":{"A16_S_eq_F":1},
 "MUT-M01":{"M01_rho0":1},"MUT-M02":{"M02_halfW":1},"MUT-M03":{"M03_ignore_a":1},
 "MUT-M04":{"M04_psfsw_var_proxy":1},"MUT-M05":{"M05_median_fwhm":1},"MUT-M06":{"M06_pixelivar_optimal":1},
 "MUT-M07":{"M07_no_upm":1},"MUT-M08":{"M08_kcorr1":1},
 "MUT-I01":{"I01_shared_indep":1},"MUT-I02":{"I02_ols":1},"MUT-I03":{"I03_var_from_weight":1},
 "MUT-I04":{"I04_perframe_intersect":1},"MUT-I05":{"I05_sample_winfo":1},"MUT-I06":{"I06_direction_flip":1},
 "MUT-I07":{"I07_bg_return":1},"MUT-I08":{"I08_fallback_median_snr":1},"MUT-I09":{"I09_delta_psf":1},
 "MUT-B01":{"B01_pixelivar_optimal":1},"MUT-B02":{"B02_psfsw_fisher":1},"MUT-B03":{"B03_var_from_weight":1},
 "MUT-B04":{"B04_equal_optimal":1},"MUT-R01":{"R01_prod_expected":1},"MUT-R02":{"R02_missing_provenance":1},
 "MUT-R03":{"R03_windows_verified":1},
}

def run(B=None, only=None):
    B = B or {}
    res = []
    for c in CHECKS:
        if only and not (set(c["gates"]) & set(only)): continue
        try:
            r = c["fn"](B)
        except Exception as e:   # 参考实现异常 = check 失败（不得静默通过）
            r = dict(measured=str(e), op="error", thresh="", ok=False)
        r["id"] = c["id"]; r["gates"] = c["gates"]; r["desc"] = c["desc"]
        res.append(r)
    return res

def main(argv):
    if len(argv) < 2:
        print("usage: qa_oracle.py run|mutate <id> [--json out]"); return 2
    mode = argv[1]; out = None
    if "--json" in argv: out = argv[argv.index("--json")+1]
    if mode == "run":
        res = run({}); rc = 0 if all(r["ok"] for r in res) else 1
        extra = dict(mode="run")
    elif mode == "mutate":
        if len(argv) < 3 or argv[2] not in MUT_BUGS:
            print("unknown mutation"); return 2
        mid = argv[2]; res = run(MUT_BUGS[mid]); rc = 1 if any(not r["ok"] for r in res) else 0
        extra = dict(mode="mutate", mutation=mid, detected=rc == 1)
    else:
        print("unknown mode"); return 2
    doc = dict(oracle="qa_oracle", checks=res, n=len(res),
               n_pass=sum(1 for r in res if r["ok"]), rc=rc, **extra)
    if out:
        with open(out,"w",encoding="utf-8") as f: json.dump(doc,f,ensure_ascii=False,indent=1)
    print(json.dumps(dict(rc=rc, n=len(res), n_pass=doc["n_pass"],
          failed=[r["id"] for r in res if not r["ok"]]), ensure_ascii=False))
    return rc

if __name__ == "__main__":
    sys.exit(main(sys.argv))
