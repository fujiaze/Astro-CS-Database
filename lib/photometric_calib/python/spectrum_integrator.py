# -*- coding: utf-8 -*-
"""
Photometric Calib 光谱积分器 (合并文件)
功能: 合并 curve_loader.py 与 integrator.py
用途: 滤光片/QE曲线加载 + Gaia BP/RP 光谱积分计算合成流量 F_syn
合并日期: 2026-07-16
合并来源 (架构重构 spec G5 Phase 3):
  - curve_loader.py (原 lib/photometric_calib/spectrum_integrator/python/curve_loader.py)
  - integrator.py (原 lib/photometric_calib/spectrum_integrator/python/integrator.py)
路径调整:
  - data_dir: 从回溯2级 (spectrum_integrator/python/ -> photometric_calib/)
    改为回溯1级 (python/ -> photometric_calib/)
  - integrator.py 中 `from curve_loader import CurveLoader` 仅在 __main__ 中出现，
    随 __main__ 一起移除；CurveLoader 已在本文件 Part 1 定义
  - integrator.py 中 `from synthetic_photometry import SyntheticPhotometry` 保留
    (python/ 目录已有 synthetic_photometry.py)
"""

# ======================================================================
# Part 1: 曲线加载器 (原 spectrum_integrator/python/curve_loader.py)
# ----------------------------------------------------------------------
# 原文件 docstring:
#   曲线加载器 (Curve Loader)
#   功能: 加载滤光片透过率曲线与相机 QE 曲线
#   用途: 光谱积分器模块的基础数据读取，提供滤光片/QE 曲线的统一访问接口
#   数据: data/response_curves/filters.json (滤光片), qe_curves.json (QE曲线)
# ======================================================================

from __future__ import annotations

import json
import logging
import os
from typing import Optional

import numpy as np

logger = logging.getLogger(__name__)

_FILTERS_FILE = "filters.json"
_QE_FILE = "qe_curves.json"


class CurveLoader:
    """滤光片/QE曲线加载器，支持按名称加载并返回 (波长, 值) 数组"""

    def __init__(self, data_dir: Optional[str] = None):
        if data_dir is None:
            base = os.path.dirname(os.path.abspath(__file__))
            # 路径调整: 原 curve_loader.py 在 lib/photometric_calib/spectrum_integrator/python/
            # 现合并到 lib/photometric_calib/python/
            # data/ 在 lib/photometric_calib/data/response_curves/
            # 原回溯 2 级 (spectrum_integrator/python -> spectrum_integrator -> photometric_calib)
            # 现回溯 1 级 (python -> photometric_calib)
            module_root = os.path.normpath(os.path.join(base, ".."))
            data_dir = os.path.join(module_root, "data", "response_curves")
        self._data_dir = data_dir
        self._filters_path = os.path.join(data_dir, _FILTERS_FILE)
        self._qe_path = os.path.join(data_dir, _QE_FILE)
        self._filters_cache: Optional[dict] = None
        self._qe_cache: Optional[dict] = None
        logger.info("曲线加载器初始化, data_dir=%s", data_dir)

    def _load_json(self, path: str, label: str) -> dict:
        logger.info("加载%s曲线文件: %s", label, path)
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        logger.info("已加载 %d 条%s曲线", len(data), label)
        return data

    def _load_filters(self) -> dict:
        if self._filters_cache is None:
            self._filters_cache = self._load_json(self._filters_path, "滤光片")
        return self._filters_cache

    def _load_qe(self) -> dict:
        if self._qe_cache is None:
            self._qe_cache = self._load_json(self._qe_path, "QE")
        return self._qe_cache

    def _extract_curve(self, data: dict, name: str, curve_type: str) -> tuple[np.ndarray, np.ndarray]:
        if name not in data:
            available = list(data.keys())
            raise KeyError(
                f"{curve_type} '{name}' 不存在, 可用名称({len(available)}条): {available}"
            )
        entry = data[name]
        wavelength = np.asarray(entry["wavelength_nm"], dtype=np.float64)
        value = np.asarray(entry["value"], dtype=np.float64)
        logger.debug(
            "%s '%s': %d 个点, 波长范围 %.1f~%.1f nm",
            curve_type, name, len(wavelength), wavelength[0], wavelength[-1],
        )
        return wavelength, value

    def load_filter(self, name: str) -> tuple[np.ndarray, np.ndarray]:
        """按名称加载滤光片曲线, 返回 (wavelength_nm, transmittance)"""
        return self._extract_curve(self._load_filters(), name, "滤光片")

    def load_qe(self, name: str) -> tuple[np.ndarray, np.ndarray]:
        """按名称加载QE曲线, 返回 (wavelength_nm, qe)"""
        return self._extract_curve(self._load_qe(), name, "QE曲线")

    def list_filters(self) -> list[str]:
        """返回所有可用滤光片名称"""
        return list(self._load_filters().keys())

    def list_qe(self) -> list[str]:
        """返回所有可用QE曲线名称"""
        return list(self._load_qe().keys())


