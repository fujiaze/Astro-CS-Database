"""
main_window.py - HEALpix 浏览器主窗口

功能：Qt 主窗口，集成单帧浏览和球数据库浏览
用途：天文巡天数据的可视化浏览工具

菜单：
- 文件: 打开 .ahpx / 打开数据库 / 导出 / 退出
- 视图: 单帧模式 / 球面模式 / STF 设置
- 工具: 投影导出 / 波段切换

运行:
    python main_window.py
    python -m healpix_browser
"""

from __future__ import annotations

import os
import sys
import json
import logging
import datetime
from typing import Optional

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

logger = logging.getLogger("healpix_browser.main_window")
if not logger.handlers:
    _log_file = os.path.join(
        _LOG_DIR, f"main_window_{datetime.datetime.now().strftime('%Y%m%d')}.log")
    _fh = logging.FileHandler(_log_file, encoding="utf-8")
    _fh.setFormatter(logging.Formatter(
        "%(asctime)s [%(levelname)s] %(message)s"))
    logger.addHandler(_fh)
    _sh = logging.StreamHandler()
    _sh.setFormatter(logging.Formatter("[Main] %(message)s"))
    logger.addHandler(_sh)
    logger.setLevel(logging.INFO)

# ============================================================================
# 导入依赖
# ============================================================================
try:
    from PyQt5.QtWidgets import (
        QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QStackedWidget,
        QMenuBar, QMenu, QToolBar, QStatusBar, QAction, QFileDialog,
        QMessageBox, QLabel, QSlider, QDoubleSpinBox, QComboBox,
        QPushButton, QGroupBox, QFormLayout, QSplitter, QFrame,
        QCheckBox, QTabWidget,
    )
    from PyQt5.QtCore import Qt, QTimer, pyqtSignal
    from PyQt5.QtGui import QIcon, QKeySequence
    PYQT5_AVAILABLE = True
    logger.info("PyQt5 可用")
except ImportError as e:
    PYQT5_AVAILABLE = False
    logger.error(f"PyQt5 不可用: {e}")

# 延迟导入视图组件 (在 __init__ 中检查可用性)
_SINGLE_FRAME_VIEW_OK = False
_SPHERE_VIEW_OK = False
_EXPORT_DIALOG_OK = False
_STF_OK = False

try:
    from single_frame_view import SingleFrameView
    _SINGLE_FRAME_VIEW_OK = True
except Exception as e:
    logger.warning(f"single_frame_view 导入失败: {e}")

try:
    from sphere_view import SphereView
    _SPHERE_VIEW_OK = True
except Exception as e:
    logger.warning(f"sphere_view 导入失败: {e}")

try:
    from export_dialog import ExportDialog
    _EXPORT_DIALOG_OK = True
except Exception as e:
    logger.warning(f"export_dialog 导入失败: {e}")

try:
    from stf import STFEngine, STFParams
    _STF_OK = True
except Exception as e:
    logger.warning(f"stf 导入失败: {e}")


# ============================================================================
# STF 控制面板
# ============================================================================

