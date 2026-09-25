#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ACSD RELEASE-02 / DATA-TYPE-MATRIX —— 可复用**物理噪声合成器**（论文方法节用）。

本模块实现"真实物理实现"的正向噪声链（负责人 2026-09-19 强制要求，GAP_AUDIT §9.41/§9.47）：

    1. 入射光子率      :  f_src(x,y) + sky(x,y)                     [e-/pix/s]（已含 QE）
    2. 乘性响应（平场）:  * m(x,y)                                   [1]      PRNU + 低阶空间项
    3. 曝光积分        :  * t                                        [s]
    4. 散粒噪声        :  Poisson( lambda )                          [e-]     源 / 天光 / 暗电流**各自独立**
    5. 暗电流          :  D(T) * t ，Poisson                          [e-]     含温度/曝光依赖
    6. 读出噪声        :  + Normal(0, sigma_R)                        [e-]
    7. 宇宙线/热像素   :  + 冲激电荷（可选）                          [e-]
    8. 增益量化        :  ADU = round( e- / g ) + bias                [ADU]    含**取整**与**饱和**
    ==>  输出：整型/浮点 ADU 帧 + 真值（期望电子面、逐项分量、provenance）

**关键物理约束（本模块的存在理由）**
    天光水平 B ↑ **必须同时**带来方差 B ↑（泊松：Var = 均值）。因此
    "把天光当作加性常数加到已含噪声的帧上"是**非物理**的：它只改均值、不改方差，
    在 SNR 类判据上**测不出任何东西**（度量恒为 0）。本模块把这种"纯加性"实现为
    **显式的负例臂**（mode="additive"），用于证明判据"真值为无效应时归零"。

**四种（非）物理臂**（判据的红/绿对照，务必在报告里写明用的是哪个）
    mode="physical"          完整物理链（默认；**唯一可用于科学结论的臂**）
    mode="additive"          **纯加性天光**：先按参考天光生成物理帧（含取整/饱和），
                             再在**成品帧**上加常数 ADU 偏移
                             （配对同种子 ⇒ 方差逐位不变 ⇒ 任何 SNR 判据必须归零）
    mode="mean_only"         非物理对照：跳过泊松（确定性期望 + 读出噪声），均值随天光变、方差不变
    mode="variance_override" 非物理对照：均值随天光变，逐像素方差被钉在参考值

**单位与符号**
    e-            电子（探测器内电荷）
    ADU           模数转换单位；g [e-/ADU] 为转换增益；bias [ADU] 为偏置基座
    sigma_R [e-]  读出噪声（电子域高斯）
    D(T) [e-/pix/s] 暗电流密度：D(T) = D_ref * 2**((T - T_ref)/T_double)

**诚实边界（不得省略）**
    * 本模块**不**声称任何真实仪器的绝对增益/口径/曝光可由此反推（§9.42：标定目标是
      测光坐标系，物理单位无意义）；参数一律是**可配置的场景参数**，须随产物一起登记。
    * 平场 m(x,y) 只乘 (源+天光)，**不乘暗电流**（暗电流在硅体内产生，PRNU 对其影响是
      二阶效应）——这是一条**显式简化**，不是遗漏。
    * 量化用 np.round（banker's rounding，半值取偶）；量化方差按 1/12 ADU^2 计入解析预测。
    * 饱和为**硬钳位**，未建模溢出/辉散（blooming）；宇宙线未建模轨迹形状（只建模电荷量）。

自检（含"纯加性 ⇒ 度量归零"的负例）：python3 noise_model.py --selftest
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass, field, asdict
from typing import Any, Dict, Optional, Tuple

import numpy as np

# 物理常数/约定 -------------------------------------------------------------
MAD_TO_SIGMA = 1.4826          # 稳健 sigma 估计（与 frame_snr_canon 一致）
GAUSSIAN_FWHM_OVER_SIGMA = 2.0 * math.sqrt(2.0 * math.log(2.0))   # 2.354820045
MOFFAT4_SIGMA_OVER_FWHM = 1.0 / 1.230310   # 生产约定：Moffat(beta=4) 二阶矩等效 sigma
QUANTIZATION_VARIANCE_ADU2 = 1.0 / 12.0

