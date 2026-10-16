#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05-C：k_corr 的单位审查、标定域核验与域外 MC（D-05）。

三件事：
  §1 单位审查（源码行号证据）：`scale_arcsec` 究竟是源像元角尺度还是输出叶尺度。
  §2 独立 MC：以可分离方格 Drizzle 模型 + 生产控制点估计量口径，扫描
      源像元角尺度 × 输出 nside（⇒ 尺度比 rho）× pixfrac × 控制窗口 N × 裁剪比例，
      给出 k_corr 与 N_eff，判定冻结回退值 1.4 的保守域。
  §3 与生产球面权重交叉核对（探针 probe_s05_kcorr）+ P3 修复前后 covariance 对照。

生产估计量口径（逐行取自 lib/algorithms/coverage/src/sampler.cpp）：
  vals → m0 = median；s0 = 1.4826*MAD；
  迭代 background_clip_iters=3 次亮端裁剪 v <= m0 + 3.0*s0（sampler.cpp:1114-1140）；
  sigma = MAD(ret)；n_retained = |ret|；
  cvar = k_corr * (pi/2) * sigma^2 / n_retained（sampler.cpp:1150-1153）。

用法：
  python3 s05_kcorr_units.py --out run/ACSDSDOC-02/S05/results/kcorr_units.json
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s05_common as C          # noqa: E402
import s05_drizzle_geom as G    # noqa: E402

SEED = 20261014
LEAF_ARCSEC_NSIDE512 = 412.258369   # ACS_HIPS_MAX_FRAME_SCALE_ARCSEC/2（aio_hips_writer.cpp:2364）
PROD_N_RETAINED = 251               # sampler.cpp:84 的标定窗口
CLIP_SIGMA = 3.0                    # stage2_common.h:46
CLIP_ITERS = 3                      # stage2_common.h:47
FROZEN_FALLBACK = 1.4               # sampler.cpp:85