if PYQT5_AVAILABLE and _STF_OK:

    class STFPanel(QWidget):
        """STF 拉伸控制面板

        功能:
        - 自动/手动拉伸切换
        - shadows/highlights/midtones 滑块
        - 预设选择 (linear/sqrt/asinh/log)
        - 直方图显示
        - 参数保存/加载
        """

        # STF 参数变化信号
        params_changed = pyqtSignal(object)  # STFParams

        def __init__(self, parent=None):
            from PyQt5.QtWidgets import (
                QWidget, QVBoxLayout, QHBoxLayout, QLabel, QSlider,
                QComboBox, QPushButton, QCheckBox, QGroupBox,
                QFormLayout, QDoubleSpinBox,
            )
            super().__init__(parent)
            self._engine = STFEngine()
            self._current_params = STFParams()
            self._current_data = None  # 当前显示的数据引用

            self._init_ui()
            logger.info("STFPanel 初始化完成")

        def _init_ui(self):
            from PyQt5.QtWidgets import (
                QVBoxLayout, QHBoxLayout, QLabel, QSlider, QComboBox,
                QPushButton, QCheckBox, QGroupBox, QFormLayout,
            )
            from PyQt5.QtCore import Qt

            layout = QVBoxLayout(self)

            # ===== 模式选择 =====
            mode_group = QGroupBox("拉伸模式")
            mode_layout = QVBoxLayout(mode_group)

            self._mode_combo = QComboBox()
            self._mode_combo.addItems(["自动 (MAD)", "手动", "预设"])
            self._mode_combo.currentIndexChanged.connect(
                self._on_mode_changed)
            mode_layout.addWidget(self._mode_combo)

            # 预设选择
            self._preset_combo = QComboBox()
            self._preset_combo.addItems(["linear", "sqrt", "asinh", "log"])
            self._preset_combo.setEnabled(False)
            self._preset_combo.currentIndexChanged.connect(
                self._on_preset_changed)
            mode_layout.addWidget(QLabel("预设:"))
            mode_layout.addWidget(self._preset_combo)

            # 自动拉伸按钮
            self._auto_btn = QPushButton("应用自动拉伸")
            self._auto_btn.clicked.connect(self._apply_auto)
            mode_layout.addWidget(self._auto_btn)

            layout.addWidget(mode_group)

            # ===== 手动参数 =====
            manual_group = QGroupBox("手动参数")
            manual_layout = QFormLayout(manual_group)

            # Shadows
            self._shadows_slider = QSlider(Qt.Horizontal)
            self._shadows_slider.setRange(0, 999)
            self._shadows_slider.setValue(0)
            self._shadows_slider.valueChanged.connect(
                self._on_slider_changed)
            self._shadows_label = QLabel("0.000")
            sh_layout = QHBoxLayout()
            sh_layout.addWidget(self._shadows_slider)
            sh_layout.addWidget(self._shadows_label)
            manual_layout.addRow("Shadows:", sh_layout)

            # Highlights
            self._highlights_slider = QSlider(Qt.Horizontal)
            self._highlights_slider.setRange(1, 1000)
            self._highlights_slider.setValue(1000)
            self._highlights_slider.valueChanged.connect(
                self._on_slider_changed)
            self._highlights_label = QLabel("1.000")
            hl_layout = QHBoxLayout()
            hl_layout.addWidget(self._highlights_slider)
            hl_layout.addWidget(self._highlights_label)
            manual_layout.addRow("Highlights:", hl_layout)

            # Midtones
            self._midtones_slider = QSlider(Qt.Horizontal)
            self._midtones_slider.setRange(1, 999)
            self._midtones_slider.setValue(500)
            self._midtones_slider.valueChanged.connect(
                self._on_slider_changed)
            self._midtones_label = QLabel("0.500")
            mt_layout = QHBoxLayout()
            mt_layout.addWidget(self._midtones_slider)
            mt_layout.addWidget(self._midtones_label)
            manual_layout.addRow("Midtones:", mt_layout)

            # Compression
            self._compression_slider = QSlider(Qt.Horizontal)
            self._compression_slider.setRange(0, 1000)
            self._compression_slider.setValue(0)
            self._compression_slider.valueChanged.connect(
                self._on_slider_changed)
            self._compression_label = QLabel("0.000")
            cp_layout = QHBoxLayout()
            cp_layout.addWidget(self._compression_slider)
            cp_layout.addWidget(self._compression_label)
            manual_layout.addRow("Compression:", cp_layout)

            layout.addWidget(manual_group)

            # ===== 保存/加载 =====
            io_layout = QHBoxLayout()
            self._save_btn = QPushButton("保存参数")
            self._save_btn.clicked.connect(self._save_params)
            self._load_btn = QPushButton("加载参数")
            self._load_btn.clicked.connect(self._load_params)
            io_layout.addWidget(self._save_btn)
            io_layout.addWidget(self._load_btn)
            layout.addLayout(io_layout)

            layout.addStretch()

        def _on_mode_changed(self, index: int):
            """模式切换"""
            is_manual = (index == 1)
            is_preset = (index == 2)
            self._shadows_slider.setEnabled(is_manual)
            self._highlights_slider.setEnabled(is_manual)
            self._midtones_slider.setEnabled(is_manual)
            self._preset_combo.setEnabled(is_preset)

        def _on_slider_changed(self):
            """滑块变化"""
            s = self._shadows_slider.value() / 1000.0
            h = self._highlights_slider.value() / 1000.0
            m = self._midtones_slider.value() / 1000.0
            c = self._compression_slider.value() / 1000.0

            self._shadows_label.setText(f"{s:.3f}")
            self._highlights_label.setText(f"{h:.3f}")
            self._midtones_label.setText(f"{m:.3f}")
            self._compression_label.setText(f"{c:.3f}")

            params = STFParams(shadows=s, highlights=h,
                               midtones=m, compression=c)
            self._current_params = params
            self.params_changed.emit(params)

        def _on_preset_changed(self):
            """预设变化"""
            preset = self._preset_combo.currentText()
            if self._current_data is not None:
                try:
                    display = self._engine.apply_preset(
                        self._current_data, preset)
                    params_map = {
                        "linear": (0.5, 0.0),
                        "sqrt": (0.25, 0.0),
                        "asinh": (0.25, 0.5),
                        "log": (0.15, 0.8),
                    }
                    m, c = params_map.get(preset, (0.5, 0.0))
                    self._current_params = STFParams(midtones=m, compression=c)
                    self._midtones_slider.setValue(int(m * 1000))
                    self._compression_slider.setValue(int(c * 1000))
                    self.params_changed.emit(self._current_params)
                except Exception as e:
                    logger.error(f"预设应用失败: {e}")

        def _apply_auto(self):
            """应用自动拉伸"""
            if self._current_data is not None:
                params = self._engine.auto_stretch(self._current_data)
                self._current_params = params
                # 更新滑块 (归一化空间, shadows=0, highlights=1)
                self._shadows_slider.setValue(int(params.shadows * 1000))
                self._highlights_slider.setValue(int(params.highlights * 1000))
                self._midtones_slider.setValue(int(params.midtones * 1000))
                self.params_changed.emit(params)

        def set_data(self, data: np.ndarray):
            """设置当前数据 (用于自动拉伸计算)"""
            self._current_data = data

        def get_params(self) -> STFParams:
            """获取当前 STF 参数"""
            return self._current_params

        def _save_params(self):
            """保存参数"""
            from PyQt5.QtWidgets import QFileDialog
            path, _ = QFileDialog.getSaveFileName(
                self, "保存 STF 参数", "", "JSON (*.json)")
            if path:
                try:
                    STFEngine.save_params(self._current_params, path)
                except Exception as e:
                    QMessageBox.warning(self, "保存失败", str(e))

        def _load_params(self):
            """加载参数"""
            from PyQt5.QtWidgets import QFileDialog
            path, _ = QFileDialog.getOpenFileName(
                self, "加载 STF 参数", "", "JSON (*.json)")
            if path:
                try:
                    params = STFEngine.load_params(path)
                    self._current_params = params
                    self._shadows_slider.setValue(int(params.shadows * 1000))
                    self._highlights_slider.setValue(int(params.highlights * 1000))
                    self._midtones_slider.setValue(int(params.midtones * 1000))
                    self._compression_slider.setValue(int(params.compression * 1000))
                    self.params_changed.emit(params)
                except Exception as e:
                    QMessageBox.warning(self, "加载失败", str(e))


