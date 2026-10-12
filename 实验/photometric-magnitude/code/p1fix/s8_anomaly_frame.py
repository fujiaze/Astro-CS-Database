#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S8 异常帧取证：`M4_T2_20251228_044805` 的 57% 帧级标度偏离是真实透过率差还是拟合伪值。

只读 `run/PHOT-FIX/p1_rerun/t2/**` 与 `testdata/M42_T2T3_mosaic_Flying_dutchman/**`；
不改生产代码、不改既有结果 JSON。输出 `results/p1fix/s8_anomaly_frame.json`。

口径依据（已读）：
  · docs/science/photometry/PHOTOMETRY.md §1/§3/§5：`I_photo = k_photo·m(x,y)·I_cal`；
    `r = log10(F_instr/F_syn)`；`scale = 10^(−location)`；判据 = 施加后残差散度；
    **各帧独立标定、跨帧 k_photo 不同属正常**；大气消光为**显式未建模项**。
  · docs/detail/registry/acsd.phase1.photometry.md:84-92/122-123：`k_photo` 是唯一帧级测光标度；
    **帧间零点差只反映帧透射率差**；跨帧 k_photo 不同不得据此判缺陷。
  · docs/detail/registry/acsd.phase1.photometry.md:32：k_photo 绝对值无物理意义。

判别设计（不靠相关性，靠机制）：
  若该帧确为**真实透过率差**（薄云/水汽），则同一批星在该帧与兄弟帧的**逐星仪器流量比**
  应是**与星等/信噪/位置无关的常数**，且该常数 ≈ k_photo 之比。
  若为**拟合伪值**，则比值应随星等/信噪/位置显著变化，或与 k_photo 之比不符。
