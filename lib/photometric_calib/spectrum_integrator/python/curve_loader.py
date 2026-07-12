"""
曲线加载器 (Curve Loader)
功能: 加载滤光片透过率曲线与相机 QE 曲线
用途: 光谱积分器模块的基础数据读取，提供滤光片/QE 曲线的统一访问接口
数据: data/response_curves/filters.json (滤光片), qe_curves.json (QE曲线)
"""

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
            # curve_loader.py 在 lib/photometric_calib/spectrum_integrator/python/
            # data/ 在 lib/photometric_calib/data/response_curves/
            # 回溯 2 级到 lib/photometric_calib/
            module_root = os.path.normpath(os.path.join(base, "..", ".."))
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