# ============================================================================
# 主窗口
# ============================================================================

if PYQT5_AVAILABLE:

    class MainWindow(QMainWindow):
        """HEALpix 浏览器主窗口

        集成:
        - 单帧浏览模式 (SingleFrameView)
        - 球面浏览模式 (SphereView)
        - STF 控制面板 (STFPanel)
        - 投影导出 (ExportDialog)
        """

        def __init__(self):
            super().__init__()
            self.setWindowTitle("HEALpix 球面可视化浏览器")
            self.setMinimumSize(1200, 800)

            # 当前模式
            self._mode = "single"  # "single" / "sphere"
            self._db_path = None
            self._current_bands = ["L"]
            self._current_nside = 512
            self._current_nested = True

            self._init_ui()
            self._init_menu()
            self._init_toolbar()
            self._init_statusbar()

            # 默认显示单帧模式
            self._switch_mode("single")

            logger.info("MainWindow 初始化完成")

        def _init_ui(self):
            """构建主 UI"""
            # 中央 widget: 分割器 (左=视图区, 右=控制面板)
            central = QWidget()
            self.setCentralWidget(central)
            layout = QHBoxLayout(central)

            splitter = QSplitter(Qt.Horizontal)

            # ===== 左侧: 视图区 (堆叠切换) =====
            self._view_stack = QStackedWidget()

            # 单帧视图
            if _SINGLE_FRAME_VIEW_OK:
                self._single_view = SingleFrameView()
                self._view_stack.addWidget(self._single_view)
            else:
                self._single_view = None
                placeholder = QLabel("单帧视图不可用\n(single_frame_view 导入失败)")
                placeholder.setAlignment(Qt.AlignCenter)
                self._view_stack.addWidget(placeholder)

            # 球面视图
            if _SPHERE_VIEW_OK:
                try:
                    self._sphere_view = SphereView()
                    self._view_stack.addWidget(self._sphere_view)
                except Exception as e:
                    logger.error(f"SphereView 创建失败: {e}")
                    self._sphere_view = None
                    placeholder = QLabel(f"球面视图不可用\n{e}")
                    placeholder.setAlignment(Qt.AlignCenter)
                    self._view_stack.addWidget(placeholder)
            else:
                self._sphere_view = None
                placeholder = QLabel("球面视图不可用\n(sphere_view 导入失败)")
                placeholder.setAlignment(Qt.AlignCenter)
                self._view_stack.addWidget(placeholder)

            splitter.addWidget(self._view_stack)

            # ===== 右侧: STF 控制面板 =====
            if _STF_OK:
                self._stf_panel = STFPanel()
                self._stf_panel.setMaximumWidth(350)
                splitter.addWidget(self._stf_panel)
                # STF 参数变化 → 刷新视图
                self._stf_panel.params_changed.connect(
                    self._on_stf_changed)
            else:
                self._stf_panel = None

            splitter.setStretchFactor(0, 4)
            splitter.setStretchFactor(1, 1)
            layout.addWidget(splitter)

        def _init_menu(self):
            """构建菜单栏"""
            menubar = self.menuBar()

            # ===== 文件菜单 =====
            file_menu = menubar.addMenu("文件(&F)")

            open_ahpx_action = QAction("打开 .ahpx 文件...", self)
            open_ahpx_action.setShortcut("Ctrl+O")
            open_ahpx_action.triggered.connect(self._on_open_ahpx)
            file_menu.addAction(open_ahpx_action)

            open_db_action = QAction("打开球数据库...", self)
            open_db_action.setShortcut("Ctrl+Shift+O")
            open_db_action.triggered.connect(self._on_open_database)
            file_menu.addAction(open_db_action)

            file_menu.addSeparator()

            export_action = QAction("投影导出...", self)
            export_action.setShortcut("Ctrl+E")
            export_action.triggered.connect(self._on_export)
            file_menu.addAction(export_action)

            file_menu.addSeparator()

            quit_action = QAction("退出", self)
            quit_action.setShortcut("Ctrl+Q")
            quit_action.triggered.connect(self.close)
            file_menu.addAction(quit_action)

            # ===== 视图菜单 =====
            view_menu = menubar.addMenu("视图(&V)")

            single_mode_action = QAction("单帧模式", self)
            single_mode_action.setShortcut("F1")
            single_mode_action.triggered.connect(
                lambda: self._switch_mode("single"))
            view_menu.addAction(single_mode_action)

            sphere_mode_action = QAction("球面模式", self)
            sphere_mode_action.setShortcut("F2")
            sphere_mode_action.triggered.connect(
                lambda: self._switch_mode("sphere"))
            view_menu.addAction(sphere_mode_action)

            view_menu.addSeparator()

            stf_action = QAction("STF 设置", self)
            stf_action.triggered.connect(self._focus_stf_panel)
            view_menu.addAction(stf_action)

            # ===== 工具菜单 =====
            tool_menu = menubar.addMenu("工具(&T)")

            band_action = QAction("波段切换", self)
            band_action.triggered.connect(self._on_band_switch)
            tool_menu.addAction(band_action)

            refresh_action = QAction("刷新渲染", self)
            refresh_action.setShortcut("F5")
            refresh_action.triggered.connect(self._on_refresh)
            tool_menu.addAction(refresh_action)

            # ===== 帮助菜单 =====
            help_menu = menubar.addMenu("帮助(&H)")

            about_action = QAction("关于", self)
            about_action.triggered.connect(self._on_about)
            help_menu.addAction(about_action)

        def _init_toolbar(self):
            """构建工具栏"""
            toolbar = self.addToolBar("主工具栏")
            toolbar.setMovable(False)

            # 打开 .ahpx
            open_btn = QAction("打开 .ahpx", self)
            open_btn.triggered.connect(self._on_open_ahpx)
            toolbar.addAction(open_btn)

            # 打开数据库
            db_btn = QAction("打开数据库", self)
            db_btn.triggered.connect(self._on_open_database)
            toolbar.addAction(db_btn)

            toolbar.addSeparator()

            # 模式切换
            single_btn = QAction("单帧模式", self)
            single_btn.triggered.connect(
                lambda: self._switch_mode("single"))
            toolbar.addAction(single_btn)

            sphere_btn = QAction("球面模式", self)
            sphere_btn.triggered.connect(
                lambda: self._switch_mode("sphere"))
            toolbar.addAction(sphere_btn)

            toolbar.addSeparator()

            # 导出
            export_btn = QAction("投影导出", self)
            export_btn.triggered.connect(self._on_export)
            toolbar.addAction(export_btn)

        def _init_statusbar(self):
            """构建状态栏"""
            self._status_label = QLabel("就绪")
            self.statusBar().addWidget(self._status_label)
            self.statusBar().showMessage("HEALpix 浏览器已启动", 3000)

        # ------------------------------------------------------------------
        # 模式切换
        # ------------------------------------------------------------------

        def _switch_mode(self, mode: str):
            """切换视图模式

            Args:
                mode: "single" 或 "sphere"
            """
            self._mode = mode
            if mode == "single":
                self._view_stack.setCurrentIndex(0)
                self._status_label.setText("模式: 单帧浏览")
                logger.info("切换到单帧模式")
            elif mode == "sphere":
                self._view_stack.setCurrentIndex(1)
                self._status_label.setText("模式: 球面浏览")
                logger.info("切换到球面模式")

        def _focus_stf_panel(self):
            """聚焦 STF 面板"""
            if self._stf_panel:
                self._stf_panel.setFocus()
                self._status_label.setText("STF 面板已聚焦")

        # ------------------------------------------------------------------
        # 文件操作
        # ------------------------------------------------------------------

        def _on_open_ahpx(self):
            """打开 .ahpx 文件"""
            path, _ = QFileDialog.getOpenFileName(
                self, "打开 .ahpx 文件", "",
                "AHPX 文件 (*.ahpx);;所有文件 (*.*)")
            if not path:
                return

            # 切换到单帧模式
            self._switch_mode("single")

            if self._single_view is not None:
                self._single_view.load_ahpx(path)
                self._status_label.setText(f"已加载: {os.path.basename(path)}")

                # 更新 STF 面板的数据引用
                if self._stf_panel and self._single_view._pixels is not None:
                    data = self._single_view._pixels
                    if data.ndim == 3:
                        data = data[:, :, 0]
                    self._stf_panel.set_data(data)

        def _on_open_database(self):
            """打开球数据库"""
            path = QFileDialog.getExistingDirectory(
                self, "打开球数据库目录", "")
            if not path:
                return

            self._db_path = path

            # 读取 meta.json 获取配置
            meta_path = os.path.join(path, "meta.json")
            if os.path.isfile(meta_path):
                try:
                    with open(meta_path, "r", encoding="utf-8") as f:
                        meta = json.load(f)
                    self._current_bands = meta.get("bands", ["L"])
                    self._current_nside = meta.get("nsideData", 512)
                    self._current_nested = meta.get("nested", True)
                except Exception as e:
                    logger.warning(f"读取 meta.json 失败: {e}")

            # 切换到球面模式
            self._switch_mode("sphere")

            if self._sphere_view is not None:
                self._sphere_view.open_database(path)
                self._status_label.setText(
                    f"已加载数据库: {os.path.basename(path)} "
                    f"(nside={self._current_nside})")

        def _on_export(self):
            """投影导出"""
            if not _EXPORT_DIALOG_OK:
                QMessageBox.warning(self, "提示",
                                    "导出模块不可用 (export_dialog 导入失败)")
                return

            if not self._db_path:
                QMessageBox.warning(self, "提示",
                                    "请先打开球数据库\n"
                                    "(文件 → 打开球数据库)")
                return

            dialog = ExportDialog(
                self,
                db_path=self._db_path,
                bands=self._current_bands,
                nside=self._current_nside,
                nested=self._current_nested,
            )
            dialog.exec_()

        def _on_band_switch(self):
            """波段切换 (快捷键)"""
            self._status_label.setText("请在视图工具栏中切换波段")

        def _on_refresh(self):
            """刷新渲染"""
            if self._mode == "sphere" and self._sphere_view is not None:
                self._sphere_view._refresh_render()
            elif self._mode == "single" and self._single_view is not None:
                self._single_view._apply_auto_stf()
            self._status_label.setText("已刷新")

        # ------------------------------------------------------------------
        # STF 回调
        # ------------------------------------------------------------------

        def _on_stf_changed(self, params):
            """STF 参数变化回调"""
            if self._mode == "single" and self._single_view is not None:
                # 重新应用 STF
                data = self._single_view._get_current_data()
                if data is not None and self._single_view._stf_engine:
                    try:
                        display = self._single_view._stf_engine.apply_stf(
                            data, params)
                        raw = self._single_view._get_current_raw_data()
                        self._single_view._image_widget.set_image(
                            display, raw, self._single_view._wcs_info)
                    except Exception as e:
                        logger.error(f"STF 应用失败: {e}")

            self._status_label.setText(
                f"STF: s={params.shadows:.3f} h={params.highlights:.3f} "
                f"m={params.midtones:.3f}")

        # ------------------------------------------------------------------
        # 关于
        # ------------------------------------------------------------------

        def _on_about(self):
            """关于对话框"""
            QMessageBox.about(
                self, "关于 HEALpix 浏览器",
                "<h3>HEALpix 球面可视化浏览器</h3>"
                "<p>天文巡天数据可视化浏览工具</p>"
                "<p>功能:</p>"
                "<ul>"
                "<li>单帧浏览 (.ahpx 格式)</li>"
                "<li>球数据库浏览 (HEALpix 球面渲染)</li>"
                "<li>STF 非破坏性拉伸 (自动/手动/预设)</li>"
                "<li>投影导出 (TAN/SIN/ZEA/AIT/CAR)</li>"
                "</ul>"
                "<p>依赖: PyQt5, vispy, numpy, healpy, astropy</p>")

        # ------------------------------------------------------------------
        # 关闭事件
        # ------------------------------------------------------------------

        def closeEvent(self, event):
            """窗口关闭: 清理资源"""
            logger.info("正在关闭主窗口...")

            if self._single_view is not None:
                self._single_view.close_reader()

            if self._sphere_view is not None:
                self._sphere_view.close_database()

            event.accept()
            logger.info("主窗口已关闭")


# ============================================================================
# 入口函数
# ============================================================================

def main():
    """主入口: 创建 QApplication 并启动主窗口"""
    if not PYQT5_AVAILABLE:
        print("错误: PyQt5 不可用")
        print("请安装: pip install PyQt5>=5.15")
        print("完整依赖: pip install PyQt5>=5.15 vispy>=0.9 numpy>=1.20 "
              "healpy>=1.16 astropy>=5.0")
        sys.exit(1)

    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    app.setApplicationName("HEALpix 浏览器")

    window = MainWindow()
    window.show()

    logger.info("应用程序启动")
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
