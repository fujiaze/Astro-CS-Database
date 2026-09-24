#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-RECON-SNR-01 / B7：**逐像素绝对 SNR 重建**的判别实验（四臂 + 归零判据）。

═══════════════════════════════════════════════════════════════════════════════
1. 显式模型（负责人假设的数学形式）
═══════════════════════════════════════════════════════════════════════════════
像素测量值的加性分解（单位一律 ADU，标度 = 校准后帧标度）：

    I(x,y) = S_src(x,y) + S_sky(x,y) + N_local(x,y)                        (M1)

  S_src   源信号电平        [ADU]  空间**快变**（PSF 尺度），∝ 局部源亮度
  S_sky   加性天光电平      [ADU]  空间**缓变**（大尺度梯度/光晕）
  N_local 本地噪声实现      [ADU]  零均值；其**方差图**空间缓变（读出/量化/暗流/偏置残差）

**两个方差面**（NOISE_MODEL.md §5 背景方差面 / §5c 加权方差面；量纲同 ADU²、用途不同）：

    sigma_slow^2(x,y) = Var[N_local] + S_sky(x,y)/g                        (M2)  缓变
    sigma_w^2(x,y)    = sigma_slow^2(x,y) + S_src(x,y)/g                   (M3)  含源项

    g : 增益 [e-/ADU]（本仓冻结约定，NOISE_MODEL §5c「增益约定」：ADU = N_e/g；
        文献里 I = O + g'·N_e + N_read 的倒数形式 g'=1/g 禁止与本仓混用，混用差 g² 倍）
    S/g 单位：电子域 Poisson 方差 N_e [e-²]，换算到 ADU 除以 g² ⇒ N_e/g² = (N_e/g)/g
        = (信号 ADU)/g ⇒ **ADU²**。与 NOISE_MODEL §5b 的 F·P(x,y)/g 同式。

**要重建的逐像素绝对 SNR**（分子**只有源**，天光只进噪声）：

    SNR(x,y) = S_src(x,y) / sigma_w(x,y)                                   (M4)

「缓变 / 快变」的**谓词**必须写死（NOISE_MODEL §5b「三个谓词判决不同，不得混用」）：
  - sigma_slow² 的**方差图**缓变 ⇒ 成立，可用稀疏控制点承载；
  - 本地噪声与天光散粒的**噪声实现**空间平滑 ⇒ **不成立**（各随机项 lag-1 自相关 ρ₁≈0）。
  ⇒ 缓变面承载的是**方差**，不是噪声实现。本实验对噪声实现的 ρ₁ 做数值复核（null 臂诊断）。

═══════════════════════════════════════════════════════════════════════════════
2. 源/天光分离（重点）
═══════════════════════════════════════════════════════════════════════════════
观测只给「源 + 天光」的和；单帧只有均值与方差两个可观测量，**不可分解**（NOISE_MODEL
§5b「可辨识性边界」：模型 μ=t(S+D+B)、V=f(μ) 时 Fisher 信息恒为秩 1）。因此分离
**只能靠支撑先验**，本实验用三条：

  P1 **缓变面可表示**：S_sky 落在稀疏张量积 B 样条空间内（节点数 ≪ 像素数）。
     ⇒ 生产 p2_sky_plane_build / p2_sky_plane_eval_block（加性天光稀疏表示）。
  P2 **源是稀疏的**：S_src = Σ_i F_i·P_i，由有限个 PSF 模板张成（稀疏支撑）。
     ⇒ 生产 p2weight 的 GLS 估计量 w_info_solve（FZ-FORMULA-WINFO/Q：F_hat = Q/W）
        + 生产 Moffat4 离散归一化轮廓。
  P3 **排异层次**：天光采样点必须落在源掩膜外；掩膜内/亮端污染点由生产
     p2_star_mask_caps / p2_star_mask_contains、p2_sky_patch_estimate（亮端裁剪 +
     contamination 门 + min_retained_fraction）与 p2_reject_plan_resolve /
     p2_reject_stack 分级剔除。

分离式（全部由 C++ 驱动在进程内直调生产函数算出，本文件只做代数组合与统计）：

    S_sky_hat = SkyPlane({(p_i, y_i, var_i) : p_i ∉ 源掩膜})               (S1)
    F_hat_i   = argmin_F (d − F·P_i)ᵀ C⁻¹ (d − F·P_i),  d = I − S_sky_hat   (S2)
    S_src_hat = Σ_i F_hat_i · P_i                                          (S3)

**先验失效的后果**（可证伪点）：P2 只张成点源 ⇒ 延展发射（星云本体）无法被 S_src_hat
表示 ⇒ 源项被低估 ⇒ SNR 被高估。M16 臂专门测这个边界。

═══════════════════════════════════════════════════════════════════════════════
3. 四臂（每臂只变一个量）+ 一个额外诊断臂
═══════════════════════════════════════════════════════════════════════════════
  T_full        var = sigma_slow² + S_src_hat/g             分子 = S_src_hat     （推导出的模型）
  T_slow        var = sigma_slow²                           分子 = S_src_hat     （漏源项 ⇒ 偏高）
  T_naive_sky   var = sigma_slow² + (S_src_hat+S_sky_hat)/g  分子 = S_src_hat     （天光双计 ⇒ 偏低）
  T_traditional var 同 T_naive_sky，**分子 = S_src_hat+S_sky_hat**（字面「天光进分子」）
  T_null        真值无源的数据上跑以上三臂（归零判据）

解析预言（用真值算，用于「实测 vs 预言」对拍）：
  ratio_full   = 1
  ratio_slow   = sqrt(sigma_w² / sigma_slow²) = sqrt(1 + (S_src/g)/sigma_slow²)
  ratio_naive  = sqrt(sigma_w² / (sigma_slow² + (S_src+S_sky)/g))
  ratio_trad   = ((S_src+S_sky)/S_src) · ratio_naive

**为什么 T_null 是判别实验的支点**：真值无源时 T_full 与 T_slow 的方差面恒等（源项 ≡ 0），
而 T_naive_sky 与它们**不收敛**——差的正是被双计的那份天光散粒。任务书「三臂必须收敛
到同一结果」对 T_naive_sky **不成立**；正确的归零判据是「源项估计归零 + T_full ≡ T_slow」。

═══════════════════════════════════════════════════════════════════════════════
4. 数据三类齐备（AGENTS §5 / ASTROCS_DESIGN §12.2）
═══════════════════════════════════════════════════════════════════════════════
  A 纯解析代数合成（真值已知）+ A_null 负例
  B testdata 真实信号模板 HST M16（WFC3/UVIS F657N drz）→ 物理前向仿真
  C testdata 真实数据 M42（Chilescope T3/M1 Red 六帧同指向）→ 一致性核对（无真值）

生产代码路径：见 code/b7_recon_driver.cpp 头注释（逐函数标注权威落点）。
本文件**不重新实现**噪声/SNR/天光/重建模型，只做代数组合与统计。
固定 seed：SEED_BASE = sci_b_common.SEED_BASE = 20260921（+ 每臂固定偏移）。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402  （复用既有夹具与口径，不另起一套）

HERE = os.path.dirname(os.path.abspath(__file__))
UNIT = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(UNIT))
RESULTS = os.path.join(UNIT, "results")
RUNDIR = os.path.join(ROOT, "run", "SCI-RECON-SNR-01")
BIN = os.path.join(RUNDIR, "bin", "b7_recon_driver")
WORK = os.path.join(RUNDIR, "work")
TESTDATA = os.path.join(ROOT, "testdata")

K_MAD = C.K_MAD_TO_SIGMA
QUANT_VAR = 1.0 / 12.0          # 量化方差 [ADU²]，LSB=1 ADU（exp06_common.py:64 同口径）

