# -*- coding: utf-8 -*-
"""S04 STEP-4 · σ_flat 的独立测量（不由 σ_obs 反推）

三个互相独立的证据面
--------------------
(A) **平场母版空间结构**（仪器标定产品，与帧残差无关）
    - 逐像素高通散度 `s_flat_pix`：母版除以平滑模型后的相对散度；
    - 折到单星：(A1) `scia_common.sigma_flat_independent`（N_eff 解析口径）；
      (A2) **直接口径**——把母版按 PSF 权重在**真实星位置**上做孔径平均，取逐星
      相对散度的稳健 σ。这一口径不需要 N_eff 近似，且用的就是本实验的星点几何。
    - 同时给「平滑分量幅度」（低阶空间场 `m(x,y)` 能吸收的部分）与「去二阶后残余」
      （= 预算项「平场高通」应指的量）。
(B) **重复位置检验**（帧间经验口径）
    - 同一颗星在不同帧落在不同像素位置；配对残差差 `Δd` 的散度给出**重复性上界**
      （含透明度、平场、PSF、色差、背景）；
    - 把 `Δd` 对**母版平场在两位置的差** `Δflat` 回归：斜率 ≈1 表示帧间差主要由
      平场结构解释；≈0 表示平场不是主项。**这是独立于 σ_obs 的检验**（Δd 是配对差，
      不是被检统计量本身）。
(C) **σ_ceiling 对 σ_flat 假设的灵敏度曲线**（正问题，不是反推）
    - σ_ceiling(σ_flat) 曲线、翻转阈值 σ_flat*、以及"若把 σ_flat 取为独立测量值，
      各帧判定是否变化"。

明确写出的不可测量项
--------------------
母版平场**施加之后**的残余平场误差（需要 repeat-flat / sky-flat / 两台独立同型相机
的对照平场）在本仓库数据下 **NOT_MEASURED**。本脚本给的是"母版自身结构"与
"帧间经验重复性"，两者的物理含义各不相同，逐项标注。

XISF 读取：本仓库无 Python XISF 读取器（`lib/infrastructure/aio/src/aio_xisf.cpp` 为
C++ 且需构建），故本脚本内置**极简单块 XISF 读取器**：解析 8 字节签名 + uint32 头长 +
XML 头，取 `<Image location="attachment:OFFSET[:LENGTH]" geometry="W:H:C" sampleFormat>`，
按 Float32/小端读取。只支持未压缩单块附件；遇其它布局显式报错，不猜。

输出：results/sigma_flat.json、results/sigma_flat_sensitivity.csv、
      results/repeat_position.csv、figures/fig4..fig6
"""
from __future__ import annotations

import csv
import json
import os
import re
import struct
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402

PERSTAR = os.path.join(C.RESULTS, "perstar")
FLATS = {
    "T2_Red": dict(path="testdata/T2 calibration files/masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf",
                   instrument="Telescope2", filter="Red", shape=(4096, 4096)),
    "T3_Red": dict(path="testdata/T3 calibration files/masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf",
                   instrument="Telescope3", filter="Red", shape=(4096, 4096)),
    "T2_Green": dict(path="testdata/T2 calibration files/masterFlat_BIN-1_4096x4096_FILTER-Green_mono.xisf",
                     instrument="Telescope2", filter="Green", shape=(4096, 4096)),
    "T3_Green": dict(path="testdata/T3 calibration files/masterFlat_BIN-1_4096x4096_FILTER-Green_mono.xisf",
                     instrument="Telescope3", filter="Green", shape=(4096, 4096)),
}
FWHM_MODEL, BETA_MODEL = 1.98, 4.0
SIGMAS = (0.0, 0.0002, 0.0005, 0.0007, 0.001, 0.0015, 0.002, 0.003, 0.005,
          0.007, 0.010, 0.013, 0.015, 0.020, 0.030)


# --------------------------------------------------------------------------
# 极简 XISF 读取器
# --------------------------------------------------------------------------
SAMPLE_DTYPE = {"Float32": "<f4", "Float64": "<f8", "UInt16": "<u2", "UInt32": "<u4",
                "Int16": "<i2", "Int32": "<i4", "Byte": "u1", "UInt8": "u1"}


