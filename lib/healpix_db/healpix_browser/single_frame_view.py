"""
single_frame_view.py - 单帧浏览视图

功能：显示单个 .ahpx 文件的像素/SNR/权重
用途：检查单帧图像质量和元数据

特性:
- 加载 .ahpx 文件 (通过 ahpx_io DLL)
- 通道切换: 像素 / SNR / 权重
- 鼠标位置 WCS 坐标显示 (RA/Dec)
- 元数据面板 (JSON 树形显示)
- STF 自动拉伸显示
"""

from __future__ import annotations

import os
import sys
import json
import math
import logging
import datetime
from typing import Optional, Dict, Any, Tuple

import numpy as np

# ============================================================================
# 路径设置
# ============================================================================
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_HEALPIX_DB_DIR = os.path.dirname(_THIS_DIR)
if _HEALPIX_DB_DIR not in sys.path:
    sys.path.insert(0, _HEALPIX_DB_DIR)

# 本模块目录 (用于导入 stf)
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

# ============================================================================
# 日志配置
# ============================================================================
_LOG_DIR = os.path.join(_THIS_DIR, "logs")
os.makedirs(_LOG_DIR, exist_ok=True)

logger = logging.getLogger("healpix_browser.single_frame_view")
if not logger.handlers:
    _log_file = os.path.join(
        _LOG_DIR, f"single_frame_{datetime.datetime.now().strftime('%Y%m%d')}.log")
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
        QWidget, QVBoxLayout, QHBoxLayout, QLabel, QComboBox,
        QPushButton, QTreeWidget, QTreeWidgetItem, QSplitter,
        QScrollArea, QFrame, QSizePolicy, QToolTip,
    )
    from PyQt5.QtGui import QImage, QPixmap, QPainter, QMouseEvent
    from PyQt5.QtCore import Qt, pyqtSignal, QPoint
    PYQT5_AVAILABLE = True
    logger.info("PyQt5 可用")
except ImportError as e:
    PYQT5_AVAILABLE = False
    logger.error(f"PyQt5 不可用: {e}")

try:
    from ahpx_io import AhpxReader
    AHPX_IO_AVAILABLE = True
    logger.info("ahpx_io 模块可用")
except Exception as e:
    AHPX_IO_AVAILABLE = False
    logger.warning(f"ahpx_io 模块不可用: {e}")

try:
    from stf import STFEngine, STFParams
    STF_AVAILABLE = True
except Exception as e:
    STF_AVAILABLE = False
    logger.warning(f"stf 模块不可用: {e}")


# ============================================================================
# 图像显示 Widget (支持鼠标追踪)
# ============================================================================

