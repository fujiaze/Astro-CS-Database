"""
sphere_view.py - 球数据库浏览视图

功能：球面渲染堆栈数据，支持拖动旋转和缩放
用途：巡天数据的球面浏览

特性:
- 打开 HEALpix 堆栈数据库 (通过 healpix_stack DLL)
- 球面渲染 (集成 SphereRenderer, vispy/matplotlib 自动选择)
- 波段切换 (L/R/G/B 等)
- RGB 合成显示
- 像素信息悬停显示
- 按 FOV 动态选择 LOD 层 (通过 healpix_lod)
"""

from __future__ import annotations

import os
import sys
import json
import math
import logging
import datetime
from typing import Optional, Dict, Any, List, Tuple

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

logger = logging.getLogger("healpix_browser.sphere_view")
if not logger.handlers:
    _log_file = os.path.join(
        _LOG_DIR, f"sphere_view_{datetime.datetime.now().strftime('%Y%m%d')}.log")
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
        QPushButton, QSpinBox, QDoubleSpinBox, QCheckBox, QSlider,
        QGroupBox, QFormLayout, QSplitter, QFrame, QSizePolicy,
        QToolTip, QMessageBox,
    )
    from PyQt5.QtCore import Qt, pyqtSignal, QTimer
    from PyQt5.QtGui import QMouseEvent, QWheelEvent
    PYQT5_AVAILABLE = True
except ImportError as e:
    PYQT5_AVAILABLE = False
    logger.error(f"PyQt5 不可用: {e}")

try:
    from healpix_stack import StackDatabase
    STACK_DLL_AVAILABLE = True
    logger.info("healpix_stack DLL 可用")
except Exception as e:
    STACK_DLL_AVAILABLE = False
    logger.warning(f"healpix_stack DLL 不可用: {e}")

try:
    from healpix_lod import LodManager
    LOD_DLL_AVAILABLE = True
    logger.info("healpix_lod DLL 可用")
except Exception as e:
    LOD_DLL_AVAILABLE = False
    logger.warning(f"healpix_lod DLL 不可用: {e}")

try:
    from sphere_renderer import SphereRenderer, VISPY_AVAILABLE
    RENDERER_AVAILABLE = True
except Exception as e:
    RENDERER_AVAILABLE = False
    logger.warning(f"sphere_renderer 不可用: {e}")

try:
    from stf import STFEngine, STFParams
    STF_AVAILABLE = True
except Exception as e:
    STF_AVAILABLE = False
    logger.warning(f"stf 模块不可用: {e}")


# ============================================================================
# 球数据库浏览视图
# ============================================================================

