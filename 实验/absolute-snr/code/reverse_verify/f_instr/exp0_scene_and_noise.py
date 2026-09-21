#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F-INSTR exp0: 真实帧作底 + 噪声模型自校验 + 场景落盘.

底数据声明: 本仓无哈勃数据 (已核: 全仓无 HST 帧, 见报告 §3.0)。按负责人令的替代条款,
以 run/RELEASE-02/L4-rebuild/norm/*/ 的**真实标定帧**作底, 并在此显式声明。

产出: run/reverse_verify/f_instr/scene.npz + exp0_noise_validation.json
"""
import glob, os, sys, json
import numpy as np
from astropy.io import fits

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from f_instr_lib import (NoiseModel, smooth_sky_map, robust_sky_stats, render_psf,
                         moffat_fwhm_to_alpha, save_json, GAIN_DEFAULT, RN_DEFAULT)

ROOT = "/workspace/Astro CS Database"
OUT = os.path.join(ROOT, "run/reverse_verify/f_instr")
PATCH = 512


def find_clean_window(sky, size=PATCH):
    """在平滑天光图上找 size x size 最平最暗窗口 (避开 M42 核心星云)."""
    ny, nx = sky.shape
    best, bestv = None, 1e30
    for y in range(0, ny - size + 1, size // 2):
        for x in range(0, nx - size + 1, size // 2):
            w = sky[y:y + size, x:x + size]
            v = np.median(w) + 3.0 * (np.percentile(w, 90) - np.percentile(w, 10))
            if v < bestv:
                bestv, best = v, (y, x)
    return best


def empirical_psf(frame, half=14, nthr=2000.0, max_stars=400):
    """从真实帧叠出经验 PSF (孤立亮星中值叠加). 返回 (psf, fwhm_est, n_used)."""
    ny, nx = frame.shape
    med, sig = robust_sky_stats(frame)
    thr = med + nthr * 0.0 + 200.0
    # 局部极大
    from scipy.ndimage import maximum_filter
    mx = maximum_filter(frame, size=5)
    cand = np.argwhere((frame == mx) & (frame > med + 150.0))
    cand = [c for c in cand if half < c[0] < ny - half and half < c[1] < nx - half]
    rng = np.random.default_rng(0)
    rng.shuffle(cand)
    stamps = []
    for (y, x) in cand:
        if len(stamps) >= max_stars:
            break
        st = frame[y - half:y + half + 1, x - half:x + half + 1].astype(np.float64)
        st = st - med
        # 孤立性: 外环内无其他亮源
        rr = np.hypot(*np.mgrid[-half:half + 1, -half:half + 1])
        if np.any((st > 0.05 * st.max()) & (rr > 8.0)):
            continue
        if st.max() < 500:
            continue
        stamps.append(st / st.sum())
    if not stamps:
        return None, None, 0
    psf = np.median(np.array(stamps), axis=0)
    psf = psf / psf.sum()
    # FWHM: 二阶矩
    yy, xx = np.mgrid[-half:half + 1, -half:half + 1]
    m00 = psf.sum(); mx1 = (psf * xx).sum() / m00; my1 = (psf * yy).sum() / m00
    vx = (psf * (xx - mx1) ** 2).sum() / m00
    vy = (psf * (yy - my1) ** 2).sum() / m00
    fwhm = 2.3548 * np.sqrt(0.5 * (vx + vy))
    return psf, float(fwhm), len(stamps)


def main():
    os.makedirs(OUT, exist_ok=True)
    paths = sorted(glob.glob(os.path.join(ROOT, "run/RELEASE-02/L4-rebuild/norm/*/calibrated_*.fts")))
    rep = {"base_data": {
        "hubble_available": False,
        "hubble_note": "全仓检索未发现 HST/哈勃帧; 按负责人令替代条款用 L4 真实标定帧作底",
        "n_real_frames": len(paths),
        "frames": [os.path.relpath(p, ROOT) for p in paths],
    }, "noise_model": {}, "patches": []}

    # ---- 选帧: 取 FWHM 最小 (视宁度最好) 与 最大 (最差) 各一, 做视宁度跨度 ----
    meta = []
    for p in paths:
        with fits.open(p, memmap=True) as h:
            meta.append((p, float(h[0].header.get("FWHM", -1)), float(h[0].header.get("ZMAG", -99))))
    meta_ok = [m for m in meta if m[1] > 0]
    meta_ok.sort(key=lambda t: t[1])
    sel = [meta_ok[0], meta_ok[len(meta_ok) // 2], meta_ok[-1]]
    rep["selected_frames"] = [{"path": os.path.relpath(m[0], ROOT), "header_FWHM_px": m[1],
                               "header_ZMAG": m[2]} for m in sel]

    # ---- 用最好视宁度帧定位"干净窗口" (全帧平滑图) ----
    with fits.open(sel[0][0], memmap=True) as h:
        full = np.asarray(h[0].data, dtype=np.float64)
    skyfull = smooth_sky_map(full, box=128)
    y0, x0 = find_clean_window(skyfull, PATCH)
    del full, skyfull
    rep["clean_window"] = {"y0": int(y0), "x0": int(x0), "size": PATCH,
                           "chosen_on": os.path.relpath(sel[0][0], ROOT)}

    scene = {}
    for tag, (p, fh, zm) in zip(["good", "mid", "poor"], sel):
        with fits.open(p, memmap=True) as h:
            patch = np.asarray(h[0].data[y0:y0 + PATCH, x0:x0 + PATCH], dtype=np.float64)
        sky = smooth_sky_map(patch, box=32)
        med, sig = robust_sky_stats(patch)
        scene["patch_" + tag] = patch
        scene["sky_" + tag] = sky
        rep["patches"].append({"tag": tag, "frame": os.path.relpath(p, ROOT),
                               "header_FWHM_px": fh,
                               "measured_median_ADU": med, "measured_sigma_ADU": sig,
                               "sky_map_min": float(sky.min()), "sky_map_max": float(sky.max()),
                               "sky_map_p90_p10": float(np.percentile(sky, 90) - np.percentile(sky, 10))})

    # ---- 经验 PSF (用真实亮星) ----
    with fits.open(sel[0][0], memmap=True) as h:
        psf_img, psf_fwhm, npsf = empirical_psf(np.asarray(h[0].data, dtype=np.float64))
    rep["empirical_psf"] = {"n_stars": npsf, "fwhm_px": psf_fwhm,
                            "frame": os.path.relpath(sel[0][0], ROOT)}
    if psf_img is not None:
        scene["emp_psf"] = psf_img
        scene["emp_psf_fwhm"] = np.array([psf_fwhm])

    # ---- 噪声模型自校验: 渲染纯天光帧, 与真实帧比噪声 ----
    nm = NoiseModel()
    rep["noise_model"] = nm.as_dict()
    checks = []
    for tag in ["good", "mid", "poor"]:
        real = scene["patch_" + tag]
        sky = scene["sky_" + tag]
        rng = np.random.default_rng(12345)
        sim, sat = nm.render(sky, None, None, rng)
        # 只统计天光区 (掩掉真实源)
        m_med, m_sig = robust_sky_stats(real)
        s_med, s_sig = robust_sky_stats(sim)
        checks.append({"tag": tag,
                       "real_median": m_med, "real_sigma": m_sig,
                       "sim_median": s_med, "sim_sigma": s_sig,
                       "sigma_ratio_sim_over_real": s_sig / m_sig,
                       "sat_frac": float(sat.mean())})
        # 分块光子转移: 真实 vs 模拟
        pt = {"real": [], "sim": []}
        B = 32
        for i in range(PATCH // B):
            for j in range(PATCH // B):
                for key, img in (("real", real), ("sim", sim)):
                    b = img[i * B:(i + 1) * B, j * B:(j + 1) * B].ravel()
                    mm, ss = robust_sky_stats(b)
                    pt[key].append([float(mm), float(ss)])
        checks[-1]["photon_transfer_real"] = pt["real"]
        checks[-1]["photon_transfer_sim"] = pt["sim"]
    rep["noise_validation"] = checks

    np.savez_compressed(os.path.join(OUT, "scene.npz"), **scene)
    save_json(os.path.join(OUT, "exp0_noise_validation.json"), rep)
    print(json.dumps({k: rep[k] for k in ("base_data", "selected_frames", "clean_window",
                                          "empirical_psf", "noise_model")}, indent=2, ensure_ascii=False)[:3000])
    for c in checks:
        print("tag=%-5s real med=%8.2f sig=%7.3f | sim med=%8.2f sig=%7.3f | ratio=%.4f sat=%.4f"
              % (c["tag"], c["real_median"], c["real_sigma"], c["sim_median"], c["sim_sigma"],
                 c["sigma_ratio_sim_over_real"], c["sat_frac"]))
    print("saved:", os.path.join(OUT, "scene.npz"))


if __name__ == "__main__":
    main()
