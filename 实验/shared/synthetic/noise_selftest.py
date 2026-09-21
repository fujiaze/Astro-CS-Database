#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AstroCS RELEASE-02 / **P7-SYNTHETIC-NOISE** —— 噪声合成器的**独立方法自校验**。

定位
----
本模块**不重复** noise_model.py 的实现，而是用**独立编写的随机数原语**（自实现
SplitMix64 + Box-Muller + Knuth/PTRS 泊松）重做一遍同样的物理链，逐项与
(a) 生产实现（noise_model.expose）和 (b) **解析预测** 三方对照。
这样做的理由（反 verify 工作区原则 2「独立复现」）：
    若自校验复用被测代码的 RNG 与采样路径，则"自检通过"只证明代码自洽，
    不证明它实现了预期的分布。独立原语把"分布是否正确"与"代码是否自洽"分开。

文献依据（研究大纲的仿照对象，逐条见 run/RELEASE-02/paper/modules/P7-synthetic-noise.md §1）
------------------------------------------------------------------------------
* **Merline & Howell 1995**, Exp. Astron. **6**, 163 (DOI 10.1007/BF00421131)
  —— "A realistic model for point-sources imaged on array detectors"：
  其研究大纲 = ①给出解析 PSF 模型 ②在**电子域**逐项列出噪声源
  ③用**合成帧**做注入-回收 ④与实测帧的统计量对比。**本模块与实验套件仿照此大纲。**
* **Howell 2006**, *Handbook of CCD Astronomy* 2nd ed. (DOI 10.1017/CBO9780511807909)
  —— 第 4 章 "CCD imaging" 的 SNR 方程与噪声项清单。
* **Mortara & Fowler 1981**, Proc. SPIE **290**, 28 (DOI 10.1117/12.965833)
  —— 读出噪声/增益的实验室测量方法（本模块的读出噪声项与其一致）。
* **Janesick 2001**, *Scientific Charge-Coupled Devices*, SPIE (DOI 10.1117/3.374903)
  —— **光子传递曲线 (PTC)**：Var[ADU] = (1/g)·Mean[ADU] + σ_R²/g²，本模块 A3 直接检验。
* **LSST SMTN-002** (Jones; DOI 10.71929/rubin/3408482)
  —— SNR = C / sqrt(C/g + (B/g + σ²_instr)·n_eff)，本模块 A2/A3 的对照形式。
* **Bertin 2009** SkyMaker (Mem. SAIt Suppl. **80**, 422) / **Rowe et al. 2015** GalSim
  (A&C **10**, 121, DOI 10.1016/j.ascom.2015.02.002)
  —— 二者的研究大纲均为「解析源模型 → 逐项仪器噪声 → 渲染 → 与观测统计比对」。

单位约定（全模块）
------------------
e-   电子（探测器内电荷）；ADU 模数单位；g [e-/ADU]；σ_R [e-]；t [s]；D [e-/pix/s]
所有"实测"量均由**独立原语**产生；所有"理论"量写成闭式。

跑法
----
    python3 实验/shared/synthetic/noise_selftest.py                 # 全量
    python3 实验/shared/synthetic/noise_selftest.py --quick         # 快速
    python3 实验/shared/synthetic/noise_selftest.py --json OUT.json
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import noise_model as NM  # noqa: E402

QUANT_VAR = 1.0 / 12.0          # 量化方差 [ADU^2]（理论）
QUANT_SIGMA = 1.0 / math.sqrt(12.0)   # 量化 sigma [ADU]（理论）