# ── 冻结实验配置（写死，禁止运行时调参） ─────────────────────────────────────
H = W = 512
SIGMA_PSF = 1.5                 # Moffat4 sigma [px]（检测块高斯 FWHM = 2.3548·sigma）
PROFILE_HALF = 15               # 轮廓网格半边长 ⇒ 31×31
PIX_SCALE_DEG = 1.0 / 512.0     # 像素 → 切平面度（天光面节点间距标度；线性 TAN 近似）
NODE_SPACING_DEG = 0.125        # 8×8 B 样条节点
SKY_BOX_PX = 16                 # 天光采样盒
GAIN = 1.3                      # [e-/ADU]
RN_E = 8.0                      # [e-]
DARK_E = 0.5                    # [e-/px]
SAT_ADU = 55000.0
SKY_BASE = 300.0                # 天光基准 [ADU]
# 主配置：缓变天光（帧内峰峰 12 ADU、团块 20 ADU）——**保证 patch 内天光变化远小于噪声**
#   （64 px patch 内天光 std ≈ 0.45 ADU ≪ σ_noise ≈ 16.4 ADU），使生产 patch 稳健尺度
#   对逐像素 σ_slow² 近乎无偏，四臂差异才可干净归因。
SKY_GRAD = 12.0
SKY_BLOB = 20.0
# 强梯度配置（A_grad）：帧内峰峰 60 ADU、团块 120 ADU ⇒ patch 内天光 std ≈ 8 ADU，
#   与噪声同量级 ⇒ 生产 patch 估计器会把「patch 内天光空间变化」计入噪声方差。
#   该配置专门量化**慢变面的分辨率偏差**，并检验其闭式预言（见 analyse_slow_bias）。
SKY_GRAD_STRONG = 60.0
SKY_BLOB_STRONG = 120.0
N_SRC = 160                     # 源数
F_LO, F_HI = 2.0e2, 1.0e5       # 源通量 [ADU]（峰值 < 饱和 55000 ADU），跨 2.7 个数量级 ⇒ 亮源杠杆臂
MARGIN = 20                     # 评价掩膜内缩 [px]

# ── 度量域（域是度量定义的一部分，先定义再跑；不是事后筛选） ──────────────────
#   D_all  : 有效掩膜全域（「逐像素」口径）
#   D_src  : S_src_true > 0（有源像素）
#   D_core : S_src_true/g ≥ 0.5·sigma_slow_true²（源散粒 ≥ 缓变方差的 50%）
#            ⇒ 解析预言 ratio_slow ≥ sqrt(1.5)=1.2247、ratio_naive ≲ 0.80，
#              两臂效应在**该域中位数**上必然可分辨（不靠极值）
#   D_snr3 : SNR_true ≥ 3（分子相对误差 ~1/SNR_F 已小的良测像素）
DOMAINS = ("D_all", "D_src", "D_core", "D_snr3", "D_point")
D_CORE_SRC_OVER_SLOW = 0.5
D_SNR3_FLOOR = 3.0

# ══════════════════════════════════════════════════════════════════════════════
# 预注册判据（**先写判据再跑**；阈值在跑实验之前固定，禁止事后凑）
# ══════════════════════════════════════════════════════════════════════════════
PREREGISTRATION = {
    "P1_full_recovers_truth": {
        "domain": "D_core",
        "rule": "解析臂 T_full 的逐像素 |SNR/SNR_true − 1| 中位数 ≤ 0.05 且 p95 ≤ 0.25",
        "median_max": 0.05, "p95_max": 0.25},
    "P1b_full_denominator_matches": {
        "domain": "D_core",
        "rule": "T_full 的方差面比值 sqrt(sigma_w,true²/sigma_arm²) 中位数落在 [0.95,1.05]",
        "lo": 0.95, "hi": 1.05},
    "P2_slow_biased_high": {
        "domain": "D_core",
        "rule": "T_slow 中位比值 median(SNR_arm/SNR_true) ≥ 1.10，且与解析预言 "
                "median(ratio_slow) 的相对差 ≤ 0.20",
        "median_ratio_min": 1.10, "pred_rel_tol": 0.20},
    "P3_naive_biased_low": {
        "domain": "D_core",
        "rule": "T_naive_sky 中位比值 ≤ 0.90，且与解析预言 median(ratio_naive) 的相对差 ≤ 0.20",
        "median_ratio_max": 0.90, "pred_rel_tol": 0.20},
    "P4_traditional_biased_high": {
        "domain": "D_core",
        "rule": "T_traditional（字面「天光进分子」）中位比值 ≥ 1.10",
        "median_ratio_min": 1.10},
    "P5_null_zeroing": {
        "domain": "D_all",
        "rule": "真值无源帧上 median(|S_src_hat|/sigma_slow) ≤ 0.05（源项估计归零）"
                " 且 T_full 与 T_slow 的方差面相对差 ≤ 1e-12（恒等）",
        "src_over_sigma_max": 0.05, "identity_tol": 1e-12},
    "P6_null_naive_diverges": {
        "domain": "D_all",
        "rule": "同一无源帧上 T_naive_sky/T_full 的方差比中位数与解析预言 "
                "median((sigma_slow²+S_sky/g)/sigma_slow²) 相对差 ≤ 0.10，"
                "且该比值本身偏离 1 超过 5%（⇒「三臂收敛」对 T_naive_sky 不成立）",
        "pred_rel_tol": 0.10, "min_dev": 0.05},
    "P7_null_gate_non_degenerate": {
        "domain": "D_core",
        "rule": "把同一 P5 判据施加到**有源**帧上必须判红（median(|S_src_hat|/sigma_slow) > 0.05）",
        "src_over_sigma_min": 0.05},
}


# ══════════════════════════════════════════════════════════════════════════════
# 驱动 I/O
# ══════════════════════════════════════════════════════════════════════════════
META_KEYS = [
    "profile_selfcheck", "rc_model", "rc_fill", "has_spatial_field", "degenerate",
    "mask_degraded", "sigma_bg_global", "variance_bg_global", "n_qualified_patches",
    "n_rejected_patches", "mask_radius_p50", "mask_frac", "n_ctrl",
    "recon_node_resid_bilinear", "recon_node_resid_bicubic", "recon_ok_bilinear",
    "recon_ok_bicubic", "rc_plan", "plan_method", "plan_nominal_n", "n_patch_ok",
    "n_patch_rejected", "rc_sky", "sky_n_samples", "sky_n_used", "sky_n_nodes",
    "sky_rms_weighted", "sky_chi2_red", "sky_iterations", "sky_n_rejected",
    "n_sky_eval_out", "n_flux_ok", "n_star_snr_ok",
    # 注意：顺序必须与 b7_recon_driver.cpp:658-669 的 push 顺序逐项一致
    "sdet_rc", "sdet_count", "n_stars_use",
    "rej_rc", "rej_n", "rej_accepted", "rej_low", "rej_high", "rej_under",
    "rej_status_last",
]


def write_driver_input(path, hdr, data, stars, src_true, var_slow_true):
    with open(path, "wb") as f:
        for k, v in hdr.items():
            f.write(("%s %r\n" % (k, v)).encode())
        f.write(b"BEGIN_DATA\n")
        f.write(np.ascontiguousarray(data, dtype="<f8").tobytes())
        f.write(np.ascontiguousarray(stars, dtype="<f8").tobytes())
        f.write(np.ascontiguousarray(src_true, dtype="<f8").tobytes())
        f.write(np.ascontiguousarray(var_slow_true, dtype="<f8").tobytes())


