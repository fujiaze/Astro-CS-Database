# -*- coding: utf-8 -*-
"""SCI-A · 星表引导测光 + 逐帧标定拟合 + 误差预算双边界判据

判据形态逐字取自 docs/plugins/algorithms_phase1/06_photometry.md §4.1（定案 6）：

    A6-GATE(frame):  n = 本帧匹配星数（Gaia 匹配 ∧ 有效域 ∧ IRLS inlier）
      sigma_obs     = 2.5·MAD(r_inliers)/0.6744897501960817                 [mag]
      sigma_floor   = (1 - 3·1.166/sqrt(n))·sigma_fit(白; 本帧匹配星通量分布)
      sigma_ceiling = (1 + 3·1.166/sqrt(n))·sqrt(Σ 预算项²; 本帧)
      PASS ⟺ sigma_floor <= sigma_obs <= sigma_ceiling
"""
from __future__ import annotations

import numpy as np

from scia_common import (Budget, MAD_TO_SIGMA, PHOTON_MAG, fit_psf, gate_verdict,
                         irls_tukey, mad_sigma, match_catalogs, moffat_profile,
                         psf_flux_variance_theory, sampling_rho, world_to_pix)


# --------------------------------------------------------------------------
# 1. 星表引导检测 + PSF 测光
# --------------------------------------------------------------------------
def guided_photometry(img, wcs, ra, dec, inst, var_map=None, tol_px=3.0,
                      border=8, chi2_max=None, wcs_shift=None, mag_limit=None,
                      mag_g=None, pos_truth=None):
    """Gaia 逆映射定位 → 只对星表位置做 PSF 拟合；拟合失败直接丢弃（不计虚警）。

    wcs_shift   : 第二轮 WCS 精化后的像素平移 [dx, dy]（头部 WCS 与 Gaia 的系统偏移）
    mag_limit   : 星等上限（星表筛选；None = 全部）
    pos_truth   : 已知真值位置时直接以真值定位（解析合成星场用）
    """
    H, W = img.shape
    xp, yp = world_to_pix(wcs, ra, dec)
    if wcs_shift is not None:
        xp = xp + float(wcs_shift[0]); yp = yp + float(wcs_shift[1])
    if pos_truth is not None:
        xp = np.asarray(pos_truth[0], float); yp = np.asarray(pos_truth[1], float)
    if mag_limit is not None and mag_g is not None:
        keep = np.asarray(mag_g, float) <= float(mag_limit)
        xp = np.where(keep, xp, np.nan); yp = np.where(keep, yp, np.nan)
    ok = np.isfinite(xp) & np.isfinite(yp) & (xp >= border) & (xp < W - border) \
        & (yp >= border) & (yp < H - border)
    idx = np.nonzero(ok)[0]
    res = dict(idx=idx, x=np.full(idx.size, np.nan), y=np.full(idx.size, np.nan),
               flux=np.full(idx.size, np.nan), flux_err=np.full(idx.size, np.nan),
               chi2=np.full(idx.size, np.nan), fit_ok=np.zeros(idx.size, bool),
               x_pred=xp[idx], y_pred=yp[idx])
    for j, i in enumerate(idx):
        f = fit_psf(img, xp[i], yp[i], inst.fwhm_px, inst.beta_fit, var_map=var_map)
        res["x"][j] = f["x"]; res["y"][j] = f["y"]; res["flux"][j] = f["flux"]
        res["flux_err"][j] = f["flux_err"]; res["chi2"][j] = f["chi2"]
        res["fit_ok"][j] = bool(f["ok"] and np.isfinite(f["flux"]) and f["flux"] > 0)
    # 位移残差（引导定位质量）：拟合成功者与预测位置的距离
    d = np.hypot(res["x"] - res["x_pred"], res["y"] - res["y_pred"])
    res["shift_px"] = d
    res["shift_ok"] = res["fit_ok"] & (d <= tol_px)
    return res