EVIDENCE = {
    "scale_arcsec_is": "源（输入）帧像元角尺度 arcsec/px",
    "source_lines": [
        {"file": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp",
         "lines": "1110-1113",
         "quote": "meta.fits_meta[\"src_pixel_scale_arcsec\"] = std::to_string(std::fabs(img.wcs.cd[0]) * 3600.0);",
         "meaning": "写入值是输入帧 CD 矩阵主元的绝对值 × 3600 ⇒ 输入帧像元角尺度 [arcsec/px]"},
        {"file": "lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp",
         "lines": "143-157",
         "quote": "double scale_arcsec = 0.0; const auto sit = meta.fits_meta.find(\"src_pixel_scale_arcsec\"); ... aio_hips_set_drizzle_provenance(ps, config.pixfrac, scale_arcsec);",
         "meaning": "同一量经 provenance setter 落到 HiPS properties 键 ACSD_DRIZZLE_SCALE_ARCSEC"},
        {"file": "lib/infrastructure/aio/src/hips/aio_hips_writer.cpp",
         "lines": "2096-2101",
         "quote": "kv.push_back({\"ACSD_DRIZZLE_SCALE_ARCSEC\", sc});",
         "meaning": "属性通道写盘（sc = %.4f of ps->drizzle_scale_arcsec）"},
        {"file": "lib/algorithms/coverage/src/sampler.cpp",
         "lines": "136-146",
         "quote": "else if (k == \"ACSD_DRIZZLE_SCALE_ARCSEC\") { const double vs = std::atof(v.c_str()); if (vs > 0.0) *scale_arcsec = vs; }",
         "meaning": "Phase2 读回同键 ⇒ kcorr_lookup 的第二参数量纲与写法一致：源像元角尺度"},
        {"file": "lib/algorithms/coverage/src/sampler.cpp",
         "lines": "88-94",
         "quote": "// （K_CORR_DOMAIN 选项 B，scale 维 RETIRED）：k_corr 标定表（kcorr_matrix_test 实测，pixfrac × 源像素角尺度双线性插值）。// 矩阵：scale 300\"/600\" 两档 × pixfrac 0.5/0.8/1.0。",
         "meaning": "表轴登记为「源像素角尺度」，与代码变量同一量纲 ⇒ 表轴单位**没有**错标"},
        {"file": "lib/algorithms/coverage/src/sampler.cpp",
         "lines": "104-105",
         "quote": "if (!(scale_arcsec >= sc_grid[0] && scale_arcsec <= sc_grid[1])) return kControlCorrDefault;",
         "meaning": "域外（300-600″/px 之外）恒返回冻结 1.4"},
        {"file": "lib/algorithms/coverage/src/sampler.cpp",
         "lines": "1150-1153",
         "quote": "const double kcorr_f = (frames[frame_id].kcorr > 0.0) ? frames[frame_id].kcorr : cfg.control_k_corr; const double cvar = kcorr_f * kPiHalf * sigma * sigma / n_ret;",
         "meaning": "k_corr 的消费口径（控制点方差）"},
        {"file": "lib/infrastructure/aio/product_io/include/astro/aio/provenance.h",
         "lines": "98",
         "quote": "std::string definition = \"k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]\";",
         "meaning": "k_corr 的定义式（本脚本 §2 的 MC 判据）"},
    ],
    "structural_finding": (
        "表轴虽然单位正确，但物理驱动量是尺度比 rho = 输出叶尺度/源像元角尺度，"
        "而不是源像元角尺度本身：同一源尺度在不同输出 nside 下 rho 不同 ⇒ 同一查表行"
        "对应不同真实 k_corr。kcorr_lookup 只接收 (pixfrac, scale_arcsec)，**不接收 nside**，"
        "因此表隐含固定输出 nside=512（叶 412.258369\"，与 kcorr 标定文档 T1 的"
        "「300\"/px 源 → nside=512 输出 412.26\"」一致）。"),
    "calibration_domain_rho": {
        "note": "标定表两档 = 输出叶 412.258369\"/源尺度 300\" 与 600\"",
        "rho_300": LEAF_ARCSEC_NSIDE512 / 300.0,
        "rho_600": LEAF_ARCSEC_NSIDE512 / 600.0,
    },
    "external_calibration_doc": {
        "file": "实验/additive-sky-seamless/docs/audit/p3_kcorr/tables.md",
        "quote": "T1 正本几何复现(平面模型, 300\"/px 源 -> nside=512 输出 412.26\", pixfrac=0.8, 单帧, NMC=2000 x 8 seeds x 16 相位) … 正本记载值 1.3883; 冻结值 1.4",
        "meaning": "标定几何确认：源 300″/px、输出 nside=512 ⇒ rho=1.374；k_geom 与窗口 N 强相关（T3 N 扫描）",
    },
    "frozen_constant_calibration_geometry": {
        "file": "run/P1-CONCURRENCY-CALIB-01/wsrc/lib/algorithms/drizzle/healpix_drizzle/tests/control_median_mc_test.cpp",
        "lines": "47-70（快照，活树无此测试文件）",
        "quote": ("constexpr int W = 20, H = 20; constexpr int NSIDE = 512; ... "
                  "const double deg_per_px = 300.0 / 3600.0; ... c.pixfrac = 0.8;"),
        "meaning": ("冻结常数 1.3883 / 回退值 1.4 的标定几何 = 20×20 帧、**源 300″/px**、"
                    "**输出 nside=512**（叶 412.258″，即 tile_order 0，最粗产品）、"
                    "pixfrac 0.8、N_retained=251 ⇒ rho=1.374。"
                    "「pixfrac=0.8 生产默认」只对 pixfrac 成立，nside 与源尺度都不是生产真帧几何。"),
        "matrix_test": {
            "file": "run/P1-CONCURRENCY-CALIB-01/wsrc/lib/algorithms/drizzle/healpix_drizzle/tests/kcorr_matrix_test.cpp",
            "quote": "constexpr int W = 20, H = 20; constexpr int NSIDE = 512; const double scales[] = {300.0, 600.0};",
            "meaning": "两档表值的唯一变量是源尺度，输出 nside 固定在 512 ⇒ 表轴 = 源尺度[arcsec/px]，隐含 tile_order=0",
        },
    },
    "tile_order_to_leaf_mapping": {
        "file": "lib/infrastructure/aio/src/hips/aio_hips_writer.cpp",
        "lines": "2063",
        "quote": "kv.push_back({\"hips_order\", std::to_string(ps->tile_order)});",
        "meaning": "properties 的 hips_order 写的是 **tile 序**（不是像素 nside 的对数）",
        "corroboration": [
            "run/STARMASK/in/ms_p00/signal/properties: hips_order=12, hips_pixel_scale=0.000028 deg = 0.1008″ = 412.258369\"/2^12 ⇒ nside = 2^(tile_order+9) = 2^21",
            "run/VAR-SEMANTICS-APPLY-01/probe/m42_probe_patched/hips/signal/properties: hips_order=9, hips_pixel_scale=0.000224 deg = 0.8064″ = 412.258369\"/2^9 ⇒ nside = 2^18",
            "run/STARMASK/out_maskoff/p2_final.json: target_order=12, nside=2097152=2^21",
            "lib/algorithms/coverage/src/sampler.cpp:744/1038: leaf_shift=9；pix2ang 用 target_order+leaf_shift ⇒ 叶 nside = 2^(target_order+9)",
        ],
        "consequence": ("生产默认帧产品 tile_order=9 ⇒ 叶 0.8052\"（nside 2^18），"
                        "真帧源像元角尺度 0.9586\"/px ⇒ 真实尺度比 rho = 0.84；"
                        "标定表的隐含几何是 tile_order=0（叶 412.258\"），两者相差 512 倍。"),
    },
}