def read_xisf_monolithic(path):
    """读未压缩单块 XISF 图像 → (ndarray, meta)。仅支持 attachment 直存。"""
    with open(path, "rb") as f:
        sig = f.read(8)
        if sig != b"XISF0100":
            raise ValueError(f"不是 XISF 1.0 文件: {sig!r}")
        (hlen,) = struct.unpack("<I", f.read(4))
        xml = f.read(hlen).decode("utf-8", "replace")
        f.seek(0, os.SEEK_END)
        fsize = f.tell()
        m = re.search(r"<Image\b[^>]*>", xml)
        if not m:
            raise ValueError("XISF 无 <Image> 元素")
        tag = m.group(0)
        loc = re.search(r'location="attachment:(\d+)(?::(\d+))?"', tag)
        if not loc:
            raise ValueError("XISF Image 非 attachment 直存（可能是内联/压缩块），"
                             "本读取器显式拒绝，不猜")
        off = int(loc.group(1))
        geom = re.search(r'geometry="(\d+):(\d+):(\d+)"', tag)
        sf = re.search(r'sampleFormat="(\w+)"', tag)
        if not (geom and sf):
            raise ValueError("XISF Image 缺 geometry/sampleFormat")
        w, h, ch = (int(x) for x in geom.groups())
        if ch != 1:
            raise ValueError(f"本读取器只支持单通道（得到 {ch}）")
        dtype = SAMPLE_DTYPE.get(sf.group(1))
        if dtype is None:
            raise ValueError(f"未知 sampleFormat {sf.group(1)!r}")
        n = w * h
        nbytes = np.dtype(dtype).itemsize * n
        f.seek(off)
        raw = f.read(nbytes)
        if len(raw) != nbytes:
            raise ValueError("XISF 数据块长度不足")
        arr = np.frombuffer(raw, dtype=dtype).reshape(h, w).astype(np.float64)
    kw = {km.group(1): km.group(2)
          for km in re.finditer(r'<FITSKeyword name="([^"]+)" value="([^"]*)"', xml)}
    meta = dict(path=os.path.relpath(path, C.REPO), sha256=C.sha256_file(path),
                header_len=int(hlen), data_offset=int(off), file_bytes=int(fsize),
                geometry=[w, h, ch], sample_format=sf.group(1),
                dtype_used=dtype, raw_min=float(arr.min()), raw_max=float(arr.max()),
                raw_median=float(np.median(arr)),
                fits_keywords={k: kw.get(k) for k in
                               ("IMAGETYP", "FILTER", "EXPTIME", "DATE-OBS", "DATE-END",
                                "OBSERVER", "INSTRUME", "TELESCOP", "FOCALLEN", "RA",
                                "DEC", "OBJECT")})
    return arr, meta


# --------------------------------------------------------------------------
# (A) 平场母版空间结构
# --------------------------------------------------------------------------
def flat_hf_structure(flat, smooth_win=9):
    """逐像素高通散度：母版 / 中值平滑模型 - 1 的稳健散度（含 MAD 与 std）。"""
    from scipy.ndimage import median_filter
    sm = median_filter(flat, size=smooth_win, mode="nearest")
    r = flat / sm - 1.0
    med = float(np.median(r))
    mad = float(np.median(np.abs(r - med)) * C.sc.MAD_TO_SIGMA)
    return dict(median=med, mad_sigma=mad, std=float(np.std(r)),
                p16=float(np.percentile(r, 16)), p84=float(np.percentile(r, 84)),
                smooth_window_px=int(smooth_win))


def poly_design(xn, yn, degree=2):
    cols = []
    for i in range(degree + 1):
        for j in range(degree + 1 - i):
            cols.append((xn ** i) * (yn ** j))
    return np.asarray(cols).T


def flat_low_order_residual(flat, degree=2, stride=4):
    """低阶多项式（归一化像面坐标）拟合母版后的相对残差散度。"""
    h, w = flat.shape
    ys, xs = np.mgrid[0:h:stride, 0:w:stride]
    v = flat[::stride, ::stride]
    xn = (xs.ravel() - w / 2.0) / (w / 2.0)
    yn = (ys.ravel() - h / 2.0) / (h / 2.0)
    A = poly_design(xn, yn, degree)
    coef, *_ = np.linalg.lstsq(A, v.ravel(), rcond=None)
    pred = A @ coef
    rel = v.ravel() / pred - 1.0
    smooth_rel = pred / np.median(pred) - 1.0
    return dict(degree=int(degree), stride_px=int(stride),
                smooth_component_p16p84=[float(np.percentile(smooth_rel, 16)),
                                         float(np.percentile(smooth_rel, 84))],
                smooth_component_std=float(np.std(smooth_rel)),
                residual_after_loworder_mad_sigma=float(
                    np.median(np.abs(rel - np.median(rel))) * C.sc.MAD_TO_SIGMA),
                residual_after_loworder_std=float(np.std(rel)),
                n_samples=int(v.size))


def psf_weights(fwhm=FWHM_MODEL, beta=BETA_MODEL, half=None):
    if half is None:
        half = int(np.ceil(3.0 * fwhm))
    g = np.arange(-half, half + 1, dtype=float)
    dx, dy = np.meshgrid(g, g)          # 索引 [gy, gx]
    return half, dx, dy, C.sc.moffat_profile(dx, dy, fwhm, beta)


def flat_at_positions(flat, x, y, half, dx, dy, P):
    """把母版按 PSF 权重在真实星位置上做孔径平均 → 逐星有效平场因子。"""
    h, w = flat.shape
    vals = np.full(len(x), np.nan)
    for k in range(len(x)):
        if not (np.isfinite(x[k]) and np.isfinite(y[k])):
            continue
        ix, iy = int(round(x[k])), int(round(y[k]))
        if ix - half < 0 or iy - half < 0 or ix + half >= w or iy + half >= h:
            continue
        stamp = flat[iy - half:iy + half + 1, ix - half:ix + half + 1]
        # PSF 权重按**像素中心相对星心的偏移**取值（非网格整数偏移）
        wgt = C.sc.moffat_profile((ix + dx) - x[k], (iy + dy) - y[k],
                                  FWHM_MODEL, BETA_MODEL)
        vals[k] = float(np.sum(wgt * stamp) / np.sum(wgt))
    return vals