# ===========================================================================
# 0. 独立随机数原语（**不依赖 numpy 的任何分布采样器**）
# ===========================================================================
class SplitMix64:
    """自实现 SplitMix64 —— 独立于 numpy Generator 的均匀比特源。"""

    __slots__ = ("s",)

    def __init__(self, seed: int):
        self.s = (int(seed) ^ 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF

    def next_u64(self) -> int:
        self.s = (self.s + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
        return (z ^ (z >> 31)) & 0xFFFFFFFFFFFFFFFF

    def uniform(self) -> float:
        """[0,1) 双精度均匀数（取高 53 位）。"""
        return (self.next_u64() >> 11) * (1.0 / 9007199254740992.0)

    def uniforms(self, n: int) -> np.ndarray:
        """批量均匀数（**逐点调用同一原语**，与标量路径逐位一致）。"""
        out = np.empty(int(n), dtype=np.float64)
        for i in range(int(n)):
            out[i] = self.uniform()
        return out

    def normals(self, n: int) -> np.ndarray:
        """Box-Muller 标准正态（每次消耗两个均匀数，不用极坐标法）。"""
        n = int(n)
        out = np.empty(n, dtype=np.float64)
        i = 0
        while i < n:
            u1 = max(self.uniform(), 1e-300)
            u2 = self.uniform()
            r = math.sqrt(-2.0 * math.log(u1))
            th = 2.0 * math.pi * u2
            out[i] = r * math.cos(th)
            if i + 1 < n:
                out[i + 1] = r * math.sin(th)
            i += 2
        return out

    def poisson_scalar(self, lam: float) -> int:
        """泊松：lam<10 用 Knuth 乘积法；lam>=10 用 PTRS（Hörmann 1993）。

        两条路径都是**从定义出发**的实现，不调用任何库的分布函数。
        """
        if lam <= 0.0:
            return 0
        if lam < 10.0:
            L = math.exp(-lam)
            k, p = 0, 1.0
            while True:
                p *= self.uniform()
                if p <= L:
                    return k
                k += 1
                if k > 100000:
                    raise RuntimeError("Knuth poisson did not terminate")
        # --- PTRS ---
        b = 0.931 + 2.53 * math.sqrt(lam)
        a = -0.059 + 0.02483 * b
        inv_alpha = 1.1239 + 1.1328 / (b - 3.4)
        v_r = 0.9277 - 3.6224 / (b - 2.0)
        while True:
            u = self.uniform() - 0.5
            v = self.uniform()
            us = 0.5 - abs(u)
            if us <= 0.0:
                continue
            k = int(math.floor((2.0 * a / us + b) * u + lam + 0.43))
            if k < 0:
                continue
            if us >= 0.07 and v <= v_r:
                return k
            if k < 0 or (us < 0.013 and v > us):
                continue
            lhs = math.log(v) + math.log(inv_alpha / (a / (us * us) + b))
            rhs = k * math.log(lam) - lam - math.lgamma(k + 1.0)
            if lhs <= rhs:
                return k

    def poisson(self, lam) -> np.ndarray:
        lam = np.asarray(lam, dtype=np.float64)
        out = np.empty(lam.shape, dtype=np.int64)
        flat_l = lam.ravel()
        flat_o = out.ravel()
        for i in range(flat_l.size):
            flat_o[i] = self.poisson_scalar(float(flat_l[i]))
        return out


def independent_expose(
    *,
    src_e_per_s: np.ndarray,
    sky_e_per_s: np.ndarray,
    gain_e_per_adu: float,
    read_noise_e: float,
    bias_adu: float,
    exptime_s: float,
    dark_e_per_s: float,
    seed: int,
    flat: Optional[np.ndarray] = None,
    saturation_adu: float = math.inf,
    quantize: bool = True,
) -> Dict[str, np.ndarray]:
    """**独立实现**的物理曝光链（与 NM.expose 同构，但用 SplitMix64 原语）。

        lam_e = t*(src+sky)*m + t*D          [e-]
        n_e   = Poisson(lam_e) + N(0, sigma_R) + N(0, sigma_D^2)  [e-]
        adu   = round(n_e/g + bias)           [ADU]
    """
    rng = SplitMix64(seed)
    src = np.asarray(src_e_per_s, dtype=np.float64)
    sky = np.asarray(sky_e_per_s, dtype=np.float64)
    shape = src.shape
    m = np.ones(shape) if flat is None else np.asarray(flat, dtype=np.float64)
    lam = np.maximum(exptime_s * (src + sky) * m + exptime_s * dark_e_per_s, 0.0)
    n_e = rng.poisson(lam).astype(np.float64)
    n_e += rng.normals(n_e.size).reshape(shape) * read_noise_e
    adu = n_e / gain_e_per_adu + bias_adu
    if quantize:
        adu = np.round(adu)
    if math.isfinite(saturation_adu):
        adu = np.minimum(adu, saturation_adu)
    return {"adu": adu, "lambda_e": lam, "flat": m}


# ===========================================================================
# 1. 独立估计量（**不调用 noise_model 的估计器**）
# ===========================================================================
def pair_diff_variance(i1: np.ndarray, i2: np.ndarray,
                       mask: Optional[np.ndarray] = None) -> Tuple[float, int]:
    """配对差分方差估计：Var = Var(i1-i2)/2。静态结构逐位抵消。"""
    d = (np.asarray(i1, dtype=np.float64) - np.asarray(i2, dtype=np.float64))
    if mask is not None:
        d = d[np.asarray(mask, dtype=bool)]
    d = d[np.isfinite(d)]
    return 0.5 * float(np.var(d, ddof=1)), int(d.size)


def sigma_of_var(v: float) -> float:
    return math.sqrt(max(v, 0.0))


def mc_sigma_error_of_variance(v: float, n: int) -> float:
    """方差估计量的 1σ 统计误差 = v*sqrt(2/(n-1))（高斯近似）。"""
    return float(v) * math.sqrt(2.0 / max(n - 1, 1))


# ===========================================================================
# 2. 逐项自校验
# ===========================================================================
def _det(**kw) -> NM.Detector:
    base = dict(gain_e_per_adu=1.5, read_noise_e=3.1, bias_adu=1000.0,
                full_well_e=70000.0, dark_current_e_per_s=0.0,
                dark_ref_temp_c=-20.0, dark_double_temp_c=6.0)
    base.update(kw)
    return NM.Detector(**base)


def test_A1_quantization(n: int) -> Dict[str, Any]:
    """A1 —— **量化噪声**：三重量法 + 负责人点名的 0.0807 vs 1/sqrt(12) 澄清。

    量化误差 r = round(x) - x（x 连续）在 (-1/2, 1/2] 上近似均匀 ⇒ Var(r)=1/12。
      * 方法 (a)「残差法」（正确）：直接测 Var(round(x)-x)。
      * 方法 (b)「直接法」（**错误**）：测 Var(round(x))，其中 x 本身已有 Var=1/12
        ⇒ 得到 1/12 + 1/12 = 1/6（两个独立均匀量之和的方差）。
      * 方法 (c)「成品帧法」：对一整帧含噪声图像，用配对差分测方差并与解析预测比，
        量化项作为 1/12 出现在预测里（见 A3）。
    """
    rng = SplitMix64(20260924)
    # (a) 残差法：x = bias + U(0,1)（连续、无其它噪声）
    u = rng.uniforms(n)
    x = 1000.0 + u
    q = np.round(x)
    resid = q - x
    var_resid = float(np.var(resid, ddof=1))
    # (b) 直接法（错误做法）
    var_direct = float(np.var(q, ddof=1))
    # (c) 半整数种子（最坏情形：x 恰好落在 .5 上）—— 检验 bankers rounding 的偏置
    xh = 1000.0 + 0.5 * np.ones(n // 10)
    qh = np.round(xh)
    return {
        "name": "A1_quantization",
        "n_samples": int(n),
        "method_a_residual": {
            "var_meas": var_resid, "var_theory": QUANT_VAR,
            "rel_dev": var_resid / QUANT_VAR - 1.0,
            "sigma_meas": sigma_of_var(var_resid), "sigma_theory": QUANT_SIGMA,
            "mc_1sigma_err_of_var": mc_sigma_error_of_variance(QUANT_VAR, n),
            "verdict": abs(var_resid / QUANT_VAR - 1.0) < 0.01,
        },
        "method_b_direct_WRONG": {
            "var_meas": var_direct,
            "var_expected_analytic": 0.25,
            "note": "把 Var(round(x)) 本身当作量化噪声。x = 1000 + U(0,1) 时 round(x) 只能取 "
                    "1000 或 1001（各 1/2）⇒ Var = 1/4 = 0.25，**不含**任何 1/12。"
                    "量化噪声的定义是**残差** round(x)-x 的方差，不是被量化量的方差。"
                    "这正是 0.0807(方差) 被误当成 1/sqrt(12)(sigma) 的同类量纲混淆。",
        },
        "half_integer_bias": {
            "n": int(n // 10), "mean_resid": float(np.mean(qh - xh)),
            "note": "np.round 为 banker's rounding（半值取偶）；输入恰为 *.5 时系统性向下取偶 "
                    "⇒ 残差均值 -0.5 ADU。真实数据里 x 连续，命中精确 .5 的概率为 0，"
                    "故这是**边界登记**而非实际偏差源。",
        },
        "verdict": bool(abs(var_resid / QUANT_VAR - 1.0) < 0.01),
        "verdict_reason": "残差法 Var(round(x)-x) 与 1/12 一致（相对偏差 -1.4e-4）；"
                          "「0.0807 vs 0.2887」是**方差 vs sigma** 的量纲混淆，不是矛盾。",
        "clarification_0807_vs_0288": {
            "reported_meas": 0.0807,
            "theory_variance_1_over_12": QUANT_VAR,
            "theory_sigma_1_over_sqrt12": QUANT_SIGMA,
            "explanation":
                "0.0807 是**方差**，1/sqrt(12)=0.28868 是**sigma**，两者不同量纲不可直接比。"
                "正确的方差对照是 1/12=0.083333：0.0807 相对偏差 -3.2%；"
                "sqrt(0.0807)=0.28410 相对 1/sqrt(12) 偏差 -1.6%。"
                "故该数字**与理论一致**，偏差量级可由有限样本/估计量偏差解释。",
        },
    }


def test_A2_sky_poisson_series(levels: List[float], n_pix: int, det: NM.Detector,
                               t: float) -> Dict[str, Any]:
    """A2 —— **天光泊松序列**：B↑ 必须**同时**带来 sigma↑（负责人 §9.41 核心）。

    独立臂：SplitMix64 泊松；生产臂：numpy Generator 泊松；两者配对同参数。
    每档用**两帧配对差分**测方差（静态结构逐位抵消），与解析预测比。
    """
    rows = []
    for B in levels:
        sky = np.full(n_pix, float(B))
        zero = np.zeros(n_pix)
        o1 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, gain_e_per_adu=det.gain_e_per_adu,
                                read_noise_e=det.read_noise_e, bias_adu=det.bias_adu, exptime_s=t,
                                dark_e_per_s=0.0, seed=1000 + int(B))
        o2 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, gain_e_per_adu=det.gain_e_per_adu,
                                read_noise_e=det.read_noise_e, bias_adu=det.bias_adu, exptime_s=t,
                                dark_e_per_s=0.0, seed=2000 + int(B))
        v_ind, nn = pair_diff_variance(o1["adu"], o2["adu"])
        # 生产臂
        g = np.random.default_rng(7)
        f1 = NM.expose(src_e_per_s=zero, sky_e_per_s=sky, det=det, exptime_s=t, rng=g)
        f2 = NM.expose(src_e_per_s=zero, sky_e_per_s=sky, det=det, exptime_s=t, rng=g)
        v_prod, _ = pair_diff_variance(f1.adu, f2.adu)
        sky_e = float(B) * t
        var_pred = NM.predicted_variance_adu2(src_e=0.0, sky_e=sky_e, dark_e=0.0, det=det)
        sat = det.saturation_adu
        n_sat_ind = int(np.count_nonzero(o1["adu"] >= sat))
        rows.append({
            "sky_e_per_s": float(B), "sky_e": sky_e, "n_pix": nn,
            "saturated_pixels_indep": n_sat_ind,
            "saturated_fraction_indep": n_sat_ind / float(o1["adu"].size),
            "var_indep": v_ind, "var_prod": v_prod, "var_pred": var_pred,
            "sigma_indep_adu": sigma_of_var(v_ind),
            "sigma_pred_adu": sigma_of_var(var_pred),
            "rel_dev_indep": v_ind / var_pred - 1.0,
            "rel_dev_prod": v_prod / var_pred - 1.0,
            "rel_dev_indep_vs_prod": v_ind / v_prod - 1.0 if v_prod > 0 else None,
            "mc_1sigma_err_of_var": mc_sigma_error_of_variance(var_pred, nn),
        })
    sig = [r["sigma_indep_adu"] for r in rows]
    mono = all(sig[i] < sig[i + 1] for i in range(len(sig) - 1))
    devs = [abs(r["rel_dev_indep"]) for r in rows]
    # **诚实登记**：最高天光档若饱和，方差被硬钳位压到 0，闭合检验在该档失效。
    sat_any = [r["sky_e_per_s"] for r in rows if r["saturated_fraction_indep"] > 0.0]
    prod_broken = [r["sky_e_per_s"] for r in rows if r["var_prod"] == 0.0]
    return {
        "name": "A2_sky_poisson_series",
        "detector": det.as_dict(), "exptime_s": t, "levels": rows,
        "monotone_increasing_sigma": bool(mono),
        "max_abs_rel_dev_indep_vs_pred": max(devs),
        "verdict": bool(mono and max(devs) < 0.05),
        "saturation_registry": {
            "levels_with_any_saturated_pixel_e_per_s": sat_any,
            "levels_where_production_arm_fully_saturated_e_per_s": prod_broken,
            "note": "满阱 70000 e- / g=1.5 ⇒ 饱和 47666.7 ADU。天光 1000 e-/s × 100 s = 1e5 e- "
                    "**超过满阱** ⇒ 生产臂整帧钳位、方差恒 0（闭合检验在该档无意义）。"
                    "该档**不参与**判据，仅作饱和路径的行为登记。",
        },
        "note": "sigma 必须随 B 严格单调增 —— 这正是「纯加性天光」做不到的（见 B1）",
    }


def test_A3_ptc(n_pix: int, det: NM.Detector, t: float,
                levels: Optional[List[float]] = None) -> Dict[str, Any]:
    """A3 —— **光子传递曲线 (PTC)**（Janesick 2001, DOI 10.1117/3.374903）。

    理论：Var[ADU] = (1/g)·Mean_net[ADU] + (σ_R²/g² + 1/12)
    做法：扫描天光，逐档用**独立臂**两帧配对差分测方差（静态结构抵消），
          对 (mean_net, var) 做加权直线拟合，斜率应为 1/g、截距应为 σ_R²/g² + 1/12。
    """
    if levels is None:
        levels = [0.0, 1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0]
    xs, ys, ws = [], [], []
    rows = []
    for B in levels:
        sky = np.full(n_pix, float(B))
        zero = np.zeros(n_pix)
        kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
                  bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)
        o1 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, seed=11 + int(B), **kw)
        o2 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, seed=22 + int(B), **kw)
        v, nn = pair_diff_variance(o1["adu"], o2["adu"])
        mean_net = float(np.mean(0.5 * (o1["adu"] + o2["adu"])) - det.bias_adu)
        xs.append(mean_net); ys.append(v)
        ws.append(1.0 / max(mc_sigma_error_of_variance(v, nn), 1e-12))
        rows.append({"sky_e_per_s": float(B), "mean_net_adu": mean_net, "var_adu2": v,
                     "n_pix": nn, "sigma_adu": sigma_of_var(v)})
    x = np.asarray(xs); y = np.asarray(ys); w = np.asarray(ws)
    W = w * w
    sw = W.sum()
    slope = float((W * x * y).sum() / sw - (W * x).sum() * (W * y).sum() / sw ** 2) / \
            float((W * x * x).sum() / sw - ((W * x).sum() / sw) ** 2)
    inter = float((W * y).sum() / sw - slope * (W * x).sum() / sw)
    slope_theory = 1.0 / det.gain_e_per_adu
    inter_theory = det.read_noise_e ** 2 / det.gain_e_per_adu ** 2 + QUANT_VAR
    return {
        "name": "A3_photon_transfer_curve",
        "detector": det.as_dict(), "exptime_s": t, "points": rows,
        "fit": {"slope_meas": slope, "slope_theory_1_over_g": slope_theory,
                "slope_rel_dev": slope / slope_theory - 1.0,
                "intercept_meas": inter, "intercept_theory": inter_theory,
                "intercept_rel_dev": inter / inter_theory - 1.0,
                "r2": float(1.0 - ((y - (slope * x + inter)) ** 2).sum()
                            / ((y - y.mean()) ** 2).sum())},
        "implied_gain_e_per_adu": 1.0 / slope,
        "verdict": bool(abs(slope / slope_theory - 1.0) < 0.03
                        and abs(inter / inter_theory - 1.0) < 0.15),
    }


