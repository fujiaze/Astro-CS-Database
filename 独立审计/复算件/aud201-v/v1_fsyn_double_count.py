# -*- coding: utf-8 -*-
"""
AUD201-V 独立复算 V1：由 XP 谱 + CCD 响应 + 滤镜透过率合成测光通量时，
星等因子进几次、lambda 因子为何在。

全部从辐射度量第一性重推，不 import 仓库代码；仓库里的曲线/数据只作输入。
"""
import json
import math
import os

import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__))
HC_NM = 1.986445857e-16  # J*nm

WL = np.arange(336.0, 1020.0 + 1e-9, 2.0)  # XPSD 采样网格 343 点


def planck_lam(T, wl):
    """谱辐出度 B_lambda [W m^-2 nm^-1 sr^-1]（只用于形状/相对刻度）。"""
    h, c, k = 6.62607015e-34, 2.99792458e8, 1.380649e-23
    wl_m = wl * 1e-9
    B = (2 * h * c**2 / wl_m**5) / (np.expm1(h * c / (wl_m * k * T)))
    return B * 1e-9  # per nm


def gauss_passband(center, fwhm, wl):
    s = fwhm / 2.3548
    t = np.exp(-0.5 * ((wl - center) / s) ** 2)
    return np.clip(t, 0.0, 1.0)


def qe_curve(wl):
    """一个宽带的典型 CCD QE 形状 [0,1]（与仓库实现无关，只为给 Q 一个非平凡形状）。"""
    q = gauss_passband(700.0, 350.0, wl)
    peak = q.max()
    return 0.9 * q / peak


T_BAND = gauss_passband(643.4, 110.0, WL)  # 光子加权有效波长 ~Baader R（文档值 643.4nm）
Q_BAND = qe_curve(WL)


def fsyn_canonical(F_lam, T, Q, wl):
    """生产口径 F_syn = int F_lam T Q lambda dlambda  [W m^-2 nm]"""
    y = F_lam * T * Q * wl
    return float(np.trapezoid(y, wl))


def photon_rate(F_lam, T, Q, wl):
    """独立地按光子计数第一性算：N_gamma = int (F_lam*lambda/(hc)) T Q dlambda [s^-1 m^-2]"""
    y = (F_lam * wl / HC_NM) * T * Q
    return float(np.trapezoid(y, wl))


def energy_flux(F_lam, T, Q, wl):
    """能量通量（无 lambda 权重）[W m^-2] —— 用于确认 lambda 不是多余的。"""
    y = F_lam * T * Q
    return float(np.trapezoid(y, wl))


# ---------------------------------------------------------------- A. lambda 因子
def check_lambda_factor():
    rng = np.random.default_rng(11)
    Tstar = float(rng.uniform(3500, 9000))
    shape = planck_lam(Tstar, WL)
    F_lam = shape * 10 ** (-0.4 * 13.7) * 1e-14
    fs = fsyn_canonical(F_lam, T_BAND, Q_BAND, WL)
    ng = photon_rate(F_lam, T_BAND, Q_BAND, WL)
    fe = energy_flux(F_lam, T_BAND, Q_BAND, WL)
    return {
        "T_star_K": Tstar,
        "F_syn_canonical": fs,
        "hc_times_photon_rate": HC_NM * ng,
        "rel_diff_vs_hcNgamma": abs(fs - HC_NM * ng) / fs,
        "energy_flux_no_lambda": fe,
        "ratio_fsyn_over_energy": fs / fe,
        "note": "F_syn == hc*N_gamma 到积分离散误差；能量通量（无 lambda）不等于它",
    }


# ------------------------------------------------- B. 星等因子进几次（绝对谱情形）
def check_mag_count():
    """同一温度、亮度差 1 mag：绝对 F_lambda 已含星等信息 —— int F_lam T Q lambda dlambda
    的比值恰为 10^-0.4，故显式再乘 10^-0.4G = 把亮度计两次。"""
    Tstar = 5500.0
    shape = planck_lam(Tstar, WL)
    out = {}
    for G in (12.0, 13.0):
        F_lam = shape * 10 ** (-0.4 * G) * 1e-14  # 绝对谱：亮度已在 F_lam 内
        out[f"Fsyn_G{G:g}"] = fsyn_canonical(F_lam, T_BAND, Q_BAND, WL)
    ratio = out["Fsyn_G13"] / out["Fsyn_G12"]
    out["ratio_G13_over_G12"] = ratio
    out["expected_10^-0.4"] = 10 ** -0.4
    out["implied_mag_from_Fsyn"] = [-2.5 * math.log10(v / out["Fsyn_G12"]) for v in
                                    (out["Fsyn_G12"], out["Fsyn_G13"])]
    return out


