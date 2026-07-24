from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any, Optional, TYPE_CHECKING

import numpy as np

from .keywords import FITSKeyword

if TYPE_CHECKING:
    from .image_metadata import ImageMetadata


class ColorSpace(IntEnum):
    """色彩空间枚举，对应 PCL ColorSpace。"""

    GRAY = 0
    RGB = 1


@dataclass(slots=True, frozen=True)
class ImageGeometry:
    """图像几何属性，对应 PCL ImageGeometry。

    Attributes
    ----------
    width : int
        像素宽度 (NAXIS1)。
    height : int
        像素高度 (NAXIS2)。
    channels : int
        通道数，默认 1。
    """

    width: int
    height: int
    channels: int = 1

    @property
    def n_pixels(self) -> int:
        """像素总数。"""
        return self.width * self.height

    @property
    def shape(self) -> tuple[int, int]:
        """图像形状 (height, width)。"""
        return (self.height, self.width)

    @property
    def n_samples(self) -> int:
        """样本总数 (pixels × channels)。"""
        return self.n_pixels * self.channels


@dataclass(slots=True, frozen=True)
class ImageColor:
    """色彩空间，对应 PCL ImageColor。

    Attributes
    ----------
    color_space : ColorSpace
        色彩空间类型。
    """

    color_space: ColorSpace = ColorSpace.GRAY


@dataclass(slots=True, frozen=True)
class ImageOptions:
    """样本属性，对应 PCL ImageOptions。

    Attributes
    ----------
    bits_per_sample : int
        每样本位数，默认 16。
    float_sample : bool
        IEEE 754 浮点格式。
    complex_sample : bool
        复数样本。
    signed_integers : bool
        有符号整数。
    """

    bits_per_sample: int = 16
    float_sample: bool = False
    complex_sample: bool = False
    signed_integers: bool = True


@dataclass(slots=True, frozen=True)
class ImageInfo:
    """只读元数据快照，对应 PCL ImageInfo。

    Attributes
    ----------
    width : int
        像素宽度。
    height : int
        像素高度。
    channels : int
        通道数。
    color_space : ColorSpace
        色彩空间。
    supported : bool
        格式是否支持。
    """

    width: int = 0
    height: int = 0
    channels: int = 1
    color_space: ColorSpace = ColorSpace.GRAY
    supported: bool = True


@dataclass
class ImageData:
    """统一图像数据容器，对应 PCL ImageVariant。

    FITS/XISF 两种格式经过适配器层转化为 ImageData，
    上层校准逻辑无需关心底层格式差异。

    Attributes
    ----------
    data : np.ndarray
        像素数据，(height, width) 或 (channels, height, width)。
    geometry : ImageGeometry
        几何属性。
    color : ImageColor
        色彩空间。
    options : ImageOptions
        样本属性。
    keywords : list of FITSKeyword
        原始 FITS 关键字。
    metadata : ImageMetadata
        完整元数据 (WCS + 观测 + 校准)。
    properties : dict
        XISF 属性字典。
    source_format : str
        来源格式: 'fits' | 'xisf'。
    source_path : str
        源文件路径。
    """

    data: np.ndarray = field(default_factory=lambda: np.zeros((1, 1), dtype=np.float32))
    geometry: ImageGeometry = field(default_factory=lambda: ImageGeometry(1, 1))
    color: ImageColor = field(default_factory=ImageColor)
    options: ImageOptions = field(default_factory=ImageOptions)
    keywords: list[FITSKeyword] = field(default_factory=list)
    metadata: Optional[ImageMetadata] = None
    properties: dict[str, Any] = field(default_factory=dict)
    source_format: str = ""
    source_path: str = ""

    @property
    def wcs(self):
        return self.metadata.wcs if self.metadata else None

    @property
    def has_wcs(self) -> bool:
        return self.metadata is not None and self.metadata.wcs is not None and self.metadata.wcs.has_wcs

    @property
    def pixel_scale_arcsec(self) -> float:
        if self.has_wcs and self.metadata and self.metadata.wcs:
            return self.metadata.wcs.pixel_scale
        return 0.0

    @property
    def width(self) -> int:
        return self.geometry.width

    @property
    def height(self) -> int:
        return self.geometry.height

    @property
    def channels(self) -> int:
        return self.geometry.channels

    @property
    def shape(self) -> tuple[int, int]:
        return self.geometry.shape

    def get_keyword(self, name: str, default: Optional[str] = None) -> Optional[str]:
        """按名称获取 FITS 关键字值。

        Parameters
        ----------
        name : str
            关键字名，大小写不敏感。
        default : str or None
            未找到时的默认值。

        Returns
        -------
        str or None
        """
        for kw in self.keywords:
            if kw.name.upper() == name.upper():
                return kw.value
        return default

    def get_keyword_float(self, name: str, default: float = 0.0) -> float:
        """按名称获取 FITS 关键字浮点值。

        Parameters
        ----------
        name : str
            关键字名。
        default : float
            未找到或转换失败时的默认值。

        Returns
        -------
        float
        """
        val = self.get_keyword(name)
        if val is None:
            return default
        try:
            return float(val)
        except (ValueError, TypeError):
            return default

    def get_keyword_int(self, name: str, default: int = 0) -> int:
        """按名称获取 FITS 关键字整数值。"""
        val = self.get_keyword(name)
        if val is None:
            return default
        try:
            return int(float(val))
        except (ValueError, TypeError):
            return default

    def to_numpy(self) -> np.ndarray:
        """获取 float32 numpy 数组，供校准运算使用。

        Returns
        -------
        np.ndarray
        """
        return self.data.astype(np.float32, copy=False)

    def to_numpy_f64(self) -> np.ndarray:
        """获取 float64 numpy 数组。

        Returns
        -------
        np.ndarray
        """
        return self.data.astype(np.float64, copy=False)

    @classmethod
    def from_data(
        cls,
        data: np.ndarray,
        source_path: str = "",
        source_format: str = "",
    ) -> ImageData:
        """从纯 numpy 数组快速构造 ImageData。

        Parameters
        ----------
        data : np.ndarray
            2D 像素数据。
        source_path : str
            源路径。
        source_format : str
            源格式。

        Returns
        -------
        ImageData
        """
        if data.ndim == 2:
            h, w = data.shape
            channels = 1
        elif data.ndim == 3:
            c, h, w = data.shape
            channels = c
        else:
            h, w = data.shape[-2:]
            channels = 1

        return cls(
            data=data.astype(np.float32, copy=False),
            geometry=ImageGeometry(width=w, height=h, channels=channels),
            source_path=source_path,
            source_format=source_format,
        )