MODE_PHYSICAL = "physical"
MODE_ADDITIVE = "additive"
MODE_MEAN_ONLY = "mean_only"
MODE_VARIANCE_OVERRIDE = "variance_override"
MODES = (MODE_PHYSICAL, MODE_ADDITIVE, MODE_MEAN_ONLY, MODE_VARIANCE_OVERRIDE)


# ---------------------------------------------------------------------------
# 1. 探测器配置
# ---------------------------------------------------------------------------
@dataclass
class Detector:
    """探测器参数（全部可配置；**不得**当作真实仪器参数反推的输入）。"""

    gain_e_per_adu: float = 1.5            # g [e-/ADU]
    read_noise_e: float = 5.0              # sigma_R [e-]
    bias_adu: float = 1000.0               # 偏置基座 [ADU]
    full_well_e: float = 120000.0          # 满阱 [e-]（饱和阈值 = full_well/g + bias）
    dark_current_e_per_s: float = 0.02     # D_ref [e-/pix/s] @ dark_ref_temp_c
    dark_ref_temp_c: float = -20.0         # 参考温度 [C]
    dark_double_temp_c: float = 6.0        # 暗电流每升温这么多度翻倍 [C]
    quantize: bool = True                  # 是否做 ADU 取整
    saturate: bool = True                  # 是否做饱和钳位
    hot_pixel_fraction: float = 0.0        # 热像素比例（暗电流被放大）
    hot_pixel_dark_gain: float = 50.0      # 热像素暗电流倍率

    @property
    def saturation_adu(self) -> float:
        return self.full_well_e / self.gain_e_per_adu + self.bias_adu

    def dark_current_at(self, temp_c: float) -> float:
        """温度依赖暗电流 D(T) = D_ref * 2**((T-T_ref)/T_double) [e-/pix/s]。"""
        return float(
            self.dark_current_e_per_s
            * 2.0 ** ((float(temp_c) - self.dark_ref_temp_c) / self.dark_double_temp_c)
        )

    def as_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        d["saturation_adu"] = self.saturation_adu
        return d


# ---------------------------------------------------------------------------
# 2. 空间项：平场乘性响应 / 天空梯度 / 热像素图
# ---------------------------------------------------------------------------
def flat_response(
    shape: Tuple[int, int],
    rng: np.random.Generator,
    *,
    prnu_rms: float = 0.01,
    low_order: float = 0.02,
    tilt_x: float = 1.0,
    tilt_y: float = 0.5,
    vignette: float = 0.0,
) -> np.ndarray:
    """乘性平场响应 m(x,y)，**几何均值归一**到 1（保证天光水平可解释）。

    m = (1 + low_order*(tilt_x*u + tilt_y*v)) * (1 + vignette*r^2) * (1 + PRNU)
    u,v ∈ [-0.5, 0.5]；PRNU 为逐像素高斯（相对 rms）。
    """
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    u = xx / max(nx - 1, 1) - 0.5
    v = yy / max(ny - 1, 1) - 0.5
    low = 1.0 + low_order * (tilt_x * u + tilt_y * v)
    if vignette:
        r2 = (u * u + v * v) / 0.5
        low = low * (1.0 - vignette * r2)
    if prnu_rms:
        low = low * (1.0 + prnu_rms * rng.normal(0.0, 1.0, size=shape))
    low = np.maximum(low, 1e-3)
    return low / float(np.exp(np.mean(np.log(low))))   # 几何均值 = 1


def sky_surface_e_per_s(
    shape: Tuple[int, int],
    *,
    level_e_per_s: float,
    grad_x_e_per_s: float = 0.0,
    grad_y_e_per_s: float = 0.0,
    grad_quad_e_per_s: float = 0.0,
    theta_deg: float = 0.0,
    moon_halo_e_per_s: float = 0.0,
    moon_center: Optional[Tuple[float, float]] = None,
    moon_scale_px: float = 400.0,
) -> np.ndarray:
    """天空（天光/光污染）空间面 [e-/pix/s]，**恒非负**。

    线性梯度（任意方位角 theta）+ 二次项 + 可选的月光/光污染光晕（高斯）。
    **该面进入泊松采样**：面越亮 ⇒ 该处泊松方差越大（这是天光影响 SNR 的唯一物理通道）。
    """
    ny, nx = shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    u = xx / max(nx - 1, 1) - 0.5
    v = yy / max(ny - 1, 1) - 0.5
    th = math.radians(theta_deg)
    s = grad_x_e_per_s * (math.cos(th) * u + math.sin(th) * v)
    s = s + grad_y_e_per_s * (-math.sin(th) * u + math.cos(th) * v)
    s = s + grad_quad_e_per_s * (u * u + v * v)
    surf = level_e_per_s + s
    if moon_halo_e_per_s:
        cy, cx = moon_center if moon_center is not None else (0.0, 0.0)
        rr2 = (yy - cy) ** 2 + (xx - cx) ** 2
        surf = surf + moon_halo_e_per_s * np.exp(-rr2 / (2.0 * moon_scale_px ** 2))
    return np.maximum(surf, 0.0)


