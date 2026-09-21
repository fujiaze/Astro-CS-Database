#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A6 示范实验（M16-SCENE）：**固定真值通量、只改 seeing** —— PSF 域通量口径的孔径无关性。

判据来源（负责人裁决 A6，GAP_AUDIT §9.43 已确证缺陷）
----------------------------------------------------
* 规范侧（冻结、正确）：`docs/science/PHOTOMETRY.md:95`（SCI-PHOT-001 §9a）
  「星点通量来自 **PSF 拟合域**」；`docs/science/PSF.md:21/87`：`flux = 2*pi*A*sx*sy/3`（beta=4），单位 ADU。
* 实现侧（缺陷，已确证）：`star_detector.cpp:151` `s.flux = m00`（**5x5 正性截断盒和**），
  被 `module_adapters.cpp:3243` 喂给测光拟合 ⇒ 5x5 盒和捕获的 PSF 能量份额**依赖 seeing**。
* 判据（尺度无关）：**固定真值通量、只改 seeing，PSF 域口径的回收通量必须与 seeing 无关**
  （5x5 盒和**不**满足 —— 这正是缺陷的量化证据）。

与既有 F-INSTR 工作的关系（**不另起炉灶**）
----------------------------------------
估计器与 seeing 臂的构造沿用 `实验/SCI-B/code/reverse_verify/f_instr/`：
  * `f_instr_lib.render_psf / est_psf_optimal / est_box5 / aperture_curve / dmag`；
  * 但**底图换成真实哈勃 M16**（`m16_scene.py` 前向渲染接口），噪声为 §9.41 物理过程，
    不是 F-INSTR 的「真实 L4 帧 + 注入」；
  * 既有 `f_instr/exp3_seeing_null.py` 是**零假设对照**（真值无效应 ⇒ 度量归零）；
    本实验的**负例臂**与它同构，但用同一批 M16 合成帧实现。

实验设计
--------
真值（一条完整物理帧）:
    T  = 真实 M16 底（F657N 星云核心窗口 [5504,896,1024,1024]，期望率面）
       + 10 颗**显式位置、显式固定通量**的注入星（位置在暗区，避免星云结构污染）
    固定量：注入星真值通量 F_true [e-]、位置 (y,x)、曝光 9600 s、增益 1.5 e-/ADU、
            读出 3.1 e-、天光 = 底图 5% 分位（真实值 0.1673 e/s）、平场/暗流/量化/饱和 全开。

seeing 臂（每臂只改 PSF 的 FWHM，其余**逐位相同**，含 RNG 种子）:
    A: fwhm = 1.0 px   （HST 近衍射极限）
    B: fwhm = 2.0 px   （名义值；与注入星实际渲染用的核一致）
    C: fwhm = 3.0 px   （地面中等视宁度；模拟「同一场景换台站/换夜」）

测光（在**真正渲染出来的像素**上做，自动含天光/平场/结构泄漏）:
    PSF 域：`est_psf_optimal`（Naylor/Horne PSF 加权最优提取，即规范要求的「PSF 拟合域」口径）；
    盒和域：`est_box5`（生产实现口径 `star_detector.cpp:151` 的复刻，负例对照）；
    孔径域：`aperture_curve` 的多半径曲线（仅供诊断，不作判据）。

判据（写死，不事后放宽）
----------------------
  P1  **PSF 域臂间散度**：PSF 域回收通量的臂间偏差 `dmag`（相对 seeing=2.0 px 臂）
      的稳健散度 MAD <= **0.05 mag**（允许 PSF 核失配带来的小偏差；名义臂与真值核一致，
      散度主要来自噪声与核形状差异）。
  P2  **PSF 域 vs 盒和域的量级**：盒和域的臂间散度必须**显著大于** PSF 域，
      且盒和域 seeing 1.0 -> 3.0 px 的偏差 >= **0.3 mag**（PSF 能量外溢的量化）。
  P3  **负例（真值无效应）**：所有臂用**同一个 PSF 核**（fwhm=2.0 px，即真值核）时，
      PSF 域与盒和域的臂间散度**都必须归零**（< 1e-12 mag，逐位相同）。
      该臂同时证明「臂间差异确实来自 PSF 核，而不是噪声重抽」。

诚实边界
--------
* 本实验的 PSF 是**解析 Moffat4 核**（与生产 canon 同约定），不是 M16 真实 PSF；
  真实 HST PSF 有衍射环/像素响应，但**判据是口径无关性**（尺度无关），不依赖 PSF 形状；
* 底图是**去噪后的期望面**（真实结构保留、真实噪声剔除），因此本实验只检验
  「口径 × seeing」的系统项，不检验 drizzle 相关噪声的影响；
* 星云核心窗口的弥散结构会通过环带本底估计**部分泄漏**进 PSF 域口径（本实验已把注入星
  放在该窗口内最平坦的 5 个位置，并逐星登记本底散度）。