def effective_pixel_weights(x, y, half, dx, dy):
    """同样本的有效像素数 N_eff = (ΣP)²/ΣP²（用于把逐像素散度折到单星）。"""
    gx = np.round(x).astype(int); gy = np.round(y).astype(int)
    ne = np.full(len(x), np.nan)
    for k in range(len(x)):
        if not (np.isfinite(x[k]) and np.isfinite(y[k])):
            continue
        wgt = C.sc.moffat_profile((gx[k] + dx) - x[k], (gy[k] + dy) - y[k],
                                  FWHM_MODEL, BETA_MODEL)
        ne[k] = float(np.sum(wgt) ** 2 / np.sum(wgt * wgt))
    return ne


# --------------------------------------------------------------------------
# (B) 重复位置检验
# --------------------------------------------------------------------------
STAR_ID = ["star_index", "ra", "dec"]


def load_perstar(var="head"):
    frames = []
    for fn in sorted(os.listdir(PERSTAR)):
        if not fn.endswith(f"__{var}.npz") or fn.endswith("__cand.npz"):
            continue
        fid = fn.replace(f"__{var}.npz", "")
        z = np.load(os.path.join(PERSTAR, fn))
        d = {k: z[k] for k in z.files}
        d["frame_id"] = fid
        d["observer"] = "Telescope2" if "_T2_" in fid else "Telescope3"
        frames.append(d)
    return frames


def repeat_position_pairs(frames, flat_map, half, dx, dy, P):
    """逐星跨帧配对：Δd（去帧零点后残差差）与 Δflat（母版在该星两位置的 PSF 平均差）。"""
    per_star = {}
    for d in frames:
        m = d["inlier"]
        med = float(np.median(d["resid_mag"][m]))
        for i in np.nonzero(m)[0]:
            per_star.setdefault(int(d["star_index"][i]), []).append(
                dict(frame=d["frame_id"], observer=d["observer"],
                     resid=d["resid_mag"][i] - med, x=d["x"][i], y=d["y"][i],
                     flux=d["flux_adu"][i], mag=d["mag_g"][i],
                     col=d["col_RminusB"][i]))
    # 逐帧平场通量：母版 PSF 平均（按仪器选母版）
    flatf = {}
    for d in frames:
        key = ("T2_Red" if d["observer"] == "Telescope2" else "T3_Red")
        flatf[d["frame_id"]] = (flat_map[key],
                                flat_at_positions(flat_map[key], d["x"], d["y"],
                                                  half, dx, dy, P))
    rows = []
    for sid, obs in per_star.items():
        if len(obs) < 2:
            continue
        for a in range(len(obs)):
            for b in range(a + 1, len(obs)):
                oa, ob = obs[a], obs[b]
                if oa["frame"] == ob["frame"]:
                    continue
                # 位置与平场
                sep_px = float(np.hypot(oa["x"] - ob["x"], oa["y"] - ob["y"]))
                fa = flatf[oa["frame"]][1]
                fb = flatf[ob["frame"]][1]
                # 用星索引在该帧数组中的位置取平场值：直接按坐标重算更稳
                fa_v = float(flat_at_positions(flatf[oa["frame"]][0],
                                               np.array([oa["x"]]), np.array([oa["y"]]),
                                               half, dx, dy, P)[0])
                fb_v = float(flat_at_positions(flatf[ob["frame"]][0],
                                               np.array([ob["x"]]), np.array([ob["y"]]),
                                               half, dx, dy, P)[0])
                if not (np.isfinite(fa_v) and np.isfinite(fb_v)
                        and fa_v > 0 and fb_v > 0):
                    continue
                dflat_mag = -2.5 * np.log10(fa_v / fb_v)
                rows.append(dict(star=sid, frame_a=oa["frame"], frame_b=ob["frame"],
                                 obs_a=oa["observer"], obs_b=ob["observer"],
                                 same_instrument=int(oa["observer"] == ob["observer"]),
                                 sep_px=sep_px, d_resid_mag=float(oa["resid"] - ob["resid"]),
                                 d_flat_mag=float(dflat_mag),
                                 mag_a=oa["mag"], col_a=oa["col"]))
    return rows, per_star


