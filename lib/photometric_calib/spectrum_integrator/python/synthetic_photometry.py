# -*- coding: utf-8 -*-
"""
Synthetic Photometry - 合成测光引擎
功能: 基于 SED + 滤光片透过率 + CCD QE 计算合成流量与合成星等
用途: 光度定标模块的合成测光阶段，为每颗 Gaia 参考星正向计算仪器系统下的理论流量

依据: docs/algorithm.md 第3节
核心公式:
  F_syn = ∫ S(λ) × T(λ) × Q(λ) × λ dλ
    S(λ): SED 光谱能量分布 (W/m²/nm)
    T(λ): 滤光片透过率 [0,1]
    Q(λ): CCD QE [0,1]，可选默认 1
    λ:    波长加权 (CCD 计量光子数 ∝ λ × 能量流量，常数 1/hc 在比值中消去)

数值方法:
  - 插值: Akima 子样条插值 (避免过冲)，滤光片/QE 范围外钳位为 0
  - 积分: Simpson 1/3 复合公式
  - 步长: 0.1 nm，积分范围取 SED/滤光片/QE 三者波长重叠区间

调用: from synthetic_photometry import SyntheticPhotometry
      f_syn = SyntheticPhotometry.compute(sed_wl, sed_flux, filter_wl, filter_trans)
依赖: numpy, scipy
"""

from __future__ import annotations

import logging

import numpy as np
from scipy.interpolate import Akima1DInterpolator
from scipy.integrate import simpson

logger = logging.getLogger(__name__)

_DEFAULT_WL_STEP = 1.0  # 原 0.1，改为 1.0nm 步长（Gaia 光谱原始步长 2nm，1nm 足够）