CLI
---
    export TMPDIR=/dev/shm/astrocs_m16
    python3 exp_a6_seeing_aperture.py            # 约 1-2 min
"""

from __future__ import annotations

import json
import math
import os
import sys
import time
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
RV = HERE.parents[1]                          # reverse_verify/
ROOT = HERE.parents[5]
sys.path.insert(0, str(ROOT / "实验" / "shared" / "synthetic"))
sys.path.insert(0, str(RV / "f_instr"))
import m16_scene as MS                        # noqa: E402
import f_instr_lib as FL                      # noqa: E402  估计器（复用，不重写）

OUT = ROOT / "run" / "reverse_verify" / "m16_scene" / "a6_seeing"
SEED = 20260924
SEEINGS = [1.0, 2.0, 3.0]
TRUE_FWHM = 2.0
N_STARS = 10


def pick_injection_sites(base_rate: np.ndarray, valid: np.ndarray, n: int,
                         rng: np.random.Generator, half: int = 24) -> list:
    """在底图**最平坦**的区域选 n 个互相分离的注入点（避免星云结构污染环带本底）。"""
    ny, nx = base_rate.shape
    step = 32
    cands = []
    for y in range(half, ny - half, step):
        for x in range(half, nx - half, step):
            if not valid[y - half:y + half, x - half:x + half].all():
                continue
            w = base_rate[y - half:y + half, x - half:x + half]
            cands.append((float(w.std() / max(abs(float(w.mean())), 1e-12)), float(w.mean()), y, x))
    cands.sort()                                   # 相对散度最小 = 最平坦
    sites = []
    for _, _, y, x in cands:
        if all((y - a) ** 2 + (x - b) ** 2 >= 200 ** 2 for a, b in sites):
            sites.append((y, x))
        if len(sites) >= n:
            break
    return sites


def build_truth(scene_path: Path, n_stars: int = N_STARS):
    """构造真值场景（显式位置 + 显式固定通量），返回 (scene, truth_sites, meta)。"""
    sc = MS.load_scene(scene_path)
    shape = tuple(int(v) for v in sc["shape"])
    base_rate, valid, rb = MS.load_real_base(sc["real_base"], shape)
    zp_ab = rb["photometric"]["zp_ab"]
    rng = np.random.default_rng(SEED)
    sites = pick_injection_sites(base_rate, valid, n_stars, rng)
    # 真值通量：AB 19.0-21.5 等 -> 电子数（曝光积分）。该范围在 F657N 上给出 SNR 数十到数百。
    mags = np.linspace(19.0, 21.5, len(sites))
    rates = MS.ab_mag_to_rate_e_per_s(mags, zp_ab)
    flux_e = rates * float(sc["exposure_s"])
    positions = [[float(y), float(x), float(f)] for (y, x), f in zip(sites, flux_e)]
    sc = dict(sc)
    sc["inject_stars"] = dict(sc["inject_stars"])
    sc["inject_stars"].update({"n": len(sites), "positions": positions,
                               "avoid_bright_base": False})
    meta = {"sites": sites, "true_flux_e": [float(v) for v in flux_e],
            "true_ab_mag": [float(v) for v in mags], "zp_ab": float(zp_ab),
            "base_flatness_std_over_mean": [
                float(base_rate[y - 24:y + 25, x - 24:x + 25].std()
                      / max(abs(float(base_rate[y - 24:y + 25, x - 24:x + 25].mean())), 1e-12))
                for (y, x) in sites]}
    return sc, meta


def main() -> int:
    t0 = time.time()
    OUT.mkdir(parents=True, exist_ok=True)
    scene_path = ROOT / "实验" / "shared" / "synthetic" / "scenes" / "m16_nebula_core.json"
    sc, tmeta = build_truth(scene_path)
    sites = tmeta["sites"]
    flux_true = np.array(tmeta["true_flux_e"])
    ny, nx = [int(v) for v in sc["shape"]]
    rep = {"experiment": "exp_a6_seeing_aperture", "scene": "m16_nebula_core",
           "criterion_source": "A6 (GAP_AUDIT 9.43): 规范 = PSF 拟合域口径; 实现 = 5x5 盒和",
           "design": {"fixed_truth_flux": True, "vary_only": "psf.fwhm_px",
                      "seeings_px": SEEINGS, "true_fwhm_px": TRUE_FWHM,
                      "n_stars": len(sites), "seed": SEED,
                      "exposure_s": float(sc["exposure_s"]),
                      "estimators": ["psf_optimal(f_instr_lib)", "box5(f_instr_lib)"]},
           "truth": tmeta, "arms": {}, "elapsed_s": 0.0}

    # ---- 渲染各臂：只改 PSF 核，其余逐位相同（含种子） ----
    kernels = {}
    for fw in SEEINGS:
        s2 = json.loads(json.dumps(sc))
        s2["psf"] = {"model": "moffat4", "fwhm_px": float(fw), "beta": 4.0}
        s2["scene_id"] = "m16_a6_fwhm%g" % fw
        frame, truth, valid = MS.render_m16_frame(s2, seed=SEED)
        adu = np.where(valid, frame.adu, 0.0)
        kernels[fw] = MS.R.psf_kernel(s2["psf"])[0]
        np.save(OUT / ("adu_fwhm%g.npy" % fw), adu.astype(np.float32))
        rep["arms"]["fwhm%g" % fw] = {
            "psf_fwhm_px": float(fw), "adu_median": float(np.median(adu[valid])),
            "adu_std_annulus": None, "per_star": []}
        print("[a6] rendered arm fwhm=%.1f px  adu_med=%.0f" % (fw, np.median(adu[valid])),
              flush=True)

    # ---- 测光：每个 seeing 臂 -> 用**该臂的核**做 PSF 域提取（自洽口径） ----
    for fw in SEEINGS:
        adu = np.load(OUT / ("adu_fwhm%g.npy" % fw)).astype(np.float64)
        ker = kernels[fw]
        ann_sigma = []
        for k, (y, x) in enumerate(sites):
            f_psf = FL.est_psf_optimal(adu, float(x), float(y), fw, beta=4.0, kind="moffat")
            f_box = FL.est_box5(adu, float(x), float(y))
            cut = adu[y - 20:y + 21, x - 20:x + 21]
            yy, xx = np.mgrid[0:cut.shape[0], 0:cut.shape[1]]
            rr = np.hypot(yy - 20, xx - 20)
            ann = (rr >= 10) & (rr <= 16)
            b, s = FL.robust_sky_stats(cut[ann])
            ann_sigma.append(float(s))
            rec = {"star_index": k, "y": float(y), "x": float(x),
                   "true_flux_e": float(flux_true[k]), "true_ab_mag": tmeta["true_ab_mag"][k],
                   "F_psf_adu": float(f_psf), "F_box5_adu": float(f_box),
                   "dmag_psf_vs_truth": float(FL.dmag(f_psf * 1.0, flux_true[k] / 1.5)),
                   "dmag_box5_vs_truth": float(FL.dmag(f_box * 1.0, flux_true[k] / 1.5)),
                   "annulus_sigma_adu": float(s)}
            rep["arms"]["fwhm%g" % fw]["per_star"].append(rec)
        rep["arms"]["fwhm%g" % fw]["adu_std_annulus"] = float(np.median(ann_sigma))
        print("[a6] measured arm fwhm=%.1f px  ann_sigma=%.1f ADU" % (fw, np.median(ann_sigma)),
              flush=True)

    # ---- 臂间散度（相对名义臂 2.0 px） ----
    ref = "fwhm%g" % TRUE_FWHM
    ref_psf = np.array([r["F_psf_adu"] for r in rep["arms"][ref]["per_star"]])
    ref_box = np.array([r["F_box5_adu"] for r in rep["arms"][ref]["per_star"]])
    summ = {}
    for fw in SEEINGS:
        key = "fwhm%g" % fw
        p = np.array([r["F_psf_adu"] for r in rep["arms"][key]["per_star"]])
        b = np.array([r["F_box5_adu"] for r in rep["arms"][key]["per_star"]])
        d_psf = FL.dmag(p, ref_psf)
        d_box = FL.dmag(b, ref_box)
        summ[key] = {"dmag_psf_vs_nominal_median": float(np.nanmedian(d_psf)),
                     "dmag_psf_vs_nominal_mad": float(1.4826 * np.nanmedian(np.abs(
                         d_psf - np.nanmedian(d_psf)))),
                     "dmag_box5_vs_nominal_median": float(np.nanmedian(d_box)),
                     "dmag_box5_vs_nominal_mad": float(1.4826 * np.nanmedian(np.abs(
                         d_box - np.nanmedian(d_box))))}
    rep["arm_divergence"] = summ
    box_span = abs(summ["fwhm1"]["dmag_box5_vs_nominal_median"]
                   - summ["fwhm3"]["dmag_box5_vs_nominal_median"])
    psf_span = abs(summ["fwhm1"]["dmag_psf_vs_nominal_median"]
                   - summ["fwhm3"]["dmag_psf_vs_nominal_median"])
    rep["span_1_to_3px_mag"] = {"psf_domain": float(psf_span), "box5_domain": float(box_span)}

    # ---- 判据 ----
    worst_psf_mad = max(summ[k]["dmag_psf_vs_nominal_mad"] for k in summ)
    worst_psf_med = max(abs(summ[k]["dmag_psf_vs_nominal_median"]) for k in summ)
    v = {}
    v["P1_psf_domain_seesing_independent"] = {
        "pass": bool(worst_psf_mad <= 0.05 and worst_psf_med <= 0.05),
        "detail": "PSF 域臂间 |中位 dmag| <= 0.05 且 MAD <= 0.05；实测 max|med|=%.5f max MAD=%.5f"
                  % (worst_psf_med, worst_psf_mad)}
    v["P2_box5_seesing_dependent"] = {
        "pass": bool(box_span >= 0.30 and box_span > 5.0 * max(psf_span, 1e-6)),
        "detail": "盒和域 1.0->3.0 px 偏差 %.4f mag 必须 >= 0.30 且 >= 5x PSF 域(%.4f)"
                  % (box_span, psf_span)}

    # ---- 解析对照：5x5 正性截断盒和捕获的 PSF 能量份额随 seeing ----
    fracs = {}
    for fw in SEEINGS:
        yy, xx = np.mgrid[-2:3, -2:3]
        p = FL.moffat_profile(xx.astype(float), yy.astype(float), float(fw), beta=4.0)
        fracs["fwhm%g" % fw] = float(p.sum())
    rep["analytic_box5_energy_fraction"] = fracs
    rep["analytic_box5_dmag_1_to_3px"] = float(
        -2.5 * math.log10(fracs["fwhm3"] / fracs["fwhm1"]))

    # ---- P3 负例（真值「无效应」⇒ 度量必须归零）----
    # P3a 同一核 + 同一种子：两臂必须**逐位相同**（dmag 恰为 0）
    s_same = json.loads(json.dumps(sc))
    s_same["psf"] = {"model": "moffat4", "fwhm_px": TRUE_FWHM, "beta": 4.0}
    f_a, _, va = MS.render_m16_frame(s_same, seed=SEED)
    f_b, _, vb = MS.render_m16_frame(s_same, seed=SEED)
    adu_a = np.where(va, f_a.adu, 0.0); adu_b = np.where(vb, f_b.adu, 0.0)
    bitwise = bool(np.array_equal(adu_a, adu_b))
    dm_a, dm_b = [], []
    for (y, x) in sites:
        dm_a.append(FL.est_psf_optimal(adu_a, float(x), float(y), TRUE_FWHM, beta=4.0))
        dm_b.append(FL.est_psf_optimal(adu_b, float(x), float(y), TRUE_FWHM, beta=4.0))
    d_same = FL.dmag(np.array(dm_a), np.array(dm_b))
    v["P3a_negative_control_identical"] = {
        "pass": bool(bitwise and np.all(d_same == 0.0)),
        "detail": "同核同种子两臂逐位相同=%s，逐星 dmag 全为 0=%s"
                  % (bitwise, bool(np.all(d_same == 0.0)))}
    # P3b 同一核 + 不同种子：真值无 seeing 效应 => 臂间**中位**偏差必须落在噪声底内
    f_c, _, vc = MS.render_m16_frame(s_same, seed=SEED + 977)
    adu_c = np.where(vc, f_c.adu, 0.0)
    dm_c = np.array([FL.est_psf_optimal(adu_c, float(x), float(y), TRUE_FWHM, beta=4.0)
                     for (y, x) in sites])
    d_noise = FL.dmag(dm_c, np.array(dm_a))
    med_noise = float(np.nanmedian(d_noise))
    mad_noise = float(1.4826 * np.nanmedian(np.abs(d_noise - med_noise)))
    v["P3b_negative_control_noise_only"] = {
        "pass": bool(abs(med_noise) <= 0.02 and mad_noise < box_span),
        "detail": "同核异种子（真值无 seeing 效应）：中位 dmag=%.5f（<=0.02），"
                  "MAD=%.5f（必须 < 盒和 seeing 跨度 %.4f）" % (med_noise, mad_noise, box_span)}
    rep["negative_control"] = {"bitwise_identical": bitwise,
                               "same_kernel_same_seed_dmag_max_abs": float(np.nanmax(np.abs(d_same))),
                               "same_kernel_diff_seed_median_dmag": med_noise,
                               "same_kernel_diff_seed_mad": mad_noise,
                               "noise_floor_adu_psf_domain": float(np.median(
                                   [r["annulus_sigma_adu"] for r in rep["arms"][ref]["per_star"]]))}
    rep["verdicts"] = v
    rep["elapsed_s"] = time.time() - t0
    with open(OUT / "result.json", "w", encoding="utf-8") as fh:
        json.dump(rep, fh, indent=1, ensure_ascii=False, default=float)
    print()
    for k, vv in v.items():
        print("  [%s] %s  %s" % ("PASS" if vv["pass"] else "FAIL", k, vv["detail"]))
    print("[a6] result -> %s  (%.1fs)" % (OUT / "result.json", rep["elapsed_s"]))
    return 0 if all(vv["pass"] for vv in v.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