def main():
    t0 = time.time()
    inv = json.load(open(os.path.join(C.RESULTS, "frame_inventory.json"), encoding="utf-8"))
    rc = json.load(open(os.path.join(C.RESULTS, "repro_comparison.json"), encoding="utf-8"))
    frames = load_perstar("head")
    out = dict(step="S04-4 sigma_flat independent measurement", base_sha=inv["base_sha"],
               fwhm_model_px=FWHM_MODEL, beta_model=BETA_MODEL,
               canonical_sigma_flat_hf=float(C.SIGMA_FLAT_HF_CANONICAL),
               canonical_source=("实验/photometric-magnitude/code/scia_calib.py:31 "
                                 "SIGMA_FLAT_HF_CANONICAL；docs/ 全树 grep '0.0007' 零命中"
                                 "（本轮独立复核）⇒ 仓内未验证预算假设"))

    # ---------------- (A) 平场母版 ----------------
    flat_map = {}
    A = {}
    for key, spec in FLATS.items():
        p = os.path.join(C.REPO, spec["path"])
        if not os.path.exists(p):
            A[key] = dict(status="MISSING", path=spec["path"])
            continue
        arr, meta = read_xisf_monolithic(p)
        flat_map[key] = arr / float(np.median(arr))
        hf = flat_hf_structure(arr)
        lo = flat_low_order_residual(arr, degree=2, stride=4)
        A[key] = dict(status="READ", instrument=spec["instrument"], filter=spec["filter"],
                      reader=meta, hf=hf, low_order=lo)
    out["A_master_flats"] = A

    # ---------------- (A2) 逐像素散度折到单星 + 真实星位置直接口径 ----------------
    half, dx, dy, P = psf_weights()
    out["A2_psf_footprint"] = dict(
        half_px=int(half), n_pixels=int(P.size),
        sum_P=float(np.sum(P)), sum_P2=float(np.sum(P * P)),
        N_eff_analytic=float(1.0 / np.sum(P * P)),
        N_eff_analytic_sigma_flat_independent=float(
            C.sc.sigma_flat_independent(1.0, FWHM_MODEL, BETA_MODEL)),
        note=("`sigma_flat_independent(s_flat)=PHOTON_MAG·s_flat/√N_eff`，故"
              "`N_eff_analytic_sigma_flat_independent` 即 1 mag 对应的 s_flat 折算因数"))

    def per_star_flat_stats(flat_norm, d):
        vals = flat_at_positions(flat_norm, d["x"], d["y"], half, dx, dy, P)
        m = d["inlier"] & np.isfinite(vals)
        if m.sum() < 10:
            return None
        v = vals[m]
        rel = v / float(np.median(v)) - 1.0
        # 去二阶空间场后的残余
        x = d["x"][m]; y = d["y"][m]
        w, h = flat_norm.shape[1], flat_norm.shape[0]
        xn = (x - w / 2.0) / (w / 2.0); yn = (y - h / 2.0) / (h / 2.0)
        Ad = poly_design(xn, yn, 2)
        coef, *_ = np.linalg.lstsq(Ad, rel, rcond=None)
        rel_after = rel - Ad @ coef
        return dict(
            n=int(m.sum()), n_clusters=int(np.unique(d["star_index"][m]).size),
            flat_eff_median=float(np.median(v)),
            rel_std=float(np.std(rel)),
            rel_mad_sigma=float(np.median(np.abs(rel - np.median(rel)))
                                * C.sc.MAD_TO_SIGMA),
            sigma_flat_mag_std=float(C.sc.PHOTON_MAG * np.std(rel)),
            sigma_flat_mag_mad=float(C.sc.PHOTON_MAG
                                     * np.median(np.abs(rel - np.median(rel)))
                                     * C.sc.MAD_TO_SIGMA),
            smooth_component_std=float(np.std(Ad @ coef)),
            smooth_component_p16p84=[float(np.percentile(Ad @ coef, 16)),
                                     float(np.percentile(Ad @ coef, 84))],
            rel_after_deg2_std=float(np.std(rel_after)),
            rel_after_deg2_mad_sigma=float(np.median(np.abs(rel_after
                                                            - np.median(rel_after)))
                                           * C.sc.MAD_TO_SIGMA),
            sigma_flat_after_deg2_mag=float(C.sc.PHOTON_MAG * np.std(rel_after)),
            sigma_flat_after_deg2_mag_mad=float(C.sc.PHOTON_MAG
                                                * np.median(np.abs(rel_after
                                                                   - np.median(rel_after)))
                                                * C.sc.MAD_TO_SIGMA))

    A2 = {}
    for key in ("T2_Red", "T3_Red"):
        if key not in flat_map:
            continue
        obs = "Telescope2" if key.startswith("T2") else "Telescope3"
        rows = []
        for d in frames:
            if d["observer"] != obs:
                continue
            st = per_star_flat_stats(flat_map[key], d)
            if st:
                st["frame_id"] = d["frame_id"]
                rows.append(st)
        if rows:
            A2[key] = dict(per_frame=rows,
                           median_sigma_flat_mag_std=float(np.median(
                               [r["sigma_flat_mag_std"] for r in rows])),
                           median_sigma_flat_mag_mad=float(np.median(
                               [r["sigma_flat_mag_mad"] for r in rows])),
                           median_sigma_flat_after_deg2_mag=float(np.median(
                               [r["sigma_flat_after_deg2_mag"] for r in rows])),
                           median_smooth_component_std=float(np.median(
                               [r["smooth_component_std"] for r in rows])))
    out["A2_per_star_flat"] = A2

    # 解析口径（N_eff 近似）对所有母版
    for key, entry in A.items():
        if entry.get("status") != "READ":
            continue
        s = entry["hf"]["mad_sigma"]
        entry["sigma_flat_via_N_eff_analytic_mag"] = float(
            C.sc.sigma_flat_independent(s, FWHM_MODEL, BETA_MODEL))
        entry["sigma_flat_via_N_eff_analytic_mag_using_std"] = float(
            C.sc.sigma_flat_independent(entry["hf"]["std"], FWHM_MODEL, BETA_MODEL))

    # ---------------- (B) 重复位置检验 ----------------
    t1 = time.time()
    rows, per_star = repeat_position_pairs(frames, flat_map, half, dx, dy, P)
    out["B_repeat_position_meta"] = dict(
        n_pairs=len(rows), n_stars_obs_ge2=sum(1 for v in per_star.values() if len(v) >= 2),
        n_stars_distinct_total=len(per_star), wall_time_s=float(time.time() - t1),
        definition=("Δd = 同星在两帧的（去各帧中位后的）星等残差之差 [mag]；"
                    "Δflat = -2.5·log10(母版在该星两位置的 PSF 加权平均之比) [mag]"))
    if rows:
        dd = np.array([r["d_resid_mag"] for r in rows])
        df = np.array([r["d_flat_mag"] for r in rows])
        sep = np.array([r["sep_px"] for r in rows])
        same = np.array([r["same_instrument"] for r in rows], bool)
        def block(mask, tag):
            if mask.sum() < 20:
                return dict(n=int(mask.sum()), status="TOO_FEW")
            y, x = dd[mask], df[mask]
            k, b = np.polyfit(x, y, 1)
            resid = y - (k * x + b)
            r_ = float(np.corrcoef(x, y)[0, 1])
            # 斜率 bootstrap（按星聚类：同一颗星的多个配对不独立）
            stars_m = star_arr[mask]
            uniq = np.unique(stars_m)
            idx_by = {s: np.nonzero(stars_m == s)[0] for s in uniq}
            rng = np.random.default_rng(20261014)
            bs = []
            for _ in range(600):
                rows_pick = np.concatenate([idx_by[uniq[j]]
                                            for j in rng.integers(0, uniq.size, uniq.size)])
                if rows_pick.size < 5:
                    continue
                bs.append(np.polyfit(x[rows_pick], y[rows_pick], 1)[0])
            bs = np.array(bs) if bs else np.array([np.nan])
            return dict(n=int(mask.sum()),
                        pairs_note=tag,
                        n_stars=int(uniq.size), n_boot=int(bs.size),
                        repeatability_sigma_mag=float(C.mad_sigma_mag(y) / np.sqrt(2.0)),
                        delta_d_mad_sigma_mag=float(C.mad_sigma_mag(y)),
                        delta_flat_mad_sigma_mag=float(C.mad_sigma_mag(x)),
                        slope_dd_vs_dflat=k,
                        slope_ci95=[float(np.nanpercentile(bs, 2.5)),
                                    float(np.nanpercentile(bs, 97.5))],
                        intercept=b, pearson_r=r_,
                        resid_mad_sigma_mag=float(C.mad_sigma_mag(resid)),
                        frac_of_dd_explained=float(1.0 - (C.mad_sigma_mag(resid)
                                                          / max(C.mad_sigma_mag(y), 1e-30))),
                        median_sep_px=float(np.median(sep[mask])))
        star_arr = np.array([r["star"] for r in rows])
        out["B_repeat_position"] = dict(
            all_pairs=block(np.ones(len(rows), bool), "all"),
            same_instrument=block(same, "T2×T2 或 T3×T3"),
            cross_instrument=block(~same, "T2×T3"),
            sep_gt_5px=block(sep > 5.0, "|Δ位置|>5 px"),
            sep_gt_10px=block(sep > 10.0, "|Δ位置|>10 px"),
            sep_le_5px=block(sep <= 5.0, "|Δ位置|≤5 px"))
    with open(os.path.join(C.RESULTS, "repeat_position.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["star", "frame_a", "obs_a", "frame_b", "obs_b", "same_instrument",
                    "sep_px", "d_resid_mag", "d_flat_mag", "mag_a", "col_a"])
        for r in rows:
            w.writerow([r["star"], r["frame_a"], r["obs_a"], r["frame_b"], r["obs_b"],
                        r["same_instrument"], "%.4f" % r["sep_px"],
                        "%.6g" % r["d_resid_mag"], "%.6g" % r["d_flat_mag"],
                        "%.6g" % r["mag_a"], "%.6g" % r["col_a"]])

    # ---------------- (C) σ_ceiling 灵敏度（正问题） ----------------
    import scia_pipeline as pl
    from scia_calib import aperture_flux, psf_vs_aperture_systematics
    sens = []
    for m in rc["frames_head"]:
        fid = m["frame_id"]
        gate = m["gate"]
        # 用已落盘的一致量重建：σ_fit_robust、σ_psfsys 来自 (a) 的 gate.budget
        sfr = gate["budget"]["sigma_fit_robust"]
        sps = gate["budget"]["sigma_psfsys"]
        ssr = gate["budget"]["sigma_skyres"]
        sq = gate["budget"]["sigma_q"]
        n = gate["n_inliers"]
        rho_hi = gate["budget"]["rho_hi"]
        row = dict(frame_id=fid, n_inliers=n, rho_hi=rho_hi,
                   sigma_obs_mag=gate["sigma_obs_mag"],
                   sigma_ceiling_flat0=float(rho_hi * np.sqrt(sfr ** 2 + sps ** 2
                                                              + ssr ** 2 + sq ** 2)))
        S = np.sqrt(sfr ** 2 + sps ** 2 + ssr ** 2 + sq ** 2)
        row["S_other_mag"] = float(S)
        # 翻转：rho_hi·sqrt(S²+σf²) = sigma_obs
        need = (gate["sigma_obs_mag"] / rho_hi) ** 2 - S ** 2
        row["flip_sigma_flat_mag"] = float(np.sqrt(need)) if need > 0 else 0.0
        row["curve"] = [dict(sigma_flat=s, sigma_ceiling=float(rho_hi * np.sqrt(S ** 2
                                                                                + s ** 2)),
                             verdict=("ABOVE_CEILING"
                                      if gate["sigma_obs_mag"] > rho_hi * np.sqrt(S ** 2 + s ** 2)
                                      else "PASS_OR_BELOW"))
                        for s in SIGMAS]
        sens.append(row)
    out["C_sensitivity"] = dict(
        canonical=float(C.SIGMA_FLAT_HF_CANONICAL), grid=list(SIGMAS), per_frame=sens,
        step8_prerev_flip_mag=float(rc["step8_original"]["pre"]["gate"].get(
            "sigma_flat_flip_threshold") or float("nan"))
        if rc["step8_original"]["pre"]["gate"].get("sigma_flat_flip_threshold") else None,
        note=("σ_ceiling(σ_flat) 是**正问题**：给定假设 σ_flat 算上界，"
              "不由 σ_obs 反推任何预算项。翻转阈值仅用于说明该假设距判定边界多远。"))
    with open(os.path.join(C.RESULTS, "sigma_flat_sensitivity.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["frame_id", "n_inliers", "sigma_obs_mag", "S_other_mag",
                    "flip_sigma_flat_mag"] + [f"ceiling@sf={s}" for s in SIGMAS])
        for r in sens:
            w.writerow([r["frame_id"], r["n_inliers"], "%.8g" % r["sigma_obs_mag"],
                        "%.8g" % r["S_other_mag"], "%.8g" % r["flip_sigma_flat_mag"]]
                       + ["%.8g" % c["sigma_ceiling"] for c in r["curve"]])

    # ---------------- (D) 直接实验：把母版平场施加到逐星通量，量 Δσ_obs ----------------
    out["D_apply_flat_to_flux"] = apply_flat_experiment(frames, flat_map, half, dx, dy, P)

    # ---------------- (E) σ_pix_e 的独立核算（常数基座 vs 散粒信号） ----------------
    out["E_sigma_pix_e_audit"] = sigma_pix_e_audit(rc, inv)

    out["wall_time_s"] = float(time.time() - t0)
    figures(out, rc)
    C.jdump(out, "sigma_flat.json")
    print(f"[sigma_flat] done {time.time()-t0:.1f}s")
    for k, v in out["A_master_flats"].items():
        if v.get("status") == "READ":
            print(f"  {k}: HF mad_sigma={v['hf']['mad_sigma']:.5f} "
                  f"sigma_flat(N_eff)={v['sigma_flat_via_N_eff_analytic_mag']:.6f} mag "
                  f"low-order smooth std={v['low_order']['smooth_component_std']:.4f} "
                  f"resid_std={v['low_order']['residual_after_loworder_std']:.5f}")
    for k, v in out["A2_per_star_flat"].items():
        print(f"  {k} per-star: sigma_flat(std)={v['median_sigma_flat_mag_std']:.6f} "
              f"mad={v['median_sigma_flat_mag_mad']:.6f} "
              f"after_deg2={v['median_sigma_flat_after_deg2_mag']:.6f} "
              f"smooth_std={v['median_smooth_component_std']:.5f}")
    if "B_repeat_position" in out:
        for k, v in out["B_repeat_position"].items():
            if v.get("status") == "TOO_FEW":
                print(f"  B {k}: TOO_FEW n={v['n']}")
            else:
                print(f"  B {k}: n={v['n']} repeat_sigma={v['repeatability_sigma_mag']:.6f} "
                      f"Dd_mad={v['delta_d_mad_sigma_mag']:.6f} "
                      f"Dflat_mad={v['delta_flat_mad_sigma_mag']:.6f} "
                      f"slope={v['slope_dd_vs_dflat']:+.4f} r={v['pearson_r']:+.4f} "
                      f"explained={v['frac_of_dd_explained']*100:+.2f}% "
                      f"medsep={v['median_sep_px']:.2f}px")
    print("  C flips:", [round(r["flip_sigma_flat_mag"], 6) for r in sens])