def read_driver_output(path, h, w):
    """按驱动实际写出的数组长度读取（n_ctrl / n_stars_use 取自 meta，不信任入参计数）。"""
    npix = h * w
    with open(path, "rb") as f:
        meta = np.frombuffer(f.read(8 * len(META_KEYS)), dtype="<f8")
        tl = int(np.frombuffer(f.read(4), dtype="<u4")[0])
        txt = f.read(tl).decode("utf-8", "replace")
        mm = {k: float(v) for k, v in zip(META_KEYS, meta)}

        def rd(n):
            return np.frombuffer(f.read(8 * n), dtype="<f8").copy()

        nc = max(int(mm["n_ctrl"]), 1)
        ns = max(int(mm["n_stars_use"]), 1)
        out = dict(mm)
        out["text_header"] = txt
        out["ctrl_x"] = rd(nc)
        out["ctrl_y"] = rd(nc)
        out["ctrl_var"] = rd(nc)
        out["ctrl_sigma"] = rd(nc)
        out["var_plane"] = rd(npix).reshape(h, w)
        out["var_recon_bilinear"] = rd(npix).reshape(h, w)
        out["var_recon_bicubic"] = rd(npix).reshape(h, w)
        out["sky_face"] = rd(npix).reshape(h, w)
        out["ssrc_est"] = rd(npix).reshape(h, w)
        out["src_term"] = rd(npix).reshape(h, w)
        out["src_term_true"] = rd(npix).reshape(h, w)
        out["var_slow_true"] = rd(npix).reshape(h, w)
        out["star_x"] = rd(ns)
        out["star_y"] = rd(ns)
        out["star_flux_sdet"] = rd(ns)
        out["star_fwhm"] = rd(ns)
        out["flux_hat"] = rd(ns)
        out["flux_var"] = rd(ns)
        out["per_star_snr"] = rd(ns)
        out["per_star_sigma_f"] = rd(ns)
    return out


def run_driver(tag, hdr, data, stars, src_true, var_slow_true, h, w):
    os.makedirs(WORK, exist_ok=True)
    fin = os.path.join(WORK, "in_%s.bin" % tag)
    fout = os.path.join(WORK, "out_%s.bin" % tag)
    write_driver_input(fin, hdr, data, stars, src_true, var_slow_true)
    env = dict(os.environ)
    env.setdefault("OMP_NUM_THREADS", "4")
    p = subprocess.run([BIN, fin, fout], capture_output=True, text=True, env=env)
    if p.returncode != 0:
        raise RuntimeError("driver failed (%s): %s" % (tag, p.stderr[-2000:]))
    out = read_driver_output(fout, h, w)
    out["stdout"] = p.stdout.strip()
    for f_ in (fin, fout):
        try:
            os.remove(f_)
        except OSError:
            pass
    return out


# ══════════════════════════════════════════════════════════════════════════════
# 数据生成
# ══════════════════════════════════════════════════════════════════════════════
def slow_sky_surface(h, w, seed_off, base=SKY_BASE, grad=SKY_GRAD, blob=SKY_BLOB):
    """缓变天光面 [ADU]：常数 + 线性梯度 + 一个高斯团块（相关长度 ≈ 场尺度/3）。"""
    r = C.rng(seed_off)
    yy, xx = np.mgrid[0:h, 0:w]
    xn = xx / float(w) - 0.5
    yn = yy / float(h) - 0.5
    th = r.uniform(0, 2 * np.pi)
    gx, gy = np.cos(th), np.sin(th)
    surf = base + grad * (gx * xn + gy * yn)
    xc, yc = r.uniform(0.2, 0.8) * w, r.uniform(0.2, 0.8) * h
    sb = w / 3.0
    surf = surf + blob * np.exp(-(((xx - xc) ** 2 + (yy - yc) ** 2) / (2.0 * sb * sb)))
    return surf


def place_sources(h, w, n, seed_off, margin=MARGIN + 8, min_sep=18.0):
    r = C.rng(seed_off)
    xs, ys, fs = [], [], []
    tries = 0
    while len(xs) < n and tries < 200 * n:
        tries += 1
        x = r.uniform(margin, w - margin)
        y = r.uniform(margin, h - margin)
        if xs and min((x - a) ** 2 + (y - b) ** 2 for a, b in zip(xs, ys)) < min_sep ** 2:
            continue
        xs.append(x)
        ys.append(y)
        fs.append(10.0 ** r.uniform(np.log10(F_LO), np.log10(F_HI)))
    return np.array(xs), np.array(ys), np.array(fs)


def render_sources(shape, xs, ys, fs, sigma_px, half=PROFILE_HALF):
    """解析真值源面 [ADU]：Σ F_i·P_i（生产 Moffat4 离散归一化轮廓，与 sci_b_common 同式）。"""
    P, _, _, _ = C.moffat4_grid(sigma_px, half)
    n = P.shape[0]
    out = np.zeros(shape, dtype=np.float64)
    for x, y, F in zip(xs, ys, fs):
        x0 = int(round(x)) - half
        y0 = int(round(y)) - half
        xa, xb = max(0, x0), min(shape[1], x0 + n)
        ya, yb = max(0, y0), min(shape[0], y0 + n)
        if xa >= xb or ya >= yb:
            continue
        out[ya:yb, xa:xb] += F * P[ya - y0:yb - y0, xa - x0:xb - x0]
    return out


def forward_frame(src_adu, sky_adu, seed_off, gain=GAIN, rn=RN_E, dark=DARK_E,
                  sat=SAT_ADU, quantize=True):
    """物理前向：电子域 Poisson(源+天光+暗流) + 电子域 Gaussian 读出 → ADU → 量化/钳位。

    与 实验/shared/synthetic/noise_model.py expose()（:226-329）同物理过程；
    本函数只做数据生成（不属被验证模型），在报告「诚实边界」中声明。
    """
    r = C.rng(seed_off)
    lam_e = np.maximum((src_adu + sky_adu + dark) * gain, 0.0)
    n_e = r.poisson(lam_e).astype(np.float64)
    if rn > 0:
        n_e += r.normal(0.0, rn, size=lam_e.shape)
    adu = n_e / gain
    if quantize:
        adu = np.round(adu)
    return np.minimum(adu, sat)


def truth_var_slow(sky_adu, gain=GAIN, rn=RN_E, dark=DARK_E):
    """真值缓变方差面 [ADU²]：天光/暗流散粒 + 读出 + 量化（NOISE_MODEL §5b 各项）。"""
    return (sky_adu + dark) / gain + (rn / gain) ** 2 + QUANT_VAR


def scene_analytic(seed_off, null=False, sky_kw=None, name=None):
    sky_kw = sky_kw or {}
    sky = slow_sky_surface(H, W, seed_off + 1, **sky_kw)
    if null:
        xs = ys = fs = np.array([])
    else:
        xs, ys, fs = place_sources(H, W, N_SRC, seed_off + 2)
    src = render_sources((H, W), xs, ys, fs, SIGMA_PSF) if fs.size else np.zeros((H, W))
    var_slow = truth_var_slow(sky)
    data = forward_frame(src, sky, seed_off + 3)
    # 星表 FWHM 用 **PSF 块**约定 FWHM_moffat4 = 1.230310*sigma（与驱动/生产调用点同源）
    stars = np.zeros((0, 4)) if fs.size == 0 else np.column_stack(
        [xs, ys, fs, np.full(fs.size, SIGMA_PSF * C.K_MOFFAT4_FWHM)])
    return dict(name=name or ("A_null" if null else "A_analytic"), data=data[None, :, :],
                src_true=src, sky_true=sky, var_slow_true=var_slow, stars=stars,
                truth_source_count=int(fs.size), truth_flux=fs, truth_xy=(xs, ys))