# -------------------- C. 复刻两条通道的差别，量化 r_i 的逐星注偏差（标量零点吸收不掉）
def xpsd_quantization(F_lam, rng):
    """模拟 XPSD/PCL 逐星量化：byte = round((F_lam - flux_min)/flux_mul)，
    flux_min 为逐星的（通常为负）下界，flux_mul 逐星不同。"""
    lo = float(np.min(F_lam))
    hi = float(np.max(F_lam))
    # PCL 风格：min 取数据下界之下、mul 由动态范围决定，逐星抖动
    flux_min = lo - rng.uniform(0.05, 0.60) * (hi - lo)
    flux_mul = (hi - flux_min) / 255.0 * rng.uniform(0.97, 1.0)
    byte = np.clip(np.rint((F_lam - flux_min) / flux_mul), 0, 255).astype(np.uint8)
    return byte, flux_min, flux_mul


def wrong_channel(byte, mag_g, T, Q, wl):
    """仓库非生产通道的语义：把 uint8 当相对谱形，再乘 10^(-0.4*G)。"""
    s = byte.astype(float) * 10 ** (-0.4 * mag_g)
    y = s * T * Q * wl
    return float(np.trapezoid(y, wl))


def mad_sigma(x, ddof=0):
    x = np.asarray(x, float)
    med = np.median(x)
    return 1.4826 * np.median(np.abs(x - med)) + (0.0 if ddof == 0 else 0.0)


def check_injected_error(n=4000):
    """构造一个自洽（无噪声）宇宙：F_instr 与生产 F_syn 严格成幂律，
    然后看错通道给 r_i = log10(F_instr/F_syn) 注入多少、能否被标量吸收。"""
    rng = np.random.default_rng(20260925)
    G = 13.0 + 2.6 * rng.random(n)          # G in [13, 15.6]
    Tstar = 3300.0 + 9000.0 * rng.random(n)
    zp_instr = 21.0
    r_ok, r_bad, r_bad_shape_only, g_list = [], [], [], []
    for i in range(n):
        shape = planck_lam(Tstar[i], WL)
        F_lam = shape * 10 ** (-0.4 * G[i]) * 1e-14
        fs_ok = fsyn_canonical(F_lam, T_BAND, Q_BAND, WL)
        byte, fmin, fmul = xpsd_quantization(F_lam, rng)
        fs_bad = wrong_channel(byte, G[i], T_BAND, Q_BAND, WL)
        # 完全无失真地只丢 G 因子（纯形状通道，若量化参数逐星恒定）
        fs_shape = fsyn_canonical(byte.astype(float), T_BAND, Q_BAND, WL) * 10 ** (-0.4 * G[i]) \
            if fmul is not None else np.nan
        F_instr = 10 ** ((zp_instr - (-2.5 * math.log10(fs_ok))) / 2.5)  # 完美仪器通量
        r_ok.append(math.log10(F_instr / fs_ok))
        r_bad.append(math.log10(F_instr / fs_bad))
        r_bad_shape_only.append(math.log10(F_instr / fs_shape))
        g_list.append(G[i])
    r_ok, r_bad, r_bad_shape_only, g_list = map(np.asarray, (r_ok, r_bad, r_bad_shape_only, g_list))

    def after_scalar_absorb(r):
        return r - np.median(r)

    ok = after_scalar_absorb(r_ok)
    bad = after_scalar_absorb(r_bad)
    bad_shape = after_scalar_absorb(r_bad_shape_only)
    corr = float(np.corrcoef(bad, 0.4 * g_list)[0, 1])
    return {
        "n": int(n),
        "sigma_dex_correct": float(mad_sigma(ok)),
        "sigma_dex_wrong_full": float(mad_sigma(bad)),
        "sigma_mag_wrong_full": float(2.5 * mad_sigma(bad)),
        "sigma_dex_wrong_shape_only": float(mad_sigma(bad_shape)),
        "injected_dex_p25": float(np.percentile(bad - ok, 25)),
        "injected_dex_median": float(np.median(bad - ok)),
        "injected_dex_p75": float(np.percentile(bad - ok, 75)),
        "corr_injected_vs_0p4G": corr,
        "slope_injected_vs_G_dex_per_mag": float(np.polyfit(g_list, bad - ok, 1)[0]),
        "MAD_sigma_G_dex_mag": float(mad_sigma(g_list)),
        "predicted_injection_dex": 0.4 * float(mad_sigma(g_list)),
    }


# --------------------------------- D. 文档声明的自洽性：0.459 dex == 1.147 mag ?
def check_doc_numbers():
    dex = 0.459
    mag = 1.147
    return {
        "dex_to_mag": 2.5 * dex,
        "stated_mag": mag,
        "consistent_within_0p001": abs(2.5 * dex - mag) < 1e-3,
        "implied_MAD_sigma_G_mag": dex / 0.4,          # 注入 = 0.4*G(dex)
        "note": "若注入项为 +0.4*G_i dex，则 MADsig(r_inj)=0.4*MADsig(G) dex"
                "，故 0.459 dex 反推样本 MADsig(G)=1.1475 mag",
    }