def hot_pixel_map(
    shape: Tuple[int, int], rng: np.random.Generator, fraction: float, gain_factor: float
) -> Optional[np.ndarray]:
    """热像素暗电流倍率图（1.0 = 正常，gain_factor = 热像素）。"""
    if fraction <= 0:
        return None
    m = np.ones(shape, dtype=float)
    n = int(round(fraction * shape[0] * shape[1]))
    if n <= 0:
        return m
    idx = rng.choice(shape[0] * shape[1], size=n, replace=False)
    m.flat[idx] = gain_factor
    return m


def cosmic_ray_charge_e(
    shape: Tuple[int, int],
    rng: np.random.Generator,
    *,
    rate_per_frame: float,
    mean_charge_e: float,
) -> np.ndarray:
    """宇宙线沉积电荷 [e-]（冲激；指数能量分布）。rate_per_frame = 每帧平均命中像素数。"""
    out = np.zeros(shape, dtype=float)
    if rate_per_frame <= 0:
        return out
    n = rng.poisson(rate_per_frame)
    if n <= 0:
        return out
    idx = rng.choice(shape[0] * shape[1], size=int(n), replace=False)
    out.flat[idx] = rng.exponential(mean_charge_e, size=int(n))
    return out


# ---------------------------------------------------------------------------
# 3. 核心：一帧的物理采样
# ---------------------------------------------------------------------------
@dataclass
class Frame:
    """一帧的产物 + 真值。"""

    adu: np.ndarray                       # 输出帧 [ADU]
    truth_e: np.ndarray                   # 期望电子面（无噪声真值）[e-]
    src_e: np.ndarray                     # 源分量期望 [e-]
    sky_e: np.ndarray                     # 天光分量期望 [e-]（**进入泊松**）
    dark_e: np.ndarray                    # 暗电流期望 [e-]
    flat: np.ndarray                      # 乘性响应 m(x,y)
    provenance: Dict[str, Any] = field(default_factory=dict)

    @property
    def saturation_adu(self) -> float:
        return float(self.provenance.get("detector", {}).get("saturation_adu", math.inf))