if PYQT5_AVAILABLE:

    class ImageDisplayWidget(QWidget):
        """图像显示 widget, 支持鼠标位置追踪和 WCS 坐标显示

        信号:
            mouse_pos_changed: 鼠标移动时发射 (x, y, pixel_value)
        """

        mouse_pos_changed = pyqtSignal(int, int, float)

        def __init__(self, parent=None):
            super().__init__(parent)
            self._pixmap = None
            self._display_data = None  # uint8 显示数据
            self._raw_data = None      # float32 原始数据
            self._wcs_info = None      # WCS 坐标信息
            self.setMouseTracking(True)
            self.setMinimumSize(400, 300)
            self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
            self._scale = 1.0
            self._offset_x = 0
            self._offset_y = 0

        def set_image(self, display_data: np.ndarray,
                      raw_data: Optional[np.ndarray] = None,
                      wcs_info: Optional[Dict] = None) -> None:
            """设置显示图像

            Args:
                display_data: uint8 显示数据 (H, W) 或 (H, W, 3)
                raw_data: 原始 float 数据 (用于鼠标读取像素值)
                wcs_info: WCS 坐标信息字典
            """
            self._display_data = display_data
            self._raw_data = raw_data
            self._wcs_info = wcs_info

            if display_data is None:
                self._pixmap = None
                self.update()
                return

            # numpy → QImage → QPixmap
            if display_data.ndim == 2:
                # 灰度 → RGB
                h, w = display_data.shape
                rgb = np.stack([display_data] * 3, axis=-1)
            elif display_data.ndim == 3:
                h, w, c = display_data.shape
                if c == 1:
                    rgb = np.repeat(display_data, 3, axis=-1)
                else:
                    rgb = display_data[:, :, :3]
            else:
                return

            # QImage 需要连续内存
            rgb = np.ascontiguousarray(rgb)
            qimage = QImage(rgb.data, w, h, w * 3, QImage.Format_RGB888)
            self._pixmap = QPixmap.fromImage(qimage).copy()  # copy 防止内存释放

            self.update()

        def paintEvent(self, event):
            """绘制图像"""
            painter = QPainter(self)
            painter.fillRect(self.rect(), Qt.black)

            if self._pixmap is None:
                painter.setPen(Qt.white)
                painter.drawText(self.rect(), Qt.AlignCenter,
                                 "未加载图像\n请通过 文件 → 打开 .ahpx 加载")
                return

            # 计算缩放和居中
            pw = self._pixmap.width()
            ph = self._pixmap.height()
            cw = self.width()
            ch = self.height()

            scale = min(cw / pw, ch / ph)
            scale = min(scale, 4.0)  # 最大 4x 放大
            self._scale = scale

            draw_w = int(pw * scale)
            draw_h = int(ph * scale)
            self._offset_x = (cw - draw_w) // 2
            self._offset_y = (ch - draw_h) // 2

            painter.drawPixmap(self._offset_x, self._offset_y,
                               draw_w, draw_h, self._pixmap)

        def mouseMoveEvent(self, event: QMouseEvent):
            """鼠标移动: 计算图像坐标和像素值"""
            if self._pixmap is None or self._display_data is None:
                return

            x = event.x() - self._offset_x
            y = event.y() - self._offset_y

            if self._scale > 0:
                img_x = int(x / self._scale)
                img_y = int(y / self._scale)
            else:
                return

            h, w = (self._display_data.shape[:2]
                    if self._display_data.ndim >= 2 else (0, 0))

            if 0 <= img_x < w and 0 <= img_y < h:
                # 读取原始像素值
                pixel_val = 0.0
                if self._raw_data is not None:
                    if self._raw_data.ndim == 2:
                        pixel_val = float(self._raw_data[img_y, img_x])
                    elif self._raw_data.ndim == 3:
                        pixel_val = float(self._raw_data[img_y, img_x, 0])

                # WCS 坐标
                wcs_str = ""
                if self._wcs_info:
                    wcs_str = self._pixel_to_wcs(img_x, img_y)

                self.mouse_pos_changed.emit(img_x, img_y, pixel_val)

                tip = f"({img_x}, {img_y}) = {pixel_val:.4e}"
                if wcs_str:
                    tip += f"\n{wcs_str}"
                QToolTip.showText(event.globalPos(), tip, self)
            else:
                QToolTip.hideText()

        def _pixel_to_wcs(self, x: int, y: int) -> str:
            """像素坐标 → WCS 坐标 (RA/Dec)

            使用简化的 TAN 投影反变换:
            RA = CRVAL1 + (x - CRPIX1) * CDELT1
            Dec = CRVAL2 + (y - CRPIX2) * CDELT2
            (简化版, 不含投影畸变)

            Args:
                x: 像素 x 坐标 (从左到右)
                y: 像素 y 坐标 (从上到下, FITS 约定从下到上)

            Returns:
                WCS 坐标字符串 "RA=xxx° Dec=xxx°"
            """
            wcs = self._wcs_info
            if not wcs:
                return ""

            crval1 = wcs.get("crval1", 0.0)
            crval2 = wcs.get("crval2", 0.0)
            cdelt1 = wcs.get("cdelt1", 1.0)
            cdelt2 = wcs.get("cdelt2", 1.0)
            crpix1 = wcs.get("crpix1", 0.0)
            crpix2 = wcs.get("crpix2", 0.0)

            # FITS 约定: y 轴向上, 显示 y 轴向下
            h = self._display_data.shape[0] if self._display_data is not None else 0
            fits_y = h - y if h > 0 else y

            ra = crval1 + (x - crpix1) * cdelt1
            dec = crval2 + (fits_y - crpix2) * cdelt2

            # RA 归一化到 [0, 360)
            ra = ra % 360.0
            return f"RA={ra:.6f}° Dec={dec:.6f}°"


