"""
__main__.py - HEALpix 浏览器入口点

运行方式:
    python -m healpix_browser
    或
    python main_window.py

功能: 启动 HEALpix 球面可视化浏览器主窗口
"""

import sys
import os

# 将本模块目录加入 sys.path
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

# 父目录 (healpix_db/) 也加入, 用于导入 ahpx_io / healpix_stack / healpix_lod
_PARENT_DIR = os.path.dirname(_THIS_DIR)
if _PARENT_DIR not in sys.path:
    sys.path.insert(0, _PARENT_DIR)

from main_window import main

if __name__ == "__main__":
    main()