def scene_hst(seed_off, crop=512, scale_p999=2000.0, n_point=60,
              pt_flux=(1.0e3, 2.0e5)):
    """testdata HST M16 F657N drz 作**真实信号模板**，做物理前向仿真。

    诚实边界：drz 是 NDRIZIM=32 的合成品（结构模板，非独立泊松样本）；
    本函数只取其**结构**，噪声一律由 forward_frame 逐像素独立重生成。
    """
    from astropy.io import fits
    from scipy.ndimage import median_filter
    p = os.path.join(TESTDATA, "HST_M16", "hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits")
    with fits.open(p, memmap=False) as hd:
        raw = np.asarray(hd[0].data, dtype=np.float64)
        hdr = hd[0].header
    ny, nx = raw.shape
    y0, x0 = ny // 2 - crop // 2, nx // 2 - crop // 2
    T = raw[y0:y0 + crop, x0:x0 + crop]
    T = np.maximum(T - np.median(T), 0.0)
    T_hp = np.maximum(T - median_filter(T, size=65, mode="nearest"), 0.0)
    alpha = scale_p999 / max(np.percentile(T_hp, 99.9), 1e-12)
    src_ext = alpha * T_hp                      # 延展分量（星云纤维）
    # 点源群：与延展分量叠加，使 B 臂同时含「可被稀疏先验表示」与「不可表示」两部分
    _flo, _fhi = F_LO, F_HI
    globals()["F_LO"], globals()["F_HI"] = pt_flux     # 点源通量域（仅本函数内）
    try:
        pxs, pys, pfs = place_sources(crop, crop, n_point, seed_off + 7)
    finally:
        globals()["F_LO"], globals()["F_HI"] = _flo, _fhi
    src_pt = render_sources((crop, crop), pxs, pys, pfs, SIGMA_PSF)
    src = src_ext + src_pt
    sky = slow_sky_surface(crop, crop, seed_off + 1)
    var_slow = truth_var_slow(sky)
    data = forward_frame(src, sky, seed_off + 3)
    return dict(name="B_hst_m16", data=data[None, :, :], src_true=src, sky_true=sky,
                var_slow_true=var_slow, stars=np.zeros((0, 4)),
                truth_source_count=int(pfs.size), truth_xy=(pxs, pys),
                src_point=src_pt, src_ext=src_ext, template=dict(
                    file=os.path.basename(p), crop=[int(y0), int(x0), crop, crop],
                    bunit=str(hdr.get("BUNIT", "")), ndrizim=int(hdr.get("NDRIZIM", 0)),
                    alpha=float(alpha), highpass_box=65, n_point=int(pfs.size),
                    point_flux_range_adu=list(pt_flux),
                    note="源面 = 模板高通分量×alpha（延展）+ %d 个 Moffat4 点源；"
                         "天光 = 注入缓变解析面" % int(pfs.size)))


def scene_m42(seed_off, crop=512, n_frames=6):
    """testdata M42（Chilescope T3/M1 Red 六帧同指向）→ 一致性核对（无真值）。"""
    from astropy.io import fits
    d = os.path.join(TESTDATA, "M42_T2T3_mosaic_Flying_dutchman", "T3", "M1")
    files = sorted(f for f in os.listdir(d) if f.endswith("300S-Red.fts"))
    files = [os.path.join(d, f) for f in files][:n_frames]
    frames, metas = [], []
    for f in files:
        with fits.open(f, memmap=False) as hd:
            raw = np.asarray(hd[0].data, dtype=np.float64)
            hdr = hd[0].header
        ny, nx = raw.shape
        y0, x0 = ny // 2 - crop // 2, nx // 2 - crop // 2
        frames.append(raw[y0:y0 + crop, x0:x0 + crop])
        metas.append(dict(file=os.path.basename(f), exptime=float(hdr.get("EXPTIME", 0.0)),
                          filter=str(hdr.get("FILTER", "")).strip(),
                          gain_header=hdr.get("GAIN"), rdnoise_header=hdr.get("RDNOISE"),
                          bunit=str(hdr.get("BUNIT", "")).strip(),
                          crop=[int(y0), int(x0), crop, crop]))
    return dict(name="C_m42_real", data=np.stack(frames, axis=0), src_true=None,
                sky_true=None, var_slow_true=None, stars=np.zeros((0, 4)),
                truth_source_count=-1, truth_xy=None, frames_meta=metas)


# ══════════════════════════════════════════════════════════════════════════════
# 臂组合与度量
# ══════════════════════════════════════════════════════════════════════════════
def base_mask(var_slow_hat, h=H, w=W, margin=MARGIN):
    m = np.zeros((h, w), dtype=bool)
    m[margin:h - margin, margin:w - margin] = True
    m &= np.isfinite(var_slow_hat) & (var_slow_hat > 0)
    return m


def domains(mask, src_true, var_slow_true, snr_true, gain=GAIN, src_point=None,
            src_ext=None):
    D = {"D_all": mask.copy()}
    has_src = src_true is not None and bool(np.any(np.asarray(src_true) > 0))
    if has_src:
        D["D_src"] = mask & (src_true > 0)
        D["D_core"] = mask & (src_true / gain >= D_CORE_SRC_OVER_SLOW * var_slow_true)
        D["D_snr3"] = mask & (snr_true >= D_SNR3_FLOOR)
        if src_point is not None and src_ext is not None:
            # 点源分量 >= 50% 的「本底+延展」方差 ⇒ 稀疏先验在该像素上原则上可表示
            D["D_point"] = mask & (src_point / gain >=
                                   D_CORE_SRC_OVER_SLOW * (var_slow_true + src_ext / gain))
        else:
            D["D_point"] = np.zeros_like(mask)
    else:
        for k in ("D_src", "D_core", "D_snr3", "D_point"):
            D[k] = np.zeros_like(mask)
    return D


def arms(out, gain=GAIN, slow_key="var_recon_bilinear"):
    """由生产分量面组合五臂：(分子, sigma) 面。"""
    vs = out[slow_key]
    src_hat = out["ssrc_est"]
    sky_pos = np.maximum(out["sky_face"], 0.0)
    g = float(gain)
    t_src = src_hat / g
    t_sky = sky_pos / g
    A = {
        "T_full": (src_hat, vs + t_src),
        "T_slow": (src_hat, vs),
        "T_naive_sky": (src_hat, vs + t_src + t_sky),
        "T_traditional": (src_hat + sky_pos, vs + t_src + t_sky),
    }
    return {k: (n, np.sqrt(np.maximum(v, 1e-300))) for k, (n, v) in A.items()}


def truth_faces(scene, gain=GAIN):
    src = scene["src_true"]
    vs = scene["var_slow_true"]
    vw = vs + src / gain
    return src, vs, vw, src / np.sqrt(vw)


def stats_on(ratio, mask):
    m = mask & np.isfinite(ratio)
    if int(m.sum()) < 16:
        return dict(n=int(m.sum()), median=float("nan"), p05=float("nan"), p25=float("nan"),
                    p75=float("nan"), p95=float("nan"))
    v = ratio[m]
    return dict(n=int(m.sum()), median=float(np.median(v)),
                p05=float(np.percentile(v, 5)), p25=float(np.percentile(v, 25)),
                p75=float(np.percentile(v, 75)), p95=float(np.percentile(v, 95)))


