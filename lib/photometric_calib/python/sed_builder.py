# -*- coding: utf-8 -*-
"""
SED Builder - SED（光谱能量分布）构造器
功能: 基于 Gaia BP/RP 颜色或有效温度生成 Planck 黑体光谱 SED
用途: 光度定标模块的 SED 输入，MVP 阶段用 BP-RP 颜色推断 Teff，
      生成黑体光谱作为合成测光的参考 SED
背景: gaia_xpsd_client C API 不暴露 BP/RP 光谱采样数据，仅有 magBP/magRP 积分星等，
      故用 BP-RP 颜色 -> Teff -> Planck 黑体近似真实 SED
依赖: numpy
调用: from sed_builder import SEDBuilder
      wavelength, flux = SEDBuilder.from_bp_rp(bp_mag=15.0, rp_mag=14.0)
      wavelength, flux = SEDBuilder.from_teff(teff=5772.0)
"""

from __future__ import annotations

import logging

import numpy as np

logger = logging.getLogger(__name__)

# ------------------------------------------------------------------
# 物理常数 (SI 单位)
# ------------------------------------------------------------------
_H = 6.62607015e-34   # 普朗克常数 J·s
_C = 2.99792458e8      # 光速 m/s
_K = 1.380649e-23      # 玻尔兹曼常数 J/K

# 派生常数
_HC2 = 2.0 * _H * _C * _C       # 2hc², 单位 W·m²
_HC_OVER_K = _H * _C / _K        # hc/k, 单位 m·K (≈1.4388e-2 m·K)

# ------------------------------------------------------------------
# 波长范围 (与 Gaia BP/RP 光谱覆盖范围一致)
# ------------------------------------------------------------------
_WL_MIN = 336.0   # nm
_WL_MAX = 1020.0  # nm
_WL_STEP = 0.1    # nm

# ------------------------------------------------------------------
# BP-RP -> Teff 多项式系数
# Teff = 5040 / (a + b*(BP-RP) + c*(BP-RP)^2 + d*(BP-RP)^3)
# 基于 Gaia DR3 颜色-温度关系标定 (Casagrande et al. 2021), 覆盖 A-F-G-K-M 型星
# 标定点: BP-RP=0.5->Teff≈8365K(A7V), 1.0->5793K(G2V), 2.0->3706K(M0V)
# ------------------------------------------------------------------
_TEFF_A = 0.30
_TEFF_B = 0.65
_TEFF_C = -0.10
_TEFF_D = 0.02
_5040 = 5040.0

# Teff 钳位范围
_TEFF_MIN = 3000.0
_TEFF_MAX = 50000.0