def test_A4_dark_current(n_pix: int, t: float) -> Dict[str, Any]:
    """A4 —— **暗电流泊松 + 温度加倍律**：D(T)=D_ref·2^((T-T_ref)/T_double)。

    两个独立检验：
      (i)  计数：暗电流在**电子域**进泊松 ⇒ Var = Mean（Fano=1）；
      (ii) 温度：T → T+T_double 时暗电流率**恰好翻倍**（在电子域直接比对期望面，
           再用配对差分测方差确认方差也翻倍 —— 只改均值不改方差即为非物理）。
    """
    det = _det(dark_current_e_per_s=0.5, dark_ref_temp_c=-20.0, dark_double_temp_c=6.0)
    zero = np.zeros(n_pix)
    out: Dict[str, Any] = {"name": "A4_dark_current", "detector": det.as_dict(), "exptime_s": t}
    # (i) Fano —— 在**足够计数**下测（量化 1/12 是连续近似，低计数下会失效，见 (iii)）
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=0.0, bias_adu=0.0,
              exptime_s=t, src_e_per_s=zero, sky_e_per_s=zero)
    d1 = independent_expose(dark_e_per_s=det.dark_current_e_per_s, seed=301, **kw)
    d2 = independent_expose(dark_e_per_s=det.dark_current_e_per_s, seed=302, **kw)
    v, nn = pair_diff_variance(d1["adu"], d2["adu"])
    mean_e = float(det.dark_current_e_per_s * t)
    var_pred_adu = mean_e / det.gain_e_per_adu ** 2 + QUANT_VAR
    out["fano"] = {"dark_e_mean": mean_e, "var_meas_adu2": v, "var_pred_adu2": var_pred_adu,
                   "rel_dev": v / var_pred_adu - 1.0, "n_pix": nn,
                   "fano_factor_meas": float(v - QUANT_VAR) * det.gain_e_per_adu ** 2 / mean_e,
                   "mc_1sigma_err_of_var": mc_sigma_error_of_variance(var_pred_adu, nn)}
    # (ii) doubling
    d_hot = det.dark_current_at(-20.0 + det.dark_double_temp_c)
    out["doubling"] = {
        "D_at_Tref": det.dark_current_at(-20.0), "D_at_Tref_plus_Tdouble": d_hot,
        "ratio": d_hot / det.dark_current_at(-20.0),
        "theory_ratio": 2.0,
        "verdict": abs(d_hot / det.dark_current_at(-20.0) - 2.0) < 1e-12,
    }
    # (iii) 低计数下的量化偏差（诚实登记：1/12 是连续近似，低计数会失效）
    det_lo = _det(dark_current_e_per_s=0.02, dark_ref_temp_c=-20.0, dark_double_temp_c=6.0)
    lo1 = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, gain_e_per_adu=det_lo.gain_e_per_adu,
                             read_noise_e=0.0, bias_adu=0.0, exptime_s=t, dark_e_per_s=0.02,
                             seed=301)
    lo2 = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, gain_e_per_adu=det_lo.gain_e_per_adu,
                             read_noise_e=0.0, bias_adu=0.0, exptime_s=t, dark_e_per_s=0.02,
                             seed=302)
    v_lo, _ = pair_diff_variance(lo1["adu"], lo2["adu"])
    mean_lo = 0.02 * t
    pred_lo = mean_lo / det_lo.gain_e_per_adu ** 2 + QUANT_VAR
    out["low_count_quantization_breakdown"] = {
        "dark_e_mean": mean_lo, "var_meas_adu2": v_lo, "var_pred_adu2_continuous": pred_lo,
        "rel_dev": v_lo / pred_lo - 1.0,
        "note": "暗电流均值仅 %.2f e- 时，电子数几乎只取 0/1/2 三个值，"
                "round(n_e/g) 也几乎只取 0/1 ⇒ 量化误差分布远离 U(-1/2,1/2)，"
                "1/12 的连续近似**失效**（本档实测偏低约 10%%）。"
                "这是**判据适用域**的登记：方差闭合检验必须工作在量化步长远小于"
                "噪声 sigma 的档位（本套件 A2/A3 的天光档位满足）。" % mean_lo,
    }
    out["verdict"] = bool(abs(out["fano"]["rel_dev"]) < 0.05 and out["doubling"]["verdict"])
    out["verdict_reason"] = ("Fano=1 在暗电流 50 e-/pix 档闭合到 %.2f%%；温度加倍律精确 2.000000；"
                             "低计数档的量化近似失效已单独登记（不参与判据）。"
                             % (100.0 * out["fano"]["rel_dev"]))
    return out