def absrel_stats(arm_snr, snr_true, mask):
    m = mask & np.isfinite(arm_snr) & np.isfinite(snr_true) & (snr_true > 0) & (arm_snr > 0)
    if int(m.sum()) < 16:
        return dict(n=int(m.sum()), median_abs_rel=float("nan"), p95_abs_rel=float("nan"))
    rel = np.abs(arm_snr[m] / snr_true[m] - 1.0)
    return dict(n=int(m.sum()), median_abs_rel=float(np.median(rel)),
                p95_abs_rel=float(np.percentile(rel, 95)))


def lag1_autocorr(x, mask):
    a = np.where(mask, x, np.nan)
    d = a[:, 1:] - a[:, :-1]
    v = d[np.isfinite(d)]
    if v.size < 64:
        return float("nan")
    v = v - np.mean(v)
    den = float(np.sum(v * v))
    return float(np.sum(v[1:] * v[:-1]) / den) if den > 0 else float("nan")


# ══════════════════════════════════════════════════════════════════════════════
# 场景执行与解析
# ══════════════════════════════════════════════════════════════════════════════
def run_scene(scene, use_sdet, slow_key="var_recon_bilinear"):
    h, w = scene["data"].shape[1], scene["data"].shape[2]
    stars = scene["stars"]
    src_true = scene["src_true"] if scene["src_true"] is not None else np.zeros((h, w))
    var_slow_true = (scene["var_slow_true"] if scene["var_slow_true"] is not None
                     else np.zeros((h, w)))
    hdr = dict(h=h, w=w, gain=GAIN, rn_e=RN_E, dark_e=DARK_E, saturation=SAT_ADU,
               n_frames=int(scene["data"].shape[0]), pix_scale_deg=PIX_SCALE_DEG,
               node_spacing_deg=NODE_SPACING_DEG, sky_box_px=SKY_BOX_PX,
               psf_sigma_px=SIGMA_PSF, profile_half=PROFILE_HALF,
               use_star_prior=1, n_stars=int(stars.shape[0]), use_sdet=int(use_sdet))
    out = run_driver(scene["name"], hdr, scene["data"], stars, src_true, var_slow_true, h, w)
    out["scene"] = scene["name"]
    out["use_sdet"] = int(use_sdet)
    out["slow_key"] = slow_key
    return out


def driver_diag(out):
    d = {k: out[k] for k in META_KEYS}
    d["text_header"] = out["text_header"]
    return d


def analyse_truth_scene(scene, out, slow_key="var_recon_bilinear"):
    """有真值的场景（A 解析 / B HST 模板）：逐像素 SNR 与真值对拍。"""
    src_true, vs_true, vw_true, snr_true = truth_faces(scene)
    mask = base_mask(out[slow_key], *src_true.shape)
    D = domains(mask, src_true, vs_true, snr_true,
                src_point=scene.get("src_point"), src_ext=scene.get("src_ext"))
    A = arms(out, slow_key=slow_key)
    sky_pos_t = np.maximum(scene["sky_true"], 0.0)
    t_src_t = src_true / GAIN
    t_sky_t = sky_pos_t / GAIN
    pred = {
        "T_full": np.ones_like(vs_true),
        "T_slow": np.sqrt(vw_true / vs_true),
        "T_naive_sky": np.sqrt(vw_true / (vs_true + t_src_t + t_sky_t)),
        "T_traditional": np.where(src_true > 0,
                                  (src_true + sky_pos_t) / np.maximum(src_true, 1e-300) *
                                  np.sqrt(vw_true / (vs_true + t_src_t + t_sky_t)), np.nan),
    }
    res = {"scene": scene["name"], "driver": driver_diag(out),
           "domain_sizes": {k: int(v.sum()) for k, v in D.items()}, "arms": {}}
    for k, (num, sig) in A.items():
        row = {"by_domain": {}}
        ratio = num / sig / np.where(snr_true > 0, snr_true, np.nan)
        for dname in DOMAINS:
            dm = D[dname]
            st = stats_on(ratio, dm)
            st.update(absrel_stats(num / sig, snr_true, dm))
            st["pred_median"] = stats_on(pred[k], dm)["median"]
            st["pred_rel_diff"] = (float(abs(st["median"] / st["pred_median"] - 1.0))
                                   if (np.isfinite(st["pred_median"]) and
                                       st["pred_median"] > 0 and np.isfinite(st["median"]))
                                   else float("nan"))
            st["den_ratio_median"] = stats_on(np.sqrt(vw_true / (sig * sig)), dm)["median"]
            st["num_ratio_median"] = stats_on(
                np.where(src_true > 0, num / np.maximum(src_true, 1e-300), np.nan), dm)["median"]
            row["by_domain"][dname] = st
        res["arms"][k] = row
    m2 = D["D_core"]
    s_over_sig = np.abs(out["ssrc_est"]) / np.sqrt(np.maximum(out[slow_key], 1e-300))
    res["src_recovery"] = dict(
        core_n=int(m2.sum()),
        median_ratio_D_core=stats_on(
            np.where(src_true > 0, out["ssrc_est"] / np.maximum(src_true, 1e-300), np.nan),
            m2)["median"],
        median_src_over_sigma_D_all=stats_on(s_over_sig, D["D_all"])["median"],
        median_src_over_sigma_D_core=stats_on(s_over_sig, m2)["median"],
        src_term_captured_fraction=float(np.sum(out["src_term"]) /
                                         max(np.sum(out["src_term_true"]), 1e-300)))
    mr = D["D_all"] & (vs_true > 0) & (out[slow_key] > 0)
    res["slow_recovery"] = stats_on(out[slow_key] / np.where(vs_true > 0, vs_true, np.nan), mr)
    if scene.get("truth_source_count", -1) > 0:
        res["detection"] = dict(
            mode=("sdet_blind" if int(out["use_sdet"]) == 1 else "given_catalog"),
            truth_n=int(scene["truth_source_count"]),
            sdet_n=int(out["sdet_count"]),
            stars_used=int(out["n_stars_use"]),
            completeness=(float(out["sdet_count"] / max(scene["truth_source_count"], 1))
                          if int(out["use_sdet"]) == 1 else None))
    res["profile_selfcheck_rel"] = out["profile_selfcheck"]
    return res


def analyse_slow_bias(scene, out, slow_key="var_recon_bilinear"):
    """慢变面的**分辨率偏差**：生产 patch 稳健尺度把「patch 内天光空间变化」也计入噪声。

    闭式预言（逐 patch，用真值算）：
        E[sigma_hat^2] / sigma_slow^2(x,y)  ~=  1 + Var_patch[S_sky]/sigma_slow^2
    其中 Var_patch 是 patch 内天光面的空间方差。该式只用到「方差可加 + patch 内取
    空间方差」两条，不引入新模型；实测比值与之对拍即判据。
    """
    sky = scene["sky_true"]
    vs_true = scene["var_slow_true"]
    nc = int(out["n_ctrl"])
    cx, cy = out["ctrl_x"][:nc], out["ctrl_y"][:nc]
    step = int(round(float(np.min(np.diff(np.unique(np.round(cx))))))) if nc > 1 else 64
    rows = []
    for x, y in zip(cx, cy):
        xi, yi = int(round(x)), int(round(y))
        h0 = step // 2
        y0, y1 = max(0, yi - h0), min(sky.shape[0], yi + h0)
        x0, x1 = max(0, xi - h0), min(sky.shape[1], xi + h0)
        if y1 - y0 < 8 or x1 - x0 < 8:
            continue
        vsky = float(np.var(sky[y0:y1, x0:x1]))
        vtrue = float(np.median(vs_true[y0:y1, x0:x1]))
        if vtrue > 0:
            rows.append((vtrue, vsky, 1.0 + vsky / vtrue))
    if len(rows) < 8:
        return dict(n=len(rows), ok=False)
    rows = np.array(rows)
    meas = out[slow_key][np.round(cy).astype(int), np.round(cx).astype(int)][:len(rows)]
    # 控制点值本身即生产估计（未插值）
    ctrl_ratio = out["ctrl_var"][:len(rows)] / rows[:, 0]
    pred = rows[:, 2]
    return dict(n=int(len(rows)), step_px=int(step),
                within_patch_sky_var_median=float(np.median(rows[:, 1])),
                sigma_slow2_true_median=float(np.median(rows[:, 0])),
                pred_ratio_median=float(np.median(pred)),
                measured_ctrl_ratio_median=float(np.median(ctrl_ratio)),
                pred_rel_diff=float(abs(np.median(ctrl_ratio) / np.median(pred) - 1.0)),
                recon_ratio_median=float(np.median(out[slow_key] / vs_true)),
                ok=True)