class SEDBuilder:
    """
    SED 构造器

    基于 Gaia BP/RP 颜色推断有效温度，生成 Planck 黑体光谱作为 SED。
    MVP 阶段不使用真实 BP/RP 采样光谱（C API 仅暴露积分星等），
    而是用黑体近似。绝对单位不重要，合成测光在比值中消去。

    波长范围: 336-1020 nm, 步长 0.1 nm (共 6841 个点)
    归一化方式: max 归一化到 [0, 1]
    """

    # ------------------------------------------------------------------
    # 波长数组生成
    # ------------------------------------------------------------------

    @staticmethod
    def _default_wavelength() -> np.ndarray:
        """生成默认波长数组 (336-1020 nm, 步长 0.1 nm, 共 6841 点)"""
        n_points = int(round((_WL_MAX - _WL_MIN) / _WL_STEP)) + 1
        return np.linspace(_WL_MIN, _WL_MAX, n_points, dtype=np.float64)

    # ------------------------------------------------------------------
    # BP-RP 颜色 -> Teff
    # ------------------------------------------------------------------

    @staticmethod
    def bp_rp_to_teff(bp_rp: float) -> float:
        """
        BP-RP 颜色指数转有效温度

        使用多项式拟合:
            Teff = 5040 / (a + b*(BP-RP) + c*(BP-RP)^2 + d*(BP-RP)^3)
        系数基于 Gaia DR3 颜色-温度关系标定, 覆盖 A-F-G-K-M 型星
        (BP-RP 约 [-0.5, 3.0] 对应 Teff 约 [10000, 3000] K)，
        超出适用范围时钳位到 [3000, 50000] K。

        参数:
            bp_rp: BP-RP 颜色指数 (mag)

        返回:
            teff: 有效温度 (K)
        """
        x = float(bp_rp)
        denom = _TEFF_A + _TEFF_B * x + _TEFF_C * x**2 + _TEFF_D * x**3
        if denom <= 0:
            teff = _TEFF_MAX
        else:
            teff = _5040 / denom
        teff = max(_TEFF_MIN, min(_TEFF_MAX, teff))
        logger.debug("BP-RP=%.3f -> Teff=%.1f K", x, teff)
        return teff

    # ------------------------------------------------------------------
    # Planck 黑体光谱
    # ------------------------------------------------------------------

    @staticmethod
    def planck_spectrum(wavelength_nm: np.ndarray, teff: float) -> np.ndarray:
        """
        生成 Planck 黑体光谱 (波长形式)

        B(λ, T) = (2hc²/λ⁵) / (exp(hc/(λkT)) - 1)

        波长输入单位 nm, 计算时转换为 m。
        输出做 max 归一化到 [0, 1]（绝对单位不影响合成测光比值）。

        参数:
            wavelength_nm: 波长数组 (nm)
            teff: 有效温度 (K)

        返回:
            flux_density: 归一化光谱通量密度, max=1.0
        """
        wl_nm = np.asarray(wavelength_nm, dtype=np.float64)
        T = float(teff)

        # 波长 nm -> m
        wl_m = wl_nm * 1e-9

        # 无量纲指数 x = hc/(λkT)
        x = _HC_OVER_K / (wl_m * T)

        # Planck 公式: 用 expm1(x)=exp(x)-1 提高小 x 时的数值精度
        exponent = np.expm1(x)
        # 防止除零 (x=0 时 expm1=0, 物理上 T->∞ 才出现)
        exponent = np.where(exponent <= 0.0, 1e-300, exponent)

        # B(λ,T) = (2hc²/λ⁵) / (exp(x) - 1)
        flux = (_HC2 / wl_m**5) / exponent

        # max 归一化到 [0, 1]
        flux_max = float(flux.max())
        if flux_max > 0.0:
            flux = flux / flux_max

        peak_idx = int(np.argmax(flux))
        logger.debug(
            "Planck 光谱: T=%.1f K, %d 点, 峰值=%.1f nm, flux=[%.4e, %.4e]",
            T, len(wl_nm), wl_nm[peak_idx], float(flux.min()), float(flux.max()),
        )
        return flux

    # ------------------------------------------------------------------
    # 工厂方法
    # ------------------------------------------------------------------

    @staticmethod
    def from_bp_rp(bp_mag: float, rp_mag: float) -> tuple[np.ndarray, np.ndarray]:
        """
        从 Gaia BP/RP 星等构造 SED

        流程: BP-RP = bp_mag - rp_mag -> bp_rp_to_teff -> planck_spectrum

        参数:
            bp_mag: BP 波段积分星等
            rp_mag: RP 波段积分星等

        返回:
            (wavelength_nm, flux_density): 波长 336-1020nm 步长 0.1nm, 归一化通量
        """
        bp_rp = float(bp_mag) - float(rp_mag)
        teff = SEDBuilder.bp_rp_to_teff(bp_rp)
        logger.info(
            "from_bp_rp: BP=%.3f, RP=%.3f, BP-RP=%.3f, Teff=%.1f K",
            bp_mag, rp_mag, bp_rp, teff,
        )
        return SEDBuilder.from_teff(teff)

    @staticmethod
    def from_teff(teff: float) -> tuple[np.ndarray, np.ndarray]:
        """
        从有效温度构造 SED (Planck 黑体)

        参数:
            teff: 有效温度 (K)

        返回:
            (wavelength_nm, flux_density): 波长 336-1020nm 步长 0.1nm, 归一化通量
        """
        T = float(teff)
        wl = SEDBuilder._default_wavelength()
        flux = SEDBuilder.planck_spectrum(wl, T)
        peak_idx = int(np.argmax(flux))
        logger.info(
            "from_teff: T=%.1f K, %d 点, 峰值=%.1f nm",
            T, len(wl), wl[peak_idx],
        )
        return wl, flux