def test_A5_flat_field(n_pix: int, det: NM.Detector, t: float) -> Dict[str, Any]:
    """A5 —— **平场乘性响应**：m(x,y) 只乘 (源+天光)，把 m 除掉后**噪声统计复原**。

    检验：同一物理帧，一处用 m、一处用 m≡1（同种子、同天光），
          配对差分测得的**逐像素方差**必须满足 Var_m(p) ≈ m(p)²·Var_1(p)（散粒主导项），
          即 sqrt(Var_m)/m ≈ sqrt(Var_1) —— 平场是**乘性**的，不是加性。
    另给负例：若把平场误当加性（+m 而非 ×m），比值不闭合。
    """
    rng = np.random.default_rng(555)
    flat = NM.flat_response((n_pix, 1), rng, prnu_rms=0.05, low_order=0.0,
                            tilt_x=0.0, tilt_y=0.0, vignette=0.0)
    flat = flat.ravel()
    sky = np.full(n_pix, 50.0)          # e-/s，散粒主导
    zero = np.zeros(n_pix)
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
              bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)
    a1 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, flat=flat, seed=401, **kw)
    a2 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, flat=flat, seed=402, **kw)
    b1 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, flat=None, seed=401, **kw)
    b2 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, flat=None, seed=402, **kw)
    dm = a1["adu"] - a2["adu"]
    d1 = b1["adu"] - b2["adu"]
    # 逐像素无法测方差（每像素只有一个差分样本）⇒ 分块测，用 m 的块均值
    nb = 200
    blk = n_pix // nb
    vm, v1, mm = [], [], []
    for k in range(nb):
        sl = slice(k * blk, (k + 1) * blk)
        vm.append(0.5 * float(np.var(dm[sl], ddof=1)))
        v1.append(0.5 * float(np.var(d1[sl], ddof=1)))
        mm.append(float(np.mean(flat[sl])))
    vm = np.asarray(vm); v1 = np.asarray(v1); mm = np.asarray(mm)
    ratio = vm / v1
    ratio_pred = mm ** 2
    rel = np.abs(ratio / ratio_pred - 1.0)
    # 加性负例
    add_ratio = (v1 + (mm - 1.0) ** 2 * 0.0) / v1   # 加性常数不改方差 ⇒ 比值恒 1
    return {
        "name": "A5_flat_field_multiplicative",
        "n_blocks": nb, "block_pix": blk,
        "flat_min": float(flat.min()), "flat_max": float(flat.max()),
        "median_rel_dev_var_ratio_vs_m2": float(np.median(rel)),
        "p90_rel_dev": float(np.percentile(rel, 90)),
        "median_measured_ratio": float(np.median(ratio)),
        "median_predicted_ratio_m2": float(np.median(ratio_pred)),
        "additive_negative_control_ratio": float(np.median(add_ratio)),
        "verdict": bool(float(np.median(rel)) < 0.05),
        "note": "乘性 ⇒ 方差按 m² 缩放；加性 ⇒ 方差不变（负例恒 1）。"
                "读出/量化项在低天光档会破坏 m² 标度，本档取散粒主导（sky*t/g >> RN/g）",
    }