def analyse_null(scene, out, slow_key="var_recon_bilinear"):
    mask = base_mask(out[slow_key], *out[slow_key].shape)
    D = domains(mask, None, None, None)
    vs = out[slow_key]
    A = arms(out, slow_key=slow_key)
    src_over_sigma = np.abs(out["ssrc_est"]) / np.sqrt(np.maximum(vs, 1e-300))
    res = {"scene": scene["name"], "driver": driver_diag(out),
           "domain_sizes": {k: int(v.sum()) for k, v in D.items()},
           "src_over_sigma": stats_on(src_over_sigma, D["D_all"]),
           "src_over_sigma_p95": float(np.percentile(src_over_sigma[D["D_all"]], 95)),
           "src_term_max_abs": float(np.max(np.abs(out["src_term"]))),
           "snr_null": stats_on(A["T_full"][0] / A["T_full"][1], D["D_all"])}
    rel = np.abs(A["T_full"][1][D["D_all"]] / A["T_slow"][1][D["D_all"]] - 1.0)
    res["full_vs_slow_sigma_rel_max"] = float(np.max(rel)) if rel.size else float("nan")
    sky_pos = np.maximum(out["sky_face"], 0.0)
    r_meas = A["T_naive_sky"][1][D["D_all"]] / A["T_full"][1][D["D_all"]]
    r_pred = np.sqrt((vs + sky_pos / GAIN) / np.maximum(vs, 1e-300))[D["D_all"]]
    res["naive_over_full_median"] = float(np.median(r_meas))
    res["naive_over_full_pred_median"] = float(np.median(r_pred))
    res["naive_over_full_pred_rel_diff"] = float(
        abs(res["naive_over_full_median"] / res["naive_over_full_pred_median"] - 1.0))
    noise_impl = scene["data"][0] - scene["src_true"] - scene["sky_true"]
    res["noise_impl_lag1_rho"] = lag1_autocorr(noise_impl, D["D_all"])
    vt = scene["var_slow_true"]
    res["var_slow_true_relspread"] = float(np.std(vt[D["D_all"]]) /
                                           max(np.mean(vt[D["D_all"]]), 1e-300))
    return res


# ══════════════════════════════════════════════════════════════════════════════
# M42 真实数据一致性核对（无真值）
# ══════════════════════════════════════════════════════════════════════════════
def analyse_m42(scene, out, slow_key="var_recon_bilinear"):
    data = scene["data"]
    f0 = data[0]
    h, w = f0.shape
    mask = base_mask(out[slow_key], h, w)
    vs = out[slow_key]
    res = {"scene": scene["name"], "n_eval_px": int(mask.sum()),
           "frames_meta": scene["frames_meta"], "driver": driver_diag(out),
           "honest_note": "无真值；全部为自洽性核对，不能证明绝对正确"}

    src_free = mask & (out["ssrc_est"] <= 0.02 * float(np.max(out["ssrc_est"])))
    d2 = f0[:, :-2] - 2.0 * f0[:, 1:-1] + f0[:, 2:]
    v = d2[src_free[:, 1:-1]]
    sig_mean = float(np.sqrt(np.var(v, ddof=1) / 6.0)) if v.size > 64 else float("nan")
    # 稳健版：1.4826*MAD(Δ²)（未分辨星云结构使**均值**版被结构污染，MAD 版抗污染）
    sig_mad = (float(C.K_MAD_TO_SIGMA * np.median(np.abs(v - np.median(v))) / np.sqrt(6.0))
               if v.size > 64 else float("nan"))
    sig_model = float(np.median(np.sqrt(np.maximum(vs[src_free], 0.0))))
    res["C1_independent_sigma"] = dict(
        second_diff_sigma_mean_adu=sig_mean, second_diff_sigma_robust_adu=sig_mad,
        model_sigma_adu=sig_model,
        ratio_robust=(sig_mad / sig_model if sig_model > 0 else float("nan")),
        ratio_mean=(sig_mean / sig_model if sig_model > 0 else float("nan")),
        contamination_factor=(sig_mean / sig_mad if sig_mad > 0 else float("nan")),
        rule="二阶差分（线性梯度精确对消）与生产方差面在源无关区的中位 σ 之比。"
             "**稳健（1.4826·MAD）版**落在 [0.5, 2.0] 视为一致；**均值版**会被未分辨"
             "星云结构污染（contamination_factor 即污染倍数），仅作诊断。")

    nsu = int(out["n_stars_use"])
    fx = out["flux_hat"][:nsu]
    ps = out["per_star_snr"][:nsu]
    ok = np.isfinite(fx) & np.isfinite(ps) & (fx > 0) & (ps > 0)
    if int(ok.sum()) >= 16:
        from scipy.stats import spearmanr
        rho, pv = spearmanr(fx[ok], ps[ok])
        res["C2_spearman_flux_vs_snr"] = dict(rho=float(rho), p=float(pv), n=int(ok.sum()))
    else:
        res["C2_spearman_flux_vs_snr"] = dict(rho=float("nan"), n=int(ok.sum()))

    ratio = np.abs(out["ssrc_est"][src_free]) / np.sqrt(np.maximum(vs[src_free], 1e-300))
    res["C3_source_free_src_over_sigma"] = dict(
        median=float(np.median(ratio)), p95=float(np.percentile(ratio, 95)),
        rule="源无关区 |S_src_hat|/σ_slow 的中位数应 ≤ 0.05")

    try:
        sys.path.insert(0, os.path.join(HERE, "exp06"))
        import exp06_common as X  # noqa: E402
        nc = int(out["n_ctrl"])
        cx, cy = out["ctrl_x"][:nc], out["ctrl_y"][:nc]
        s_ctrl = out["ctrl_sigma"][:nc]
        D_ctrl = np.array([out["sky_face"][int(round(y)), int(round(x))]
                           for x, y in zip(cx, cy)])
        lev = X.lever_arm_diag(D_ctrl, float(np.median(s_ctrl) ** 2))
        fit = X.fit_nlf(D_ctrl, s_ctrl)
        res["C4_gain_identifiability"] = dict(
            lever_arm=lev,
            fit={k: v for k, v in fit.items() if isinstance(v, (int, float, bool))},
            rule="§5c 路径 1：V=σ0²+S/g 是 2 未知量 1 观测量，需亮源杠杆臂；"
                 "lever_var < 0.15 时斜率不可辨识（exp06_common.py:411-432）")
    except Exception as exc:  # pragma: no cover
        res["C4_gain_identifiability"] = dict(error=repr(exc))
    return res