class SyntheticPhotometry:
    """合成测光引擎，计算 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ"""

    # ------------------------------------------------------------------
    # 内部工具: 清洗曲线数据 (排序 + 去重)
    # ------------------------------------------------------------------

    @staticmethod
    def _prepare_curve(wl, val) -> tuple[np.ndarray, np.ndarray]:
        """排序并去除重复波长点，返回严格单调递增的 (wl, val)"""
        wl = np.asarray(wl, dtype=np.float64)
        val = np.asarray(val, dtype=np.float64)
        order = np.argsort(wl, kind="mergesort")
        wl_s = wl[order]
        val_s = val[order]
        keep = np.concatenate(([True], np.diff(wl_s) > 0.0))
        return wl_s[keep], val_s[keep]

    # ------------------------------------------------------------------
    # Akima 插值 + 范围外钳位
    # ------------------------------------------------------------------

    @staticmethod
    def _interp_clamped(x_src: np.ndarray, y_src: np.ndarray,
                        x_grid: np.ndarray, fill: float = 0.0) -> np.ndarray:
        """Akima 子样条插值，超出源数据范围的点钳位为 fill"""
        akima = Akima1DInterpolator(x_src, y_src, extrapolate=False)
        y_grid = akima(x_grid)
        y_grid = np.where(np.isfinite(y_grid), y_grid, fill)
        return y_grid

    # ------------------------------------------------------------------
    # 合成流量计算 (单星)
    # ------------------------------------------------------------------

    @staticmethod
    def compute(sed_wl, sed_flux, filter_wl, filter_trans,
                qe_wl=None, qe_val=None, wl_step=_DEFAULT_WL_STEP) -> float:
        """
        计算单颗星的合成流量 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ

        参数:
            sed_wl, sed_flux: SED 波长(nm)与流量密度
            filter_wl, filter_trans: 滤光片波长(nm)与透过率 [0,1]
            qe_wl, qe_val: 可选 CCD QE 波长(nm)与值 [0,1]，默认 Q=1
            wl_step: 积分步长 (nm)，默认 0.1

        返回:
            F_syn: 合成流量 (标量)
        """
        sw, sf = SyntheticPhotometry._prepare_curve(sed_wl, sed_flux)
        fw, fv = SyntheticPhotometry._prepare_curve(filter_wl, filter_trans)

        # 积分范围: 三者(或两者)波长范围交集
        wl_min = max(sw[0], fw[0])
        wl_max = min(sw[-1], fw[-1])
        has_qe = qe_wl is not None and qe_val is not None
        if has_qe:
            qw, qv = SyntheticPhotometry._prepare_curve(qe_wl, qe_val)
            wl_min = max(wl_min, qw[0])
            wl_max = min(wl_max, qw[-1])

        if wl_min >= wl_max:
            logger.warning("波长重叠区间为空: [%.2f, %.2f], 返回 0.0", wl_min, wl_max)
            return 0.0

        # 0.1nm 等间距网格
        n_points = int(round((wl_max - wl_min) / wl_step)) + 1
        if n_points < 2:
            logger.warning("积分点数不足 (<2): [%.2f, %.2f], 返回 0.0", wl_min, wl_max)
            return 0.0
        grid = np.linspace(wl_min, wl_max, n_points, dtype=np.float64)

        # Akima 插值到网格 (滤光片/QE 范围外钳位为 0)
        s_grid = SyntheticPhotometry._interp_clamped(sw, sf, grid)
        t_grid = SyntheticPhotometry._interp_clamped(fw, fv, grid)
        if has_qe:
            q_grid = SyntheticPhotometry._interp_clamped(qw, qv, grid)
        else:
            q_grid = np.ones_like(grid)

        # 被积函数: S(λ)·T(λ)·Q(λ)·λ
        integrand = s_grid * t_grid * q_grid * grid

        # Simpson 1/3 复合积分
        f_syn = float(simpson(integrand, x=grid))
        logger.debug(
            "compute: 范围=[%.1f, %.1f]nm, %d点, F_syn=%.6e",
            wl_min, wl_max, n_points, f_syn,
        )
        return f_syn

    # ------------------------------------------------------------------
    # 批量计算
    # ------------------------------------------------------------------

    @staticmethod
    def compute_batch(sed_wl_list, sed_flux_list, filter_wl, filter_trans,
                      qe_wl=None, qe_val=None, wl_step=_DEFAULT_WL_STEP) -> np.ndarray:
        """
        批量计算多颗星的合成流量

        参数:
            sed_wl_list, sed_flux_list: 每颗星的 SED (列表，元素为 ndarray)
            filter_wl, filter_trans: 滤光片曲线 (所有星共用)
            qe_wl, qe_val: 可选 QE 曲线
            wl_step: 积分步长 (nm)

        返回:
            F_syn 数组 (n_stars,)
        """
        n = len(sed_wl_list)
        if n != len(sed_flux_list):
            raise ValueError(
                "sed_wl_list 与 sed_flux_list 长度不一致: %d vs %d" % (n, len(sed_flux_list))
            )
        results = np.empty(n, dtype=np.float64)
        for i in range(n):
            results[i] = SyntheticPhotometry.compute(
                sed_wl_list[i], sed_flux_list[i],
                filter_wl, filter_trans, qe_wl, qe_val, wl_step,
            )
        logger.info(
            "compute_batch: %d 颗星, F_syn 范围=[%.4e, %.4e]",
            n, float(results.min()), float(results.max()),
        )
        return results

    # ------------------------------------------------------------------
    # 合成星等
    # ------------------------------------------------------------------

    @staticmethod
    def compute_magnitude(sed_wl, sed_flux, filter_wl, filter_trans,
                          qe_wl=None, qe_val=None, wl_step=_DEFAULT_WL_STEP) -> float:
        """
        计算合成星等 m_syn = -2.5 × log10(F_syn) + C (C=0，在比值中消去)

        参数: 同 compute
        返回:
            m_syn: 合成星等 (标量)，F_syn<=0 时返回 inf
        """
        f_syn = SyntheticPhotometry.compute(
            sed_wl, sed_flux, filter_wl, filter_trans,
            qe_wl, qe_val, wl_step,
        )
        if f_syn <= 0.0:
            logger.warning("F_syn=%.6e <= 0, 无法计算星等, 返回 inf", f_syn)
            return float("inf")
        m_syn = -2.5 * np.log10(f_syn)
        logger.debug("compute_magnitude: F_syn=%.6e, m_syn=%.4f", f_syn, m_syn)
        return float(m_syn)