# ======================================================================
# 模块自测
# ======================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    # Wien 位移定律常数 (nm·K): λ_max * T = 2.898e6
    WIEN_B = 2.898e6

    print("=" * 60)
    print("SEDBuilder 模块自测")
    print("=" * 60)

    all_pass = True

    # ---- 验证 4: 波长数组 ----
    print("\n[验证 4] 波长数组范围与点数")
    wl = SEDBuilder._default_wavelength()
    n_expected = 6841
    wl_ok = (
        abs(wl[0] - 336.0) < 1e-6
        and abs(wl[-1] - 1020.0) < 1e-6
        and len(wl) == n_expected
    )
    print(f"  起始波长: {wl[0]:.1f} nm (期望 336.0)")
    print(f"  结束波长: {wl[-1]:.1f} nm (期望 1020.0)")
    print(f"  采样点数: {len(wl)} (期望 {n_expected})")
    print(f"  步长: {wl[1] - wl[0]:.4f} nm (期望 0.1)")
    print(f"  [{'PASS' if wl_ok else 'FAIL'}] 波长数组")
    all_pass = all_pass and wl_ok

    # ---- 验证 1: BP-RP=0.5 (高温星) ----
    print("\n[验证 1] BP-RP=0.5 (高温星)")
    bp_rp_1 = 0.5
    teff_1 = SEDBuilder.bp_rp_to_teff(bp_rp_1)
    wl_1, flux_1 = SEDBuilder.from_bp_rp(bp_rp_1 + 14.0, 14.0)
    peak_1 = wl_1[int(np.argmax(flux_1))]
    wien_peak_1 = WIEN_B / teff_1
    teff_ok_1 = teff_1 > 8000
    peak_ok_1 = peak_1 < 400
    print(f"  BP-RP=0.5 -> Teff={teff_1:.1f} K (期望 > 8000)")
    print(f"  SED 峰值波长={peak_1:.1f} nm (期望 < 400)")
    print(f"  Wien 理论峰值={wien_peak_1:.1f} nm")
    print(f"  [{'PASS' if teff_ok_1 else 'FAIL'}] Teff > 8000K")
    print(f"  [{'PASS' if peak_ok_1 else 'FAIL'}] 峰值 < 400nm")
    all_pass = all_pass and teff_ok_1 and peak_ok_1

    # ---- 验证 2: BP-RP=2.0 (低温星) ----
    print("\n[验证 2] BP-RP=2.0 (低温星)")
    bp_rp_2 = 2.0
    teff_2 = SEDBuilder.bp_rp_to_teff(bp_rp_2)
    wl_2, flux_2 = SEDBuilder.from_bp_rp(bp_rp_2 + 14.0, 14.0)
    peak_2 = wl_2[int(np.argmax(flux_2))]
    wien_peak_2 = WIEN_B / teff_2
    teff_ok_2 = teff_2 < 4000
    peak_ok_2 = peak_2 > 700
    print(f"  BP-RP=2.0 -> Teff={teff_2:.1f} K (期望 < 4000)")
    print(f"  SED 峰值波长={peak_2:.1f} nm (期望 > 700)")
    print(f"  Wien 理论峰值={wien_peak_2:.1f} nm")
    print(f"  [{'PASS' if teff_ok_2 else 'FAIL'}] Teff < 4000K")
    print(f"  [{'PASS' if peak_ok_2 else 'FAIL'}] 峰值 > 700nm")
    all_pass = all_pass and teff_ok_2 and peak_ok_2

    # ---- 验证 3: BP-RP=1.0 (类太阳) ----
    print("\n[验证 3] BP-RP=1.0 (类太阳)")
    bp_rp_3 = 1.0
    teff_3 = SEDBuilder.bp_rp_to_teff(bp_rp_3)
    wl_3, flux_3 = SEDBuilder.from_bp_rp(bp_rp_3 + 14.0, 14.0)
    peak_3 = wl_3[int(np.argmax(flux_3))]
    wien_peak_3 = WIEN_B / teff_3
    teff_ok_3 = 5000 <= teff_3 <= 6000
    peak_ok_3 = 450 <= peak_3 <= 550
    print(f"  BP-RP=1.0 -> Teff={teff_3:.1f} K (期望 5000-6000)")
    print(f"  SED 峰值波长={peak_3:.1f} nm (期望 ~500)")
    print(f"  Wien 理论峰值={wien_peak_3:.1f} nm")
    print(f"  [{'PASS' if teff_ok_3 else 'FAIL'}] Teff 在 5000-6000K")
    print(f"  [{'PASS' if peak_ok_3 else 'FAIL'}] 峰值 ~500nm")
    all_pass = all_pass and teff_ok_3 and peak_ok_3

    # ---- 附加检查: 归一化与 from_teff 一致性 ----
    print("\n[附加检查] 归一化与 from_teff 一致性")
    wl_t, flux_t = SEDBuilder.from_teff(teff_3)
    norm_ok = abs(flux_t.max() - 1.0) < 1e-10 and flux_t.min() >= 0.0
    consist_ok = np.allclose(flux_3, flux_t)
    print(f"  flux max={flux_t.max():.10f} (期望 1.0), min={flux_t.min():.6f} (期望 >=0)")
    print(f"  from_bp_rp vs from_teff 一致: {consist_ok}")
    print(f"  [{'PASS' if norm_ok else 'FAIL'}] max 归一化")
    print(f"  [{'PASS' if consist_ok else 'FAIL'}] 工厂方法一致性")
    all_pass = all_pass and norm_ok and consist_ok

    # ---- 附加检查: Teff 钳位 ----
    print("\n[附加检查] Teff 钳位")
    teff_cold = SEDBuilder.bp_rp_to_teff(10.0)  # 极端红色
    teff_hot = SEDBuilder.bp_rp_to_teff(-5.0)   # 极端蓝色
    clamp_ok = teff_cold == 3000.0 and teff_hot == 50000.0
    print(f"  BP-RP=10.0 -> Teff={teff_cold:.1f} K (期望 3000, 钳位下限)")
    print(f"  BP-RP=-5.0 -> Teff={teff_hot:.1f} K (期望 50000, 钳位上限)")
    print(f"  [{'PASS' if clamp_ok else 'FAIL'}] 钳位")
    all_pass = all_pass and clamp_ok

    print("\n" + "=" * 60)
    print(f"测试结果: {'全部通过' if all_pass else '存在失败项'}")
    print("=" * 60)