def apply_flat_experiment(frames, flat_map, half, dx, dy, P):
    """把母版平场（及其分解分量）按 PSF 加权施加到逐星通量上，直接量 σ_obs 的变化。

    模型：未做平场校正的帧满足 `F_obs,i = F_true,i · m(x_i,y_i)`（m 归一化到中位 1）。
    施加 `F' = F/m_i` ⇔ 星等域 `resid' = resid - 2.5·log10(m_i)`。

    变体：
      full_flat       —— 施加完整母版（= 逐像素平场校正）
      smooth_deg2     —— 只施加母版的二阶多项式平滑分量（σ_obs 对 `m(x,y)` 的敏感度）
      after_deg2      —— 只施加"母版除以二阶分量"的残余（预算项「平场高通」的实测对应量）
    """
    from scia_calib import calibrate
    res = []
    for d in frames:
        key = "T2_Red" if d["observer"] == "Telescope2" else "T3_Red"
        if key not in flat_map:
            continue
        flat = flat_map[key]
        fe = flat_at_positions(flat, d["x"], d["y"], half, dx, dy, P)
        use = d["inlier"] & np.isfinite(fe) & (fe > 0)
        if use.sum() < 20:
            res.append(dict(frame_id=d["frame_id"], status="TOO_FEW", n=int(use.sum())))
            continue
        rel = fe / float(np.median(fe[use])) - 1.0
        h, w = flat.shape
        xn = (d["x"] - w / 2.0) / (w / 2.0); yn = (d["y"] - h / 2.0) / (h / 2.0)
        A = poly_design(xn, yn, 2)
        coef, *_ = np.linalg.lstsq(A[use], rel[use], rcond=None)
        smooth = A @ coef
        hf = (1.0 + rel) / (1.0 + smooth) - 1.0
        variants = {
            "full_flat": 2.5 * np.log10(1.0 + rel),
            "smooth_deg2": 2.5 * np.log10(1.0 + smooth),
            "after_deg2": 2.5 * np.log10(1.0 + hf),
        }
        F, fs, x, y = d["flux_adu"], d["f_syn_R"], d["x"], d["y"]
        base = calibrate(F, fs, None, x, y, m_degree=0, valid=use)
        row = dict(frame_id=d["frame_id"], observer=d["observer"], n=int(use.sum()),
                   n_clusters=int(np.unique(d["star_index"][use]).size),
                   flat_key=key,
                   flat_eff_rel_std=float(np.std(rel[use])),
                   flat_eff_rel_mad=float(np.median(np.abs(rel[use]
                                                           - np.median(rel[use])))
                                          * C.sc.MAD_TO_SIGMA),
                   smooth_component_std=float(np.std(smooth[use])),
                   after_deg2_std=float(np.std(hf[use])),
                   sigma_obs_baseline=float(base["sigma_obs_mag"]),
                   variants={})
        for name, corr in variants.items():
            corr = corr.copy()
            corr[~use] = 0.0
            Fp = F * 10.0 ** (-0.4 * corr)
            cal = calibrate(Fp, fs, None, x, y, m_degree=0, valid=use)
            row["variants"][name] = dict(
                sigma_obs_mag=float(cal["sigma_obs_mag"]),
                delta_sigma_obs_mag=float(cal["sigma_obs_mag"] - base["sigma_obs_mag"]),
                reduction_pct=float((base["sigma_obs_mag"] - cal["sigma_obs_mag"])
                                    / base["sigma_obs_mag"] * 100),
                implied_sigma_flat_mag=float(np.sqrt(max(
                    base["sigma_obs_mag"] ** 2 - cal["sigma_obs_mag"] ** 2, 0.0))))
        res.append(row)
    ok = [r for r in res if r.get("variants")]
    summary = {}
    if ok:
        for name in ("full_flat", "smooth_deg2", "after_deg2"):
            summary[name] = dict(
                median_delta_sigma_obs_mag=float(np.median(
                    [r["variants"][name]["delta_sigma_obs_mag"] for r in ok])),
                median_implied_sigma_flat_mag=float(np.median(
                    [r["variants"][name]["implied_sigma_flat_mag"] for r in ok])),
                median_reduction_pct=float(np.median(
                    [r["variants"][name]["reduction_pct"] for r in ok])))
    return dict(per_frame=res, summary=summary)