# ══════════════════════════════════════════════════════════════════════════════
# 判据
# ══════════════════════════════════════════════════════════════════════════════
def evaluate_gates(R):
    P = PREREGISTRATION
    aA = R["A_analytic"]["arms"]
    aN = R["A_null"]
    g = {}
    full = aA["T_full"]["by_domain"]["D_core"]
    g["P1_domain_n"] = full["n"]
    g["P1_median_abs_rel"] = full["median_abs_rel"]
    g["P1_p95_abs_rel"] = full["p95_abs_rel"]
    g["P1_full_recovers_truth"] = bool(
        np.isfinite(full["median_abs_rel"]) and
        full["median_abs_rel"] <= P["P1_full_recovers_truth"]["median_max"] and
        full["p95_abs_rel"] <= P["P1_full_recovers_truth"]["p95_max"])
    g["P1b_full_den_ratio_median"] = full["den_ratio_median"]
    g["P1b_full_denominator_matches"] = bool(
        P["P1b_full_denominator_matches"]["lo"] <= full["den_ratio_median"]
        <= P["P1b_full_denominator_matches"]["hi"])
    slow = aA["T_slow"]["by_domain"]["D_core"]
    g["P2_slow_median_ratio"] = slow["median"]
    g["P2_slow_pred_median"] = slow["pred_median"]
    g["P2_slow_pred_rel_diff"] = slow["pred_rel_diff"]
    g["P2_slow_biased_high"] = bool(
        slow["median"] >= P["P2_slow_biased_high"]["median_ratio_min"] and
        slow["pred_rel_diff"] <= P["P2_slow_biased_high"]["pred_rel_tol"])
    nv = aA["T_naive_sky"]["by_domain"]["D_core"]
    g["P3_naive_median_ratio"] = nv["median"]
    g["P3_naive_pred_median"] = nv["pred_median"]
    g["P3_naive_pred_rel_diff"] = nv["pred_rel_diff"]
    g["P3_naive_biased_low"] = bool(
        nv["median"] <= P["P3_naive_biased_low"]["median_ratio_max"] and
        nv["pred_rel_diff"] <= P["P3_naive_biased_low"]["pred_rel_tol"])
    tr = aA["T_traditional"]["by_domain"]["D_core"]
    g["P4_traditional_median_ratio"] = tr["median"]
    g["P4_traditional_biased_high"] = bool(
        tr["median"] >= P["P4_traditional_biased_high"]["median_ratio_min"])
    g["P5_src_over_sigma_median"] = aN["src_over_sigma"]["median"]
    g["P5_full_vs_slow_sigma_rel_max"] = aN["full_vs_slow_sigma_rel_max"]
    g["P5_null_zeroing"] = bool(
        aN["src_over_sigma"]["median"] <= P["P5_null_zeroing"]["src_over_sigma_max"] and
        aN["full_vs_slow_sigma_rel_max"] <= P["P5_null_zeroing"]["identity_tol"])
    g["P6_naive_over_full_median"] = aN["naive_over_full_median"]
    g["P6_naive_over_full_pred_median"] = aN["naive_over_full_pred_median"]
    g["P6_naive_over_full_pred_rel_diff"] = aN["naive_over_full_pred_rel_diff"]
    g["P6_null_naive_diverges"] = bool(
        aN["naive_over_full_pred_rel_diff"] <= P["P6_null_naive_diverges"]["pred_rel_tol"] and
        abs(aN["naive_over_full_median"] - 1.0) > P["P6_null_naive_diverges"]["min_dev"])
    g["P7_positive_frame_src_over_sigma_median"] = \
        R["A_analytic"]["src_recovery"]["median_src_over_sigma_D_core"]
    g["P7_null_gate_non_degenerate"] = bool(
        R["A_analytic"]["src_recovery"]["median_src_over_sigma_D_core"] >
        P["P7_null_gate_non_degenerate"]["src_over_sigma_min"])
    # 诊断量（不计入 all_pass）
    g["DIAG_null_noise_impl_lag1_rho"] = aN["noise_impl_lag1_rho"]
    g["DIAG_null_var_slow_relspread"] = aN["var_slow_true_relspread"]
    g["DIAG_null_snr_median"] = aN["snr_null"]["median"]
    g["DIAG_null_src_term_max_abs"] = aN["src_term_max_abs"]
    g["DIAG_analytic_src_term_captured"] = \
        R["A_analytic"]["src_recovery"]["src_term_captured_fraction"]
    g["DIAG_analytic_slow_recovery_median"] = R["A_analytic"]["slow_recovery"]["median"]
    if "A_analytic_sdet" in R:
        sA = R["A_analytic_sdet"]
        g["DIAG_sdet_completeness"] = sA.get("detection", {}).get("completeness")
        g["DIAG_sdet_num_ratio_D_core"] = \
            sA["arms"]["T_full"]["by_domain"]["D_core"]["num_ratio_median"]
        g["DIAG_sdet_den_ratio_D_core"] = \
            sA["arms"]["T_full"]["by_domain"]["D_core"]["den_ratio_median"]
        g["DIAG_sdet_median_abs_rel_D_core"] = \
            sA["arms"]["T_full"]["by_domain"]["D_core"]["median_abs_rel"]
        g["DIAG_sdet_src_term_captured"] = sA["src_recovery"]["src_term_captured_fraction"]
    g["DIAG_A_num_ratio_D_core"] = \
        R["A_analytic"]["arms"]["T_full"]["by_domain"]["D_core"]["num_ratio_median"]
    g["DIAG_A_slow_bias_pred_ratio"] = R["A_analytic"]["slow_bias"].get("pred_ratio_median")
    g["DIAG_A_slow_bias_measured_ctrl_ratio"] = \
        R["A_analytic"]["slow_bias"].get("measured_ctrl_ratio_median")
    g["DIAG_A_slow_bias_pred_rel_diff"] = R["A_analytic"]["slow_bias"].get("pred_rel_diff")
    gb = R["A_grad"]["slow_bias"]
    g["DIAG_grad_within_patch_sky_var"] = gb.get("within_patch_sky_var_median")
    g["DIAG_grad_slow_bias_pred_ratio"] = gb.get("pred_ratio_median")
    g["DIAG_grad_slow_bias_measured_ctrl_ratio"] = gb.get("measured_ctrl_ratio_median")
    g["DIAG_grad_slow_bias_pred_rel_diff"] = gb.get("pred_rel_diff")
    g["DIAG_grad_Tfull_median_abs_rel_D_core"] = \
        R["A_grad"]["arms"]["T_full"]["by_domain"]["D_core"]["median_abs_rel"]
    g["DIAG_grad_Tfull_den_ratio_D_core"] = \
        R["A_grad"]["arms"]["T_full"]["by_domain"]["D_core"]["den_ratio_median"]
    g["DIAG_grad_Tslow_median_ratio_D_core"] = \
        R["A_grad"]["arms"]["T_slow"]["by_domain"]["D_core"]["median"]
    g["DIAG_grad_Tnaive_median_ratio_D_core"] = \
        R["A_grad"]["arms"]["T_naive_sky"]["by_domain"]["D_core"]["median"]
    if "B_hst_m16" in R:
        b = R["B_hst_m16"]["arms"]["T_full"]["by_domain"]
        g["DIAG_B_Tfull_median_abs_rel_D_core"] = b["D_core"]["median_abs_rel"]
        g["DIAG_B_Tfull_median_abs_rel_D_point"] = b["D_point"]["median_abs_rel"]
        g["DIAG_B_Tfull_num_ratio_D_point"] = b["D_point"]["num_ratio_median"]
        g["DIAG_B_Tfull_den_ratio_D_point"] = b["D_point"]["den_ratio_median"]
        g["DIAG_B_Tfull_median_abs_rel_D_snr3"] = b["D_snr3"]["median_abs_rel"]
        g["DIAG_B_Tfull_median_abs_rel_D_all"] = b["D_all"]["median_abs_rel"]
        g["DIAG_B_src_term_captured"] = R["B_hst_m16"]["src_recovery"]["src_term_captured_fraction"]
        g["DIAG_B_Tslow_median_ratio_D_core"] = \
            R["B_hst_m16"]["arms"]["T_slow"]["by_domain"]["D_core"]["median"]
        g["DIAG_B_Tnaive_median_ratio_D_core"] = \
            R["B_hst_m16"]["arms"]["T_naive_sky"]["by_domain"]["D_core"]["median"]
        g["DIAG_B_spearman_note"] = "B 臂无 C2；见 C_m42_real"
    if "C_m42_real" in R:
        c1 = R["C_m42_real"]["C1_independent_sigma"]
        g["DIAG_C1_ratio_robust"] = c1.get("ratio_robust")
        g["DIAG_C1_ratio_mean"] = c1.get("ratio_mean")
        g["DIAG_C1_contamination_factor"] = c1.get("contamination_factor")
        g["DIAG_C2_spearman_rho"] = R["C_m42_real"]["C2_spearman_flux_vs_snr"].get("rho")
        g["DIAG_C3_src_free_median"] = \
            R["C_m42_real"]["C3_source_free_src_over_sigma"].get("median")
        g["DIAG_C4_lever_var"] = \
            R["C_m42_real"]["C4_gain_identifiability"].get("lever_arm", {}).get("lever_var")
        g["DIAG_C4_gain_hat"] = \
            R["C_m42_real"]["C4_gain_identifiability"].get("fit", {}).get("gain_hat")
        g["DIAG_C4_r2"] = R["C_m42_real"]["C4_gain_identifiability"].get("fit", {}).get("r2")
        g["DIAG_C_rej_n"] = R["C_m42_real"]["driver"].get("rej_n")
        g["DIAG_C_rej_high"] = R["C_m42_real"]["driver"].get("rej_high")
        g["DIAG_C_plan_nominal_n"] = R["C_m42_real"]["driver"].get("plan_nominal_n")
    return g


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=C.SEED_BASE)
    ap.add_argument("--out", default=os.path.join(RESULTS, "b7_absolute_snr_recon.json"))
    ap.add_argument("--quick", action="store_true", help="跳过 HST/M42 臂")
    ap.add_argument("--use-sdet", type=int, default=1)
    ap.add_argument("--slow-key", default="var_recon_bilinear",
                    choices=["var_recon_bilinear", "var_recon_bicubic", "var_plane"])
    a = ap.parse_args()
    t0 = time.time()
    seed = int(a.seed)
    R = {"experiment": "SCI-RECON-SNR-01 / B7 逐像素绝对 SNR 重建判别实验",
         "preregistration": PREREGISTRATION,
         "domains": {"D_all": "有效掩膜全域", "D_src": "S_src_true>0",
                     "D_core": "S_src_true/g >= %g*sigma_slow_true^2" % D_CORE_SRC_OVER_SLOW,
                     "D_snr3": "SNR_true >= %g" % D_SNR3_FLOOR},
         "frozen_config": dict(H=H, W=W, sigma_psf_px=SIGMA_PSF, profile_half=PROFILE_HALF,
                               pix_scale_deg=PIX_SCALE_DEG, node_spacing_deg=NODE_SPACING_DEG,
                               sky_box_px=SKY_BOX_PX, gain_e_per_adu=GAIN, rn_e=RN_E,
                               dark_e=DARK_E, sat_adu=SAT_ADU, sky_base=SKY_BASE,
                               sky_grad=SKY_GRAD, sky_blob=SKY_BLOB, n_src=N_SRC,
                               flux_range_adu=[F_LO, F_HI], margin_px=MARGIN,
                               quant_var_adu2=QUANT_VAR, seed_base=C.SEED_BASE,
                               seed_used=seed, use_sdet=int(a.use_sdet),
                               slow_face_key=a.slow_key),
         "production_path": {
             "driver": "实验/absolute-snr/code/b7_recon_driver.cpp",
             "build": "实验/absolute-snr/code/b7_build_driver.sh",
             "linked_production_sources": [
                 "lib/algorithms/noise_snr/cpp/src/noise_model.cpp",
                 "lib/algorithms/noise_snr/cpp/src/snr_science.cpp",
                 "lib/algorithms/noise_snr/cpp/src/information_weight.cpp",
                 "lib/algorithms/integration/v6/src/weight_chain.cpp",
                 "build/libastrocs_phase2.a (sky_plane.cpp / rejection.cpp)",
                 "build/libastrocs_p1_sdet.a (sdet_api.cpp)"],
             "not_reachable": []},
         "generated_at": C.now()}

    # 主臂 A_analytic：源位置与 PSF 尺度**给定**（等价于生产权威路径
    # 「星表引导检测 + PSF 块拟合」，star_detector.h:69-76），通量仍由生产 GLS 在帧上重估。
    # 这样把「检测/PSF 尺度」这一**非被测环节**的误差隔离掉，判据检验的是推导出的方差模型。
    scA = scene_analytic(seed + 0)
    outA = run_scene(scA, 0, a.slow_key)
    R["A_analytic"] = analyse_truth_scene(scA, outA, a.slow_key)
    R["A_analytic"]["slow_bias"] = analyse_slow_bias(scA, outA, a.slow_key)
    R["A_analytic"]["detection_mode"] = "given_catalog (use_sdet=0)"
    # 盲检测消融臂：全生产盲检测（sdet，诊断/初值路径）+ sdet 量得的 FWHM
    outAs = run_scene(scA, 1, a.slow_key)
    R["A_analytic_sdet"] = analyse_truth_scene(scA, outAs, a.slow_key)
    R["A_analytic_sdet"]["detection_mode"] = "sdet_blind (use_sdet=1)"
    scN = scene_analytic(seed + 100000, null=True)
    outN = run_scene(scN, 1, a.slow_key)
    R["A_null"] = analyse_null(scN, outN, a.slow_key)
    # 强天光梯度臂：慢变面分辨率偏差（patch 内天光变化 ≫ 噪声时）
    scG = scene_analytic(seed + 0, sky_kw=dict(grad=SKY_GRAD_STRONG, blob=SKY_BLOB_STRONG),
                         name="A_grad")
    outG = run_scene(scG, 0, a.slow_key)
    R["A_grad"] = analyse_truth_scene(scG, outG, a.slow_key)
    R["A_grad"]["slow_bias"] = analyse_slow_bias(scG, outG, a.slow_key)
    R["A_grad"]["config_note"] = ("强天光梯度（帧内峰峰 %g ADU / 团块 %g ADU）："
                                  "patch 内天光空间变化与噪声同量级 ⇒ 慢变面被系统性抬高"
                                  % (SKY_GRAD_STRONG, SKY_BLOB_STRONG))

    if not a.quick:
        scB = scene_hst(seed + 200000)
        outB = run_scene(scB, a.use_sdet, a.slow_key)
        R["B_hst_m16"] = analyse_truth_scene(scB, outB, a.slow_key)
        R["B_hst_m16"]["template"] = scB["template"]

        scC = scene_m42(seed + 300000)
        outC = run_scene(scC, a.use_sdet, a.slow_key)
        R["C_m42_real"] = analyse_m42(scC, outC, a.slow_key)

    g = evaluate_gates(R)
    R["gates"] = g
    R["all_pass"] = bool(all(v for k, v in g.items()
                             if isinstance(v, bool) and not k.startswith("DIAG_")))
    R["wall_s"] = time.time() - t0
    C.save_json(a.out, R)
    print("wrote", a.out)
    print(json.dumps(g, ensure_ascii=False, indent=1, default=str))
    return 0


if __name__ == "__main__":
    sys.exit(main())