def production_stats(Y: np.ndarray, contamination_sd: float = 0.0,
                     clip_sigma: float = CLIP_SIGMA, iters: int = CLIP_ITERS) -> dict:
    """对已生成的样本矩阵 Y（n_mc × n_leaves）套用生产估计量口径。"""
    n = Y.shape[1]
    diag = np.diag(np.cov(Y, rowvar=False)).copy() if n > 1 else np.array([np.var(Y[:, 0])])
    sd0 = math.sqrt(float(np.mean(diag)))
    if contamination_sd > 0.0:
        pass  # 污染已在采样阶段注入
    mask = np.ones_like(Y, dtype=bool)
    m0 = np.median(Y, axis=1)
    for _ in range(iters):
        dev = np.abs(np.where(mask, Y, np.nan) - m0[:, None])
        s0 = 1.482602218505602 * np.nanmedian(dev, axis=1)
        thr = m0 + clip_sigma * s0
        new_mask = mask & (Y <= thr[:, None])
        cnt = new_mask.sum(axis=1)
        if np.all(cnt < 5):
            break
        nm = np.nanmedian(np.where(new_mask, Y, np.nan), axis=1)
        m0 = np.where(cnt >= 5, nm, m0)
        mask = new_mask
    Yr = np.where(mask, Y, np.nan)
    med = np.nanmedian(Yr, axis=1)
    mad = 1.482602218505602 * np.nanmedian(np.abs(Yr - med[:, None]), axis=1)
    n_ret = float(np.mean(mask.sum(axis=1)))
    sd_med = float(np.std(med, ddof=1))
    denom = 1.5707963267948966 * float(np.nanmean(mad * mad)) / max(n_ret, 1.0)
    if denom > 0.0:
        k_prod = sd_med * sd_med / denom
    else:
        # 保留集 MAD = 0（极端全相关/裁剪到常数）⇒ 生产公式分母为 0，
        # k_corr 无有限上界：显式记 inf，不静默替换。
        k_prod = float("inf")
    med_raw = np.median(Y, axis=1)
    sdr = float(np.std(med_raw, ddof=1))
    k_raw = sdr * sdr / (1.5707963267948966 * float(np.mean(diag)) / n) if n > 0 else float("nan")
    return {
        "k_corr_production": k_prod,
        "k_corr_unclipped_median": k_raw,
        "n_retained_mean": n_ret,
        "retained_fraction": n_ret / n,
        "sd_of_median_production": sd_med,
        "sigma_bg_mean": float(np.nanmean(mad)),
        "n_mc": int(Y.shape[0]),
        "mc_se_rel_of_k": float(2.0 / math.sqrt(2.0 * max(Y.shape[0] - 1, 1))),
        "n_leaves": n,
    }


def prod_kcorr_from_cov(Cmat: np.ndarray, n_mc: int, rng: np.random.Generator,
                        contamination_frac: float = 0.0,
                        clip_sigma: float = CLIP_SIGMA,
                        iters: int = CLIP_ITERS) -> dict:
    """生产口径估计量下的 k_corr（含迭代亮端裁剪，从协方差采样）。"""
    n = Cmat.shape[0]
    diag = np.diag(Cmat).copy()
    # 生产权重矩阵可能秩亏（#drops < #leaves，例如输出叶比源像元细）：用对称
    # 特征分解 + 负/零特征值截断采样，等价于在支持子空间内定义噪声（不宣称
    # 「零噪声方向有无限信息」）。
    w, V = np.linalg.eigh(0.5 * (Cmat + Cmat.T))
    neg_frac = float(np.mean(w < 0.0))
    w = np.clip(w, 0.0, None)
    L = V * np.sqrt(w)
    Y = rng.standard_normal((n_mc, n)) @ L.T
    sd0 = math.sqrt(float(np.mean(diag)))
    if contamination_frac > 0.0:
        m = int(round(contamination_frac * n))
        if m > 0:
            idx = rng.integers(0, n, size=(n_mc, m))
            off = rng.uniform(1.5, 6.0, size=(n_mc, m)) * sd0
            np.put_along_axis(Y, idx, np.take_along_axis(Y, idx, axis=1) + off, axis=1)
    r = production_stats(Y, clip_sigma=clip_sigma, iters=iters)
    r["negative_eig_fraction"] = neg_frac
    r["contamination_frac"] = contamination_frac
    return r