def expose(
    *,
    src_e_per_s: np.ndarray,
    sky_e_per_s: np.ndarray,
    det: Detector,
    exptime_s: float,
    rng: np.random.Generator,
    temp_c: Optional[float] = None,
    flat: Optional[np.ndarray] = None,
    hot_map: Optional[np.ndarray] = None,
    cr_rate_per_frame: float = 0.0,
    cr_mean_charge_e: float = 1000.0,
    mode: str = MODE_PHYSICAL,
    variance_override_e2: Optional[float] = None,
    additive_offset_adu: float = 0.0,
) -> Frame:
    """按给定模式生成一帧（见模块 docstring 的四种臂）。

    物理链（mode="physical"）::

        lam_e   = t * (src + sky) * m + t * D(T) * hot      [e-]
        n_e     = Poisson(lam_e) + Normal(0, sigma_R) + CR  [e-]
        adu     = round(n_e / g + bias)  ,  钳位到饱和      [ADU]
    """
    if mode not in MODES:
        raise ValueError("unknown mode %r; expected one of %s" % (mode, MODES))
    t = float(exptime_s)
    T = det.dark_ref_temp_c if temp_c is None else float(temp_c)

    src = np.asarray(src_e_per_s, dtype=float)
    sky = np.asarray(sky_e_per_s, dtype=float)
    if src.shape != sky.shape:
        raise ValueError("src/sky shape mismatch")
    shape = src.shape
    m = np.ones(shape) if flat is None else np.asarray(flat, dtype=float)

    dark_rate = det.dark_current_at(T)
    dark_map = np.full(shape, dark_rate, dtype=float)
    if hot_map is not None:
        dark_map = dark_map * np.asarray(hot_map, dtype=float)

    src_e = t * src * m
    sky_e = t * sky * m
    dark_e = t * dark_map
    lam_e = np.maximum(src_e + sky_e + dark_e, 0.0)

    prov: Dict[str, Any] = {
        "mode": mode,
        "exptime_s": t,
        "temp_c": T,
        "detector": det.as_dict(),
        "dark_current_e_per_s_used": dark_rate,
        "quantization": "round(banker)" if det.quantize else "none",
        "saturation": "hard-clip" if det.saturate else "none",
        "flat_applied_to": "src+sky (dark not multiplied; explicit simplification)",
        "poisson_terms": ["src", "sky", "dark"],
        "cosmic_ray_rate_per_frame": cr_rate_per_frame,
        "additive_offset_adu": additive_offset_adu,
        "variance_override_e2": variance_override_e2,
    }

    if mode in (MODE_MEAN_ONLY, MODE_VARIANCE_OVERRIDE):
        if mode == MODE_MEAN_ONLY:
            n_e = lam_e.copy()
        else:
            v = float(variance_override_e2 if variance_override_e2 is not None else 0.0)
            n_e = lam_e + rng.normal(0.0, 1.0, size=shape) * math.sqrt(max(v, 0.0))
        prov["physical"] = False
        prov["note"] = "非物理对照臂：跳过泊松，方差被人为固定（只改均值、不改方差）"
    else:
        n_e = rng.poisson(lam_e).astype(float)
        prov["physical"] = True

    if cr_rate_per_frame > 0:
        n_e = n_e + cosmic_ray_charge_e(
            shape, rng, rate_per_frame=cr_rate_per_frame, mean_charge_e=cr_mean_charge_e
        )
    n_e = n_e + rng.normal(0.0, det.read_noise_e, size=shape)

    adu = n_e / det.gain_e_per_adu + det.bias_adu
    if det.quantize:
        adu = np.round(adu)
    if det.saturate:
        sat = det.saturation_adu
        adu = np.minimum(adu, sat)
        prov["saturated_pixels"] = int(np.count_nonzero(adu >= sat))
    if mode == MODE_ADDITIVE and additive_offset_adu:
        # **纯加性天光**：在**已量化、已钳位**的成品帧上加常数 —— 只改均值、不改方差。
        # （必须在量化之后加，否则取整会把常数"重新量化"，破坏"逐位不变"的配对性质。）
        adu = adu + float(additive_offset_adu)
        prov["additive_applied_after_quantization"] = True

    return Frame(
        adu=adu,
        truth_e=lam_e,
        src_e=src_e,
        sky_e=sky_e,
        dark_e=dark_e,
        flat=m,
        provenance=prov,
    )


# ---------------------------------------------------------------------------
# 4. 解析预测（闭合检验用；与 canon §2 式 (2.1) 一致）
# ---------------------------------------------------------------------------
def predicted_variance_adu2(
    *, src_e: float, sky_e: float, dark_e: float, det: Detector
) -> float:
    """逐像素方差解析预测 [ADU^2]：

        Var_adu = (S_e + B_e + D_e)/g^2 + sigma_R^2/g^2 + 1/12
    （前三项：散粒；第四项：读出；第五项：ADU 量化。饱和像素必须剔除后再比。）
    """
    g = det.gain_e_per_adu
    return (
        (src_e + sky_e + dark_e) / (g * g)
        + (det.read_noise_e ** 2) / (g * g)
        + QUANTIZATION_VARIANCE_ADU2
    )


def sky_sigma_adu(*, sky_e: float, dark_e: float, det: Detector) -> float:
    """空白天光区逐像素 rms 预测 [ADU]（canon 的 sigma_sky）。"""
    g = det.gain_e_per_adu
    return math.sqrt(
        (sky_e + dark_e) / (g * g) + (det.read_noise_e ** 2) / (g * g) + QUANTIZATION_VARIANCE_ADU2
    )


