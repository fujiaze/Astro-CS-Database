"""
healpix_browser - HEALpix 球面可视化浏览器模块

功能: 天文巡天数据的可视化浏览工具
- 单帧浏览 (.ahpx 文件)
- 球数据库浏览 (球面渲染, 拖动旋转, 滚轮缩放)
- STF 非破坏性拉伸 (自动/手动/预设)
- 投影导出 (TAN/SIN/ZEA/AIT/CAR)

子模块:
- stf: Screen Transfer Function 拉伸引擎
- sphere_renderer: 球面渲染引擎 (vispy/matplotlib)
- single_frame_view: 单帧浏览视图
- sphere_view: 球数据库浏览视图
- export_dialog: 投影导出对话框
- main_window: Qt 主窗口

运行:
    python -m healpix_browser
"""

from __future__ import annotations

import os
import sys

# 确保模块路径可用
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_PARENT_DIR = os.path.dirname(_THIS_DIR)
for _p in [_THIS_DIR, _PARENT_DIR]:
    if _p not in sys.path:
        sys.path.insert(0, _p)

# 版本号
__version__ = "1.0.0"

# 模块可用性标志 (延迟检测)
_STF_AVAILABLE = False
try:
    from stf import STFEngine, STFParams
    _STF_AVAILABLE = True
except Exception:
    pass

__all__ = ["__version__"]