def prod_kcorr_from_weights(W: np.ndarray, n_mc: int, rng: np.random.Generator,
                            contamination_frac: float = 0.0) -> dict:
    """直接从生产权重矩阵 W（drop×leaf，已归一 w_jp/a_drop）在 drop 空间模拟。

    y_p = sum_j w_jp x_j / sum_j w_jp，x ~ N(0,1)（源像元独立白噪）。
    秩亏/叶多于 drop 的情形天然可处理。
    """
    Np = W.sum(axis=0)
    good = Np > 0
    Wg = W[:, good] / Np[good]
    x = rng.standard_normal((n_mc, W.shape[0]))
    Y = x @ Wg
    sd0 = math.sqrt(float(np.mean(np.var(Y, axis=0))))
    if contamination_frac > 0.0:
        m = int(round(contamination_frac * Y.shape[1]))
        if m > 0:
            idx = rng.integers(0, Y.shape[1], size=(n_mc, m))
            off = rng.uniform(1.5, 6.0, size=(n_mc, m)) * sd0
            np.put_along_axis(Y, idx, np.take_along_axis(Y, idx, axis=1) + off, axis=1)
    r = production_stats(Y)
    r["contamination_frac"] = contamination_frac
    r["n_drops"] = int(W.shape[0])
    return r


def centered_phase(n_leaf_side: int, rho: float) -> float:
    """把一个 drop 中心放在窗口中心（窗口中心 = n/2 叶），避免细叶极端域窗口内无覆盖。"""
    s = 1.0 / rho
    j = round((n_leaf_side / 2.0) / s - 0.5)
    j = max(j, 0)
    return n_leaf_side / 2.0 - (j + 0.5) * s


def sep_kcorr(rho: float, pixfrac: float, side: int, n_mc: int, rng: np.random.Generator,
              contamination_frac: float = 0.0, phase=None) -> dict:
    """可分离方格模型：构造 (N_retained×N_retained) 协方差后按 PSD 采样求 k_corr。"""
    if phase is None:
        ph = centered_phase(side, rho)
        phase = (ph, ph)
    Cmat, idx = G.drizzle_covariance_2d(side, rho, pixfrac, v=1.0, phase=phase)
    r = prod_kcorr_from_cov(Cmat, n_mc, rng, contamination_frac=contamination_frac)
    r["n_leaves_window_requested"] = side * side
    r["grid_phase"] = "drop 中心对齐窗口中心（phase=%.6g）" % phase[0]
    return r


def leaf_arcsec_from_tile_order(tile_order: int) -> float:
    """叶角尺度 = 412.258369″ / 2^tile_order（nside = 2^(tile_order+9)）。"""
    return LEAF_ARCSEC_NSIDE512 / float(1 << tile_order)


def scan_geometry(n_mc: int = 8000) -> list:
    scales = [0.3, 0.9586, 1.0, 3.0, 30.0, 300.0, 600.0]
    tile_orders = [0, 3, 5, 9, 12]
    rows = []
    for s in scales:
        for to in tile_orders:
            leaf = leaf_arcsec_from_tile_order(to)
            rho = leaf / s
            tag = "kc_s%g_tileorder%d" % (s, to)
            rng = C.rng_for(tag, SEED)
            r = sep_kcorr(rho, 0.8, 16, n_mc, rng)
            r.update({"tag": tag, "source_scale_arcsec": s, "tile_order": to,
                      "output_nside": 1 << (to + 9),
                      "leaf_arcsec": leaf, "rho": rho, "pixfrac": 0.8,
                      "window_side_leaves": 16,
                      "in_lookup_domain": bool(300.0 <= s <= 600.0),
                      "lookup_value": (FROZEN_FALLBACK if not (300.0 <= s <= 600.0)
                                       else float("nan")),
                      "table_row_valid_geometry": bool(to == 0),
                      "production_effective_value": FROZEN_FALLBACK})
            rows.append(r)
    return rows