def canon_snr(
    *, flux_e: float, sky_e: float, dark_e: float, det: Detector, sum_p2: float
) -> float:
    """canon 帧级 SNR（FRAME-SNR-CANON 式 2.7）：SNR = F*sqrt(sum P^2)/sigma_sky。

    flux_e = 源总通量 [e-]；sum_p2 = Σ P_i^2（离散归一化 PSF）。
    """
    return float(flux_e) * math.sqrt(float(sum_p2)) / sky_sigma_adu(
        sky_e=sky_e, dark_e=dark_e, det=det
    )


def robust_sigma_adu(img_adu: np.ndarray, mask: Optional[np.ndarray] = None) -> float:
    """稳健逐像素 rms [ADU]：1.4826 * MAD（与生产/canon 估计量一致）。

    **诚实边界（实测发现，必须登记）**：ADU 取整后 MAD 是**离散取值**，当 sigma 只有
    几 ADU 时其偏差可达 ±10%（自检 T2 首版即因此失败）。因此：
      * MAD 只用于**平移不变性**类判据（加常数前后比较，偏差自动抵消）；
      * **方差闭合**类判据必须用 clipped_std_adu（一致估计量）。
    """
    v = np.asarray(img_adu, dtype=float)
    if mask is not None:
        v = v[mask]
    med = float(np.median(v))
    return MAD_TO_SIGMA * float(np.median(np.abs(v - med)))


def clipped_std_adu(
    img_adu: np.ndarray,
    mask: Optional[np.ndarray] = None,
    *,
    n_sigma: float = 5.0,
    iters: int = 3,
) -> float:
    """sigma 裁剪后的样本 rms [ADU]（**一致估计量**，用于方差闭合检验）。

    量化数据上 MAD 有离散偏差，而样本方差无偏（量化方差 1/12 已计入解析预测）。
    裁剪用于剔除宇宙线/热像素/饱和翼。
    """
    v = np.asarray(img_adu, dtype=float)
    if mask is not None:
        v = v[mask]
    v = v[np.isfinite(v)]
    for _ in range(max(int(iters), 1)):
        mu = float(np.mean(v))
        sd = float(np.std(v))
        if sd <= 0:
            break
        keep = np.abs(v - mu) <= n_sigma * sd
        if keep.all():
            break
        v = v[keep]
    return float(np.std(v, ddof=1)) if v.size > 1 else 0.0


