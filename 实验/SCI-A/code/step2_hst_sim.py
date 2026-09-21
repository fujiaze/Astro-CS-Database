# -*- coding: utf-8 -*-
"""SCI-A 步骤 2 · HST 真实模板上的完整物理前向仿真（最高设计 §12.2 第 1 类）

模板：HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits（真实 HST 数据，
      BUNIT=ELECTRONS/S，带 PHOTFLAM/PHOTPLAM/PHOTBW 与 TAN WCS）
做法：24×24 块平均到 0.95"/px（与 testdata FLI/KAF-16803 系统同量级），
      模板只作**纯信号模板**（提供真实星云大尺度结构 = 天光空间结构），
      星点按 Gaia DR3SP 逆映射位置注入，SED 取真实 Gaia XP 谱，
      星等为已知注入值；随后走完整前向：
        源/天光/暗流 电子域 Poisson → 读出 Gaussian → 增益/饱和/量化 →
        平场乘性响应 m(x,y)（含逐像素散度）与天空梯度
固定 seed；两帧不同透明度（inject_scale 不同）。

产出：run/SCI-401/sim/frame_*.npz（运行产物）+ results/step2_hst_sim.json
"""
from __future__ import annotations

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import scia_common as sc
import scia_gaia as sg
import scia_sim as ss

HST = "HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits"
QE_MODEL = "KAF-16803"        # 流水线模型通带用的 QE（生产口径）
QE_INJECT = "GSENSE400BSI"    # 注入用"真实仪器" QE（制造非零颜色项，避免自证式 Oracle）
BIN = 24
SIMDIR = os.path.join(sc.RUN, "sim")


def load_template():
    from astropy.io import fits
    from scipy.ndimage import gaussian_filter, median_filter
    h = fits.open(os.path.join(sc.REPO, HST), memmap=True)
    d = np.asarray(h[0].data, dtype=np.float64)          # e-/s
    hdr = h[0].header.copy()
    h.close()
    H, W = d.shape
    Hb, Wb = H // BIN, W // BIN
    b = d[:Hb * BIN, :Wb * BIN].reshape(Hb, BIN, Wb, BIN).mean(axis=(1, 3))
    b = median_filter(b, size=7, mode="nearest")
    b = gaussian_filter(b, 2.0, mode="nearest")
    b = np.clip(b, 0, None)
    return b, hdr, (Hb, Wb)


def binned_wcs(hdr, shape):
    """块平均后的 WCS：CRPIX 缩放，CD 放大 BIN 倍（块中心约定）。"""
    cd = np.array([[hdr["CD1_1"], hdr["CD1_2"]], [hdr["CD2_1"], hdr["CD2_2"]]], float)
    crpix = np.array([hdr["CRPIX1"], hdr["CRPIX2"]], float)
    new_crpix = (crpix - 0.5) / BIN + 0.5
    return sc.make_wcs((hdr["CRVAL1"], hdr["CRVAL2"]), new_crpix, cd * BIN, shape=shape)


def build_sky(template, sky_adu_med, grad=(0.06, -0.04)):
    """把模板归一化成天光率图 [ADU/px]，并叠加线性天空梯度（真值已知）。"""
    t = template / max(np.median(template), 1e-30)
    t = np.clip(t, 0.0, 6.0)                     # 限幅，避免个别亮区主导
    t = 0.45 + 0.55 * t                          # 天光基座：避免低结构区被 16bit 下界裁切
    H, W = t.shape
    yy, xx = np.mgrid[0:H, 0:W]
    xn = (xx - (W - 1) / 2.0) / max(W - 1, 1) * 2.0
    yn = (yy - (H - 1) / 2.0) / max(H - 1, 1) * 2.0
    g = 1.0 + grad[0] * xn + grad[1] * yn
    sky = sky_adu_med * t * g
    return np.clip(sky, 0.0, None), dict(sky_adu_med=sky_adu_med, grad=grad,
                                         sky_median=float(np.median(sky)),
                                         sky_p10=float(np.percentile(sky, 10)),
                                         sky_p90=float(np.percentile(sky, 90)))


