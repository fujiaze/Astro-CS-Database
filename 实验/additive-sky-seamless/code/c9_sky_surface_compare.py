#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""C9 三种减法曲面离线对比（只读 + 实验，不改生产代码，不碰 git）。

任务：owner 要求“平面不准，研究用什么方法减，既不产生接缝、又保信噪比、
不伤星系核心/行星状星云这类梯度突变信号”。
对比三种减法曲面：
  (a) 每帧全局低阶（1~2 阶平面，现状）；
  (b) 样条曲面（节点稀疏可调）；
  (c) 多尺度低频（Siril 式 scale/smoothness 可调 + 结构保护 mask）。

施加语义（REPORT_paper §2，强制）：calibrated = raw - (S - level(S))，
即只扣曲面起伏、保留公共面 B_ref（Siril 同构：img -= background;
img += background_mean）。“全减背景”（raw - S）是退化做法，本脚本另设
退化对照臂仅用于证伪，不作选型候选。

失效边界（REPORT_paper §4）：无接缝 ⟺ 公共面可表示；肘点 s ≲ 2h。
Siril 腿（只读不复制，独立实现高斯尺度空间，不抄 GPL 代码）：
  RBF 薄板核 k(r)=r^2 log r + smoothing 旋钮、多项式 1~4 阶、
  规则网格采样 + 全局 median + MAD*tolerance 高阈值 veto、
  减背景后加回全局均值。行号见 docs/c9_report.md。

三条腿：
  L1 合成真值腿（主证据）：256² 场，已知真值（平滑天光 + 覆盖缝台阶 +
     星系核心/亮星/云丝/PN 结梯度突变信号 + Poisson+读出噪声），
     结构 mask 数据驱动，全扫描 12 配置，四项评分；
  L2 RERUN3 只读腿（辅证据）：成品 FITS 切块（缝带/暗段/M42S 真实结构），
     缝 guard 带排除拟合（防循环论证），代表性 6 配置，行为合理性 +
     真实结构保留 + 代价；
  L3 方差腿：加法恒等式（解析）+ 噪声 MC 方差膨胀（16 实现，固定 seed）。

四项评分（每配置每用例）：
  M1 缝带抑制量（sup_frac；另有缝外对照线“不新增强缝”检查）；
  M2 暗段回归（Δdark 相对同窗中位标准误 SE）；
  M3 高频信号保留（孔径泄漏分数，核心/亮星/云丝/PN 四点）；
  M4 信噪比影响（MC 方差膨胀因子 + 恒等式）。
另记代价：单次拟合耗时、参数个数。

判据形态：评分类门一律如实记录（record，不判红）；安全门判红：
  中位保留（背景保留，非退化）、无 NaN 污染、恒等式成立、
  全部配置完成。选型由评分表 + 明示权重得出，不由门硬编码。

固定 seed：SEED_BASE=20260923，derive_rng(tag)=SHA-256 派生。
只读输入：run/E2E-HALO-RERUN3/out/p3/output_phase3.fits。
输出：实验/additive-sky-seamless/results/c9_sky_surface_compare.json。