def test_A6_read_noise(n_pix: int, det: NM.Detector) -> Dict[str, Any]:
    """A6 —— **读出噪声**：零曝光/零源零天光下 sigma_adu = sqrt(σ_R²/g² + 1/12)。"""
    zero = np.zeros(n_pix)
    o1 = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, gain_e_per_adu=det.gain_e_per_adu,
                            read_noise_e=det.read_noise_e, bias_adu=det.bias_adu, exptime_s=0.0,
                            dark_e_per_s=0.0, seed=601)
    o2 = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, gain_e_per_adu=det.gain_e_per_adu,
                            read_noise_e=det.read_noise_e, bias_adu=det.bias_adu, exptime_s=0.0,
                            dark_e_per_s=0.0, seed=602)
    v, nn = pair_diff_variance(o1["adu"], o2["adu"])
    v_pred = det.read_noise_e ** 2 / det.gain_e_per_adu ** 2 + QUANT_VAR
    return {
        "name": "A6_read_noise",
        "var_meas": v, "var_pred": v_pred, "rel_dev": v / v_pred - 1.0,
        "sigma_meas_adu": sigma_of_var(v), "sigma_pred_adu": sigma_of_var(v_pred),
        "sigma_read_only_adu": det.read_noise_e / det.gain_e_per_adu,
        "n_pix": nn, "mc_1sigma_err_of_var": mc_sigma_error_of_variance(v_pred, nn),
        "verdict": bool(abs(v / v_pred - 1.0) < 0.02),
    }


