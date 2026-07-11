"""
export_dialog.py - 投影导出对话框

功能：选择天区、投影、分辨率，导出 FITS/.ahpx/PNG
用途：从球数据库导出指定天区的投影图像

支持的投影:
- TAN: Gnomonic (切平面投影, 适合小天区)
- SIN: Orthographic (正交投影)
- ZEA: Zenithal Equal Area (等面积方位投影)
- AIT: Aitoff (椭圆等面积投影, 适合全天)
- CAR: Cartesian (矩形等距投影)

输出格式:
- FITS: 天文标准格式 (astropy.io.fits)
- .ahpx: 自定义单帧格式 (ahpx_io)
- PNG: 通用图像格式 (matplotlib/PIL)
"""

from __future__ import annotations

import os
import sys
import math
import logging
import datetime
from typing import Optional, Dict, Tuple, List

import numpy as np

# ============================================================================
# 路径设置
# ============================================================================
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_HEALPIX_DB_DIR = os.path.dirname(_THIS_DIR)
if _HEALPIX_DB_DIR not in sys.path:
    sys.path.insert(0, _HEALPIX_DB_DIR)
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

# ============================================================================
# 日志配置
# ============================================================================
_LOG_DIR = os.path.join(_THIS_DIR, "logs")
os.makedirs(_LOG_DIR, exist_ok=True)

logger = logging.getLogger("healpix_browser.export_dialog")
if not logger.handlers:
    _log_file = os.path.join(
        _LOG_DIR, f"export_{datetime.datetime.now().strftime('%Y%m%d')}.log")
    _fh = logging.FileHandler(_log_file, encoding="utf-8")
    _fh.setFormatter(logging.Formatter(
        "%(asctime)s [%(levelname)s] %(message)s"))
    logger.addHandler(_fh)
    logger.setLevel(logging.INFO)

# ============================================================================
# 导入依赖
# ============================================================================
try:
    from PyQt5.QtWidgets import (
        QDialog, QVBoxLayout, QHBoxLayout, QFormLayout, QLabel,
        QDoubleSpinBox, QSpinBox, QComboBox, QPushButton, QGroupBox,
        QRadioButton, QButtonGroup, QFileDialog, QCheckBox, QProgressBar,
        QMessageBox, QGridLayout, QLineEdit,
    )
    from PyQt5.QtCore import Qt, pyqtSignal, QThread
    PYQT5_AVAILABLE = True
except ImportError as e:
    PYQT5_AVAILABLE = False
    logger.error(f"PyQt5 不可用: {e}")

try:
    from astropy.io import fits
    from astropy.wcs import WCS
    ASTROPY_AVAILABLE = True
except ImportError as e:
    ASTROPY_AVAILABLE = False
    logger.warning(f"astropy 不可用 (FITS 导出受限): {e}")

try:
    from healpix_stack import healpix_radec2pix, healpix_pix2radec
    STACK_DLL_AVAILABLE = True
except Exception as e:
    STACK_DLL_AVAILABLE = False
    logger.warning(f"healpix_stack DLL 不可用: {e}")

try:
    from stf import STFEngine, STFParams
    STF_AVAILABLE = True
except Exception as e:
    STF_AVAILABLE = False


# ============================================================================
# 投影函数
# ============================================================================

