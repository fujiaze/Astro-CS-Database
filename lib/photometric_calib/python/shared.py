# -*- coding: utf-8 -*-
"""
Photometric Calib 共享组件 (合并文件)
功能: 合并 data_types.py 与 pc_logger.py，提供数据结构与日志接口
用途: 为 photometric_calib 模块提供统一的数据结构与日志接口
合并日期: 2026-07-16
合并来源 (架构重构 spec G5 Phase 3):
  - data_types.py (原 lib/photometric_calib/shared/python/data_types.py)
  - pc_logger.py (原 lib/photometric_calib/shared/python/pc_logger.py)
路径调整: _LOG_DIR 从回溯3级 (shared/python/ -> photometric_calib/)
          改为回溯1级 (python/ -> photometric_calib/)，即 dirname×2
"""

# ======================================================================
# Part 1: 数据结构 (原 shared/python/data_types.py)
# ----------------------------------------------------------------------
# 原文件 docstring:
#   公共数据结构
#   功能: 定义光谱积分器和梯度估算器共享的数据结构
#   用途: 为 photometric_calib 双程序提供统一的输入/输出数据载体，减少重复定义
#   依赖: dataclasses, numpy
# ======================================================================

from dataclasses import dataclass, field
import numpy as np


@dataclass
class GaiaSpectrumStarPy:
    ra: float = 0.0
    dec: float = 0.0
    mag_g: float = 0.0
    mag_bp: float = 0.0
    mag_rp: float = 0.0
    source_id: int = 0
    spectrum: np.ndarray = None  # uint8[343]


@dataclass
class FSynResult:
    source_id: int = 0
    ra: float = 0.0
    dec: float = 0.0
    mag_g: float = 0.0
    f_syn: float = 0.0


# ======================================================================
# Part 2: 日志系统 (原 shared/python/pc_logger.py)
# ----------------------------------------------------------------------
# 原文件 docstring:
#   Photometric Calib Logger - 鲁棒流量校准模块日志系统（共享组件）
#   功能: 封装 Python logging 模块，支持同时输出到文件和控制台
#   用途: 为 photometric_calib 双程序（spectrum_integrator / gradient_estimator）
#         提供统一日志接口，文件输出到 lib/photometric_calib/logs/ 目录，UTF-8 编码
#   依赖: Python 标准库 logging
# ======================================================================

import logging
import os
import sys
from datetime import datetime

_BASE_LOGGER = "photometric_calib"
# 路径调整: 原 shared/python/pc_logger.py 回溯3级到 photometric_calib/
# 现 python/shared.py 回溯1级 (dirname×2) 到 photometric_calib/
_LOG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"

_initialized = False
_log_file = None


def _init_logging():
    """初始化文件与控制台 handler，仅执行一次"""
    global _initialized, _log_file
    if _initialized:
        return
    os.makedirs(_LOG_DIR, exist_ok=True)
    _log_file = os.path.join(
        _LOG_DIR, "calib_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"
    )
    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)

    file_handler = logging.FileHandler(_log_file, encoding="utf-8")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)

    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.DEBUG)
    console_handler.setFormatter(formatter)

    base_logger = logging.getLogger(_BASE_LOGGER)
    base_logger.setLevel(logging.DEBUG)
    base_logger.propagate = False
    base_logger.addHandler(file_handler)
    base_logger.addHandler(console_handler)

    _initialized = True
    base_logger.info("日志系统初始化完成，日志文件: %s", _log_file)


def get_logger(name=None):
    """获取 photometric_calib 模块 logger，name 为子模块名（可选）"""
    _init_logging()
    if name:
        logger_name = _BASE_LOGGER + "." + name
    else:
        logger_name = _BASE_LOGGER
    logger = logging.getLogger(logger_name)
    logger.setLevel(logging.DEBUG)
    return logger
