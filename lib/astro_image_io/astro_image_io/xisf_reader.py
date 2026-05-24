"""XISF 格式读取适配器。

优先 PCL C++ 绑定，回退到纯 Python 实现。
"""

from __future__ import annotations

import logging
from typing import Optional

from ._types import PathLike
from .image_data import ImageData, ImageInfo
from .keywords import FITSKeyword
from .image_metadata import ImageMetadata
from .reader import ImageReader

logger = logging.getLogger(__name__)


class XISFReader(ImageReader):
    """XISF 格式读取适配器，对应 PCL XISFReader。

    优先 PCL C++ 绑定，回退到纯 Python 实现。

    Examples
    --------
    >>> reader = XISFReader()
    >>> img = reader.read("M31_300s_L.xisf")
    """

    def __init__(self) -> None:
        self._backend = self._init_backend()

    def _init_backend(self) -> ImageReader:
        try:
            from ._xisf_pcl import XISFReaderPCL
            logger.info("XISF: 使用 PCL C++ 绑定后端")
            return XISFReaderPCL()
        except ImportError:
            from ._xisf_py import XISFReaderPy
            logger.info("XISF: 使用纯 Python 后端")
            return XISFReaderPy()

    def read(self, path: PathLike) -> ImageData:
        return self._backend.read(path)

    def read_info(self, path: PathLike) -> ImageInfo:
        return self._backend.read_info(path)

    def read_keywords(self, path: PathLike) -> list[FITSKeyword]:
        return self._backend.read_keywords(path)

    def read_metadata(self, path: PathLike) -> ImageMetadata:
        return self._backend.read_metadata(path)

    def read_wcs(self, path: PathLike):
        return self._backend.read_wcs(path)

    def read_header_only(self, path: PathLike) -> ImageData:
        return self._backend.read_header_only(path)