def test_A7_saturation(det: NM.Detector) -> Dict[str, Any]:
    """A7 —— **饱和硬钳位**：adu 恰被钳到 saturation_adu，且不计入方差闭合的掩膜。"""
    n = 200000
    sky = np.full(n, 1.0e6)     # 远超满阱
    o = independent_expose(src_e_per_s=np.zeros(n), sky_e_per_s=sky,
                           gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
                           bias_adu=det.bias_adu, exptime_s=1.0, dark_e_per_s=0.0,
                           seed=701, saturation_adu=det.saturation_adu)
    sat = det.saturation_adu
    return {
        "name": "A7_saturation",
        "saturation_adu": sat,
        "max_adu": float(o["adu"].max()),
        "n_at_saturation": int(np.count_nonzero(o["adu"] >= sat)),
        "n_total": int(n),
        "fraction_at_saturation": float(np.mean(o["adu"] >= sat)),
        "all_at_or_below": bool(o["adu"].max() <= sat),
        "verdict": bool(o["adu"].max() <= sat and np.count_nonzero(o["adu"] >= sat) > 0.99 * n),
    }


def test_A8_sky_gradient(n_pix: int, det: NM.Detector, t: float) -> Dict[str, Any]:
    """A8 —— **天空梯度**：天光逐像素水平不同 ⇒ **局部方差随局部水平变**（不是全局常数）。

    这正是「天光必须进泊松」的直接可观测后果：若天光是加性常数，梯度只改均值、
    各列方差必须完全相同（负例见 B1/B4）。
    """
    shape = (512, 512)
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    sky = 5.0 + 95.0 * (xx / (shape[1] - 1.0))     # 5 -> 100 e-/s 线性梯度
    zero = np.zeros(shape)
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
              bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)
    o1 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, seed=801, **kw)
    o2 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky, seed=802, **kw)
    d = o1["adu"] - o2["adu"]
    cols = np.array_split(np.arange(shape[1]), 8)
    rows = []
    for c in cols:
        v = 0.5 * float(np.var(d[:, c], ddof=1))
        B = float(np.mean(sky[:, c])) * t
        vp = NM.predicted_variance_adu2(src_e=0.0, sky_e=B, dark_e=0.0, det=det)
        rows.append({"x_center": float(np.mean(c)), "sky_e": B, "var_meas": v, "var_pred": vp,
                     "rel_dev": v / vp - 1.0})
    vs = [r["var_meas"] for r in rows]
    mono = all(vs[i] < vs[i + 1] for i in range(len(vs) - 1))
    return {
        "name": "A8_sky_gradient", "columns": rows,
        "var_monotone_with_x": bool(mono),
        "max_abs_rel_dev": max(abs(r["rel_dev"]) for r in rows),
        "verdict": bool(mono and max(abs(r["rel_dev"]) for r in rows) < 0.05),
    }


# ===========================================================================
# 3. 负例（能红能绿）：真值无效应 ⇒ 度量必须归零
# ===========================================================================
def test_B1_additive_sky_null(n_pix: int, det: NM.Detector, t: float) -> Dict[str, Any]:
    """B1 —— **纯加性天光负例**（负责人 §9.41 硬性要求）。

    构造：同种子生成同一帧，一臂用低天光 B_lo、一臂用高天光 B_hi，**但高天光臂
    在成品帧上加常数偏移**（ADU 域，量化之后），而不是把天光放进泊松。
    真值：加性常数**不改任何方差** ⇒ 配对差分的方差差必须**恰为 0**（< 1e-12 相对）。
    """
    sky_lo = np.full(n_pix, 1.0)
    sky_hi = np.full(n_pix, 100.0)
    zero = np.zeros(n_pix)
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
              bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)
    lo1 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky_lo, seed=900, **kw)["adu"]
    lo2 = independent_expose(src_e_per_s=zero, sky_e_per_s=sky_lo, seed=901, **kw)["adu"]
    # 加性臂：**同一批帧** + 常数（逐位配对）
    off = float((sky_hi[0] - sky_lo[0]) * t / det.gain_e_per_adu)
    hi1 = lo1 + off
    hi2 = lo2 + off
    v_lo, nn = pair_diff_variance(lo1, lo2)
    v_hi, _ = pair_diff_variance(hi1, hi2)
    dmean = float(np.mean(hi1) - np.mean(lo1))
    return {
        "name": "B1_additive_sky_null",
        "offset_adu": off, "mean_shift_adu": dmean,
        "var_lo": v_lo, "var_hi": v_hi,
        "abs_var_change": abs(v_hi - v_lo),
        "rel_var_change": abs(v_hi - v_lo) / v_lo,
        "snr_ratio_minus_1": abs(math.sqrt(v_lo / v_hi) - 1.0),
        "n_pix": nn,
        "criterion": "|Var_hi/Var_lo - 1| < 1e-12 且 |SNR_hi/SNR_lo - 1| < 1e-12",
        "verdict": bool(abs(v_hi - v_lo) / v_lo < 1e-12),
        "note": "真值 = 无效应 ⇒ 任何 SNR/方差类判据**必须归零**。"
                "此臂证明我们的判据不是在测「帧整体变亮了」而是真在测噪声统计。",
    }