def sigma_pix_e_audit(rc, inv):
    """用帧内独立噪声测量反算「真正载有散粒噪声的信号」与常数基座。

    `step8_real_frame.py:151` 取 `σ_pix_e = sqrt(RN² + sky_med·gain)`，把**整个**天空中位数
    当成泊松信号。但真实光帧的天空中位数含**不产生散粒噪声的常数基座**（偏置/基线）。
    帧内独立测量给的是**邻元差分** RMS：`σ_adu = MAD(Δimg)/0.6745/√2`，
    它直接量出逐像素实际噪声 ⇒ 可反解散粒信号：

        σ_meas[e-] = σ_adu·gain
        S_shot[ADU] = max((σ_meas² − RN²)/gain, 0)
        常数基座[ADU] = sky_med − S_shot

    然后用 σ_meas 取代模型 σ_pix_e 重算预算（同一 MC tag），量 σ_ceiling 与判定的变化。
    **这是独立测量，不由 σ_obs 反推任何量。**
    """
    import scia_pipeline as pl
    gain = C.INSTRUMENT_HARDCODE["gain"]
    rn = C.INSTRUMENT_HARDCODE["read_noise"]
    from scia_calib import SIGMA_FLAT_HF_CANONICAL
    per = []
    for m in rc["frames_head"]:
        fid = m["frame_id"]
        ii = m["in_frame_instrument"]
        g = m["gate"]
        z = np.load(os.path.join(PERSTAR, f"{fid}__head.npz"))
        F = z["flux_adu"]; ok = z["inlier"] & np.isfinite(F) & (F > 0)
        F = F[ok]
        bg_adu = ii["bg_rms_adu_adjacent_diff"]
        sig_meas = bg_adu * gain
        s_shot = max((sig_meas ** 2 - rn ** 2) / gain, 0.0)
        ped = ii["sky_median_adu"] - s_shot
        items = g["items"]
        sps = items["sigma_psfsys_inframe_r4"]
        struct = items["structure_factor"]
        n = g["n_inliers"]
        b_corr = pl.build_budget(F, C.instrument(), sig_meas, sps, 0.0, 0.0,
                                 float(SIGMA_FLAT_HF_CANONICAL), struct, n, tag="s04")
        per.append(dict(
            frame_id=fid, n=int(F.size), n_inliers=int(n),
            sky_median_adu=ii["sky_median_adu"],
            bg_rms_adu_adjacent_diff=bg_adu,
            sigma_pix_e_model=ii["sigma_pix_e_model"],
            sigma_pix_e_measured=sig_meas,
            model_over_measured_ratio=float(ii["sigma_pix_e_model"] / sig_meas),
            shot_noise_bearing_sky_adu=float(s_shot),
            constant_pedestal_adu=float(ped),
            pedestal_fraction_of_sky_median=float(ped / ii["sky_median_adu"]),
            sigma_fit_robust_stored=g["budget"]["sigma_fit_robust"],
            sigma_fit_robust_with_measured_pix=b_corr.sigma_fit_robust,
            sigma_ceiling_stored=g["sigma_ceiling"],
            sigma_ceiling_with_measured_pix=float(b_corr.sigma_ceiling),
            sigma_obs_mag=g["sigma_obs_mag"],
            verdict_stored=g["verdict"],
            verdict_with_measured_pix=C.sc.gate_verdict(g["sigma_obs_mag"], b_corr)))
    return dict(
        rule=("σ_pix_e(model) = sqrt(RN² + sky_median·gain)；σ_pix_e(measured) = "
              "MAD(adjacent diff)/0.6745/√2 · gain；散粒信号 = (σ_meas² − RN²)/gain；"
              "基座 = sky_median − 散粒信号"),
        gain=gain, read_noise_e=rn, mc_tag="s04",
        per_frame=per,
        median_model_over_measured=float(np.median(
            [p["model_over_measured_ratio"] for p in per])),
        median_pedestal_adu=float(np.median([p["constant_pedestal_adu"] for p in per])),
        median_pedestal_fraction=float(np.median(
            [p["pedestal_fraction_of_sky_median"] for p in per])),
        direction_note=("把常数基座从散粒项里剔除**下调** σ_pix_e ⇒ σ_fit_robust 与 "
                        "σ_ceiling（本帧集的主导项是 σ_psfsys，占比见 variance_share）"
                        "随之下降 ⇒ ABOVE_CEILING 更难被「通过」，方向上仍 fail-closed。"
                        "该修正对判定的影响逐帧列在 per_frame。"))