"""
from __future__ import annotations

import json
import math
import re
import statistics as st
import warnings
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree

warnings.filterwarnings("ignore")

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun/t2"
INP = ROOT / "testdata/M42_T2T3_mosaic_Flying_dutchman"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"

ANOM = "20251228_044805"
SIB = ["20251224_045919", "20251216_050117"]


def load(name):
    return json.loads((SRC / name).read_text(encoding="utf-8"))


def tag_of(fkey):
    """各种帧键形式 → 统一 `YYYYMMDD_HHMMSS`。

    M42_M4_T2_flying_dutchman-20251228_044805-300S-Red   → 20251228_044805
    cleaned_M42_M1_T2_flying_dutchman-20251212@012404-... → 20251212_012404
    """
    m = re.search(r"(\d{8})[@_](\d{6})", fkey)
    return ("%s_%s" % (m.group(1), m.group(2))) if m else fkey


def hdr(path):
    from astropy.io import fits
    return fits.getheader(path)


def find_input(tag):
    """tag = YYYYMMDD_HHMMSS → 输入 FITS（文件名用 @ 分隔）。"""
    d, t = tag.split("_")
    hits = list(INP.glob("T*/*/*%s@%s*" % (d, t)))
    return hits[0] if hits else None


def mad(v):
    v = sorted(v)
    m = v[len(v) // 2]
    return sorted(abs(x - m) for x in v)[len(v) // 2] / 0.6744897501960817


out = {}

# ---------------------------------------------------------------- 载入
src = {tag_of(f["file"]): f for f in load("p1_sources.json")["frames"]}
snr = {tag_of(f["file"]): f for f in load("p1_snr.json")["frames"]}
flux = {tag_of(f["file"]): f for f in load("p1_flux.json")["frames"]}
phot = load("p1_phot.json")
psd, pfit, sg = phot["photscale_detail"], phot["photscale_fit"], phot["spatial_gain"]
KEY = {tag_of(k): k for k in psd}
ZP = {tag_of(f["frame_key"]): f for f in phot["frames"]} if isinstance(phot["frames"][0], dict) else {}
zero = {}
for f in phot["frames"]:
    if isinstance(f, dict) and "frame_key" in f:
        zero[tag_of(f["frame_key"])] = f

# 零点（XP 那一路）逐帧读数：从 p1_snr/p1_phot 的帧级键里找
zp_keys = [k for k in phot if "zero_point" in json.dumps(phot[k])[:2000]]
out["_probe_zp_keys"] = zp_keys

# ---------------------------------------------------------------- 1. 逐项对比
def frame_row(tag):
    s, n, p = src[tag], snr[tag], psd[KEY[tag]]
    g = sg[KEY[tag]]
    inp = find_input(tag)
    h = hdr(inp) if inp else {}
    return dict(
        tag=tag, filter=h.get("FILTER"), exptime=h.get("EXPTIME"),
        airmass=h.get("AIRMASS"), objctalt=h.get("OBJCTALT"),
        has_ZMAG=bool(inp and "ZMAG" in h), ZMAG=(h.get("ZMAG") if inp and "ZMAG" in h else None),
        n_detected=s.get("n_detected"), n_sources=s.get("n_sources"),
        n_psf_valid=s.get("n_psf_valid"), n_fit_input=s.get("n_fit_input"),
        n_saturated=s.get("n_saturated"), n_edge=s.get("n_edge"),
        background=s.get("background"), noise_sigma=s.get("noise_sigma"),
        detection_mode=s.get("detection_mode"),
        median_snr=n.get("median_snr"), median_source_snr=n.get("median_source_snr"),
        n_snr_input=n.get("n_snr_input"),
        n_matched=p.get("n_matched"), n_psf_domain=p.get("n_psf_domain"),
        k_photo=p.get("k_photo"), sigma_residual_dex=p.get("sigma_residual_dex"),
        order=g.get("order"), order_requested=g.get("order_requested"),
        kappa=g.get("kappa"), field_ptp_dex=g.get("field_ptp_dex"),
        field_rms_dex=g.get("field_rms_dex"), n_used=g.get("n_used"),
        coverage_blocks=g.get("coverage_blocks"),
        coverage_bbox_frac=g.get("coverage_bbox_frac"),
        degraded_reason=g.get("degraded_reason"), status=g.get("status"),
        noise_floor_dex=g.get("noise_floor_dex"),
        sigma_spatial_residual_dex=g.get("sigma_spatial_residual_dex"),
    )


all_tags = sorted(src)
m4t2 = [t for t in all_tags if t.startswith(("20251216_050117", "20251224_045919",
                                              "20251228_044805"))]
out["item1_rows_m4t2"] = [frame_row(t) for t in sorted(m4t2)]
out["item1_rows_all"] = [frame_row(t) for t in all_tags]

# ---------------------------------------------------------------- 2/3. 逐星分布与空间
def star_rows(tag, valid_only=True):
    r = flux[tag]["results"]
    if valid_only:
        r = [x for x in r if x.get("valid")]
    return r


def dist_summary(vals):
    if not vals:
        return dict(n=0)
    v = sorted(vals)
    return dict(n=len(v), min=v[0], p16=v[int(0.16 * len(v))], median=st.median(v),
                p84=v[int(0.84 * len(v))], max=v[-1], mean=st.fmean(v), mad=mad(v))


def flux_to_mag(f):
    return -2.5 * math.log10(f) if f and f > 0 else None


star = {}
for tag in sorted(m4t2):
    rows = star_rows(tag)
    mags = [flux_to_mag(x["flux"]) for x in rows if flux_to_mag(x["flux"]) is not None]
    snrs = [x["snr"] for x in rows]
    xs = [x["x"] for x in rows]
    ys = [x["y"] for x in rows]
    # 空间覆盖：4x4 象限计数 + 径向
    q = [0] * 16
    for x, y in zip(xs, ys):
        q[min(3, int(x // 1024)) * 4 + min(3, int(y // 1024))] += 1
    star[tag] = dict(
        n_valid=len(rows),
        mag=dist_summary(mags), snr=dist_summary(snrs),
        x_span=[min(xs), max(xs)], y_span=[min(ys), max(ys)],
        quad16=q,
        bbox_area_frac=((max(xs) - min(xs)) * (max(ys) - min(ys))) / (4096.0 * 4096.0),
    )
out["item2_item3_stars"] = star

# ---------------------------------------------------------------- 4. 剔除敏感性
def log10C(tag):
    k = psd[KEY[tag]].get("k_photo")
    return math.log10(k) if k and k > 0 else None


m4 = [t for t in all_tags if t.startswith(("20251216_050117", "20251224_045919",
                                           "20251228_044805"))]
lk_m4 = {t: log10C(t) for t in m4}
lk_all = {t: log10C(t) for t in all_tags}


def sd(vals):
    return st.stdev(vals) if len(vals) > 1 else float("nan")


def sens(lk, drop):
    keep = [v for t, v in lk.items() if t != drop and v is not None]
    with_ = [v for v in lk.values() if v is not None]
    return dict(n_with=len(with_), sd_with=sd(with_),
                n_without=len(keep), sd_without=sd(keep),
                median_with=st.median(with_), median_without=st.median(keep),
                ratio=sd(with_) / sd(keep) if keep and sd(keep) else None)


out["item4_removal"] = dict(
    m4_group=sens(lk_m4, ANOM),
    all16=sens(lk_all, ANOM),
    log10k_m4=lk_m4, log10k_all=lk_all,
    note="log10 C' 的 SD 用 log10 k_photo 的 SD 代表（k_photo 是唯一帧级测光标度）；"
         "同时给 log10 C' 本身的中位变化。",
)

# ---------------------------------------------------------------- 5. 饱和/背景量级
item5 = {}
for tag in sorted(m4t2):
    s = src[tag]
    nd = s.get("n_detected") or 0
    nsat = s.get("n_saturated") or 0
    item5[tag] = dict(
        n_detected=nd, n_saturated=nsat,
        sat_frac=(nsat / nd if nd else None),
        background=s.get("background"), noise_sigma=s.get("noise_sigma"),
        n_used=sg[KEY[tag]].get("n_used"),
        n_matched=psd[KEY[tag]].get("n_matched"),
    )
out["item5_saturation_background"] = item5

# ---------------------------------------------------------------- 6. 外部零点可用性
out["item6_external"] = dict(
    header_has_ZMAG={t: (frame_row(t)["has_ZMAG"]) for t in sorted(m4t2)},
    header_airmass={t: frame_row(t)["airmass"] for t in sorted(m4t2)},
    note="输入头键名经全量扫描确认：ZMAG 存在于 196 帧中的 166 帧；M4_T2 全部 13 帧均无 ZMAG，"
         "故本异常帧与其两名兄弟帧**都没有外部零点可用**（组级缺失，非该帧独有）。"
         "可用的独立头量为 AIRMASS / OBJCTALT。",
)

# ---------------------------------------------------------------- 关键判别：逐星流量比
#
# 帧间有抖动（dither），且不同夜次可能有旋转 ⇒ 先粗搜平移，再拟合**相似变换**
# （旋转+平移+各向同性缩放）并迭代收紧，最后按 2 px 匹配。本方法只用星位，不用拟合量。
def _fit_similarity(src, dst):
    c_s, c_d = src.mean(0), dst.mean(0)
    X, Y = src - c_s, dst - c_d
    M = np.zeros((2 * len(X), 2))
    rhs = np.zeros(2 * len(X))
    M[0::2, 0] = X[:, 0]; M[0::2, 1] = -X[:, 1]; rhs[0::2] = Y[:, 0]
    M[1::2, 0] = X[:, 1]; M[1::2, 1] = X[:, 0]; rhs[1::2] = Y[:, 1]
    a, b = np.linalg.lstsq(M, rhs, rcond=None)[0]
    return float(a), float(b), c_s, c_d


def align(a_tag, b_tag, tol=2.0, coarse=120.0, step=2.0):
    """返回 (pairs, transform_info)。pairs 元素含 fa/fb/snr/x/y。"""
    A, B = star_rows(a_tag), star_rows(b_tag)
    if not A or not B:
        return [], dict(n_pairs=0)
    ax = np.array([[x["x"], x["y"]] for x in A])
    bx = np.array([[x["x"], x["y"]] for x in B])
    tree = cKDTree(bx)
    best = (0, 0.0, 0.0)
    for dx in np.arange(-coarse, coarse + 1e-9, step):
        for dy in np.arange(-coarse, coarse + 1e-9, step):
            dist, _ = tree.query(ax + np.array([dx, dy]), k=1)
            n = int(np.sum(dist < 3.0))
            if n > best[0]:
                best = (n, float(dx), float(dy))
    _, dx, dy = best
    dist, idx = tree.query(ax + np.array([dx, dy]), k=1)
    sel = dist < 3.0
    src, dst = ax[sel], bx[idx[sel]]
    a = b = None
    for _ in range(3):
        if len(src) < 8:
            break
        a, b, c_s, c_d = _fit_similarity(src, dst)
        R = np.array([[a, -b], [b, a]])
        mapped = (ax - c_s) @ R.T + c_d
        dist, idx = tree.query(mapped, k=1)
        sel = dist < tol
        src, dst = ax[sel], bx[idx[sel]]
    pairs = []
    for i in np.where(sel)[0]:
        j = int(idx[i])
        pairs.append(dict(fa=A[i]["flux"], fb=B[j]["flux"], snr_a=A[i]["snr"],
                          snr_b=B[j]["snr"], x=A[i]["x"], y=A[i]["y"], dpx=float(dist[i])))
    info = dict(n_pairs=len(pairs), n_a=len(A), n_b=len(B),
                match_frac=len(pairs) / len(A) if A else 0.0,
                coarse_shift=[dx, dy],
                scale=(float(math.hypot(a, b)) if a is not None else None),
                rot_deg=(float(math.degrees(math.atan2(b, a))) if a is not None else None))
    return pairs, info


def match_pairs(a_tag, b_tag, tol=2.0):
    return align(a_tag, b_tag, tol=tol)[0]


def pair_report(a_tag, b_tag):
    P, info = align(a_tag, b_tag)
    if not P:
        return dict(n=0, align=info)
    # 可用性：配对率过低 ⇒ 帧间未对齐（不同指向或未解出的变换），比值不可用
    info["usable"] = bool(info.get("match_frac", 0) >= 0.5)
    r = [p["fa"] / p["fb"] for p in P if p["fb"] > 0]
    mag = [-2.5 * math.log10(x) for x in r if x > 0]
    # 按信噪/星等分箱看比值是否随样本域变化
    by_snr = {}
    for lo, hi in ((0, 30), (30, 60), (60, 120), (120, 1e9)):
        sel = [p["fa"] / p["fb"] for p in P if lo <= p["snr_b"] < hi and p["fb"] > 0]
        by_snr["snr_b %g-%g" % (lo, hi)] = dict(n=len(sel),
                                                median_ratio=st.median(sel) if sel else None)
    # 按半径分箱
    by_r = {}
    for lo, hi in ((0, 1000), (1000, 2000), (2000, 3000), (3000, 1e9)):
        sel = [p["fa"] / p["fb"] for p in P
               if lo <= math.hypot(p["x"] - 2048, p["y"] - 2048) < hi and p["fb"] > 0]
        by_r["r %g-%g" % (lo, hi)] = dict(n=len(sel), median_ratio=st.median(sel) if sel else None)
    return dict(n_pairs=len(P), align=info, median_ratio=st.median(r), mad_ratio=mad(r),
                median_mag=st.median(mag), mad_mag=mad(mag),
                ratio_p16=sorted(r)[int(0.16 * len(r))],
                ratio_p84=sorted(r)[int(0.84 * len(r))],
                by_snr_bin=by_snr, by_radius_bin=by_r,
                dpx_median=st.median([p["dpx"] for p in P]))


kp = {t: psd[KEY[t]].get("k_photo") for t in m4t2}
rat = {}
for s in SIB:
    key = "%s/%s" % (ANOM, s)
    rep = pair_report(ANOM, s)
    kp_ratio = (kp[ANOM] / kp[s]) if kp.get(s) else None
    rep["k_photo_ratio_anom_over_sib"] = kp_ratio
    rep["expected_instr_flux_ratio_if_pure_transparency"] = (
        1.0 / kp_ratio if kp_ratio else None)
    rat[key] = rep
out["decisive_matched_flux_ratio"] = rat

# ---------------------------------------------------------------- 写盘
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "s8_anomaly_frame.json").write_text(
    json.dumps(out, indent=2, ensure_ascii=False, default=float), encoding="utf-8")

# ---------------------------------------------------------------- 打印
print("== S8 异常帧取证 ==")
print("-- 1. M4_T2 逐项对比 --")
cols = ("tag", "n_detected", "n_sources", "n_saturated", "background", "n_matched",
        "k_photo", "sigma_residual_dex", "kappa", "field_ptp_dex", "order",
        "coverage_blocks", "airmass", "has_ZMAG")
print("  " + " ".join("%-12s" % c for c in cols))
for r in out["item1_rows_m4t2"]:
    print("  " + " ".join("%-12s" % (("%.6g" % r[c]) if isinstance(r[c], float) else r[c])
                          for c in cols))
print()
print("-- 4. 剔除敏感性（log10 k_photo 的 SD）--")
for k, v in out["item4_removal"].items():
    if isinstance(v, dict) and "sd_with" in v:
        print("  %-8s SD 含=%.6f 去=%.6f  比=%.3f  中位 含=%.6f 去=%.6f"
              % (k, v["sd_with"], v["sd_without"], v["ratio"],
                 v["median_with"], v["median_without"]))
print()
print("-- 6. 外部零点 --")
print("  ZMAG 可用性:", out["item6_external"]["header_has_ZMAG"])
print("  AIRMASS     :", out["item6_external"]["header_airmass"])
print()
print("-- 关键判别：逐星仪器流量比（异常/兄弟）--")
for k, v in rat.items():
    if not v.get("n_pairs"):
        print("  %s: 无配对" % k); continue
    if not v.get("align", {}).get("usable"):
        print("  %s: **不可用**（配对率 %.1f%% ⇒ 帧间未对齐，比值为伪配对）"
              % (k, 100 * v["align"].get("match_frac", 0)))
        continue
    print("  %s  n=%d (配对率 %.0f%%)  比值中位=%.4f (MAD %.4f)  → %.3f mag   p16=%.4f p84=%.4f"
          % (k, v["n_pairs"], 100 * v["align"]["match_frac"], v["median_ratio"],
             v["mad_ratio"], v["median_mag"], v["ratio_p16"], v["ratio_p84"]))
    print("      k_photo 比=%.4f  ⇒ 纯透过率预期流量比=%.4f"
          % (v["k_photo_ratio_anom_over_sib"],
             v["expected_instr_flux_ratio_if_pure_transparency"]))
    print("      按 snr_b 分箱:", {a: (b["n"], None if b["median_ratio"] is None else round(b["median_ratio"], 4))
                                   for a, b in v["by_snr_bin"].items()})
    print("      按半径分箱:", {a: (b["n"], None if b["median_ratio"] is None else round(b["median_ratio"], 4))
                                for a, b in v["by_radius_bin"].items()})