def make_frames():
    os.makedirs(SIMDIR, exist_ok=True)
    inst = ss.Instrument()
    tmpl, hdr, shape = load_template()
    wcs = binned_wcs(hdr, shape)
    cat = sg.XpCatalog(sg.dump_cone(274.7216, -13.8415, 0.075, 21.5, "m16"))
    xp, yp = sc.world_to_pix(wcs, cat.ra, cat.dec)
    H, W = shape
    inside = (xp > 12) & (xp < W - 12) & (yp > 12) & (yp < H - 12)
    # 注入星等：取 Gaia G，限制在 [13.0, 19.0]（避免整帧饱和主导，且与真实观测量级一致）
    mag_inj = cat.magG.copy()
    use = inside & (mag_inj >= 13.5) & (mag_inj <= 19.5)
    idx = np.nonzero(use)[0]
    print(f"[step2] template {shape}, stars in frame {int(inside.sum())}, injected {idx.size}")

    sky_map, sky_info = build_sky(tmpl, 199.6, grad=(0.06, -0.04))
    # inject_scale：让 mag 16 的星 ≈ 8e3 ADU（300 s，与 testdata 量级一致）
    Tw, Tv = sc.load_filter("Baader R")
    Qw, Qv = sc.load_qe(QE_MODEL)
    f_syn_ref = np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, Tv, Tw, Qv, Qw,
                                   cat.magG[i] - 16.0) for i in idx])
    inject_scale = 4.0e3 / float(np.median(f_syn_ref))
    f_syn = np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, Tv, Tw, Qv, Qw,
                               cat.magG[i] - 16.0) for i in idx]) * inject_scale

    r = sc.rng("hst:mcoef")
    mcoef = np.array([1.0, 0.035, -0.028, 0.012, -0.009, 0.007])
    out = dict(shape=list(shape), bin=BIN, inject_scale=inject_scale,
               n_injected=int(idx.size), sky=sky_info, m_coeffs=mcoef.tolist(),
               wcs=dict(crval=[hdr["CRVAL1"], hdr["CRVAL2"]], crpix=list(wcs.wcs.crpix),
                        cd=(wcs.wcs.cd * 1.0).tolist(), ctype=list(wcs.wcs.ctype)),
               instrument=dict(gain=inst.gain, read_noise=inst.read_noise,
                               dark_rate=inst.dark_rate, fwhm_px=inst.fwhm_px,
                               beta_fit=inst.beta_fit, beta_inject=inst.beta_inject,
                               ellipticity=inst.ellipticity, saturation_adu=inst.saturation_adu),
               frames=[])
    # 通带失配：注入用"真实仪器" QE（GSENSE400BSI），流水线模型用 KAF-16803
    # ⇒ 颜色项 sigma_color 非零（不是自证式 Oracle）
    Qi_w, Qi_v = sc.load_qe(QE_INJECT)
    out["qe_inject"] = QE_INJECT
    out["qe_model"] = QE_MODEL
    # 扩展星场（位置为解析合成、SED 取自真实 Gaia XP；用于收紧判据的统计量）
    rex = sc.rng("hst:extfield")
    n_ext = 500
    xe = rex.uniform(12, shape[1] - 12, n_ext)
    ye = rex.uniform(12, shape[0] - 12, n_ext)
    mag_e = 15.5 + 4.0 * rex.random(n_ext) ** 1.4   # 避免翼叠加把天光抬高/大量饱和
    src_e = rex.choice(np.nonzero(inside)[0], n_ext, replace=True)
    for tag, trans, use_ext in (("A", 1.0, False), ("B", 0.62, False), ("C", 1.0, True)):
        truth = ss.Truth(inject_scale=inject_scale * trans, m_coeffs=mcoef, m_degree=2,
                         sky_adu=199.6, exp_time=300.0, ref_err_mag=0.002,
                         flat_pix_sigma=0.0032, seed_tag=f"hst{tag}")
        if use_ext:
            px, py = xe, ye
            mag_gaia = mag_e
            src_ids = src_e
        else:
            px, py = xp[idx], yp[idx]
            mag_gaia = cat.magG[idx]
            src_ids = idx
        rr = sc.rng(f"hst{tag}:referr")
        mag_eff = mag_gaia + rr.normal(0.0, truth.ref_err_mag, mag_gaia.size)
        # 原始合成积分（**不含** inject_scale）：这才是流水线看到的 F_syn；
        # F_instr[ADU] = inject_scale · m(x,y) · f_syn_inject_raw
        f_inj_raw = np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, Tv, Tw, Qi_v, Qi_w,
                                       m - 16.0) for i, m in zip(src_ids, mag_eff)])
        f_syn_model = np.array([sc.f_syn(cat.spectrum(int(i)), cat.wl_nm, Tv, Tw, Qv, Qw,
                                         m - 16.0) for i, m in zip(src_ids, mag_eff)])
        f_eff = f_inj_raw * inject_scale * trans
        # f_eff 是**积分后**的仪器通量 [ADU]；前向模型要总电子数 ⇒ ×gain
        adu, info = ss.forward(truth, inst, shape, px, py, f_eff * inst.gain, sky_map,
                               tag=f"frame{tag}")
        ra_arr = cat.ra[src_ids] if not use_ext else np.full(px.size, np.nan)
        dec_arr = cat.dec[src_ids] if not use_ext else np.full(px.size, np.nan)
        np.savez_compressed(os.path.join(SIMDIR, f"frame_{tag}.npz"),
                            img=adu.astype(np.float32), m=info["m"].astype(np.float32),
                            sky_map_adu=sky_map.astype(np.float32),
                            src_e=info["src_e"].astype(np.float32),
                            mu=info["mu"].astype(np.float32),
                            x=px, y=py, ra=ra_arr, dec=dec_arr,
                            mag_gaia=mag_gaia, mag_eff=mag_eff, f_syn=f_syn_model,
                            f_syn_inject=f_inj_raw, xp_byte=cat.bytes[src_ids].astype(np.uint8),
                            flux_min=cat.flux_min[src_ids], flux_mul=cat.flux_mul[src_ids],
                            inject_scale=inject_scale * trans)
        # 用中位天光电平与实测逐像素噪声做基础统计
        d = np.diff(adu, axis=1)
        bg_rms = float(np.median(np.abs(d - np.median(d))) * sc.MAD_TO_SIGMA / np.sqrt(2.0))
        out["frames"].append(dict(
            tag=tag, transparency=trans, inject_scale=inject_scale * trans,
            extended_field=bool(use_ext), n_stars=int(px.size),
            img_median_adu=float(np.median(adu)), img_p01=float(np.percentile(adu, 1)),
            img_p99=float(np.percentile(adu, 99)), img_max=float(adu.max()),
            n_saturated=int((adu >= inst.saturation_adu).sum()),
            bg_rms_adu_adjacent_diff=bg_rms,
            f_syn_median=float(np.median(f_eff)), f_syn_model_median=float(np.median(f_syn_model)),
            mag_eff_median=float(np.median(mag_eff))))
        print(f"[step2] frame {tag}: median={np.median(adu):.1f} ADU, "
              f"bg_rms={bg_rms:.2f} ADU, saturated={int((adu >= inst.saturation_adu).sum())}")
    sc.jdump(out, os.path.join(sc.RESULTS, "step2_hst_sim.json"))
    return out


if __name__ == "__main__":
    make_frames()