def figures(out, rc):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    figs = []
    # Fig 4: σ_ceiling(σ_flat) 曲线 + 独立测量值 + 约定值
    fig, ax = plt.subplots(figsize=(8.8, 5.0))
    for r in out["C_sensitivity"]["per_frame"]:
        xs = [c["sigma_flat"] for c in r["curve"]]
        ys = [c["sigma_ceiling"] for c in r["curve"]]
        lab = r["frame_id"].split("-")[-1].replace("-300S-Red", "").replace(".fts", "")
        ax.plot(xs, ys, "-", lw=1.2, alpha=0.85, label=lab)
        ax.plot([r["flip_sigma_flat_mag"]], [r["sigma_obs_mag"]], "x", color="k", ms=5)
        ax.axhline(r["sigma_obs_mag"], color="grey", lw=0.4, alpha=0.25)
    ax.axvline(0.0007, color="r", ls="--", lw=1.3, label="canonical 0.0007 mag")
    vals = [out["A2_per_star_flat"][k]["median_sigma_flat_mag_std"]
            for k in out["A2_per_star_flat"]]
    for v in vals:
        ax.axvline(v, color="g", ls=":", lw=1.3)
    ax.plot([], [], "g:", label="independent master-flat estimate (per-star)")
    ax.set_xlabel(r"assumed $\sigma_{\rm flat}$ [mag]")
    ax.set_ylabel(r"$\sigma_{\rm ceiling}$ [mag]")
    ax.set_title("S04 Fig.4  ceiling vs assumed sigma_flat;  x = flip threshold,"
                 "  y = measured sigma_obs")
    ax.legend(fontsize=7, ncol=2, title="frame (date@time)"); ax.grid(alpha=0.3)
    fig.tight_layout()
    p4 = os.path.join(C.FIGURES, "fig4_sigma_flat_sensitivity.png")
    fig.savefig(p4, dpi=140); plt.close(fig); figs.append(p4)

    # Fig 5: 重复位置 Δd vs Δflat 与 |Δpos|
    rows = []
    p = os.path.join(C.RESULTS, "repeat_position.csv")
    with open(p, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows.append(r)
    if rows:
        import numpy as np
        dd = np.array([float(r["d_resid_mag"]) for r in rows])
        df = np.array([float(r["d_flat_mag"]) for r in rows])
        sp = np.array([float(r["sep_px"]) for r in rows])
        fig, axs = plt.subplots(1, 2, figsize=(11.5, 4.3))
        axs[0].plot(df, dd, ".", ms=2.5, alpha=0.4)
        k, b = np.polyfit(df, dd, 1)
        xs = np.linspace(df.min(), df.max(), 10)
        axs[0].plot(xs, k * xs + b, "r-", lw=1.5, label=f"slope={k:+.3f}")
        axs[0].set_xlabel(r"$\Delta$ flat (master, PSF-averaged) [mag]")
        axs[0].set_ylabel(r"$\Delta d$ (paired residual difference) [mag]")
        axs[0].legend(fontsize=8); axs[0].grid(alpha=0.3)
        axs[1].plot(sp, np.abs(dd), ".", ms=2.5, alpha=0.4)
        axs[1].set_xlabel("pixel separation between the two observations [px]")
        axs[1].set_ylabel(r"$|\Delta d|$ [mag]")
        axs[1].grid(alpha=0.3)
        fig.suptitle("S04 Fig.5  repeat-position test: paired residual difference vs"
                     " flat difference / position shift")
        fig.tight_layout()
        p5 = os.path.join(C.FIGURES, "fig5_repeat_position.png")
        fig.savefig(p5, dpi=140); plt.close(fig); figs.append(p5)
    out["figures"] = [os.path.relpath(x, C.REPO) for x in figs]


if __name__ == "__main__":
    main()