def aperture_flux(img, x, y, r_ap=10.0, r_in=12.0, r_out=18.0):
    """独立孔径测光（诊断/交叉验证口径）：圆孔径 - 环带中位背景。"""
    H, W = img.shape
    out = np.full(len(x), np.nan)
    for k in range(len(x)):
        if not (np.isfinite(x[k]) and np.isfinite(y[k])):
            continue
        x0, y0 = int(round(x[k])), int(round(y[k]))
        R = int(np.ceil(r_out)) + 1
        if x0 - R < 0 or y0 - R < 0 or x0 + R >= W or y0 + R >= H:
            continue
        sub = img[y0 - R:y0 + R + 1, x0 - R:x0 + R + 1]
        yy, xx = np.mgrid[-R:R + 1, -R:R + 1]
        rr = np.hypot(xx, yy)
        ap = rr <= r_ap
        ann = (rr >= r_in) & (rr <= r_out)
        bkg = np.median(sub[ann])
        out[k] = float(np.sum(sub[ap] - bkg))
    return out


def psf_vs_aperture_systematics(f_psf, f_ap, f_err_psf=None):
    """06.md §2.1 的 sigma_psfsys 口径：同帧内 PSF 域 vs 独立孔径的比值稳健散度 [mag]。"""
    m = np.isfinite(f_psf) & np.isfinite(f_ap) & (f_psf > 0) & (f_ap > 0)
    if m.sum() < 5:
        return dict(sigma_psfsys=np.nan, n=int(m.sum()))
    ratio = f_psf[m] / f_ap[m]
    s = 2.5 * mad_sigma(np.log10(ratio)) / np.log10(np.e) * 0.0 + 2.5 * mad_sigma(np.log10(ratio))
    # 噪声预期（孔径与 PSF 域共有背景/平场项在比值中相消，只剩白噪声）
    noise = np.nan
    if f_err_psf is not None:
        e = f_err_psf[m]
        noise = float(2.5 * np.median(e / f_psf[m]) / np.log(10.0) * 0.0 +
                      2.5 / np.log(10.0) * np.median(e / f_psf[m]))
    return dict(sigma_psfsys=float(s), n=int(m.sum()), noise_expect_mag=noise,
                ratio_median=float(np.median(ratio)))


# --------------------------------------------------------------------------
# 2. 逐帧标定拟合
# --------------------------------------------------------------------------
def calibrate(f_instr, f_syn, mag_g=None, x=None, y=None, m_degree=0,
              mag_tolerance=3.0, valid=None):
    """逐帧独立标定：r_i = log10(F_instr/F_syn) → IRLS/Tukey location → k_photo=10^-loc。

    m_degree>0 时，在全局 k 之后用同一批星拟合低阶空间增益 m(x,y)（星等残差的二维多项式）。
    返回 dict（含 location/S/sigma_residual/k_photo/inliers/...）。
    """
    f_instr = np.asarray(f_instr, float); f_syn = np.asarray(f_syn, float)
    n0 = f_instr.size
    good = np.isfinite(f_instr) & np.isfinite(f_syn) & (f_instr > 0) & (f_syn > 0)
    if valid is not None:
        good &= np.asarray(valid, bool)
    ii = np.nonzero(good)[0]
    out = dict(n_input=int(n0), n_valid=int(ii.size), location=np.nan, S=np.nan,
               k_photo=np.nan, sigma_residual=np.nan, sigma_obs_mag=np.nan,
               n_inliers=0, outlier_rate=np.nan, m_degree=int(m_degree),
               m_coeffs=None, m_names=None, delta_after_m=np.nan, status="NO_DATA")
    if ii.size < 3:
        return out
    r = np.log10(f_instr[ii] / f_syn[ii])
    # 星等一致性预过滤（SCI-PHOT-001 §5）
    if mag_g is not None:
        delta = -2.5 * np.log10(f_instr[ii]) - np.asarray(mag_g, float)[ii]
        keep = np.abs(delta - np.median(delta)) <= mag_tolerance
        if keep.sum() >= 3:
            r = r[keep]; ii = ii[keep]
    res = irls_tukey(r)
    if not np.isfinite(res["location"]):
        return out
    loc = res["location"]
    out.update(location=float(loc), S=float(res["S"]),
               k_photo=float(10.0 ** (-loc)),
               sigma_residual=float(res["sigma_residual"]),
               sigma_obs_mag=float(2.5 * res["sigma_residual"]),
               n_inliers=int(res["n_in"]),
               outlier_rate=float(1.0 - res["n_in"] / max(r.size, 1)),
               resid_mag_inliers=(2.5 * (r[res["inliers"]] - loc)).tolist(),
               status="OK")
    # 低阶空间增益
    if m_degree > 0 and x is not None and y is not None:
        inl = res["inliers"]
        xs = np.asarray(x, float)[ii][inl]; ys = np.asarray(y, float)[ii][inl]
        dm = (-2.5 * np.log10(f_instr[ii][inl])) - (-2.5 * loc) - (-2.5 * np.log10(f_syn[ii][inl]))
        if xs.size >= (m_degree + 1) * (m_degree + 2) // 2 + 3:
            A, names = _poly_design(xs, ys, m_degree)
            coef, *_ = np.linalg.lstsq(A, dm, rcond=None)
            pred = A @ coef
            # dm/pred 都是**星等**残差（不是 dex）⇒ 不再乘 2.5（此前多乘了 2.5）
            # 语义：pred(x,y) ≈ -2.5·log10(m_gain(x,y))，故 **归一化修正场**
            #       m_corr(x,y) = 10^(+0.4·pred) = 1/m_gain
            out.update(m_coeffs=coef.tolist(), m_names=names,
                       delta_after_m=float(mad_sigma(dm - pred)),
                       sigma_obs_after_m_mag=float(2.5 * 1.0 / np.log(10.0) * 0.0
                                                   + mad_sigma(dm - pred)),
                       m_corr_definition="m_corr(x,y) = 10^(+0.4 * poly(x,y)) = 1/m_gain")
    return out