if PYQT5_AVAILABLE and RENDERER_AVAILABLE:

    class SphereView(QWidget):
        """球数据库浏览视图

        功能:
        - 打开堆栈数据库
        - 球面渲染 (vispy/matplotlib)
        - 波段切换
        - RGB 合成
        - 像素信息悬停
        """

        def __init__(self, parent=None):
            super().__init__(parent)
            self._db = None                # StackDatabase 实例
            self._db_path = None           # 数据库路径
            self._meta = None              # meta.json 配置
            self._bands = []               # 波段名列表 ["L", "R", "G", "B"]
            self._nside_data = 512         # 数据层 nside
            self._nside_lod = [512]        # LOD 各层 nside
            self._tile_nside = 512         # tile nside
            self._nested = True            # 排序方式
            self._renderer = None          # SphereRenderer
            self._stf_engine = STFEngine() if STF_AVAILABLE else None
            self._current_band = 0         # 当前波段索引
            self._rgb_mode = False         # RGB 合成模式
            self._rgb_mapping = {}         # 波段→RGB通道映射
            self._last_mouse_pos = None    # 上次鼠标位置 (用于拖动)
            self._fov_deg = 180.0          # 当前视场角

            self._init_ui()
            logger.info("SphereView 初始化完成")

        def _init_ui(self):
            """构建 UI"""
            layout = QVBoxLayout(self)

            # ===== 顶部工具栏 =====
            toolbar = QHBoxLayout()

            # 波段选择
            toolbar.addWidget(QLabel("波段:"))
            self._band_combo = QComboBox()
            self._band_combo.currentIndexChanged.connect(
                self._on_band_changed)
            toolbar.addWidget(self._band_combo)

            # RGB 合成开关
            self._rgb_check = QCheckBox("RGB 合成")
            self._rgb_check.toggled.connect(self._on_rgb_toggled)
            toolbar.addWidget(self._rgb_check)

            # RGB 通道映射
            toolbar.addWidget(QLabel("R:"))
            self._r_combo = QComboBox()
            toolbar.addWidget(self._r_combo)
            toolbar.addWidget(QLabel("G:"))
            self._g_combo = QComboBox()
            toolbar.addWidget(self._g_combo)
            toolbar.addWidget(QLabel("B:"))
            self._b_combo = QComboBox()
            toolbar.addWidget(self._b_combo)
            self._r_combo.currentIndexChanged.connect(self._on_rgb_mapping_changed)
            self._g_combo.currentIndexChanged.connect(self._on_rgb_mapping_changed)
            self._b_combo.currentIndexChanged.connect(self._on_rgb_mapping_changed)

            self._update_rgb_combos_enabled(False)

            toolbar.addStretch()

            # LOD 层选择
            toolbar.addWidget(QLabel("LOD 层:"))
            self._lod_combo = QComboBox()
            self._lod_combo.currentIndexChanged.connect(self._on_lod_changed)
            toolbar.addWidget(self._lod_combo)

            # 刷新按钮
            self._refresh_btn = QPushButton("刷新")
            self._refresh_btn.clicked.connect(self._refresh_render)
            toolbar.addWidget(self._refresh_btn)

            layout.addLayout(toolbar)

            # ===== 主区域: 球面渲染 + 信息面板 =====
            splitter = QSplitter(Qt.Horizontal)

            # 球面渲染区
            self._render_container = QFrame()
            self._render_container.setFrameShape(QFrame.StyledPanel)
            render_layout = QVBoxLayout(self._render_container)
            render_layout.setContentsMargins(0, 0, 0, 0)

            # 创建 SphereRenderer
            try:
                self._renderer = SphereRenderer()
                native = self._renderer.native_widget
                if native is not None:
                    native.setParent(self._render_container)
                    native.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
                    render_layout.addWidget(native)
                else:
                    render_layout.addWidget(QLabel("渲染器初始化失败"))
            except Exception as e:
                logger.error(f"渲染器初始化失败: {e}")
                render_layout.addWidget(QLabel(f"渲染器初始化失败: {e}"))

            splitter.addWidget(self._render_container)

            # 信息面板
            info_frame = QFrame()
            info_frame.setFrameShape(QFrame.StyledPanel)
            info_frame.setMaximumWidth(300)
            info_layout = QFormLayout(info_frame)

            self._info_nside = QLabel("--")
            self._info_bands = QLabel("--")
            self._info_tiles = QLabel("--")
            self._info_fov = QLabel("--")
            self._info_hover = QLabel("--")

            info_layout.addRow("数据层 nside:", self._info_nside)
            info_layout.addRow("波段:", self._info_bands)
            info_layout.addRow("tile 数:", self._info_tiles)
            info_layout.addRow("视场角:", self._info_fov)
            info_layout.addRow("悬停信息:", self._info_hover)

            splitter.addWidget(info_frame)
            splitter.setStretchFactor(0, 4)
            splitter.setStretchFactor(1, 1)

            layout.addWidget(splitter)

            # 鼠标事件处理 (安装在渲染容器上)
            self._render_container.setMouseTracking(True)
            self._render_container.installEventFilter(self)

        # ------------------------------------------------------------------
        # 数据库操作
        # ------------------------------------------------------------------

        def open_database(self, db_path: str) -> None:
            """打开堆栈数据库

            Args:
                db_path: 数据库目录路径 (包含 meta.json)
            """
            logger.info(f"打开数据库: {db_path}")

            if not STACK_DLL_AVAILABLE:
                QMessageBox.critical(
                    self, "错误",
                    "healpix_stack DLL 不可用, 无法打开数据库。\n"
                    "请确保 healpix_stack.dll 已编译。")
                return

            # 关闭旧数据库
            if self._db is not None:
                try:
                    self._db.close()
                except Exception:
                    pass
                self._db = None

            try:
                self._db = StackDatabase.open(db_path)
                self._db_path = db_path

                # 读取 meta.json
                meta_path = os.path.join(db_path, "meta.json")
                if os.path.isfile(meta_path):
                    with open(meta_path, "r", encoding="utf-8") as f:
                        self._meta = json.load(f)
                else:
                    self._meta = {}

                # 解析配置
                self._nside_data = self._meta.get("nsideData", 512)
                self._nside_lod = self._meta.get("nsideLod", [self._nside_data])
                self._tile_nside = self._meta.get("tileNside", self._nside_data)
                self._nested = self._meta.get("nested", True)
                self._bands = self._meta.get("bands", ["L"])

                logger.info(f"  nsideData={self._nside_data}, "
                            f"bands={self._bands}, "
                            f"tileNside={self._tile_nside}, "
                            f"nested={self._nested}")

                # 更新 UI
                self._update_band_combo()
                self._update_lod_combo()
                self._update_info_panel()

                # 加载数据并渲染
                self._load_and_render()

            except Exception as e:
                logger.error(f"打开数据库失败: {e}", exc_info=True)
                QMessageBox.critical(self, "打开数据库失败", str(e))

        def _update_band_combo(self):
            """更新波段选择下拉框"""
            self._band_combo.clear()
            for band in self._bands:
                self._band_combo.addItem(band)

            # 更新 RGB 通道映射下拉框
            for combo in [self._r_combo, self._g_combo, self._b_combo]:
                combo.clear()
                for band in self._bands:
                    combo.addItem(band)

            # 默认 RGB 映射 (如果有 R/G/B 波段)
            band_upper = [b.upper() for b in self._bands]
            defaults = {"R": 0, "G": 0, "B": 0}
            for i, b in enumerate(band_upper):
                if b == "R":
                    defaults["R"] = i
                elif b == "G":
                    defaults["G"] = i
                elif b == "B":
                    defaults["B"] = i
                elif b == "L" and defaults["R"] == 0:
                    defaults["R"] = defaults["G"] = defaults["B"] = i

            self._r_combo.setCurrentIndex(defaults["R"])
            self._g_combo.setCurrentIndex(defaults["G"])
            self._b_combo.setCurrentIndex(defaults["B"])

        def _update_lod_combo(self):
            """更新 LOD 层选择"""
            self._lod_combo.clear()
            for i, nside in enumerate(self._nside_lod):
                self._lod_combo.addItem(f"Level {i} (nside={nside})")

        def _update_info_panel(self):
            """更新信息面板"""
            self._info_nside.setText(str(self._nside_data))
            self._info_bands.setText(", ".join(self._bands))
            n_tiles = 12 * self._tile_nside * self._tile_nside
            self._info_tiles.setText(str(n_tiles))
            self._info_fov.setText(f"{self._fov_deg:.1f}°")

        def _update_rgb_combos_enabled(self, enabled: bool):
            """启用/禁用 RGB 通道映射下拉框"""
            for combo in [self._r_combo, self._g_combo, self._b_combo]:
                combo.setEnabled(enabled)

        # ------------------------------------------------------------------
        # 数据加载与渲染
        # ------------------------------------------------------------------

        def _load_and_render(self):
            """加载 tile 数据并渲染球面"""
            if self._db is None:
                return

            try:
                # 读取所有 tile 数据 (简化: 读取 tileNside 级别的所有 tile)
                # 对于大 nside, 这里只读取部分 tile 做演示
                # 实际应按视场范围只读可见 tile
                n_tiles_to_read = min(
                    12 * self._tile_nside * self._tile_nside,
                    1000)  # 限制读取数量

                all_pixels = []
                all_values = []

                for tile_ipix in range(n_tiles_to_read):
                    try:
                        tile_data = self._db.read_tile(tile_ipix)
                        pixels = tile_data.get("pixels", [])
                        bands_data = tile_data.get("bands", [])

                        if not pixels or not bands_data:
                            continue

                        # 取当前波段的值
                        band_idx = self._current_band
                        if band_idx < len(bands_data):
                            values = bands_data[band_idx].get("values", [])
                            if values:
                                all_pixels.extend(pixels[:len(values)])
                                all_values.extend(values[:len(values)])
                    except Exception:
                        continue

                if not all_pixels:
                    logger.warning("未读取到任何 tile 数据")
                    QMessageBox.warning(
                        self, "无数据",
                        "数据库中未找到有效数据。\n"
                        "可能数据库为空或 tile 读取失败。")
                    return

                pixels_arr = np.array(all_pixels, dtype=np.int64)
                values_arr = np.array(all_values, dtype=np.float64)

                logger.info(f"加载 {len(pixels_arr)} 个像素, "
                            f"值范围 [{values_arr.min():.4e}, "
                            f"{values_arr.max():.4e}]")

                # STF 拉伸 (不修改原始数据, 只影响显示)
                if self._stf_engine and not self._rgb_mode:
                    params = self._stf_engine.auto_stretch(values_arr)
                    display_values = self._stf_engine.apply_stf(
                        values_arr, params).astype(np.float64) / 255.0
                else:
                    display_values = values_arr

                # 设置渲染器数据
                self._renderer.set_data(
                    pixels_arr, display_values,
                    self._tile_nside, self._nested)
                self._renderer.render()

            except Exception as e:
                logger.error(f"加载渲染数据失败: {e}", exc_info=True)
                QMessageBox.critical(self, "渲染失败", str(e))

        def _refresh_render(self):
            """刷新渲染"""
            self._load_and_render()

        # ------------------------------------------------------------------
        # 事件回调
        # ------------------------------------------------------------------

        def _on_band_changed(self, index: int):
            """波段切换"""
            if 0 <= index < len(self._bands):
                self._current_band = index
                logger.info(f"切换波段: {self._bands[index]}")
                self._load_and_render()

        def _on_rgb_toggled(self, checked: bool):
            """RGB 合成开关"""
            self._rgb_mode = checked
            self._update_rgb_combos_enabled(checked)
            logger.info(f"RGB 合成: {'开启' if checked else '关闭'}")
            if checked:
                self._load_rgb_render()
            else:
                self._load_and_render()

        def _on_rgb_mapping_changed(self):
            """RGB 通道映射变化"""
            if self._rgb_mode:
                self._load_rgb_render()

        def _on_lod_changed(self, index: int):
            """LOD 层切换"""
            logger.info(f"切换 LOD 层: {index}")
            # 这里简化: LOD 层切换后重新加载
            self._load_and_render()

        def _load_rgb_render(self):
            """RGB 合成渲染"""
            if self._db is None:
                return

            try:
                r_idx = self._r_combo.currentIndex()
                g_idx = self._g_combo.currentIndex()
                b_idx = self._b_combo.currentIndex()

                # 分别读取 R/G/B 波段数据
                n_tiles_to_read = min(
                    12 * self._tile_nside * self._tile_nside, 500)

                all_pixels = []
                r_values = []
                g_values = []
                b_values = []

                for tile_ipix in range(n_tiles_to_read):
                    try:
                        tile_data = self._db.read_tile(tile_ipix)
                        pixels = tile_data.get("pixels", [])
                        bands_data = tile_data.get("bands", [])
                        if not pixels or not bands_data:
                            continue

                        r_vals = (bands_data[r_idx].get("values", [])
                                  if r_idx < len(bands_data) else [])
                        g_vals = (bands_data[g_idx].get("values", [])
                                  if g_idx < len(bands_data) else [])
                        b_vals = (bands_data[b_idx].get("values", [])
                                  if b_idx < len(bands_data) else [])

                        n = min(len(pixels), len(r_vals),
                                len(g_vals), len(b_vals))
                        if n > 0:
                            all_pixels.extend(pixels[:n])
                            r_values.extend(r_vals[:n])
                            g_values.extend(g_vals[:n])
                            b_values.extend(b_vals[:n])
                    except Exception:
                        continue

                if not all_pixels:
                    return

                pixels_arr = np.array(all_pixels, dtype=np.int64)
                r_arr = np.array(r_values, dtype=np.float64)
                g_arr = np.array(g_values, dtype=np.float64)
                b_arr = np.array(b_values, dtype=np.float64)

                # 各通道独立 STF
                if self._stf_engine:
                    r_display = self._stf_engine.apply_stf(
                        r_arr, self._stf_engine.auto_stretch(r_arr)
                    ).astype(np.float64) / 255.0
                    g_display = self._stf_engine.apply_stf(
                        g_arr, self._stf_engine.auto_stretch(g_arr)
                    ).astype(np.float64) / 255.0
                    b_display = self._stf_engine.apply_stf(
                        b_arr, self._stf_engine.auto_stretch(b_arr)
                    ).astype(np.float64) / 255.0
                    # RGB 合成: 取三通道均值作为亮度
                    rgb_values = (r_display + g_display + b_display) / 3.0
                else:
                    rgb_values = (r_arr + g_arr + b_arr) / 3.0

                self._renderer.set_data(
                    pixels_arr, rgb_values,
                    self._tile_nside, self._nested)
                self._renderer.render()

                logger.info(f"RGB 合成渲染完成: {len(pixels_arr)} 像素")

            except Exception as e:
                logger.error(f"RGB 渲染失败: {e}", exc_info=True)

        # ------------------------------------------------------------------
        # 鼠标事件 (拖动旋转 + 滚轮缩放)
        # ------------------------------------------------------------------

        def eventFilter(self, obj, event):
            """事件过滤器: 处理渲染区域的鼠标事件"""
            if obj == self._render_container:
                if event.type() == 0x0001:  # QEvent.MouseButtonPress
                    self._on_mouse_press(event)
                    return True
                elif event.type() == 0x0002:  # QEvent.MouseButtonRelease
                    self._on_mouse_release(event)
                    return True
                elif event.type() == 0x0005:  # QEvent.MouseMove
                    self._on_mouse_move(event)
                    return True
                elif event.type() == 0x001F:  # QEvent.Wheel
                    self._on_wheel(event)
                    return True
            return super().eventFilter(obj, event)

        def _on_mouse_press(self, event: QMouseEvent):
            """鼠标按下: 开始拖动"""
            if event.button() == Qt.LeftButton:
                self._last_mouse_pos = (event.x(), event.y())

        def _on_mouse_release(self, event: QMouseEvent):
            """鼠标释放: 结束拖动"""
            if event.button() == Qt.LeftButton:
                self._last_mouse_pos = None

        def _on_mouse_move(self, event: QMouseEvent):
            """鼠标移动: 拖动旋转"""
            if self._last_mouse_pos is not None:
                dx = event.x() - self._last_mouse_pos[0]
                dy = event.y() - self._last_mouse_pos[1]
                self._last_mouse_pos = (event.x(), event.y())

                if self._renderer:
                    self._renderer.on_rotate(dx, dy)
                    self._renderer.render()

            # 更新悬停信息
            self._update_hover_info(event.x(), event.y())

        def _on_wheel(self, event: QWheelEvent):
            """滚轮缩放"""
            delta = event.angleDelta().y()
            if self._renderer:
                self._renderer.on_zoom(delta)
                self._renderer.render()

                # 更新 FOV 估算 (简化)
                # 滚轮向上=放大, FOV 减小
                factor = 0.9 if delta > 0 else 1.1
                self._fov_deg = max(1.0, min(180.0, self._fov_deg * factor))
                self._info_fov.setText(f"{self._fov_deg:.1f}°")

                # 按 FOV 更新 LOD
                self._renderer.update_lod_level(self._fov_deg)

        def _update_hover_info(self, x: int, y: int):
            """更新鼠标悬停时的像素信息"""
            # 简化: 显示鼠标坐标
            self._info_hover.setText(f"屏幕 ({x}, {y})")

        # ------------------------------------------------------------------
        # 清理
        # ------------------------------------------------------------------

        def close_database(self):
            """关闭数据库, 释放资源"""
            if self._db is not None:
                try:
                    self._db.close()
                except Exception:
                    pass
                self._db = None
            logger.info("球数据库已关闭")

else:
    # 依赖不可用时的占位类
    class SphereView:
        """PyQt5 或渲染器不可用时的占位类"""

        def __init__(self, *args, **kwargs):
            missing = []
            if not PYQT5_AVAILABLE:
                missing.append("PyQt5")
            if not RENDERER_AVAILABLE:
                missing.append("sphere_renderer")
            raise RuntimeError(
                f"依赖不可用: {', '.join(missing)}\n"
                f"请安装: pip install PyQt5>=5.15 vispy>=0.9")
