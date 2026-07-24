"""统一数据架构模块 (src/data)。

提供 FITS/XISF 格式无关的图像数据容器，参考 PCL (PixInsight Class Library)
数据模型实现。

核心类:
    ImageGeometry  — 图像几何属性 (宽/高/通道)
    ImageColor     — 色彩空间 (Gray/RGB)
    ImageOptions   — 样本属性 (位深度/浮点/符号)
    ImageInfo      — 只读元数据快照
    ImageData      — 统一图像数据容器
    FITSKeyword    — FITS 头关键字
    WCSKeywords    — WCS 坐标关键字
    ImageMetadata  — 完整元数据

适配器:
    FITSReader     — FITS 读取 (astropy.io.fits)
    XISFReader     — XISF 读取 (PCL绑定 + 纯Python回退)
    FITSWriter     — FITS 写入
    ImageReaderFactory — 自动格式检测与分发

Examples
--------
>>> from data import ImageReaderFactory
>>> factory = ImageReaderFactory()
>>> img = factory.read("M31_300s_L.fits")
>>> print(img.width, img.height, img.wcs.pixel_scale)
>>> data = img.to_numpy()
"""

from ._types import NDArrayFloat, NDArrayFloat64, NDArrayInt, ImageArray, PathLike
from .errors import (
    CalibrationError, FrameReadError, FrameGroupingError,
    DarkOptimizationError, XISFError, XISFParseError, XISFUnsupportedError,
)
from .keywords import FITSKeyword, make_keyword
from .image_data import (
    ColorSpace, ImageGeometry, ImageColor, ImageOptions,
    ImageInfo, ImageData,
)
from .wcs_keywords import WCSKeywords
from .observation_metadata import ObservationMetadata, CalibrationMetadata
from .image_metadata import ImageMetadata
from .reader import ImageReader, ImageWriter, ImageReaderFactory
from .fits_reader import FITSReader
from .writer import FITSWriter, XISFWriter
from .xisf_reader import XISFReader

__all__ = [
    # 类型
    "NDArrayFloat", "NDArrayFloat64", "NDArrayInt", "ImageArray", "PathLike",
    # 异常
    "CalibrationError", "FrameReadError", "FrameGroupingError",
    "DarkOptimizationError", "XISFError", "XISFParseError", "XISFUnsupportedError",
    # 核心
    "ColorSpace", "ImageGeometry", "ImageColor", "ImageOptions",
    "ImageInfo", "ImageData", "FITSKeyword", "make_keyword",
    # WCS
    "WCSKeywords",
    # 元数据
    "ObservationMetadata", "CalibrationMetadata", "ImageMetadata",
    # 适配器
    "ImageReader", "ImageWriter", "ImageReaderFactory",
    "FITSReader", "FITSWriter", "XISFReader", "XISFWriter",
]