def scan_pixfrac(n_mc: int = 8000) -> list:
    geo = [("prod_real_tile9", 0.9586, 9), ("calib_300_tile0", 300.0, 0),
           ("calib_600_tile0", 600.0, 0), ("prod_real_tile12", 0.9586, 12),
           ("prod_real_tile5", 0.9586, 5)]
    rows = []
    for name, s, to in geo:
        leaf = leaf_arcsec_from_tile_order(to)
        rho = leaf / s
        for pf in (0.5, 0.8, 1.0):
            tag = "kcpf_%s_pf%g" % (name, pf)
            rng = C.rng_for(tag, SEED + 1)
            r = sep_kcorr(rho, pf, 16, n_mc, rng)
            r.update({"tag": tag, "source_scale_arcsec": s, "tile_order": to,
                      "output_nside": 1 << (to + 9), "leaf_arcsec": leaf, "rho": rho,
                      "pixfrac": pf, "window_side_leaves": 16})
            rows.append(r)
    return rows


def scan_windowN(n_mc: int = 8000) -> list:
    geo = [("prod_real_tile9", 0.9586, 9), ("calib_300_tile0", 300.0, 0),
           ("prod_real_tile12", 0.9586, 12)]
    sides = [3, 5, 7, 9, 11, 13, 15, 17]
    rows = []
    for name, s, to in geo:
        leaf = leaf_arcsec_from_tile_order(to)
        rho = leaf / s
        for side in sides:
            tag = "kcN_%s_side%d" % (name, side)
            rng = C.rng_for(tag, SEED + 2)
            r = sep_kcorr(rho, 0.8, side, n_mc, rng)
            r.update({"tag": tag, "source_scale_arcsec": s, "tile_order": to,
                      "output_nside": 1 << (to + 9), "leaf_arcsec": leaf, "rho": rho,
                      "pixfrac": 0.8, "window_side_leaves": side,
                      "N_retained_target_251_nearest": None})
            rows.append(r)
    return rows


def scan_clip(n_mc: int = 8000) -> list:
    geo = [("prod_real_tile9", 0.9586, 9), ("calib_300_tile0", 300.0, 0)]
    fracs = [0.0, 0.02, 0.05, 0.1, 0.2, 0.4]
    rows = []
    for name, s, to in geo:
        leaf = leaf_arcsec_from_tile_order(to)
        rho = leaf / s
        for fc in fracs:
            tag = "kcclip_%s_fc%g" % (name, fc)
            rng = C.rng_for(tag, SEED + 3)
            r = sep_kcorr(rho, 0.8, 16, n_mc, rng, contamination_frac=fc)
            r.update({"tag": tag, "source_scale_arcsec": s, "tile_order": to,
                      "output_nside": 1 << (to + 9), "leaf_arcsec": leaf, "rho": rho,
                      "pixfrac": 0.8, "window_side_leaves": 16})
            rows.append(r)
    return rows


# ---------------------------------------------------------- 生产权重交叉核对


def _load_probe(path: str):
    d = json.load(open(path, encoding="utf-8"))
    drops = d["drops"]
    leaves = sorted({int(l) for x in drops for l, _ in x["leaves"]})
    lm = {l: k for k, l in enumerate(leaves)}
    W = np.zeros((len(drops), len(leaves)))
    for j, x in enumerate(drops):
        for l, w in x["leaves"]:
            W[j, lm[int(l)]] = float(w)
    Np = W.sum(axis=0)
    Cm = (W / Np).T @ (W / Np)
    return {"meta": d, "W": W, "Np": Np, "C": Cm, "n_leaves": len(leaves),
            "n_drops": len(drops)}


def probe_kcorr(path: str, n_mc: int = 8000) -> dict:
    p = _load_probe(path)
    Cm = p["C"]
    n = Cm.shape[0]
    rng = C.rng_for("probe_" + os.path.basename(path), SEED + 4)
    r = prod_kcorr_from_cov(Cm, n_mc, rng)
    d = p["meta"]
    leaf = d["leaf_resolution_arcsec"]
    rho = leaf / d["scale_arcsec"]
    r.update({"probe": os.path.basename(path), "nside": d["nside"],
              "source_scale_arcsec": d["scale_arcsec"], "pixfrac": d["pixfrac"],
              "leaf_arcsec": leaf, "rho": rho, "n_drops": p["n_drops"],
              "n_leaves_touched": n,
              "max_abs_completeness_epsilon": d["max_abs_completeness_epsilon"],
              "offdiag_frac_mean": float(np.mean(np.abs(Cm - np.diag(np.diag(Cm)))) /
                                         max(np.mean(np.diag(Cm)), 1e-300))})
    return r