# ======================================================================
# Part 2: 光谱积分器 (原 spectrum_integrator/python/integrator.py)
# ----------------------------------------------------------------------
# 原文件 docstring:
#   Spectrum Integrator - 光谱积分器主程序
#   功能: 使用 Gaia DR3SP 真实 BP/RP 光谱计算合成流量 F_syn
#   用途: 光谱积分器模块主程序，将 Gaia BP/RP 光谱 (uint8[343], 336-1020nm) 作为 S(λ)，
#         结合滤光片透过率 T(λ) 与可选 CCD QE Q(λ)，通过 SyntheticPhotometry 引擎积分
#         F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ，用于测光定标参考星理论流量计算
#   依赖: numpy, synthetic_photometry (Akima插值+Simpson积分), logging
#   调用: from integrator import SpectrumIntegrator
#         integrator = SpectrumIntegrator(filter_wl, filter_trans, qe_wl, qe_val, spectrum_wl)
#         f_syn = integrator.integrate_star(spectrum_uint8)
#         results = integrator.integrate_batch(stars)
#         integrator.save_results(results, output_path, "Baader R", "GSENSE2020BSI")
#   数据流: Gaia uint8光谱 -> float64 S(λ) -> SyntheticPhotometry.compute -> F_syn 标量
# ======================================================================

from typing import List

from synthetic_photometry import SyntheticPhotometry

logger = logging.getLogger(__name__)

_WL_STEP = 0.1
_SPECTRUM_SOURCE = "gaia_bp_rp"