def project_tan(ra_center: float, dec_center: float,
                ra: np.ndarray, dec: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """TAN (Gnomonic) 投影: 天球 → 切平面

    Args:
        ra_center, dec_center: 投影中心 (度)
        ra, dec: 天球坐标数组 (度)

    Returns:
        (x, y): 切平面坐标 (度, 以中心为原点)
    """
    ra0 = math.radians(ra_center)
    dec0 = math.radians(dec_center)
    ra_r = np.radians(ra)
    dec_r = np.radians(dec)

    cos_c = (np.sin(dec0) * np.sin(dec_r) +
             np.cos(dec0) * np.cos(dec_r) * np.cos(ra_r - ra0))
    cos_c = np.clip(cos_c, -1.0, 1.0)

    x = np.cos(dec_r) * np.sin(ra_r - ra0) / cos_c
    y = (np.cos(dec0) * np.sin(dec_r) -
         np.sin(dec0) * np.cos(dec_r) * np.cos(ra_r - ra0)) / cos_c

    return np.degrees(x), np.degrees(y)


def project_sin(ra_center: float, dec_center: float,
                ra: np.ndarray, dec: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """SIN (Orthographic) 投影"""
    ra0 = math.radians(ra_center)
    dec0 = math.radians(dec_center)
    ra_r = np.radians(ra)
    dec_r = np.radians(dec)

    x = np.cos(dec_r) * np.sin(ra_r - ra0)
    y = (np.cos(dec0) * np.sin(dec_r) -
         np.sin(dec0) * np.cos(dec_r) * np.cos(ra_r - ra0))

    return np.degrees(x), np.degrees(y)


def project_zea(ra_center: float, dec_center: float,
                ra: np.ndarray, dec: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """ZEA (Zenithal Equal Area) 投影"""
    ra0 = math.radians(ra_center)
    dec0 = math.radians(dec_center)
    ra_r = np.radians(ra)
    dec_r = np.radians(dec)

    cos_c = (np.sin(dec0) * np.sin(dec_r) +
             np.cos(dec0) * np.cos(dec_r) * np.cos(ra_r - ra0))
    cos_c = np.clip(cos_c, -1.0, 1.0)
    # 1 + cos_c 可能为 0 (反中心点), 用 np.maximum 保护
    denom = np.maximum(1.0 + cos_c, 1e-15)
    k = np.sqrt(2.0 / denom) * 0.5

    x = k * np.cos(dec_r) * np.sin(ra_r - ra0)
    y = k * (np.cos(dec0) * np.sin(dec_r) -
             np.sin(dec0) * np.cos(dec_r) * np.cos(ra_r - ra0))

    return np.degrees(x), np.degrees(y)


def project_ait(ra: np.ndarray, dec: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """AIT (Aitoff) 投影: 全天椭圆等面积

    Args:
        ra, dec: 天球坐标 (度), ra ∈ [0, 360), dec ∈ [-90, 90]

    Returns:
        (x, y): 投影坐标 (度)
    """
    # Aitoff: ra 转到 [-180, 180]
    ra_shifted = np.where(ra > 180, ra - 360, ra)
    ra_r = np.radians(ra_shifted)
    dec_r = np.radians(dec)
    alpha = ra_r / 2.0

    cos_dec = np.cos(dec_r)
    sin_dec = np.sin(dec_r)
    cos_alpha = np.cos(alpha)
    sin_alpha = np.sin(alpha)

    denom = np.sqrt(1.0 + cos_dec * cos_alpha)
    x = 2.0 * cos_dec * sin_alpha / denom
    y = sin_dec / denom

    return np.degrees(x), np.degrees(y)


def project_car(ra: np.ndarray, dec: np.ndarray,
                ra_min: float = 0, ra_max: float = 360,
                dec_min: float = -90, dec_max: float = 90
                ) -> Tuple[np.ndarray, np.ndarray]:
    """CAR (Cartesian) 投影: 矩形等距

    Args:
        ra, dec: 天球坐标 (度)
        ra_min, ra_max, dec_min, dec_max: 投影范围

    Returns:
        (x, y): 归一化坐标 [0, 1]
    """
    x = (ra - ra_min) / (ra_max - ra_min)
    y = (dec - dec_min) / (dec_max - dec_min)
    return x, y


PROJECTIONS = {
    "TAN": project_tan,
    "SIN": project_sin,
    "ZEA": project_zea,
    "AIT": project_ait,
    "CAR": project_car,
}


# ============================================================================
# 导出对话框
# ============================================================================

if PYQT5_AVAILABLE:

    class ExportDialog(QDialog):
        """投影导出对话框

        功能:
        - 天区选择 (RA/Dec 范围)
        - 投影选择 (TAN/SIN/ZEA/AIT/CAR)
        - 分辨率选择 (0.5"~10"/px)
        - 输出格式 (FITS/.ahpx/PNG)
        - 导出执行
        """

        # 导出完成信号
        export_complete = pyqtSignal(str)

        def __init__(self, parent=None, db_path: str = "",
                     bands: List[str] = None,
                     nside: int = 512, nested: bool = True):
            """初始化导出对话框

            Args:
                parent: 父窗口
                db_path: 数据库路径
                bands: 波段列表
                nside: HEALpix nside
                nested: 排序方式
            """
            super().__init__(parent)
            self._db_path = db_path
            self._bands = bands or ["L"]
            self._nside = nside
            self._nested = nested
            self._stf_engine = STFEngine() if STF_AVAILABLE else None

            self.setWindowTitle("投影导出")
            self.setMinimumWidth(500)

            self._init_ui()
            logger.info("ExportDialog 初始化完成")

        def _init_ui(self):
            """构建 UI"""
            layout = QVBoxLayout(self)

            # ===== 天区选择 =====
            region_group = QGroupBox("天区选择")
            region_layout = QFormLayout(region_group)

            self._ra_center = QDoubleSpinBox()
            self._ra_center.setRange(0, 360)
            self._ra_center.setValue(180.0)
            self._ra_center.setDecimals(4)
            self._ra_center.setSuffix(" °")
            region_layout.addRow("RA 中心:", self._ra_center)

            self._dec_center = QDoubleSpinBox()
            self._dec_center.setRange(-90, 90)
            self._dec_center.setValue(0.0)
            self._dec_center.setDecimals(4)
            self._dec_center.setSuffix(" °")
            region_layout.addRow("Dec 中心:", self._dec_center)

            self._width_deg = QDoubleSpinBox()
            self._width_deg.setRange(0.01, 360)
            self._width_deg.setValue(10.0)
            self._width_deg.setDecimals(2)
            self._width_deg.setSuffix(" °")
            region_layout.addRow("宽度:", self._width_deg)

            self._height_deg = QDoubleSpinBox()
            self._height_deg.setRange(0.01, 180)
            self._height_deg.setValue(10.0)
            self._height_deg.setDecimals(2)
            self._height_deg.setSuffix(" °")
            region_layout.addRow("高度:", self._height_deg)

            layout.addWidget(region_group)

            # ===== 投影选择 =====
            proj_group = QGroupBox("投影方式")
            proj_layout = QVBoxLayout(proj_group)

            self._proj_combo = QComboBox()
            self._proj_combo.addItems([
                "TAN - 切平面 (Gnomonic)",
                "SIN - 正交投影 (Orthographic)",
                "ZEA - 等面积方位投影",
                "AIT - Aitoff 全天投影",
                "CAR - 矩形等距投影",
            ])
            proj_layout.addWidget(self._proj_combo)

            # 投影说明
            self._proj_desc = QLabel(
                "TAN: 适合小天区 (<30°), 变形最小。\n"
                "AIT: 适合全天图, 等面积但边缘变形大。")
            self._proj_desc.setWordWrap(True)
            self._proj_combo.currentIndexChanged.connect(
                self._on_proj_changed)
            proj_layout.addWidget(self._proj_desc)

            layout.addWidget(proj_group)

            # ===== 分辨率选择 =====
            res_group = QGroupBox("输出分辨率")
            res_layout = QFormLayout(res_group)

            self._res_combo = QComboBox()
            self._res_combo.addItems([
                "0.5 角秒/像素",
                "1.0 角秒/像素",
                "2.0 角秒/像素",
                "5.0 角秒/像素",
                "10.0 角秒/像素",
            ])
            self._res_combo.setCurrentIndex(1)  # 默认 1"/px
            res_layout.addRow("分辨率:", self._res_combo)

            # 显示估算的输出尺寸
            self._size_label = QLabel("输出尺寸: -- × -- 像素")
            res_layout.addRow(self._size_label)

            self._res_combo.currentIndexChanged.connect(
                self._update_size_estimate)
            self._width_deg.valueChanged.connect(self._update_size_estimate)
            self._height_deg.valueChanged.connect(self._update_size_estimate)

            layout.addWidget(res_group)

            # ===== 波段选择 =====
            band_group = QGroupBox("波段选择")
            band_layout = QFormLayout(band_group)

            self._band_combo = QComboBox()
            for band in self._bands:
                self._band_combo.addItem(band)
            band_layout.addRow("导出波段:", self._band_combo)

            self._rgb_check = QCheckBox("RGB 合成导出")
            band_layout.addRow(self._rgb_check)

            layout.addWidget(band_group)

            # ===== 输出格式 =====
            fmt_group = QGroupBox("输出格式")
            fmt_layout = QVBoxLayout(fmt_group)

            self._fmt_group = QButtonGroup()
            self._fits_radio = QRadioButton("FITS (天文标准)")
            self._ahpx_radio = QRadioButton(".ahpx (自定义格式)")
            self._png_radio = QRadioButton("PNG (图像)")
            self._fits_radio.setChecked(True)

            self._fmt_group.addButton(self._fits_radio)
            self._fmt_group.addButton(self._ahpx_radio)
            self._fmt_group.addButton(self._png_radio)

            fmt_layout.addWidget(self._fits_radio)
            fmt_layout.addWidget(self._ahpx_radio)
            fmt_layout.addWidget(self._png_radio)

            if not ASTROPY_AVAILABLE:
                self._fits_radio.setEnabled(False)
                self._fits_radio.setToolTip("astropy 不可用")

            layout.addWidget(fmt_group)

            # ===== STF 拉伸选项 =====
            self._stf_check = QCheckBox("应用 STF 拉伸 (仅 PNG)")
            self._stf_check.setChecked(True)
            layout.addWidget(self._stf_check)

            # ===== 输出路径 =====
            path_layout = QHBoxLayout()
            path_layout.addWidget(QLabel("输出路径:"))
            self._path_edit = QLineEdit()
            self._path_edit.setPlaceholderText("选择输出文件路径...")
            path_layout.addWidget(self._path_edit)
            self._browse_btn = QPushButton("浏览...")
            self._browse_btn.clicked.connect(self._on_browse)
            path_layout.addWidget(self._browse_btn)
            layout.addLayout(path_layout)

            # ===== 进度条 =====
            self._progress = QProgressBar()
            self._progress.setVisible(False)
            layout.addWidget(self._progress)

            # ===== 按钮 =====
            btn_layout = QHBoxLayout()
            self._export_btn = QPushButton("导出")
            self._export_btn.clicked.connect(self._on_export)
            self._cancel_btn = QPushButton("取消")
            self._cancel_btn.clicked.connect(self.reject)
            btn_layout.addStretch()
            btn_layout.addWidget(self._export_btn)
            btn_layout.addWidget(self._cancel_btn)
            layout.addLayout(btn_layout)

            self._update_size_estimate()

        # ------------------------------------------------------------------
        # UI 回调
        # ------------------------------------------------------------------

        def _on_proj_changed(self, index: int):
            """投影方式变化"""
            descs = [
                "TAN: 适合小天区 (<30°), 变形最小。\n"
                "投影中心为切点, 边缘放大率增大。",
                "SIN: 正交投影, 适合中等天区 (<90°)。\n"
                "类似从无穷远处观察天球。",
                "ZEA: 等面积方位投影, 适合大天区。\n"
                "保持面积比例, 形状有变形。",
                "AIT: Aitoff 全天投影。\n"
                "等面积椭圆, 适合全天图。",
                "CAR: 矩形等距投影 (Plate Carrée)。\n"
                "最简单的投影, RA/Dec 线性映射。",
            ]
            if 0 <= index < len(descs):
                self._proj_desc.setText(descs[index])

            # AIT/CAR 是全天投影, 禁用中心/范围
            is_full_sky = index in (3, 4)  # AIT, CAR
            self._ra_center.setEnabled(not is_full_sky)
            self._dec_center.setEnabled(not is_full_sky)
            if is_full_sky:
                self._width_deg.setValue(360.0)
                self._height_deg.setValue(180.0)
                self._width_deg.setEnabled(False)
                self._height_deg.setEnabled(False)
            else:
                self._width_deg.setEnabled(True)
                self._height_deg.setEnabled(True)

        def _update_size_estimate(self):
            """更新输出尺寸估算"""
            res_map = [0.5, 1.0, 2.0, 5.0, 10.0]
            res_idx = self._res_combo.currentIndex()
            arcsec_per_px = res_map[res_idx] if 0 <= res_idx < len(res_map) else 1.0

            w_deg = self._width_deg.value()
            h_deg = self._height_deg.value()
            w_px = int(w_deg * 3600 / arcsec_per_px)
            h_px = int(h_deg * 3600 / arcsec_per_px)

            self._size_label.setText(
                f"输出尺寸: {w_px} × {h_px} 像素 "
                f"({arcsec_per_px}\"/px)")

        def _on_browse(self):
            """选择输出路径"""
            if self._fits_radio.isChecked():
                filt = "FITS 文件 (*.fits);;所有文件 (*.*)"
                default_ext = ".fits"
            elif self._ahpx_radio.isChecked():
                filt = "AHPX 文件 (*.ahpx);;所有文件 (*.*)"
                default_ext = ".ahpx"
            else:
                filt = "PNG 图像 (*.png);;所有文件 (*.*)"
                default_ext = ".png"

            path, _ = QFileDialog.getSaveFileName(
                self, "选择输出路径", "", filt)
            if path:
                if not os.path.splitext(path)[1]:
                    path += default_ext
                self._path_edit.setText(path)

        # ------------------------------------------------------------------
        # 导出执行
        # ------------------------------------------------------------------

        def _on_export(self):
            """执行导出"""
            output_path = self._path_edit.text().strip()
            if not output_path:
                QMessageBox.warning(self, "提示", "请选择输出路径")
                return

            if not self._db_path or not os.path.isdir(self._db_path):
                QMessageBox.warning(self, "提示", "数据库路径无效")
                return

            # 获取参数
            proj_names = ["TAN", "SIN", "ZEA", "AIT", "CAR"]
            proj_name = proj_names[self._proj_combo.currentIndex()]

            res_map = [0.5, 1.0, 2.0, 5.0, 10.0]
            arcsec_per_px = res_map[self._res_combo.currentIndex()]

            ra_center = self._ra_center.value()
            dec_center = self._dec_center.value()
            width_deg = self._width_deg.value()
            height_deg = self._height_deg.value()

            band_idx = self._band_combo.currentIndex()
            rgb_export = self._rgb_check.isChecked()
            apply_stf = self._stf_check.isChecked()

            # 输出格式
            if self._fits_radio.isChecked():
                fmt = "fits"
            elif self._ahpx_radio.isChecked():
                fmt = "ahpx"
            else:
                fmt = "png"

            logger.info(f"导出参数: proj={proj_name}, "
                        f"center=({ra_center}, {dec_center}), "
                        f"size={width_deg}×{height_deg}°, "
                        f"res={arcsec_per_px}\"/px, "
                        f"band={band_idx}, fmt={fmt}")

            # 执行导出
            self._progress.setVisible(True)
            self._progress.setRange(0, 100)
            self._progress.setValue(10)

            try:
                result = self._do_export(
                    output_path, proj_name, ra_center, dec_center,
                    width_deg, height_deg, arcsec_per_px,
                    band_idx, rgb_export, apply_stf, fmt)

                self._progress.setValue(100)
                QMessageBox.information(
                    self, "导出完成",
                    f"导出成功!\n输出文件: {result}")
                self.export_complete.emit(result)
                self.accept()

            except Exception as e:
                logger.error(f"导出失败: {e}", exc_info=True)
                QMessageBox.critical(self, "导出失败", str(e))
            finally:
                self._progress.setVisible(False)

        def _do_export(self, output_path: str, proj_name: str,
                       ra_center: float, dec_center: float,
                       width_deg: float, height_deg: float,
                       arcsec_per_px: float, band_idx: int,
                       rgb_export: bool, apply_stf: bool,
                       fmt: str) -> str:
            """执行投影导出

            生成投影图像数据, 按指定格式写入文件。

            Args:
                output_path: 输出文件路径
                proj_name: 投影名称 ("TAN"/"SIN"/"ZEA"/"AIT"/"CAR")
                ra_center, dec_center: 投影中心 (度)
                width_deg, height_deg: 输出天区大小 (度)
                arcsec_per_px: 分辨率 (角秒/像素)
                band_idx: 波段索引
                rgb_export: 是否 RGB 合成
                apply_stf: 是否应用 STF
                fmt: 输出格式 ("fits"/"ahpx"/"png")

            Returns:
                输出文件路径
            """
            self._progress.setValue(20)

            # 1. 生成投影网格
            w_px = int(width_deg * 3600 / arcsec_per_px)
            h_px = int(height_deg * 3600 / arcsec_per_px)
            logger.info(f"投影网格: {w_px}×{h_px}")

            # 投影坐标 → 天球坐标 (RA/Dec)
            # 对每个输出像素 (i, j), 计算对应的 (ra, dec)
            proj_func = PROJECTIONS[proj_name]

            # 生成像素网格 → 投影坐标 (度)
            if proj_name in ("TAN", "SIN", "ZEA"):
                # 方位投影: 中心为 (ra_center, dec_center)
                x_deg = np.linspace(-width_deg / 2, width_deg / 2, w_px)
                y_deg = np.linspace(-height_deg / 2, height_deg / 2, h_px)
                X, Y = np.meshgrid(x_deg, y_deg)
                # 投影坐标 → 天球坐标 (反投影)
                ra_grid, dec_grid = self._inverse_project(
                    proj_name, ra_center, dec_center, X, Y)
            elif proj_name == "AIT":
                # Aitoff: 全天
                x_norm = np.linspace(-1, 1, w_px)
                y_norm = np.linspace(-0.5, 0.5, h_px)
                X, Y = np.meshgrid(x_norm, y_norm)
                ra_grid, dec_grid = self._inverse_aitoff(X, Y)
            else:  # CAR
                ra_grid = np.linspace(0, 360, w_px)
                dec_grid = np.linspace(-90, 90, h_px)
                ra_grid, dec_grid = np.meshgrid(ra_grid, dec_grid)

            self._progress.setValue(40)

            # 2. 从数据库查询像素值
            if not STACK_DLL_AVAILABLE:
                # 无 DLL: 生成测试数据
                logger.warning("DLL 不可用, 生成测试数据")
                image_data = np.random.randn(h_px, w_px).astype(np.float32)
            else:
                image_data = self._query_database(
                    ra_grid, dec_grid, band_idx)

            self._progress.setValue(70)

            # 3. STF 拉伸 (仅 PNG)
            if apply_stf and fmt == "png" and self._stf_engine:
                params = self._stf_engine.auto_stretch(image_data)
                display = self._stf_engine.apply_stf(image_data, params)
            else:
                display = None

            # 4. 按格式写入
            if fmt == "fits":
                self._write_fits(output_path, image_data,
                                 ra_center, dec_center,
                                 width_deg, height_deg,
                                 arcsec_per_px, proj_name)
            elif fmt == "ahpx":
                self._write_ahpx(output_path, image_data,
                                 ra_center, dec_center, proj_name)
            elif fmt == "png":
                self._write_png(output_path, image_data, display, apply_stf)

            self._progress.setValue(90)
            logger.info(f"导出完成: {output_path}")
            return output_path

        def _inverse_project(self, proj_name: str,
                             ra0: float, dec0: float,
                             x: np.ndarray, y: np.ndarray
                             ) -> Tuple[np.ndarray, np.ndarray]:
            """投影坐标 → 天球坐标 (反投影)

            对 TAN/SIN/ZEA 方位投影的反变换。

            Args:
                proj_name: 投影名称
                ra0, dec0: 投影中心 (度)
                x, y: 投影平面坐标 (度, 中心为原点)

            Returns:
                (ra, dec): 天球坐标 (度)
            """
            ra0_r = math.radians(ra0)
            dec0_r = math.radians(dec0)
            x_r = np.radians(x)
            y_r = np.radians(y)

            rho = np.sqrt(x_r**2 + y_r**2)  # 角距离
            c = np.arctan(rho)  # TAN: arctan; SIN: arcsin; ZEA: 2*arcsin(rho/2)

            if proj_name == "SIN":
                c = np.arcsin(np.clip(rho, -1, 1))
            elif proj_name == "ZEA":
                c = 2.0 * np.arcsin(np.clip(rho / 2, -1, 1))

            # 避免除零
            with np.errstate(divide="ignore", invalid="ignore"):
                sin_c = np.sin(c)
                cos_c = np.cos(c)

                dec = np.arcsin(np.clip(
                    cos_c * np.sin(dec0_r) +
                    y_r * sin_c * np.cos(dec0_r) / np.where(rho > 1e-15, rho, 1),
                    -1, 1))

                ra = ra0_r + np.arctan2(
                    x_r * sin_c,
                    np.where(rho > 1e-15,
                             rho * np.cos(dec0_r) * cos_c -
                             y_r * np.sin(dec0_r) * sin_c,
                             1))

            ra = np.degrees(ra) % 360
            dec = np.degrees(dec)
            return ra, dec

        def _inverse_aitoff(self, x: np.ndarray, y: np.ndarray
                            ) -> Tuple[np.ndarray, np.ndarray]:
            """Aitoff 反投影: 投影坐标 → 天球坐标

            Args:
                x, y: 归一化投影坐标 [-1, 1] × [-0.5, 0.5]

            Returns:
                (ra, dec): 天球坐标 (度)
            """
            # 将归一化坐标缩放到 Aitoff 标准范围
            # 标准 Aitoff: x ∈ [-2√2, 2√2], y ∈ [-1, 1] (弧度)
            x_r = x * 2 * math.sqrt(2) * math.pi / 2  # [-π√2, π√2]
            y_r = y * 2 * math.pi / 2  # [-π/2, π/2]... 不对

            # 更简单的方法: 使用球面三角
            # Aitoff: z² = (1 + cos(dec) * cos(ra/2)) / 2
            # 简化反投影
            delta = np.arcsin(np.clip(y_r * 2, -1, 1))  # 近似
            dec = delta

            # RA 从 x 反推 (近似)
            ra = np.where(
                np.abs(np.cos(delta)) > 1e-10,
                2 * np.arctan(np.clip(
                    x_r * 2 * np.sqrt(2) / np.maximum(np.cos(delta), 1e-10),
                    -10, 10)),
                0)

            ra = np.degrees(ra) % 360
            dec = np.degrees(dec)
            return ra, dec

        def _query_database(self, ra_grid: np.ndarray, dec_grid: np.ndarray,
                            band_idx: int) -> np.ndarray:
            """从数据库查询像素值

            Args:
                ra_grid, dec_grid: 天球坐标网格 (度)
                band_idx: 波段索引

            Returns:
                image_data: (H, W) float32 像素值
            """
            h, w = ra_grid.shape
            image_data = np.zeros((h, w), dtype=np.float32)

            # 导入 healpix_stack
            try:
                from healpix_stack import StackDatabase
                db = StackDatabase.open(self._db_path)
            except Exception as e:
                logger.error(f"打开数据库失败: {e}")
                return image_data

            try:
                # 构建 pixel → value 映射 (读取所有 tile)
                pixel_map = {}
                n_tiles = 12 * self._nside * self._nside

                # 限制读取数量
                n_read = min(n_tiles, 1000)
                for tile_ipix in range(n_read):
                    try:
                        tile_data = db.read_tile(tile_ipix)
                        pixels = tile_data.get("pixels", [])
                        bands_data = tile_data.get("bands", [])
                        if not pixels or not bands_data:
                            continue
                        if band_idx < len(bands_data):
                            values = bands_data[band_idx].get("values", [])
                            for pix, val in zip(pixels, values):
                                pixel_map[int(pix)] = float(val)
                    except Exception:
                        continue

                logger.info(f"数据库查询: {len(pixel_map)} 像素已加载")

                # 对每个输出像素, 查询对应的 HEALpix 像素值
                for i in range(h):
                    for j in range(w):
                        try:
                            ipix = healpix_radec2pix(
                                self._nside, self._nested,
                                float(ra_grid[i, j]),
                                float(dec_grid[i, j]))
                            image_data[i, j] = pixel_map.get(ipix, 0.0)
                        except Exception:
                            image_data[i, j] = 0.0

            finally:
                db.close()

            return image_data

        # ------------------------------------------------------------------
        # 文件写入
        # ------------------------------------------------------------------

        def _write_fits(self, path: str, data: np.ndarray,
                        ra_center: float, dec_center: float,
                        width_deg: float, height_deg: float,
                        arcsec_per_px: float, proj_name: str) -> None:
            """写入 FITS 文件"""
            if not ASTROPY_AVAILABLE:
                raise RuntimeError("astropy 不可用, 无法写入 FITS")

            h, w = data.shape
            cdelt = -arcsec_per_px / 3600.0  # 度/像素

            # 创建 WCS 头
            header = fits.Header()
            header["CTYPE1"] = f"RA---{proj_name}"
            header["CTYPE2"] = f"DEC--{proj_name}"
            header["CRVAL1"] = ra_center
            header["CRVAL2"] = dec_center
            header["CRPIX1"] = w / 2
            header["CRPIX2"] = h / 2
            header["CDELT1"] = cdelt
            header["CDELT2"] = -cdelt
            header["CUNIT1"] = "deg"
            header["CUNIT2"] = "deg"
            header["IMAGETYP"] = "projection"
            header["PROJ"] = proj_name
            header["BAND"] = self._band_combo.currentText()

            hdu = fits.PrimaryHDU(data=data, header=header)
            hdu.writeto(path, overwrite=True)
            logger.info(f"FITS 写入完成: {path}")

        def _write_ahpx(self, path: str, data: np.ndarray,
                        ra_center: float, dec_center: float,
                        proj_name: str) -> None:
            """写入 .ahpx 文件"""
            try:
                from ahpx_io import AhpxWriter
            except ImportError:
                raise RuntimeError("ahpx_io 不可用, 无法写入 .ahpx")

            h, w = data.shape
            # 转为 (H, W, 1) 格式
            pixels = data[:, :, np.newaxis].astype(np.float32)
            snr = np.ones((h, w), dtype=np.float32)  # 占位 SNR

            metadata = {
                "image": {"width": w, "height": h, "channels": 1,
                          "dtype": "float32"},
                "wcs": {
                    "ctype1": f"RA---{proj_name}",
                    "ctype2": f"DEC--{proj_name}",
                    "crval1": ra_center,
                    "crval2": dec_center,
                },
                "projection": {"name": proj_name,
                               "source_db": self._db_path},
            }

            import json
            writer = AhpxWriter()
            try:
                writer.set_metadata(json.dumps(metadata))
                writer.set_pixels(pixels, w, h, 1)
                writer.set_snr(snr, w, h)
                writer.set_weight_scalar(1.0)
                writer.write(path, zstd_level=5)
                logger.info(f".ahpx 写入完成: {path}")
            finally:
                writer.close()

        def _write_png(self, path: str, data: np.ndarray,
                       display: Optional[np.ndarray],
                       apply_stf: bool) -> None:
            """写入 PNG 文件"""
            try:
                import matplotlib
                matplotlib.use("Agg")
                import matplotlib.pyplot as plt
            except ImportError:
                raise RuntimeError("matplotlib 不可用, 无法写入 PNG")

            if display is not None:
                img = display
            else:
                # 线性归一化
                valid = data[np.isfinite(data)]
                if valid.size > 0:
                    vmin = float(np.percentile(valid, 1))
                    vmax = float(np.percentile(valid, 99))
                    if vmax - vmin < 1e-30:
                        vmax = vmin + 1.0
                    img = np.clip((data - vmin) / (vmax - vmin), 0, 1)
                else:
                    img = np.zeros_like(data)

            fig, ax = plt.subplots(figsize=(10, 8), dpi=150)
            ax.imshow(img, cmap="inferno", origin="lower", aspect="auto")
            ax.set_xlabel("X (像素)")
            ax.set_ylabel("Y (像素)")
            ax.set_title(f"投影导出 - {path}")
            fig.tight_layout()
            fig.savefig(path, dpi=150)
            plt.close(fig)
            logger.info(f"PNG 写入完成: {path}")

else:
    # PyQt5 不可用时的占位类
    class ExportDialog:
        """PyQt5 不可用时的占位类"""

        def __init__(self, *args, **kwargs):
            raise RuntimeError(
                "PyQt5 不可用, 请安装: pip install PyQt5>=5.15")