def check_two_quantization_regimes(n=4000):
    """判别实验：'+0.4*G_i 加性项'（=把亮度计两次）只在 **量化尺度与星无关** 时成立。
    regime GLOBAL : flux_mul 全体同一常数  => byte 载有亮度 => x10^-0.4G 计两次
    regime ADAPT  : flux_mul 逐星随亮度自适应（PCL/XPSD 语义：byte=(F-flux_min)/flux_mul）
                    => byte 近似与亮度无关 => x10^-0.4G 是"用星等替代丢失的逐星尺度"，
                       注入项的 G 依赖几乎完全抵消，残差是 **色项**。
    """
    out = {}
    # ---- PROPORTIONAL 档为解析式（无需模拟）：byte_i = F_lam,i / fmul（全局尺度、零下界）
    #      => I_i = fs_ok,i/fmul => 注入项 = 0.4*G_i + log10(fmul) —— 这才是"计两次"的签名
    rng0 = np.random.default_rng(779)
    G0 = 13.0 + 2.6 * rng0.random(n)
    inj0 = 0.4 * G0
    inj0 = inj0 - np.median(inj0)
    out["PROPORTIONAL"] = {
        "mode": "analytic (byte ∝ 绝对谱, 全局尺度, 零下界)",
        "MADsigma_injected_dex": float(mad_sigma(inj0)),
        "MADsigma_injected_mag": float(2.5 * mad_sigma(inj0)),
        "corr_injected_vs_G": 1.0,
        "slope_dex_per_mag_G": 0.4,
        "r2_vs_G": 1.0,
        "r2_vs_invT": 0.0,
    }
    for regime in ("GLOBAL", "ADAPTIVE"):
        rng = np.random.default_rng(777 if regime == "GLOBAL" else 778)
        G = 13.0 + 2.6 * rng.random(n)
        Tstar = 3300.0 + 9000.0 * rng.random(n)
        zp_instr = 21.0
        # 先算全部绝对谱（GLOBAL 定标要用全样本最大流）
        F_lams = np.array([planck_lam(T, WL) * 10 ** (-0.4 * g) * 1e-14
                           for T, g in zip(Tstar, G)])
        gmax = float(F_lams.max())
        ok, bad, garr, tarr = [], [], [], []
        fmul_global = gmax / 255.0            # 与星无关的公共量化尺度
        fmin_global = -0.10 * gmax
        for i in range(n):
            F_lam = F_lams[i]
            fs_ok = fsyn_canonical(F_lam, T_BAND, Q_BAND, WL)
            if regime == "ADAPTIVE":
                byte, fmin, fmul = xpsd_quantization(F_lam, rng)
            elif regime == "GLOBAL":          # 同一 fmul + 非零 fmin（公共下界）
                fmin, fmul = fmin_global, fmul_global
                byte = np.clip(np.rint((F_lam - fmin) / fmul), 0, 255).astype(np.uint8)
            else:   # PROPORTIONAL: byte 与绝对谱成正比（零下界 + 全局尺度）
                fmin, fmul = 0.0, fmul_global
                byte = np.clip(np.rint(F_lam / fmul), 0, 255).astype(np.uint8)
            fs_bad = wrong_channel(byte, G[i], T_BAND, Q_BAND, WL)
            ok.append(math.log10(fs_ok))
            bad.append(math.log10(fs_bad))
            garr.append(G[i])
            tarr.append(Tstar[i])
        ok, bad, garr, tarr = map(np.asarray, (ok, bad, garr, tarr))
        inj = (-bad) - (-ok)          # r_bad - r_ok = log10(fs_ok) - log10(fs_bad)
        inj = inj - np.median(inj)
        # 色轴代理：1/T（Saha 色指数的一阶量）
        color = 1.0e4 / tarr
        fit_g = np.polyfit(garr, inj, 1)
        fit_c = np.polyfit(color, inj, 1)
        out[regime] = {
            "MADsigma_injected_dex": float(mad_sigma(inj)),
            "MADsigma_injected_mag": float(2.5 * mad_sigma(inj)),
            "corr_injected_vs_G": float(np.corrcoef(inj, garr)[0, 1]),
            "slope_dex_per_mag_G": float(fit_g[0]),
            "corr_injected_vs_invT": float(np.corrcoef(inj, color)[0, 1]),
            "slope_dex_per_invT": float(fit_c[0]),
            "r2_vs_G": float(np.corrcoef(inj, garr)[0, 1] ** 2),
            "r2_vs_invT": float(np.corrcoef(inj, color)[0, 1] ** 2),
        }
    return out


def main():
    res = {
        "A_lambda_factor": check_lambda_factor(),
        "B_mag_count_absolute_spectrum": check_mag_count(),
        "C_injected_error": check_injected_error(),
        "D_doc_selfconsistency": check_doc_numbers(),
        "E_quantization_regime_discriminator": check_two_quantization_regimes(),
    }
    txt = json.dumps(res, indent=2, ensure_ascii=False)
    with open(os.path.join(OUT, "v1_fsyn_double_count.json"), "w", encoding="utf-8") as f:
        f.write(txt)
    print(txt)


if __name__ == "__main__":
    main()