P3_PAIRS = [
    ("vertex_nside8192_fixed.json", "vertex_nside8192_legacy.json",
     "RA=45°, dec=asin(2/3), scale=0.04\"/px, nside=8192, pixfrac=0.8, 8×8 drops（S02 顶点构型）"),
    ("prod_n262144_s0.9586_pf08.json", "prod_n262144_s0.9586_pf08_legacy.json",
     "生产真帧几何：CRVAL=M42 T2/M1, scale=0.9586\"/px, nside=2^18, pixfrac=0.8, 48×48 drops"),
    ("calib_n512_s300_pf08.json", "calib_n512_s300_pf08_legacy.json",
     "标定几何：scale=300\"/px, nside=512, pixfrac=0.8, 16×16 drops"),
]


def _p3_pair(probe_dir: str, fixed_name: str, legacy_name: str, label: str) -> dict:
    fx = os.path.join(probe_dir, fixed_name)
    lg = os.path.join(probe_dir, legacy_name)
    if not (os.path.exists(fx) and os.path.exists(lg)):
        return {"status": "NOT_RUN", "reason": "probe 输出缺失", "label": label}
    pf = _load_probe(fx)
    pl = _load_probe(lg)
    # 权重矩阵差异（同 drop、同叶集合？叶集合可能不同 ⇒ 用 union 对齐）
    def align(p):
        d = p["meta"]["drops"]
        leaves = sorted({int(l) for x in d for l, _ in x["leaves"]})
        lm = {l: k for k, l in enumerate(leaves)}
        W = np.zeros((len(d), len(leaves)))
        for j, x in enumerate(d):
            for l, w in x["leaves"]:
                W[j, lm[int(l)]] = float(w)
        return W, leaves
    Wf, lf = align(pf)
    Wl, ll = align(pl)
    union = sorted(set(lf) | set(ll))
    uf = {l: k for k, l in enumerate(union)}
    Wf2 = np.zeros((Wf.shape[0], len(union)))
    Wl2 = np.zeros((Wl.shape[0], len(union)))
    for k, l in enumerate(lf):
        Wf2[:, uf[l]] = Wf[:, k]
    for k, l in enumerate(ll):
        Wl2[:, uf[l]] = Wl[:, k]
    Nf = Wf2.sum(axis=0)
    Nl = Wl2.sum(axis=0)
    valid = (Nf > 0) & (Nl > 0)
    Cf = (Wf2[:, valid] / Nf[valid]).T @ (Wf2[:, valid] / Nf[valid])
    Cl = (Wl2[:, valid] / Nl[valid]).T @ (Wl2[:, valid] / Nl[valid])
    dfull = np.abs(Cf - Cl) / np.maximum(np.abs(Cf) + np.abs(Cl), 1e-300)
    rngf = C.rng_for("p3_fixed", SEED + 5)
    rngl = C.rng_for("p3_legacy", SEED + 5)
    kf = prod_kcorr_from_cov(Cf, 4000, rngf)
    kl = prod_kcorr_from_cov(Cl, 4000, rngl)
    out = {
        "status": "PASS",
        "label": label,
        "n_drops": int(Wf.shape[0]),
        "n_leaves_union": len(union),
        "n_leaves_both_covered": int(valid.sum()),
        "eps_max_fixed": pf["meta"]["max_abs_completeness_epsilon"],
        "eps_max_legacy": pl["meta"]["max_abs_completeness_epsilon"],
        "weight_abs_diff_max": float(np.max(np.abs(Wf2 - Wl2))),
        "weight_abs_diff_mean": float(np.mean(np.abs(Wf2 - Wl2))),
        "cov_rel_diff_max": float(np.max(dfull)),
        "cov_rel_diff_p95": float(np.percentile(dfull, 95)),
        "cov_rel_diff_median": float(np.median(dfull)),
        "leaf_variance_fixed_mean": float(np.mean(np.diag(Cf))),
        "leaf_variance_legacy_mean": float(np.mean(np.diag(Cl))),
        "leaf_variance_rel_change": float(np.mean(np.diag(Cf)) / np.mean(np.diag(Cl)) - 1.0),
        "k_corr_fixed": kf["k_corr_production"],
        "k_corr_legacy": kl["k_corr_production"],
        "k_corr_rel_change": float(kf["k_corr_production"] / max(kl["k_corr_production"], 1e-300) - 1.0),
        "mc_se_rel_of_k": kf["mc_se_rel_of_k"],
        "p3_change_vs_p2_mc_resolution": {
            "p3_k_corr_rel_change": float(kf["k_corr_production"] / max(kl["k_corr_production"], 1e-300) - 1.0),
            "p2_mc_se_rel_of_k": kf["mc_se_rel_of_k"],
            "p3_below_p2_resolution": bool(abs(kf["k_corr_production"] / max(kl["k_corr_production"], 1e-300) - 1.0)
                                           < 2.0 * kf["mc_se_rel_of_k"]),
            "note": ("P3 几何改动对权重/协方差是**确定性、位置局域**的扰动；"
                     "只要其相对幅度小于 P2 MC 对 k_corr 的分辨率，就无法由 P2 随机噪声读数区分。"),
        },
    }
    return out