# ---------------------------------------------------------------------------
# 5. 自检（含负例：纯加性 ⇒ 度量必须归零）
# ---------------------------------------------------------------------------
def selftest(verbose: bool = True) -> Dict[str, Any]:
    """噪声模型自检。返回逐项结果（可写 JSON）。

    判据（写死，不事后放宽）：
      T1 泊松方差=均值            : |Var/Mean - 1| < 0.02（大样本）
      T2 天光 B↑ ⇒ 方差 B↑        : 方差随 B 单调增，且闭合到解析预测 < 5%
      T3 纯加性（配对同种子）     : |sigma(加常数) - sigma(原)| / sigma < 1e-12
      T4 纯加性 ⇒ SNR 变化归零    : |SNR_ctrl/SNR_ref - 1| < 1e-12
      T5 增益量化                  : 方差闭合含 1/12 ADU^2 项（<5%）
      T6 饱和钳位                  : 亮像素被钳到 saturation_adu
      T7 暗电流温度依赖            : D(T+6C)/D(T) == 2（±1e-12）
    """
    rng = np.random.default_rng(20260919)
    det = Detector(gain_e_per_adu=1.5, read_noise_e=5.0, bias_adu=1000.0, full_well_e=120000.0)
    res: Dict[str, Any] = {"criteria": {}, "verdicts": {}}

    # T1 泊松方差 = 均值（大样本，直接对 rng.poisson 检验）
    lam = 137.0
    x = rng.poisson(lam, size=4_000_000).astype(float)
    ratio = float(x.var() / x.mean())
    res["criteria"]["T1_poisson_var_over_mean"] = ratio
    res["verdicts"]["T1"] = bool(abs(ratio - 1.0) < 0.02)

    # T2 天光 B↑ ⇒ 方差 B↑ + 解析闭合
    shape = (256, 256)
    rows = []
    for B in (10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0):
        zero = np.zeros(shape)
        sky = np.full(shape, B)
        f = expose(src_e_per_s=zero, sky_e_per_s=sky, det=det, exptime_s=1.0, rng=rng)
        sig = clipped_std_adu(f.adu)   # 一致估计量（MAD 在量化数据上有离散偏差）
        pred = sky_sigma_adu(sky_e=B, dark_e=det.dark_current_at(det.dark_ref_temp_c), det=det)
        rows.append({"sky_e": B, "sigma_adu": sig, "sigma_pred_adu": pred,
                     "rel_dev": sig / pred - 1.0})
    mono = all(rows[i + 1]["sigma_adu"] > rows[i]["sigma_adu"] for i in range(len(rows) - 1))
    worst = max(abs(r["rel_dev"]) for r in rows)
    res["criteria"]["T2_sky_series"] = rows
    res["verdicts"]["T2"] = bool(mono and worst < 0.05)

    # T3/T4 纯加性负例（配对同种子）
    def draw(seed: int) -> np.ndarray:
        r = np.random.default_rng(seed)
        return expose(
            src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, 30.0),
            det=det, exptime_s=1.0, rng=r,
        ).adu

    ref = draw(7)
    delta_adu = 500.0                       # 等价于"把天光加 750 e-"，但纯加性
    ctrl = ref + delta_adu
    s_ref, s_ctrl = robust_sigma_adu(ref), robust_sigma_adu(ctrl)
    res["criteria"]["T3_sigma_ref_adu"] = s_ref
    res["criteria"]["T3_sigma_ctrl_adu"] = s_ctrl
    res["criteria"]["T3_rel_change"] = abs(s_ctrl - s_ref) / s_ref
    res["verdicts"]["T3"] = bool(res["criteria"]["T3_rel_change"] < 1e-12)
    res["criteria"]["T4_snr_ratio_minus_1"] = abs((1.0 / s_ctrl) / (1.0 / s_ref) - 1.0)
    res["verdicts"]["T4"] = bool(res["criteria"]["T4_snr_ratio_minus_1"] < 1e-12)

    # T5 量化：含 1/12 ADU^2 项则闭合
    B = 30.0
    f = expose(src_e_per_s=np.zeros(shape), sky_e_per_s=np.full(shape, B), det=det,
               exptime_s=1.0, rng=rng)
    var_meas = clipped_std_adu(f.adu) ** 2
    var_pred = predicted_variance_adu2(src_e=0.0, sky_e=B,
                                       dark_e=det.dark_current_at(det.dark_ref_temp_c), det=det)
    res["criteria"]["T5_var_meas_adu2"] = var_meas
    res["criteria"]["T5_var_pred_adu2"] = var_pred
    res["criteria"]["T5_rel_dev"] = var_meas / var_pred - 1.0
    res["verdicts"]["T5"] = bool(abs(var_meas / var_pred - 1.0) < 0.05)

    # T6 饱和
    det_s = Detector(gain_e_per_adu=1.5, read_noise_e=5.0, bias_adu=1000.0, full_well_e=1500.0)
    f = expose(src_e_per_s=np.full((32, 32), 1e6), sky_e_per_s=np.zeros((32, 32)),
               det=det_s, exptime_s=1.0, rng=rng)
    res["criteria"]["T6_max_adu"] = float(f.adu.max())
    res["criteria"]["T6_saturation_adu"] = det_s.saturation_adu
    res["verdicts"]["T6"] = bool(abs(float(f.adu.max()) - det_s.saturation_adu) < 1e-9)

    # T7 暗电流温度依赖
    d0 = det.dark_current_at(-20.0)
    d1 = det.dark_current_at(-14.0)
    res["criteria"]["T7_dark_ratio"] = d1 / d0
    res["verdicts"]["T7"] = bool(abs(d1 / d0 - 2.0) < 1e-12)

    res["all_pass"] = bool(all(res["verdicts"].values()))
    if verbose:
        print(json.dumps(res, indent=2, ensure_ascii=False, default=float))
    return res


def main() -> int:
    ap = argparse.ArgumentParser(description="物理噪声合成器（noise_model）自检/信息")
    ap.add_argument("--selftest", action="store_true", help="运行自检（含负例）")
    ap.add_argument("--json", type=str, default=None, help="自检结果写 JSON 路径")
    a = ap.parse_args()
    if a.selftest:
        r = selftest()
        if a.json:
            with open(a.json, "w", encoding="utf-8") as fh:
                json.dump(r, fh, indent=2, ensure_ascii=False, default=float)
        return 0 if r["all_pass"] else 1
    print(__doc__)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