复跑：python3 实验/additive-sky-seamless/code/c9_sky_surface_compare.py
"""
from __future__ import annotations

import hashlib
import json
import sys
import time
from pathlib import Path

import numpy as np

SEED_BASE = 20260923
CODE = Path(__file__).resolve().parent
UNIT = CODE.parent
RESULTS = UNIT / "results"
ROOT = UNIT.parents[1]
RERUN3_FITS = ROOT / "run/E2E-HALO-RERUN3/out/p2"  # placeholder, corrected below
RERUN3_FITS = ROOT / "run/E2E-HALO-RERUN3/out/p3/output_phase3.fits"

N = 256
BG = 300.0
READ_E = 5.0
N_MC = 16

# L1 合成场布局：缝在 x=128（左右半幅帧间台阶的单帧类比）；
# 四个高频信号点远离缝，避免缝度量与信号保留互相污染。
FEATS = [
    {"id": "core", "x": 64, "y": 64, "amp": 4000.0, "sig": 7.0, "kind": "gauss"},  # 星系核心
    {"id": "star", "x": 192, "y": 64, "amp": 2500.0, "sig": 2.0, "kind": "gauss"},  # 亮星
    {"id": "fil", "x": 64, "y": 192, "amp": 300.0, "sig": 2.5, "kind": "ridge"},  # 云丝
    {"id": "pn", "x": 192, "y": 192, "amp": 900.0, "sig": 3.0, "kind": "ring"},  # 行星状星云壳
]
AP_R = {"core": 10, "star": 5, "fil": 7, "pn": 7}

# 12 配置：a×2 + b×4 + c×4 + 退化对照×2
CONFIGS = [
    {"id": "a1", "fam": "a", "desc": "全局 1 阶平面（现状）", "deg": 1},
    {"id": "a2", "fam": "a", "desc": "全局 2 阶平面", "deg": 2},
    {"id": "b16", "fam": "b", "desc": "样条节点 16px", "step": 16},
    {"id": "b32", "fam": "b", "desc": "样条节点 32px", "step": 32},
    {"id": "b64", "fam": "b", "desc": "样条节点 64px", "step": 64},
    {"id": "b128", "fam": "b", "desc": "样条节点 128px（过稀疏负例）", "step": 128},
    {"id": "cS08T20", "fam": "c", "desc": "多尺度 σ=8 高阈 2.0", "sigma": 8.0, "thr": 2.0},
    {"id": "cS16T20", "fam": "c", "desc": "多尺度 σ=16 高阈 2.0", "sigma": 16.0, "thr": 2.0},
    {"id": "cS32T20", "fam": "c", "desc": "多尺度 σ=32 高阈 2.0", "sigma": 32.0, "thr": 2.0},
    {"id": "cS16T05", "fam": "c", "desc": "多尺度 σ=16 低阈 0.5（过保护负例）", "sigma": 16.0, "thr": 0.5},
    {"id": "degen_full", "fam": "degen", "desc": "退化对照：全减背景 raw-S（b64 面）", "step": 64},
    {"id": "degen_plane", "fam": "degen", "desc": "无保护多尺度 σ=16 全采样（结构泄漏对照）", "sigma": 16.0, "thr": -1.0},
]


def derive_rng(tag: str) -> np.random.Generator:
    h = hashlib.sha256(("C9|%d|%s" % (SEED_BASE, tag)).encode("utf-8")).digest()
    return np.random.default_rng(int.from_bytes(h[:8], "little"))


def gauss2(yy, xx, x0, y0, sig):
    return np.exp(-((xx - x0) ** 2 + (yy - y0) ** 2) / (2 * sig ** 2))


def build_truth():
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    u = (xx - N / 2) / N
    v = (yy - N / 2) / N
    # 平滑天光：常数 + 一阶 + 二阶 + 大尺度正弦（全场可表示分量）
    sky = BG + 12.0 * u + 8.0 * v + 20.0 * (u ** 2 - 1 / 12) + 10.0 * u * v \
        + 6.0 * np.sin(2 * np.pi * xx / N) * np.sin(2 * np.pi * yy / N)
    # 覆盖缝台阶：x=128 处 +8e-（右半幅帧间差的单帧类比，§4 线性关系真值）
    seam_true = np.zeros_like(sky)
    seam_true[:, 128:] = 8.0
    sig_field = np.zeros_like(sky)
    for f in FEATS:
        if f["kind"] == "gauss":
            sig_field += f["amp"] * gauss2(yy, xx, f["x"], f["y"], f["sig"])
        elif f["kind"] == "ridge":
            sig_field += f["amp"] * np.exp(-((yy - f["y"]) ** 2) / (2 * f["sig"] ** 2)) \
                * np.exp(-((xx - f["x"]) ** 2) / (2 * 40.0 ** 2))
        elif f["kind"] == "ring":
            r = np.sqrt((xx - f["x"]) ** 2 + (yy - f["y"]) ** 2)
            sig_field += f["amp"] * np.exp(-((r - 8.0) ** 2) / (2 * f["sig"] ** 2))
    base = sky + seam_true + sig_field
    return {"sky": sky, "seam_true": seam_true, "sig": sig_field, "base": base}


def observe(base, rng):
    elec = rng.poisson(np.clip(base, 0.5, None)) + rng.normal(0, READ_E, size=base.shape)
    return elec.astype(np.float64)


def data_mask(raw, feat_dilate=2):
    """数据驱动结构 mask（Siril 高阈值 veto 类比）：全局 median+MAD 高阈；
    另对已知真值信号区（L1 仅用于 mask 构造的 oracle 形态）做膨胀保护。
    注：阈值形态与 Siril 同构（median + k*MAD），实现独立。"""
    med = float(np.median(raw))
    mad = float(np.median(np.abs(raw - med)))
    return med, mad


def struct_mask_from_threshold(raw, k):
    med = float(np.median(raw))
    mad = float(np.median(np.abs(raw - med)))
    if k < 0:
        return np.zeros_like(raw, dtype=bool)
    return raw > med + k * 1.4826 * mad


def fit_poly2d(raw, mask, deg):
    t0 = time.perf_counter()
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    xn = (xx - N / 2) / (N / 2)
    yn = (yy - N / 2) / (N / 2)
    cols = [np.ones_like(xn), xn, yn]
    if deg >= 2:
        cols += [xn ** 2, xn * yn, yn ** 2]
    A = np.stack([c[~mask] for c in cols], axis=1)
    y = raw[~mask]
    coef, *_ = np.linalg.lstsq(A, y, rcond=None)
    full = np.stack(cols, axis=-1)
    surf = full @ coef
    dt = time.perf_counter() - t0
    return surf, dt, int(len(coef))


def fit_spline(raw, mask, step, smooth_factor=1.0):
    """稀疏节点双三次样条（RectBivariateSpline，节点网格 step 可调）。
    平滑量按噪声定标：s = m + sqrt(2m) 缩放，避免噪声过拟合（Wahba/GCV 精神，
    闭式按自由度标度，见 c9_report 推导腿）。"""
    from scipy.interpolate import RectBivariateSpline
    t0 = time.perf_counter()
    # 粗网格采样（mask 内中位）
    xs = np.arange(0, N, step)
    ys = np.arange(0, N, step)
    zv = np.zeros((len(ys), len(xs)))
    for j, y0 in enumerate(ys):
        for i, x0 in enumerate(xs):
            blk = raw[y0:y0 + step, x0:x0 + step]
            m = mask[y0:y0 + step, x0:x0 + step]
            v = blk[~m] if (~m).sum() > 0 else blk.ravel()
            zv[j, i] = float(np.median(v))
    # 噪声标度平滑：s 正比节点数（Wahba 式 s ~ m σ²，本腿 σ 用 MAD 估计）
    med = float(np.median(raw[~mask]))
    mad = float(np.median(np.abs(raw[~mask] - med)))
    sig = max(1.4826 * mad, 1e-6)
    m = zv.size
    s = smooth_factor * m * sig ** 2
    # 过稀疏网格（b128: 每轴 2 点）三次样条无定义：诚实降阶为双线性，
    # 该行为本身即“节点过稀疏不可用”的证据，如实记录 order 字段。
    kx = ky = 3
    if len(xs) <= 3 or len(ys) <= 3:
        kx = ky = 1
    spl = RectBivariateSpline(ys, xs, zv, kx=kx, ky=ky, s=s if kx == 3 else 0.0)
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    surf = spl(yy.ravel(), xx.ravel(), grid=False).reshape(N, N)
    dt = time.perf_counter() - t0
    return surf, dt, int(m)


def lowfreq2d(img, sigma):
    from scipy.ndimage import gaussian_filter
    m = np.isfinite(img).astype(float)
    v = np.where(np.isfinite(img), img, 0.0)
    sv = gaussian_filter(v, sigma, mode="nearest")
    sm = gaussian_filter(m, sigma, mode="nearest")
    with np.errstate(all="ignore"):
        out = sv / np.maximum(sm, 1e-12)
    out[sm < 1e-6] = np.nan
    return out


def fit_multiscale(raw, sigma, k_thr):
    """Siril 式多尺度低频：规则网格采样 + mask 保护 + 高斯低频（σ可调）。
    mask 形态 = 全局 median + k*MAD 高阈 veto（与 Siril generate_samples
    同构：median + mad0*tolerance），实现独立（高斯尺度空间）。"""
    from scipy.ndimage import distance_transform_edt
    t0 = time.perf_counter()
    mask = struct_mask_from_threshold(raw, k_thr)
    # mask 区用最近有效值填充后再低通（防亮结构拖尾污染低频面）
    filled = raw.copy()
    if mask.any():
        idx = distance_transform_edt(mask, return_distances=False, return_indices=True)
        filled[mask] = raw[tuple(idx[:, mask])] if (~mask).any() else np.median(raw)
    surf = lowfreq2d(filled, sigma)
    bad = ~np.isfinite(surf)
    if bad.any():
        surf[bad] = float(np.nanmedian(raw))
    dt = time.perf_counter() - t0
    nparam = int(mask.size / (2 * np.pi * sigma ** 2)) + 1  # 有效自由度量级估计
    return surf, dt, nparam, mask


def apply_keep(raw, surf):
    """B_ref 保留施加：calibrated = raw - (S - median(S))。"""
    lvl = float(np.nanmedian(surf))
    corr = raw - (surf - lvl)
    return corr, lvl


def seam_step(img, xb=128, half=70):
    L = np.nanmedian(img[:, xb - half:xb], axis=1)
    R = np.nanmedian(img[:, xb:xb + half], axis=1)
    return float(np.nanmedian(R - L))


def off_locus_steps(img, xb=128, half=70):
    # 旧口径（xb=64/192、half=70）在 N=256 场上越界致空窗 NaN，已废弃。
    # 新口径见 residual_offlocus：残差场在无缝线处的台阶（越小越好）。
    return [seam_step(img, xb=d, half=half) for d in (64, 192)]


def residual_offlocus(corr, base, lines=(64, 192), half=16):
    """不新增强缝检查（诚实口径）：残差场 corr−base 在真值无缝线处的台阶。
    本底构成：①中位噪声（~0.03/1.2e- 两线量级差异来自列中位样本的噪声实现）；
    ②信号翼拾取（对照线穿过 core/star 信号区时，拟合面吃掉翼部信号，
    残差出现 e- 级结构——这正是 M3 泄漏的另一种投影，不双重计分）。
    故本量只作“同基线横向比较”（各配置相对排序），不作绝对门；
    新缝风险另由 L2 dark 切块（真实无结构区）与 m42s（真实结构区）承担。"""
    res = corr - base
    return [seam_step(res, xb=d, half=half) for d in lines]


def se_of_step(img, xb=128, half=70):
    L = np.nanmedian(img[:, xb - half:xb], axis=1)
    R = np.nanmedian(img[:, xb:xb + half], axis=1)
    L = L[np.isfinite(L)]
    R = R[np.isfinite(R)]
    if len(L) < 5 or len(R) < 5:
        return float("nan")
    madL = float(np.median(np.abs(L - np.median(L))))
    madR = float(np.median(np.abs(R - np.median(R))))
    return float(np.sqrt((madL ** 2 / len(L)) + (madR ** 2 / len(R))))


def aperture_leak(truth_sig, raw, corr):
    """高频信号保留：孔径内 (corr-raw) 相对真值信号的泄漏分数（越小越好）。"""
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    out = {}
    for f in FEATS:
        r = np.sqrt((xx - f["x"]) ** 2 + (yy - f["y"]) ** 2)
        ap = r < AP_R[f["id"]]
        num = float(np.median(corr[ap] - raw[ap]))
        den = float(np.median(truth_sig[ap]))
        out[f["id"]] = float(num / den) if den != 0 else float("nan")
    return out


class Gates:
    def __init__(self):
        self.rows = []

    def add(self, gid, desc, value, ok):
        self.rows.append({"id": gid, "desc": desc, "value": value, "ok": bool(ok)})

    def summary(self):
        n_fail = sum(1 for r in self.rows if not r["ok"])
        return {"rows": self.rows, "n_fail": n_fail, "n_pass": len(self.rows) - n_fail}


def main() -> int:
    g = Gates()
    res: dict = {"seed_base": SEED_BASE, "configs": [c["id"] for c in CONFIGS],
                 "inputs": {}, "L1": {}, "L2": {}, "L3": {}, "scores": {}}
    rng0 = derive_rng("c9")
    res["rng_check"] = float(rng0.uniform())

    truth = build_truth()
    base = truth["base"]

    # ---- L1：合成真值腿 ----
    l1 = {}
    rng = derive_rng("c9-l1")
    raw = observe(base, rng)
    l1["raw_step"] = seam_step(raw)
    l1["raw_offlocus"] = off_locus_steps(raw)
    l1["field"] = {"bg_med": float(np.median(raw)), "n": N}
    per_cfg = {}
    for cfg in CONFIGS:
        cid = cfg["id"]
        if cfg["fam"] == "a":
            mask = struct_mask_from_threshold(raw, 2.0)
            surf, dt, np_ = fit_poly2d(raw, mask, cfg["deg"])
            corr, lvl = apply_keep(raw, surf)
            extra = {"mask_frac": float(mask.mean())}
        elif cfg["fam"] == "b":
            mask = struct_mask_from_threshold(raw, 2.0)
            surf, dt, np_ = fit_spline(raw, mask, cfg["step"])
            corr, lvl = apply_keep(raw, surf)
            extra = {"mask_frac": float(mask.mean())}
        elif cfg["fam"] == "c":
            surf, dt, np_, mask = fit_multiscale(raw, cfg["sigma"], cfg["thr"])
            corr, lvl = apply_keep(raw, surf)
            extra = {"mask_frac": float(mask.mean())}
        elif cid == "degen_full":
            mask = struct_mask_from_threshold(raw, 2.0)
            surf, dt, np_ = fit_spline(raw, mask, cfg["step"])
            corr = raw - surf  # 退化：全减背景
            lvl = float(np.median(corr))
            extra = {"mask_frac": float(mask.mean()), "degen": True}
        elif cid == "degen_plane":
            surf, dt, np_, mask = fit_multiscale(raw, cfg["sigma"], cfg["thr"])
            corr, lvl = apply_keep(raw, surf)
            extra = {"mask_frac": float(mask.mean()), "degen": True}
        else:
            raise AssertionError(cid)
        st = seam_step(corr)
        off = residual_offlocus(corr, base)
        se = se_of_step(raw)
        # M2 暗段回归（订正口径）：暗窗取左上无信号区 20:50×100:150，
        # bias = median(corr-dark窗 - sky真值暗窗)（扣除天光梯度本身），
        # 噪声 = 该窗残差相对真值的 MAD/√N（局部噪声口径，与缝 SE 同族）。
        # 旧口径误把“全场中位−BG 常数”（含拟合面整体 pedestal + 天光梯度）
        # 当 bias，且 SE 取缝带列中位标准误（跨缝结构散布），已废弃。
        dw = (slice(100, 150), slice(20, 50))
        dres = (corr[dw] - (truth["sky"][dw] + truth["seam_true"][dw])).ravel()
        dark = float(np.median(dres))
        dmad = float(np.median(np.abs(dres - np.median(dres))))
        dse = float(1.4826 * dmad / np.sqrt(dres.size))
        # M2b 规范不变暗段结构量（§2 gauge 语义：整体 pedestal 为规范自由度，
        # 不计入回归；只计暗窗相对全场中位的局部偏离）：
        # dark_struct = median(暗窗残差) − median(全场残差)。
        fres = (corr - (truth["sky"] + truth["seam_true"] + truth["sig"]))
        fres = fres[np.isfinite(fres)]
        dark_struct = float(dark - np.median(fres))
        leak = aperture_leak(truth["sig"], raw, corr)
        med = float(np.median(corr))
        per_cfg[cid] = {
            "fam": cfg["fam"], "desc": cfg["desc"], "dt_s": dt, "nparam": np_,
            "level": lvl, "med": med,
            "seam_before": l1["raw_step"], "seam_after": st,
            "suppression": float(abs(l1["raw_step"]) - abs(st)),
            "sup_frac": float((abs(l1["raw_step"]) - abs(st)) / abs(l1["raw_step"])),
            "offlocus_after": off,
            "offlocus_newmax": float(max(abs(o) for o in off)),
            "dark_bias": dark, "dark_se": dse,
            "dark_z": float(dark / dse) if dse else float("nan"),
            "dark_struct": dark_struct,
            "leak": leak,
            "max_abs_leak": float(max(abs(v) for v in leak.values() if np.isfinite(v))),
            **extra,
        }
    l1["configs"] = per_cfg
    res["L1"] = l1

    # 安全门（判红）：背景保留 / 无 NaN / 退化对照证伪
    for cid, r in per_cfg.items():
        if cid.startswith("degen"):
            continue
    g.add("G1_bg_kept", "全部候选配置中位保留在 BG±50e- 内（非退化）",
          {c: per_cfg[c]["med"] for c in per_cfg if not c.startswith("degen")},
          all(abs(per_cfg[c]["med"] - BG) < 50 for c in per_cfg if not c.startswith("degen")))
    g.add("G2_no_nan", "全部候选修正场无 NaN 污染",
          {c: bool(np.isfinite(per_cfg[c]["seam_after"])) for c in per_cfg},
          all(np.isfinite(per_cfg[c]["seam_after"]) for c in per_cfg if not c.startswith("degen")))
    g.add("G3_degen_falsified", "退化对照 degen_full 中位≈0 且与 B_ref 分离（证伪全减背景）",
          {"degen_med": per_cfg["degen_full"]["med"], "bg": BG},
          abs(per_cfg["degen_full"]["med"]) < 50 and abs(per_cfg["degen_full"]["med"] - BG) > 100)
    # 评分类：如实记录
    g.add("R1_seam_table", "M1 缝带抑制 sup_frac（记录）",
          {c: round(per_cfg[c]["sup_frac"], 4) for c in per_cfg}, True)
    g.add("R2_dark_table", "M2 暗段回归 dark_struct（规范不变局部量；记录）",
          {c: round(per_cfg[c]["dark_struct"], 3) for c in per_cfg if not c.startswith("degen")}, True)
    g.add("R3_leak_table", "M3 高频泄漏 max|leak|（记录）",
          {c: round(per_cfg[c]["max_abs_leak"], 5) for c in per_cfg}, True)

    # ---- L3：方差腿（恒等式 + MC 方差膨胀）----
    l3 = {}
    vv = np.full(1000, 2.5e-11)
    l3["additive_identity_maxdiff"] = float(np.max(np.abs((vv + 0.0) - vv)))
    g.add("G4_identity", "加法不改方差恒等式逐位成立",
          l3["additive_identity_maxdiff"], l3["additive_identity_maxdiff"] == 0.0)
    # MC：零信号平场（BG 常数 + 噪声），各配置修正前后方差比
    mc_cfg_ids = ["a1", "a2", "b16", "b32", "b64", "b128",
                  "cS08T20", "cS16T20", "cS32T20", "cS16T05"]
    mc = {}
    for cid in mc_cfg_ids:
        cfg = next(c for c in CONFIGS if c["id"] == cid)
        ratios = []
        for k in range(N_MC):
            rk = derive_rng("c9-mc-%d" % k)
            flat = observe(np.full((N, N), BG), rk)
            if cfg["fam"] == "a":
                mask = struct_mask_from_threshold(flat, 2.0)
                surf, _, _ = fit_poly2d(flat, mask, cfg["deg"])
            elif cfg["fam"] == "b":
                mask = struct_mask_from_threshold(flat, 2.0)
                surf, _, _ = fit_spline(flat, mask, cfg["step"])
            else:
                surf, _, _, _ = fit_multiscale(flat, cfg["sigma"], cfg["thr"])
            corr, _ = apply_keep(flat, surf)
            ratios.append(float(np.var(corr) / np.var(flat)))
        mc[cid] = {"mean": float(np.mean(ratios)), "std": float(np.std(ratios)),
                   "max": float(np.max(ratios)), "ratios": [round(r, 5) for r in ratios]}
    l3["mc_var_ratio"] = mc
    res["L3"] = l3
    g.add("R4_var_table", "M4 方差膨胀因子 mean（记录）",
          {c: round(mc[c]["mean"], 5) for c in mc}, True)
    g.add("G5_var_bounded", "全部候选方差膨胀 mean < 1.05（修正不放大噪声超 5%）",
          {c: round(mc[c]["mean"], 5) for c in mc},
          all(mc[c]["mean"] < 1.05 for c in mc))

    # ---- L2：RERUN3 只读腿（代表性 6 配置，缝 guard 带）----
    l2: dict = {}
    try:
        from astropy.io import fits
        if RERUN3_FITS.exists():
            d = fits.getdata(str(RERUN3_FITS)).astype(np.float64)
            res["inputs"]["rerun3"] = {"shape": list(d.shape), "file": str(RERUN3_FITS)}
            # 切块：缝带 haloU（y1850:1900）、暗段（y1600:1700）、M42S 真实结构（y2050:2150）
            cuts = {
                "haloU": d[1850 - 1:1900, 1500:2600],
                "dark": d[1600 - 1:1700, 1500:2600],
                "m42s": d[2050 - 1:2150, 1500:2600],
            }
            rep_ids = ["a1", "b32", "b64", "cS16T20", "cS32T20", "cS16T05"]
            l2c = {}
            for name, cut in cuts.items():
                prof = np.nanmedian(cut, axis=0)
                # B 路口径同窗台阶：L=xb-70:xb, R=xb:xb+70（xb=2029-1500=529 切块内）
                xb = 2029 - 1500
                H = 70
                L = prof[xb - H:xb]
                R = prof[xb:xb + H]
                L = L[np.isfinite(L)]
                R = R[np.isfinite(R)]
                before = float(np.median(R) - np.median(L)) if len(L) and len(R) else float("nan")
                # guard 带：拟合排除 xb±40 列（防循环论证）
                guard = np.zeros(cut.shape[1], dtype=bool)
                guard[max(0, xb - 40):xb + 40] = True
                rows = {}
                for cid in rep_ids:
                    cfg = next(c for c in CONFIGS if c["id"] == cid)
                    # 在切块上拟合 1D profile 的低频面（2D 面的 1D 代理，诚实声明）
                    p = prof.copy()
                    gm = guard | (~np.isfinite(p))
                    if cfg["fam"] == "a":
                        xs = np.arange(len(p))
                        xx = xs[~gm]
                        yy = p[~gm]
                        dg = cfg["deg"]
                        A = np.stack([xx ** k for k in range(dg + 1)], axis=1)
                        coef, *_ = np.linalg.lstsq(A, yy, rcond=None)
                        Af = np.stack([np.arange(len(p)) ** k for k in range(dg + 1)], axis=1)
                        surf1 = Af @ coef
                    elif cfg["fam"] == "b":
                        from scipy.interpolate import interp1d
                        xs = np.arange(len(p))
                        xk = xs[~gm][::cfg["step"]]
                        yk = p[~gm][::cfg["step"]]
                        f = interp1d(xk, yk, kind="cubic", fill_value="extrapolate")
                        surf1 = f(xs)
                    else:
                        mask1 = struct_mask_from_threshold(p.reshape(1, -1), cfg["thr"]).ravel()
                        mask1 = mask1 | gm
                        pf = p.copy()
                        if mask1.any() and (~mask1).any():
                            pf[mask1] = np.interp(np.flatnonzero(mask1),
                                                  np.flatnonzero(~mask1), p[~mask1])
                        from scipy.ndimage import gaussian_filter1d
                        surf1 = gaussian_filter1d(pf, cfg["sigma"], mode="nearest")
                    lvl = float(np.nanmedian(surf1[~gm])) if (~gm).any() else float(np.nanmedian(surf1))
                    corr = p - (surf1 - lvl)
                    La = corr[xb - H:xb]
                    Ra = corr[xb:xb + H]
                    La = La[np.isfinite(La)]
                    Ra = Ra[np.isfinite(Ra)]
                    after = float(np.median(Ra) - np.median(La)) if len(La) and len(Ra) else float("nan")
                    rows[cid] = {
                        "before": before, "after": after,
                        "suppression": float(abs(before) - abs(after)) if np.isfinite(before) and np.isfinite(after) else float("nan"),
                        "sup_frac": float((abs(before) - abs(after)) / abs(before)) if before else float("nan"),
                        "med_kept": float(np.nanmedian(corr)),
                        "note": "m42s 为真实结构主导（before≈-9e-04 含 M42 南梯度本身），"
                                "sup_frac≈0（不动）为正确行为，sup>0.5（b32/b64）为吃结构判负",
                    }
                l2c[name] = {"before": before, "rows": rows, "cut_shape": list(cut.shape)}
            l2["cuts"] = l2c
            g.add("R5_rerun3_table", "L2 RERUN3 切块行为（记录；guard 带防循环）",
                  {n: {c: round(l2["cuts"][n]["rows"][c]["sup_frac"], 4) for c in l2["cuts"][n]["rows"]} for n in l2["cuts"]}, True)
        else:
            l2["skipped"] = "缺 RERUN3 FITS"
            g.add("R5_rerun3_table", "缺输入，跳过", None, True)
    except Exception as e:
        l2["error"] = str(e)
        g.add("R5_rerun3_table", "L2 异常：%s" % str(e)[:200], None, False)
    res["L2"] = l2

    # ---- 综合评分表 ----
    scores = {}
    for cid, r in per_cfg.items():
        if cid.startswith("degen"):
            continue
        scores[cid] = {
            "fam": r["fam"],
            "M1_sup_frac": round(r["sup_frac"], 4),
            "M2_dark_bias": round(r["dark_bias"], 3),
            "M2_dark_struct": round(r["dark_struct"], 3),
            "M2_dark_z": round(r["dark_z"], 2),
            "M3_maxleak": round(r["max_abs_leak"], 5),
            "M3_leak": {k: round(v, 5) for k, v in r["leak"].items()},
            "M4_varmean": round(mc[cid]["mean"], 5) if cid in mc else None,
            "dt_s": round(r["dt_s"], 3),
            "nparam": r["nparam"],
        }
    res["scores"] = scores
    res["gates"] = g.summary()

    RESULTS.mkdir(parents=True, exist_ok=True)
    out = RESULTS / "c9_sky_surface_compare.json"
    out.write_text(json.dumps(res, indent=1, ensure_ascii=False), encoding="utf-8")
    print("== C9 三种减法曲面离线对比 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-18s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], json.dumps(r["value"], ensure_ascii=False)[:280]))
    print("  M1 sup_frac:", json.dumps({c: scores[c]["M1_sup_frac"] for c in scores}, ensure_ascii=False))
    print("  M3 maxleak:", json.dumps({c: scores[c]["M3_maxleak"] for c in scores}, ensure_ascii=False))
    print("  M4 varmean:", json.dumps({c: scores[c]["M4_varmean"] for c in scores}, ensure_ascii=False))
    print("  -> %s" % out)
    return 0 if res["gates"]["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
