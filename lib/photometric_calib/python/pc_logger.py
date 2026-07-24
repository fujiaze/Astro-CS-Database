"""
Photometric Calib Logger - 鲁棒流量校准模块日志系统
功能: 封装 Python logging 模块，支持同时输出到文件和控制台
用途: 为 photometric_calib 模块提供统一日志接口，文件输出到 logs/ 目录，UTF-8 编码
依赖: Python 标准库 logging
"""

import logging
import os
import sys
from datetime import datetime

_BASE_LOGGER = "photometric_calib"
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