# ======================================================================
# 模块自测
# ======================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    from sed_builder import SEDBuilder
    from curve_loader import CurveLoader

    print("=" * 60)
    print("SyntheticPhotometry 模块自测")
    print("=" * 60)

    all_pass = True

    # 构造 SED (T=5800K 黑体) + 加载 Baader R 滤光片
    sed_wl, sed_flux = SEDBuilder.from_teff(5800.0)
    loader = CurveLoader()
    f_wl, f_val = loader.load_filter("Baader R")
    print("\nSED: %d 点, 范围 %.1f~%.1f nm" % (len(sed_wl), sed_wl[0], sed_wl[-1]))
    print("滤光片 Baader R: %d 点, 范围 %.1f~%.1f nm" % (len(f_wl), f_wl[0], f_wl[-1]))

    # ---- 验证 1: 合成流量计算 ----
    print("\n[验证 1] 合成流量 F_syn")
    f_syn = SyntheticPhotometry.compute(sed_wl, sed_flux, f_wl, f_val)
    print("  F_syn (Simpson) = %.6e" % f_syn)
    syn_ok = f_syn > 0.0 and np.isfinite(f_syn)
    print("  [%s] F_syn 为正有限值" % ("PASS" if syn_ok else "FAIL"))
    all_pass = all_pass and syn_ok

    # ---- 验证 2: Simpson vs trapz 参考积分, 误差 < 0.1% ----
    print("\n[验证 2] Simpson vs trapz 参考积分 (误差 < 0.1%)")
    # 重建网格与插值 (与 compute 内部一致)
    sw, sf = SyntheticPhotometry._prepare_curve(sed_wl, sed_flux)
    fw, fv = SyntheticPhotometry._prepare_curve(f_wl, f_val)
    wl_min = max(sw[0], fw[0])
    wl_max = min(sw[-1], fw[-1])
    n_pts = int(round((wl_max - wl_min) / 0.1)) + 1
    grid = np.linspace(wl_min, wl_max, n_pts, dtype=np.float64)
    s_g = SyntheticPhotometry._interp_clamped(sw, sf, grid)
    t_g = SyntheticPhotometry._interp_clamped(fw, fv, grid)
    integrand_ref = s_g * t_g * grid
    _trapz = getattr(np, "trapezoid", np.trapz)
    f_ref = float(_trapz(integrand_ref, grid))
    rel_err = abs(f_syn - f_ref) / f_ref if f_ref > 0 else float("inf")
    print("  F_syn (Simpson) = %.6e" % f_syn)
    print("  F_ref (trapz)   = %.6e" % f_ref)
    print("  相对误差 = %.6f%%" % (rel_err * 100))
    err_ok = rel_err < 0.001
    print("  [%s] 误差 < 0.1%%" % ("PASS" if err_ok else "FAIL"))
    all_pass = all_pass and err_ok

    # ---- 验证 3: 无 QE 与 QE=1 结果一致 ----
    print("\n[验证 3] 无 QE 与 QE=1 一致性")
    f_no_qe = SyntheticPhotometry.compute(sed_wl, sed_flux, f_wl, f_val)
    qe_wl = sed_wl.copy()
    qe_val = np.ones_like(sed_wl)
    f_qe1 = SyntheticPhotometry.compute(sed_wl, sed_flux, f_wl, f_val, qe_wl, qe_val)
    qe_diff = abs(f_no_qe - f_qe1)
    print("  F(无QE) = %.6e" % f_no_qe)
    print("  F(QE=1) = %.6e" % f_qe1)
    print("  差值    = %.6e" % qe_diff)
    qe_ok = qe_diff < 1e-12
    print("  [%s] 无QE == QE=1" % ("PASS" if qe_ok else "FAIL"))
    all_pass = all_pass and qe_ok

    # ---- 验证 4: 批量计算与单星一致 ----
    print("\n[验证 4] 批量计算")
    temps = [4000.0, 5800.0, 8000.0]
    wl_list, flux_list = [], []
    for T in temps:
        w, f = SEDBuilder.from_teff(T)
        wl_list.append(w)
        flux_list.append(f)
    batch = SyntheticPhotometry.compute_batch(wl_list, flux_list, f_wl, f_val)
    print("  批量结果: %s" % batch)
    batch_ok = len(batch) == 3 and np.all(batch > 0) and np.all(np.isfinite(batch))
    single = SyntheticPhotometry.compute(wl_list[1], flux_list[1], f_wl, f_val)
    consist_ok = abs(single - batch[1]) < 1e-12
    print("  [%s] 批量结果形状与有限性" % ("PASS" if batch_ok else "FAIL"))
    print("  [%s] 批量与单星一致" % ("PASS" if consist_ok else "FAIL"))
    all_pass = all_pass and batch_ok and consist_ok

    # ---- 验证 5: 合成星等 ----
    print("\n[验证 5] 合成星等")
    m_syn = SyntheticPhotometry.compute_magnitude(sed_wl, sed_flux, f_wl, f_val)
    m_expected = -2.5 * np.log10(f_syn)
    mag_ok = abs(m_syn - m_expected) < 1e-10
    print("  m_syn = %.6f, 期望 = %.6f" % (m_syn, m_expected))
    print("  [%s] m_syn = -2.5*log10(F_syn)" % ("PASS" if mag_ok else "FAIL"))
    all_pass = all_pass and mag_ok

    print("\n" + "=" * 60)
    print("测试结果: %s" % ("全部通过" if all_pass else "存在失败项"))
    print("=" * 60)