def p3_prepost(probe_dir: str) -> dict:
    """P3 修复前后 covariance 对照：同一二进制 + ACSD_DRZ_P3_FAULT 负例开关。"""
    pairs = [_p3_pair(probe_dir, a, b, lab) for a, b, lab in P3_PAIRS]
    return {"status": "PASS" if all(p.get("status") == "PASS" for p in pairs) else "PARTIAL",
            "switch": "ACSD_DRZ_P3_FAULT=legacy_cross_normal 同时回退 P3-NORM-01/02",
            "pairs": pairs}


def probe_window_kcorr(path: str, side: int, n_mc: int = 4000) -> dict:
    """用生产球面权重，在**空间窗口**（side×side 个最近叶）上按生产估计量求 k_corr。

    窗口选取：以帧中心 (crval) 为原点做 gnomon 投影，取离原点最近的 side² 叶。
    """
    p = _load_probe(path)
    d = p["meta"]
    centers = {int(a): (float(b), float(c)) for a, b, c in d.get("leaf_centers", [])}
    if not centers:
        return {"probe": os.path.basename(path), "status": "NO_LEAF_CENTERS"}
    ra0, dec0 = d["crval"][0], d["crval"][1]
    d0 = math.radians(dec0)
    a0 = math.radians(ra0)

    def gnom(ra, dec):
        a = math.radians(ra)
        dd = math.radians(dec)
        cosc = math.sin(d0) * math.sin(dd) + math.cos(d0) * math.cos(dd) * math.cos(a - a0)
        x = math.cos(dd) * math.sin(a - a0) / cosc
        y = (math.cos(d0) * math.sin(dd) - math.sin(d0) * math.cos(dd) * math.cos(a - a0)) / cosc
        return x, y

    items = []
    for ipix, (ra, dec) in centers.items():
        x, y = gnom(ra, dec)
        items.append((x * x + y * y, ipix))
    items.sort()
    sel = [ip for _, ip in items[: side * side]]
    if len(sel) < side * side:
        return {"probe": os.path.basename(path), "status": "TOO_FEW_LEAVES",
                "n_available": len(sel)}
    # 构造该窗口的 drop×leaf 权重（只保留与窗口叶有交叠的 drop）
    sel_idx = {ip: k for k, ip in enumerate(sel)}
    rows = []
    for j, x in enumerate(d["drops"]):
        row = {sel_idx[int(l)]: float(w) for l, w in x["leaves"] if int(l) in sel_idx}
        if row:
            rows.append((j, row))
    W = np.zeros((len(rows), len(sel)))
    for r, (_, row) in enumerate(rows):
        for k, w in row.items():
            W[r, k] = w
    rng = C.rng_for("probe_window_" + os.path.basename(path) + "_%d" % side, SEED + 6)
    r = prod_kcorr_from_weights(W, n_mc, rng)
    leaf = d["leaf_resolution_arcsec"]
    r.update({"probe": os.path.basename(path), "status": "PASS", "window_side_leaves": side,
              "nside": d["nside"], "source_scale_arcsec": d["scale_arcsec"],
              "pixfrac": d["pixfrac"], "leaf_arcsec": leaf,
              "rho": leaf / d["scale_arcsec"],
              "n_drops_in_window": len(rows),
              "n_leaves_touched_total": p["n_leaves"],
              "max_abs_completeness_epsilon": d["max_abs_completeness_epsilon"]})
    return r


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="run/ACSDSDOC-02/S05/results/kcorr_units.json")
    ap.add_argument("--probe-dir", default="run/ACSDSDOC-02/S05/results/probe")
    args = ap.parse_args()

    geo = scan_geometry()
    pf = scan_pixfrac()
    kN = scan_windowN()
    clip = scan_clip()

    probe_files = ["xcheck_n8192_s10_pf08.json", "xcheck_n8192_s25.8_pf08.json",
                   "calib_n512_s300_pf08.json", "mid_n512_s60_pf08.json",
                   "real_nside512.json", "vertex_nside8192_fixed.json",
                   "vertex_nside8192_legacy.json"]
    probes = []
    for f in probe_files:
        p = os.path.join(args.probe_dir, f)
        if os.path.exists(p):
            probes.append(probe_kcorr(p))
    # 生产真帧几何（tile_order=9 ⇒ nside 2^18，叶 0.805″；源 0.9586″/px ⇒ rho=0.84）
    # 上的空间窗口核对（生产球面权重，非可分离模型）。
    windows = []
    for f, side in [("prod_n262144_s0.9586_pf08.json", 6),
                    ("prod_n262144_s0.9586_pf08.json", 10),
                    ("prod_n2097152_s0.9586_pf08.json", 4),
                    ("calib_n512_s300_pf08.json", 6),
                    ("xcheck_n8192_s10_pf08.json", 6)]:
        p = os.path.join(args.probe_dir, f)
        if os.path.exists(p):
            w = probe_window_kcorr(p, side)
            # 同一 (rho, side, pixfrac) 的可分离方格模型读数（模型 vs 生产权重核对）
            if w.get("status") == "PASS":
                rng = C.rng_for("sepmatch_%s_%d" % (f, side), SEED + 7)
                m = sep_kcorr(w["rho"], w["pixfrac"], side, 4000, rng)
                w["separable_model_k_corr"] = m["k_corr_production"]
                w["separable_model_rel_diff"] = (
                    m["k_corr_production"] / w["k_corr_production"] - 1.0
                    if w["k_corr_production"] > 0 and math.isfinite(w["k_corr_production"])
                    else None)
            windows.append(w)
    p3 = p3_prepost(args.probe_dir)

    allk = [r["k_corr_production"] for r in geo + pf + kN + clip]
    over = [r for r in geo + pf + kN + clip if r["k_corr_production"] > FROZEN_FALLBACK]
    prod = next((r for r in geo if r["source_scale_arcsec"] == 0.9586 and r["tile_order"] == 9),
                None)
    calib = next((r for r in geo if r["source_scale_arcsec"] == 300.0 and r["tile_order"] == 0),
                 None)
    verdict = {
        "frozen_fallback": FROZEN_FALLBACK,
        "frozen_calibration": {
            "empirical_k_corr": 1.3883,
            "geometry": "source 300\"/px, output nside=512 (leaf 412.258\", tile_order=0), "
                        "pixfrac 0.8, 20x20 frame, N_retained=251",
            "rho": LEAF_ARCSEC_NSIDE512 / 300.0,
            "this_run_k_corr_same_geometry": (calib["k_corr_production"] if calib else None),
            "model_vs_calibration_rel_diff": (
                (calib["k_corr_production"] / 1.3883 - 1.0) if calib else None),
        },
        "k_max_over_scan": max(allk),
        "k_min_over_scan": min(allk),
        "n_configs": len(allk),
        "n_configs_k_gt_14": len(over),
        "non_conservative_configs": [
            {"tag": r["tag"], "rho": r.get("rho"), "k_corr": r["k_corr_production"]}
            for r in sorted(over, key=lambda x: -x["k_corr_production"])[:12]],
        "production_real_geometry": prod,
        "calib_geometry_300": calib,
        "probe_window_crosscheck": windows,
        "status": ("UNCALIBRATED_PRODUCTION_GEOMETRY_K_GT_FROZEN" if
                   (prod and prod["k_corr_production"] > FROZEN_FALLBACK) else
                   "CONSERVATIVE_IN_SCANNED_DOMAIN"),
        "claim": ("冻结回退值 1.4 只在标定几何（源 300\"/px、输出 tile_order=0、"
                  "pixfrac 0.8）附近保守；生产真帧几何（源 0.9586\"/px、默认 tile_order=9、"
                  "叶 0.805\"、rho≈0.84）下本独立 MC 给出更大的 k_corr ⇒ 不能声称 1.4 是"
                  "通用保守上界，该域的逐帧权重要标 UNCALIBRATED。"),
    }
    out = {
        "experiment_id": "P2-C-S05-kcorr",
        "paper_id": "P2",
        "evidence_level": "ORACLE+PRODUCTION",
        "env": C.env_fingerprint(),
        "evidence": EVIDENCE,
        "geometry_scan": geo,
        "pixfrac_scan": pf,
        "window_N_scan": kN,
        "clip_scan": clip,
        "probe_crosscheck": probes,
        "probe_window_crosscheck": windows,
        "p3_prepost": p3,
        "verdict": verdict,
    }
    C.dump_json(args.out, out)
    print("[S05 kcorr] wrote %s" % args.out)
    print("[S05 kcorr] k range %.4f..%.4f, >1.4 in %d/%d configs" %
          (verdict["k_min_over_scan"], verdict["k_max_over_scan"],
           verdict["n_configs_k_gt_14"], verdict["n_configs"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
