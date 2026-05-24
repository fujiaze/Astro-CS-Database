from __future__ import annotations

from typing import Optional


class CalibrationError(Exception):
    """校准管线通用异常基类。"""

    def __init__(self, message: str, *, frame_path: Optional[str] = None) -> None:
        super().__init__(message)
        self.frame_path = frame_path


class FrameReadError(CalibrationError):
    """帧读取异常。"""


class FrameGroupingError(CalibrationError):
    """帧分组异常。"""


class DarkOptimizationError(CalibrationError):
    """暗场优化异常。"""


class XISFError(Exception):
    """XISF 格式处理异常基类。"""


class XISFParseError(XISFError):
    """XISF 解析异常。"""


class XISFUnsupportedError(XISFError):
    """不支持的 XISF 特性。"""