def _poly_design(xs, ys, degree):
    x0, x1 = float(np.min(xs)), float(np.max(xs))
    y0, y1 = float(np.min(ys)), float(np.max(ys))
    xn = (xs - 0.5 * (x0 + x1)) / max(0.5 * (x1 - x0), 1e-9)
    yn = (ys - 0.5 * (y0 + y1)) / max(0.5 * (y1 - y0), 1e-9)
    cols, names = [], []
    for i in range(degree + 1):
        for j in range(degree + 1 - i):
            cols.append((xn ** i) * (yn ** j)); names.append(f"x{i}y{j}")
    return np.asarray(cols).T, names


# --------------------------------------------------------------------------
# 3. 误差预算（逐项在本帧上算，真值已知时同时给"注入值"与"实测值"）
# --------------------------------------------------------------------------
def budget_from_frame(flux_adu, inst, sigma_pix_e, sky_adu_med=None, n_used_ap=201.0,
                      sigma_psfsys=np.nan, sigma_color=0.0, sigma_gaia=0.0,
                      sigma_flat=0.0, sigma_q=None, n_eff=None,
                      structure_factor=1.0, tag="budget"):
    """按 06.md §2.1 的九项合成预算（全部尺度无关，单位 mag）。

    flux_adu: 匹配样本的仪器通量 [ADU]（用中位通量代表本帧亮度分布）
    sigma_pix_e: 逐像素噪声 [e-]（白噪声口径 = 读出 ⊕ 天光散粒 ⊕ 暗流）
    """
    from scia_common import (_mean_psf_weighted, _sum_psf_sq, mc_sigma_obs,
                             noise_sigma_mag, psf_fit_variance_exact)
    flux_adu = np.asarray(flux_adu, float)
    F = float(np.median(flux_adu))
    F_e = F * inst.gain
    if n_eff is None:
        n_eff = 1.0 / _sum_psf_sq(inst.fwhm_px, inst.beta_fit)
    w_psf = _mean_psf_weighted(inst.fwhm_px, inst.beta_fit)

    # (1) 白噪声口径的逐星拟合不确定度（源泊松 + 天光散粒 + 读出 + 拟合自由度）
    #     用 (幅度,背景) 线性拟合的精确 Fisher 方差（见 psf_fit_variance_exact）
    sig_i_white = noise_sigma_mag(flux_adu, inst, sigma_pix_e)
    # (2) 含天光面结构（结构项只放大逐像素噪声，不改源泊松）
    sig_i_robust = noise_sigma_mag(flux_adu, inst,
                                   float(np.hypot(sigma_pix_e,
                                                  sigma_pix_e * (structure_factor - 1.0))))
    # (3) 天光扣除残差：δB = σ_pix·k_struct/√N_used，经 N_eff 折算到通量（逐星）
    dB = sigma_pix_e * structure_factor / np.sqrt(n_used_ap)
    sig_sky_i = PHOTON_MAG * (dB * np.sqrt(n_eff)) / np.maximum(flux_adu * inst.gain, 1e-30)
    # (4) 量化：V_q = q²/12 [ADU²]（逐星）
    if sigma_q is None:
        sig_q_i = PHOTON_MAG * (np.sqrt(1.0 / 12.0) * np.sqrt(n_eff)) / np.maximum(flux_adu, 1e-30)
        sig_q = float(np.median(sig_q_i))
    else:
        sig_q = float(sigma_q)

    sys2 = (float(sigma_psfsys) if np.isfinite(sigma_psfsys) else 0.0) ** 2 \
        + float(sigma_color) ** 2 + float(sigma_gaia) ** 2 + float(sigma_flat) ** 2 \
        + float(sig_q) ** 2
    sig_sys = float(np.sqrt(sys2))

    # 两种合成口径：
    #  (a) 06.md 字面形态：sigma_fit 取匹配样本**中位通量**处的值
    varF_white = float(psf_fit_variance_exact(np.array([F]), inst, sigma_pix_e)[0])
    sig_white_med = PHOTON_MAG * float(np.sqrt(varF_white)) / F
    sig_robust_med = PHOTON_MAG * float(np.sqrt(float(psf_fit_variance_exact(
        np.array([F]), inst, sigma_pix_e * float(structure_factor))[0]))) / F
    #  (b) 逐星合成（本实验用于宽通量跨度样本，Monte Carlo 到 2.5·MAD 统计量）
    sig_white_mc = mc_sigma_obs(sig_i_white, 0.0, tag=tag + ":white")
    # **只合成逐像素噪声**；系统项（psfsys/color/gaia/flat/q）由 Budget.sigma_ceiling
    # 在平方和中各计一次。此前把 sig_sys 也折进 sig_robust_mc 属二次计入（放宽上界）。
    sig_robust_mc = mc_sigma_obs(sig_i_robust, 0.0, tag=tag + ":robust")

    b = Budget(sigma_fit_white=float(sig_white_mc), sigma_fit_robust=float(sig_robust_mc),
               sigma_psfsys=float(sigma_psfsys) if np.isfinite(sigma_psfsys) else 0.0,
               sigma_color=float(sigma_color), sigma_gaia=float(sigma_gaia),
               sigma_flat=float(sigma_flat), sigma_skyres=float(np.median(sig_sky_i)),
               sigma_q=float(sig_q), n=0)
    b.items = dict(
        composition="MC over per-star sigma -> 2.5*MAD (flux-distribution correct)",
        sigma_fit_white_median_flux_form=float(sig_white_med),
        sigma_fit_robust_median_flux_form=float(sig_robust_med),
        sigma_fit_white_mc=float(sig_white_mc),
        sigma_fit_robust_mc=float(sig_robust_mc),
        sigma_sys_quadrature=float(sig_sys),
        n_eff=float(n_eff), w_psf=float(w_psf),
        flux_median_adu=float(F), flux_p10_adu=float(np.percentile(flux_adu, 10)),
        flux_p90_adu=float(np.percentile(flux_adu, 90)),
        sigma_pix_e=float(sigma_pix_e), structure_factor=float(structure_factor),
        sigma_skyres_median=float(np.median(sig_sky_i)),
        sigma_q_median=float(np.median(sig_q_i)) if sigma_q is None else float(sigma_q),
    )
    return b


def color_term_from_seds(sed_list, wl, T, T_wl, Q_a, Q_a_wl, Q_b, Q_b_wl, f_syn_a=None):
    """通带失配颜色项 sigma_color [mag]：同一批真实 XP 谱在两个 QE 曲线下的合成通量比散度。

    这就是 06.md §2.1 的 sigma_color 口径（生产取 Q≡1 时的下界），此处用"注入 QE vs
    拟合 QE"给出本仿真帧的真实颜色项。
    """
    from scia_common import f_syn
    if f_syn_a is None:
        fa = np.array([f_syn(s, wl, T, T_wl, Q_a, Q_a_wl) for s in sed_list])
    else:
        fa = np.asarray(f_syn_a, float)
    fb = np.array([f_syn(s, wl, T, T_wl, Q_b, Q_b_wl) for s in sed_list])
    good = (fa > 0) & (fb > 0)
    dm = -2.5 * np.log10(fa[good] / fb[good])
    return dict(sigma_color=float(np.std(dm)), mean_mag=float(np.mean(dm)),
                slope_note="两个 QE 曲线之间的通带失配等效星等偏移", n=int(good.sum()))