class SpectrumIntegrator:
    """光谱积分器，对 Gaia BP/RP 光谱逐星计算合成流量 F_syn

    将 Gaia 光谱 (uint8[343], 336-1020nm) 转为 float64 作为 S(λ)，结合滤光片
    透过率 T(λ) 与可选 CCD QE Q(λ)，调用 SyntheticPhotometry.compute 积分:
        F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ
    """

    def __init__(self, filter_wl, filter_trans, qe_wl=None, qe_val=None, spectrum_wl=None):
        """初始化积分器

        Args:
            filter_wl, filter_trans: 滤光片波长(nm)与透过率 [0,1]
            qe_wl, qe_val: QE 波长(nm)与值 [0,1]，可选，None 则 QE=1
            spectrum_wl: Gaia 光谱波长数组 [336, 338, ..., 1020] nm；
                         None 时在首次积分时自动设置
        """
        self._filter_wl = np.asarray(filter_wl, dtype=np.float64)
        self._filter_trans = np.asarray(filter_trans, dtype=np.float64)
        self._qe_wl = np.asarray(qe_wl, dtype=np.float64) if qe_wl is not None else None
        self._qe_val = np.asarray(qe_val, dtype=np.float64) if qe_val is not None else None
        self._spectrum_wl = np.asarray(spectrum_wl, dtype=np.float64) if spectrum_wl is not None else None
        self._wl_step = _WL_STEP

        has_qe = self._qe_wl is not None and self._qe_val is not None
        logger.info(
            "初始化光谱积分器: 滤光片波长范围 %.1f~%.1f nm (%d 点), QE=%s, 光谱波长=%s",
            float(self._filter_wl[0]), float(self._filter_wl[-1]), len(self._filter_wl),
            "已提供" if has_qe else "无(Q=1)",
            "已设置" if self._spectrum_wl is not None else "待首次积分设置",
        )

    def integrate_star(self, spectrum_uint8, spectrum_wl=None, mag_g=None) -> float:
        """单颗星积分

        Args:
            spectrum_uint8: np.ndarray uint8[343]，Gaia BP/RP 光谱（归一化光谱形状）
            spectrum_wl: 可选波长数组，None 则用 self._spectrum_wl
            mag_g: Gaia G 星等，用于绝对通量归一化。
                    Gaia BP/RP uint8 光谱不含绝对通量信息，
                    需乘以 10^(-0.4*mag_g) 恢复星等对应的流量比例。

        Returns:
            F_syn: 合成流量标量
        """
        # uint8 (0-255) 转为 float64，作为归一化光谱形状 S_norm(λ)
        sed_flux = np.asarray(spectrum_uint8, dtype=np.float64)

        # 用 G 星等归一化到绝对通量: S(λ) = S_norm(λ) × 10^(-0.4×mag_g)
        if mag_g is not None and np.isfinite(mag_g):
            sed_flux = sed_flux * (10.0 ** (-0.4 * mag_g))
        else:
            logger.warning("mag_g 缺失或无效，F_syn 未做星等归一化")

        if spectrum_wl is None:
            spectrum_wl = self._spectrum_wl
        if spectrum_wl is None:
            raise ValueError(
                "未设置光谱波长数组，请在 __init__ 或 integrate_star 中传入 spectrum_wl"
            )
        spectrum_wl = np.asarray(spectrum_wl, dtype=np.float64)

        # 首次积分自动缓存波长数组
        if self._spectrum_wl is None:
            self._spectrum_wl = spectrum_wl
            logger.info(
                "自动设置光谱波长数组: %.1f~%.1f nm (%d 点)",
                float(spectrum_wl[0]), float(spectrum_wl[-1]), len(spectrum_wl),
            )

        f_syn = SyntheticPhotometry.compute(
            spectrum_wl, sed_flux,
            self._filter_wl, self._filter_trans,
            self._qe_wl, self._qe_val,
            wl_step=self._wl_step,
        )
        return f_syn

    def integrate_batch(self, stars: list) -> list:
        """批量积分

        Args:
            stars: list[GaiaSpectrumStarPy]，每颗星含 spectrum, mag_g 字段

        Returns:
            list[dict]，每项含 source_id, ra, dec, mag_g, f_syn
        """
        n = len(stars)
        logger.info("开始批量积分, 星数=%d", n)
        results: List[dict] = []
        f_values: List[float] = []

        for i, star in enumerate(stars):
            f_syn = self.integrate_star(star.spectrum, mag_g=star.mag_g)
            results.append({
                "source_id": int(getattr(star, "source_id", 0)),
                "ra": float(star.ra),
                "dec": float(star.dec),
                "mag_g": float(star.mag_g),
                "f_syn": float(f_syn),
            })
            f_values.append(f_syn)
            if (i + 1) % 100 == 0:
                logger.info("积分进度: %d/%d", i + 1, n)

        if f_values:
            f_arr = np.asarray(f_values)
            logger.info(
                "批量积分完成: 星数=%d, F_syn 范围=[%.6e, %.6e]",
                n, float(f_arr.min()), float(f_arr.max()),
            )
        else:
            logger.info("批量积分完成: 星数=0")
        return results

    def save_results(self, results, output_path, filter_name, qe_name=None,
                     ra_center=None, dec_center=None, radius_deg=None):
        """保存结果到 JSON 文件

        Args:
            results: integrate_batch 返回的 list[dict]
            output_path: 输出 JSON 路径
            filter_name: 滤光片名称
            qe_name: QE 名称 (可选)
            ra_center, dec_center, radius_deg: 搜索中心与半径 (可选)
        """
        data = {
            "filter_name": filter_name,
            "qe_name": qe_name,
            "wl_step": self._wl_step,
            "spectrum_source": _SPECTRUM_SOURCE,
            "n_stars": len(results),
            "ra_center": ra_center,
            "dec_center": dec_center,
            "radius_deg": radius_deg,
            "stars": results,
        }
        out_dir = os.path.dirname(os.path.abspath(output_path))
        if out_dir and not os.path.isdir(out_dir):
            os.makedirs(out_dir, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        logger.info("结果已保存: %s (%d 颗星)", output_path, len(results))

    @staticmethod
    def load_results(json_path) -> dict:
        """从 JSON 文件读取结果"""
        with open(json_path, "r", encoding="utf-8") as f:
            return json.load(f)