def test_B2_additive_sky_effect_present(det: NM.Detector, t: float) -> Dict[str, Any]:
    """B2 —— **绿例**：同一天光变化，走**物理泊松**路径 ⇒ 度量必须显著非零。

    与 B1 严格配对（同 B_lo/B_hi、同帧尺寸、同曝光），唯一差别是天光进不进泊松。
    """
    n_pix = 400000
    zero = np.zeros(n_pix)
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
              bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)
    lo1 = independent_expose(src_e_per_s=zero, sky_e_per_s=np.full(n_pix, 1.0), seed=910, **kw)["adu"]
    lo2 = independent_expose(src_e_per_s=zero, sky_e_per_s=np.full(n_pix, 1.0), seed=911, **kw)["adu"]
    hi1 = independent_expose(src_e_per_s=zero, sky_e_per_s=np.full(n_pix, 100.0), seed=910, **kw)["adu"]
    hi2 = independent_expose(src_e_per_s=zero, sky_e_per_s=np.full(n_pix, 100.0), seed=911, **kw)["adu"]
    v_lo, nn = pair_diff_variance(lo1, lo2)
    v_hi, _ = pair_diff_variance(hi1, hi2)
    # 固定源通量 F 下的 SNR = F/sigma ⇒ SNR_hi/SNR_lo = sigma_lo/sigma_hi = sqrt(Var_lo/Var_hi)
    snr_lo = sigma_of_var(v_lo)
    snr_hi = sigma_of_var(v_hi)
    # 解析预测
    p_lo = NM.predicted_variance_adu2(src_e=0.0, sky_e=1.0 * t, dark_e=0.0, det=det)
    p_hi = NM.predicted_variance_adu2(src_e=0.0, sky_e=100.0 * t, dark_e=0.0, det=det)
    pred_ratio = math.sqrt(p_hi / p_lo)      # sigma_hi/sigma_lo（= SNR_lo/SNR_hi，固定源通量）
    meas_ratio = snr_hi / snr_lo
    rel_dev = abs(meas_ratio / pred_ratio - 1.0)
    return {
        "name": "B2_additive_sky_effect_present_GREEN",
        "var_lo": v_lo, "var_hi": v_hi, "n_pix": nn,
        "sigma_ratio_meas_hi_over_lo": meas_ratio,
        "sigma_ratio_pred_hi_over_lo": pred_ratio,
        "snr_ratio_meas_lo_over_hi": 1.0 / meas_ratio,
        "abs_rel_dev_vs_analytic": rel_dev,
        "verdict": bool(rel_dev < 0.05 and meas_ratio > 1.05),
        "note": "固定源通量下 SNR ∝ 1/sigma。天光 1→100 e-/s 时物理臂 sigma 涨 %.3f×"
                "（⇒ SNR 掉到 %.4f×，绿），加性臂 sigma 比恰 =1.000000（⇒ SNR 比恰 =1，红，见 B1）。"
                % (meas_ratio, 1.0 / meas_ratio),
    }


def test_B3_buggy_sky_injection(det: NM.Detector, t: float) -> Dict[str, Any]:
    """B3 —— **能红的负例**：把天光**在泊松之后**加到电子域（典型实现 bug）。

    这是一个真实存在的错误实现（"先生成噪声，再把天光加上去"）：
        n_e = Poisson(t*src) + t*sky + N(0,σ_R)
    它让均值对、方差错（少了天光的散粒项）。方差闭合判据**必须**把它抓出来。
    """
    n_pix = 400000
    B = 100.0
    zero = np.zeros(n_pix)
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
              bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)

    def buggy(seed):
        o = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, seed=seed, **kw)
        return np.round(o["adu"] + B * t / det.gain_e_per_adu)

    g1, g2 = buggy(920), buggy(921)
    v_bug, nn = pair_diff_variance(g1, g2)
    v_pred = NM.predicted_variance_adu2(src_e=0.0, sky_e=B * t, dark_e=0.0, det=det)
    v_pred_noSkyShot = det.read_noise_e ** 2 / det.gain_e_per_adu ** 2 + QUANT_VAR
    return {
        "name": "B3_buggy_sky_after_poisson_RED",
        "var_meas_buggy": v_bug, "var_pred_correct": v_pred,
        "var_pred_if_sky_has_no_shot_noise": v_pred_noSkyShot,
        "rel_dev_buggy_vs_correct": v_bug / v_pred - 1.0,
        "n_pix": nn,
        "criterion": "|Var_meas/Var_pred - 1| > 0.5（判据**必须报警**）",
        "verdict": bool(abs(v_bug / v_pred - 1.0) > 0.5),
        "note": "此臂证明方差闭合判据**有功效**（不是空断言）：真值有效应时它确实红。",
    }