# ============================================================================
# 单帧浏览视图
# ============================================================================

if PYQT5_AVAILABLE:

    class SingleFrameView(QWidget):
        """单帧浏览视图

        功能:
        - 加载 .ahpx 文件
        - 通道切换 (像素/SNR/权重)
        - STF 自动拉伸
        - 鼠标位置 WCS 坐标
        - 元数据树形面板
        """

        def __init__(self, parent=None):
            super().__init__(parent)
            self._reader = None
            self._pixels = None      # (H, W, C) float32
            self._snr = None         # (H, W) float32
            self._weight = None      # float / (gh, gw) / (H, W)
            self._metadata = None    # dict
            self._wcs_info = None    # dict
            self._current_channel = "pixel"
            self._stf_engine = STFEngine() if STF_AVAILABLE else None

            self._init_ui()
            logger.info("SingleFrameView 初始化完成")

        def _init_ui(self):
            """构建 UI 布局"""
            layout = QVBoxLayout(self)

            # ===== 顶部工具栏 =====
            toolbar = QHBoxLayout()

            toolbar.addWidget(QLabel("通道:"))
            self._channel_combo = QComboBox()
            self._channel_combo.addItems(["像素", "SNR", "权重"])
            self._channel_combo.currentIndexChanged.connect(
                self._on_channel_changed)
            toolbar.addWidget(self._channel_combo)

            # 多通道时选择波段
            toolbar.addWidget(QLabel("波段:"))
            self._band_combo = QComboBox()
            self._band_combo.setEnabled(False)
            self._band_combo.currentIndexChanged.connect(
                self._on_band_changed)
            toolbar.addWidget(self._band_combo)

            # STF 重置按钮
            self._auto_stf_btn = QPushButton("自动 STF")
            self._auto_stf_btn.clicked.connect(self._apply_auto_stf)
            toolbar.addWidget(self._auto_stf_btn)

            toolbar.addStretch()

            # 坐标显示
            self._coord_label = QLabel("坐标: --")
            self._coord_label.setMinimumWidth(300)
            toolbar.addWidget(self._coord_label)

            layout.addLayout(toolbar)

            # ===== 主区域: 图像 + 元数据 (分割器) =====
            splitter = QSplitter(Qt.Horizontal)

            # 图像显示
            self._image_widget = ImageDisplayWidget()
            self._image_widget.mouse_pos_changed.connect(
                self._on_mouse_pos_changed)
            splitter.addWidget(self._image_widget)

            # 元数据面板
            meta_frame = QFrame()
            meta_frame.setFrameShape(QFrame.StyledPanel)
            meta_layout = QVBoxLayout(meta_frame)
            meta_layout.addWidget(QLabel("元数据:"))
            self._meta_tree = QTreeWidget()
            self._meta_tree.setHeaderLabels(["键", "值"])
            self._meta_tree.setColumnWidth(0, 200)
            meta_layout.addWidget(self._meta_tree)
            meta_frame.setMaximumWidth(400)
            splitter.addWidget(meta_frame)

            splitter.setStretchFactor(0, 3)
            splitter.setStretchFactor(1, 1)
            layout.addWidget(splitter)

        # ------------------------------------------------------------------
        # 文件加载
        # ------------------------------------------------------------------

        def load_ahpx(self, path: str) -> None:
            """加载 .ahpx 文件

            Args:
                path: .ahpx 文件路径
            """
            logger.info(f"加载 .ahpx 文件: {path}")

            if not AHPX_IO_AVAILABLE:
                self._show_error("ahpx_io 模块不可用",
                                 "无法加载 .ahpx 文件, 请确保 ahpx_io.dll 已编译。")
                return

            # 关闭旧 reader
            if self._reader is not None:
                try:
                    self._reader.close()
                except Exception:
                    pass
                self._reader = None

            try:
                self._reader = AhpxReader(path)
                self._pixels = self._reader.read_pixels()
                logger.info(f"  像素: shape={self._pixels.shape}, "
                            f"dtype={self._pixels.dtype}")

                try:
                    self._snr = self._reader.read_snr()
                    logger.info(f"  SNR: shape={self._snr.shape}")
                except Exception as e:
                    logger.warning(f"  读取 SNR 失败: {e}")
                    self._snr = None

                try:
                    self._weight = self._reader.read_weight()
                    logger.info(f"  权重: type={type(self._weight)}")
                except Exception as e:
                    logger.warning(f"  读取权重失败: {e}")
                    self._weight = None

                # 解析元数据
                header_json = self._reader.header_json
                try:
                    self._metadata = json.loads(header_json)
                except json.JSONDecodeError:
                    self._metadata = {"raw": header_json}

                # 提取 WCS 信息
                self._wcs_info = self._metadata.get("wcs", None)

                # 更新元数据树
                self._update_meta_tree(self._metadata)

                # 更新波段选择
                self._update_band_combo()

                # 自动 STF 并显示
                self._apply_auto_stf()

            except Exception as e:
                logger.error(f"加载 .ahpx 失败: {e}", exc_info=True)
                self._show_error("加载失败", str(e))

        # ------------------------------------------------------------------
        # 通道切换
        # ------------------------------------------------------------------

        def _on_channel_changed(self, index: int) -> None:
            """通道切换回调"""
            channels = ["pixel", "snr", "weight"]
            if 0 <= index < len(channels):
                self._current_channel = channels[index]
                logger.info(f"切换通道: {self._current_channel}")
                self._refresh_display()

        def _on_band_changed(self, index: int) -> None:
            """波段切换回调"""
            self._refresh_display()

        def _update_band_combo(self) -> None:
            """更新波段选择下拉框"""
            self._band_combo.clear()
            if self._pixels is not None and self._pixels.ndim == 3:
                n_channels = self._pixels.shape[2]
                if n_channels > 1:
                    for i in range(n_channels):
                        self._band_combo.addItem(f"通道 {i}")
                    self._band_combo.setEnabled(True)
                    return
            self._band_combo.setEnabled(False)

        # ------------------------------------------------------------------
        # STF 拉伸
        # ------------------------------------------------------------------

        def _apply_auto_stf(self) -> None:
            """应用自动 STF 拉伸"""
            if self._stf_engine is None:
                logger.warning("STF 引擎不可用, 使用线性归一化")
                self._refresh_display()
                return

            data = self._get_current_data()
            if data is None:
                return

            try:
                params = self._stf_engine.auto_stretch(data)
                display = self._stf_engine.apply_stf(data, params)
                raw = self._get_current_raw_data()
                self._image_widget.set_image(display, raw, self._wcs_info)
                logger.info("自动 STF 已应用")
            except Exception as e:
                logger.error(f"STF 失败: {e}")
                self._refresh_display()

        def _refresh_display(self) -> None:
            """刷新显示 (当前通道, 线性归一化)"""
            data = self._get_current_data()
            if data is None:
                return

            # 简单线性归一化
            valid = data[np.isfinite(data)]
            if valid.size == 0:
                display = np.zeros(data.shape, dtype=np.uint8)
            else:
                vmin = float(np.percentile(valid, 1))
                vmax = float(np.percentile(valid, 99))
                if vmax - vmin < 1e-30:
                    vmax = vmin + 1.0
                norm = np.clip((data - vmin) / (vmax - vmin), 0, 1)
                display = (norm * 255).astype(np.uint8)

            raw = self._get_current_raw_data()
            self._image_widget.set_image(display, raw, self._wcs_info)

        def _get_current_data(self) -> Optional[np.ndarray]:
            """获取当前通道的显示数据 (2D)"""
            if self._current_channel == "pixel":
                if self._pixels is None:
                    return None
                if self._pixels.ndim == 3:
                    band_idx = max(0, self._band_combo.currentIndex())
                    return self._pixels[:, :, band_idx]
                return self._pixels
            elif self._current_channel == "snr":
                return self._snr
            elif self._current_channel == "weight":
                if isinstance(self._weight, (int, float)):
                    return np.array([[float(self._weight)]])
                elif isinstance(self._weight, np.ndarray):
                    return self._weight
                return None
            return None

        def _get_current_raw_data(self) -> Optional[np.ndarray]:
            """获取当前通道的原始数据 (用于鼠标读取)"""
            return self._get_current_data()

        # ------------------------------------------------------------------
        # 鼠标坐标
        # ------------------------------------------------------------------

        def _on_mouse_pos_changed(self, x: int, y: int, val: float) -> None:
            """鼠标位置变化回调"""
            wcs_str = ""
            if self._wcs_info:
                # 复用 ImageDisplayWidget 的 WCS 转换
                wcs_str = self._image_widget._pixel_to_wcs(x, y)

            self._coord_label.setText(
                f"({x}, {y}) = {val:.4e}" +
                (f"  {wcs_str}" if wcs_str else ""))

        # ------------------------------------------------------------------
        # 元数据树
        # ------------------------------------------------------------------

        def _update_meta_tree(self, data: Dict[str, Any],
                              parent: Optional[QTreeWidgetItem] = None) -> None:
            """更新元数据树形面板

            Args:
                data: 元数据字典
                parent: 父节点 (递归用)
            """
            if parent is None:
                self._meta_tree.clear()
                parent = self._meta_tree

            if isinstance(data, dict):
                for key, val in data.items():
                    if isinstance(val, dict):
                        item = QTreeWidgetItem(parent, [str(key), ""])
                        self._update_meta_tree(val, item)
                    elif isinstance(val, list):
                        item = QTreeWidgetItem(parent, [str(key), ""])
                        for i, v in enumerate(val):
                            QTreeWidgetItem(item, [f"[{i}]", str(v)])
                    else:
                        QTreeWidgetItem(parent, [str(key), str(val)])
            elif isinstance(data, list):
                for i, v in enumerate(data):
                    if isinstance(v, dict):
                        item = QTreeWidgetItem(parent, [f"[{i}]", ""])
                        self._update_meta_tree(v, item)
                    else:
                        QTreeWidgetItem(parent, [f"[{i}]", str(v)])

        # ------------------------------------------------------------------
        # 错误提示
        # ------------------------------------------------------------------

        def _show_error(self, title: str, message: str) -> None:
            """显示错误信息"""
            from PyQt5.QtWidgets import QMessageBox
            QMessageBox.critical(self, title, message)

        def close_reader(self) -> None:
            """关闭文件 reader, 释放资源"""
            if self._reader is not None:
                try:
                    self._reader.close()
                except Exception:
                    pass
                self._reader = None
            logger.info("单帧视图 reader 已关闭")

else:
    # PyQt5 不可用时提供占位类
    class SingleFrameView:
        """PyQt5 不可用时的占位类"""

        def __init__(self, *args, **kwargs):
            raise RuntimeError(
                "PyQt5 不可用, 请安装: pip install PyQt5>=5.15")