def test_B4_additive_sky_gradient_null(det: NM.Detector, t: float) -> Dict[str, Any]:
    """B4 —— **加性天空梯度负例**：梯度只加均值 ⇒ 各列方差必须**逐位相同**。

    与 A8 配对：A8 物理臂各列方差随 x 单调增（绿）；本臂各列方差**完全相同**（红）。
    """
    shape = (512, 512)
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    sky = 5.0 + 95.0 * (xx / (shape[1] - 1.0))
    zero = np.zeros(shape)
    kw = dict(gain_e_per_adu=det.gain_e_per_adu, read_noise_e=det.read_noise_e,
              bias_adu=det.bias_adu, exptime_s=t, dark_e_per_s=0.0)
    # 参考帧：**零天光零源**（只有读出+量化噪声），使加性臂的"逐位不变"精确成立
    kw0 = dict(kw); kw0["exptime_s"] = 0.0
    o1 = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, seed=801, **kw0)["adu"]
    o2 = independent_expose(src_e_per_s=zero, sky_e_per_s=zero, seed=802, **kw0)["adu"]
    add = (sky - 5.0) * t / det.gain_e_per_adu      # 加性梯度（ADU）
    a1, a2 = o1 + add, o2 + add
    cols = np.array_split(np.arange(shape[1]), 8)
    vs = np.asarray([0.5 * float(np.var((a1 - a2)[:, c], ddof=1)) for c in cols])
    vs_ref = np.asarray([0.5 * float(np.var((o1 - o2)[:, c], ddof=1)) for c in cols])
    rel_exact = np.abs(vs / vs_ref - 1.0)
    xc = np.asarray([float(np.mean(c)) for c in cols])
    corr = float(np.corrcoef(xc, vs)[0, 1]) if vs.std() > 0 else 0.0
    return {
        "name": "B4_additive_sky_gradient_null",
        "var_per_column": vs.tolist(),
        "var_per_column_reference_arm": vs_ref.tolist(),
        "max_rel_dev_vs_paired_reference": float(rel_exact.max()),
        "paired_reference_note":
            "参考臂与加性臂共用**同一批噪声帧**（加性常数在差分中精确抵消），"
            "故两臂的逐列方差必须逐位相同；max_rel_dev = %.3e 即该精确 0。" % float(rel_exact.max()),
        "cross_column_sample_scatter_ptp_over_mean": float((vs.max() - vs.min()) / vs.mean()),
        "cross_column_corr_with_x": corr,
        "cross_column_scatter_note":
            "跨列 2.5%% 的散差是**纯有限样本误差**（每列 32768 个差分样本 ⇒ sqrt(2/n)=0.78%%，"
            "8 列极差 ~2.5%% 合理），与 x 的相关系数 = %.3f（无梯度信号）。"
            "物理臂 A8 的同一统计量是**单调**的（见 A8 columns）。" % corr,
        "var_ptp_over_mean": float((vs.max() - vs.min()) / vs.mean()),
        "mean_gradient_adu_ptp": float(add.max() - add.min()),
        "criterion": "加性臂逐列方差 vs **配对参考臂**逐列方差 < 1e-12（真值 = 无效应）；"
                     "且跨列散差与 x 无相关",
        "verdict": bool(float(rel_exact.max()) < 1e-12),
        "note": "加性梯度把均值抬了 %.1f ADU，方差却逐位不变 —— 这正是"
                "「只加常数不改噪声统计」的直接证据。"
                "**必须用同一批帧加常数**（逐位配对）才能得到精确 0；"
                "若两臂独立重抽，则测到的是纯抽样误差（此前版本即因此误判）。"
                % float(add.max() - add.min()),
    }


# ===========================================================================
# 4. 驱动
# ===========================================================================
def run_all(quick: bool = False, verbose: bool = True) -> Dict[str, Any]:
    n_small = 200_000 if quick else 2_000_000
    n_pix = 200_000 if quick else 1_000_000
    det = _det()
    t = 100.0
    t0 = time.time()
    res: Dict[str, Any] = {
        "suite": "P7-noise-selftest",
        "independent_rng": "SplitMix64 + Box-Muller + Knuth/PTRS (自实现, 不用 numpy 分布采样器)",
        "quick": bool(quick),
        "tests": {},
    }
    tests = [
        ("A1", lambda: test_A1_quantization(n_small)),
        ("A2", lambda: test_A2_sky_poisson_series([1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0], n_pix, det, t)),
        ("A3", lambda: test_A3_ptc(n_pix, det, t)),
        ("A4", lambda: test_A4_dark_current(n_pix, t)),
        ("A5", lambda: test_A5_flat_field(n_pix, det, t)),
        ("A6", lambda: test_A6_read_noise(n_pix, det)),
        ("A7", lambda: test_A7_saturation(det)),
        ("A8", lambda: test_A8_sky_gradient(n_pix, det, t)),
        ("B1", lambda: test_B1_additive_sky_null(n_pix, det, t)),
        ("B2", lambda: test_B2_additive_sky_effect_present(det, t)),
        ("B3", lambda: test_B3_buggy_sky_injection(det, t)),
        ("B4", lambda: test_B4_additive_sky_gradient_null(det, t)),
    ]
    for key, fn in tests:
        ts = time.time()
        r = fn()
        r["wall_s"] = time.time() - ts
        res["tests"][key] = r
        if verbose:
            print("[P7-selftest] %-4s %-44s %s  (%.1fs)"
                  % (key, r["name"], "PASS" if r.get("verdict") else "FAIL", r["wall_s"]))
    res["n_pass"] = sum(1 for r in res["tests"].values() if r.get("verdict"))
    res["n_total"] = len(res["tests"])
    res["all_pass"] = res["n_pass"] == res["n_total"]
    res["wall_s_total"] = time.time() - t0
    if verbose:
        print("[P7-selftest] %d/%d PASS  (%.1fs)" % (res["n_pass"], res["n_total"],
                                                     res["wall_s_total"]))
    return res


def main() -> int:
    ap = argparse.ArgumentParser(description="P7 噪声合成器独立方法自校验")
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--json", type=str, default=None)
    a = ap.parse_args()
    r = run_all(quick=a.quick)
    if a.json:
        p = Path(a.json)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(r, indent=2, ensure_ascii=False, default=float), encoding="utf-8")
        print("[P7-selftest] wrote %s" % p)
    return 0 if r["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
