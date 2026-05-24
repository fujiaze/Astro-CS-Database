from __future__ import annotations

import os
from dataclasses import dataclass, field
from typing import ClassVar, Optional, get_type_hints

from ._types import PathLike
from .image_data import ImageData, ImageInfo, ImageGeometry
from .keywords import FITSKeyword
from .image_metadata import ImageMetadata
from .wcs_keywords import WCSKeywords


class ImageReader:
    """图像读取适配器接口 — Protocol-like 基类。

    每种格式 (FITS/XISF) 实现此接口。
    """

    def read(self, path: PathLike) -> ImageData:
        raise NotImplementedError

    def read_info(self, path: PathLike) -> ImageInfo:
        raise NotImplementedError

    def read_keywords(self, path: PathLike) -> list[FITSKeyword]:
        raise NotImplementedError

    def read_metadata(self, path: PathLike) -> ImageMetadata:
        raise NotImplementedError

    def read_wcs(self, path: PathLike) -> Optional[WCSKeywords]:
        raise NotImplementedError

    def read_header_only(self, path: PathLike) -> ImageData:
        raise NotImplementedError


class ImageWriter:
    """图像写入适配器接口 — Protocol-like 基类。"""

    def write(self, image: ImageData, path: PathLike) -> str:
        raise NotImplementedError


@dataclass
class ImageReaderFactory:
    """格式自动检测 + 适配器分发。

    根据文件扩展名自动选择 FITSReader 或 XISFReader。

    Attributes
    ----------
    fits_reader : ImageReader
        FITS 格式读取器 (延迟初始化)。
    xisf_reader : ImageReader or None
        XISF 格式读取器 (延迟初始化)。
    """

    FITS_EXTENSIONS: ClassVar[set[str]] = {
        ".fits", ".fit", ".fts", ".FITS", ".FIT", ".FTS",
        ".fits.gz", ".fit.gz", ".fts.gz",
        ".FITS.GZ", ".FIT.GZ", ".FTS.GZ",
    }
    XISF_EXTENSIONS: ClassVar[set[str]] = {".xisf", ".XISF"}

    _fits_reader: ImageReader = field(init=False, default=None)
    _xisf_reader: Optional[ImageReader] = field(init=False, default=None)

    def _get_fits_reader(self) -> ImageReader:
        if self._fits_reader is None:
            from .fits_reader import FITSReader
            self._fits_reader = FITSReader()
        return self._fits_reader

    def _get_xisf_reader(self) -> Optional[ImageReader]:
        if self._xisf_reader is None:
            try:
                from .xisf_reader import XISFReader
                self._xisf_reader = XISFReader()
            except ImportError:
                pass
        return self._xisf_reader

    @staticmethod
    def get_extension(path: PathLike) -> str:
        """获取文件扩展名（小写，含点）。"""
        ext = os.path.splitext(str(path))[1]
        if ext == "":
            base = os.path.basename(str(path))
            if base.lower().endswith(".fits.gz"):
                return ".fits.gz"
            if base.lower().endswith(".fit.gz"):
                return ".fit.gz"
        return ext

    def get_reader(self, path: PathLike) -> ImageReader:
        """根据文件扩展名获取读取器。

        Parameters
        ----------
        path : PathLike
            文件路径。

        Returns
        -------
        ImageReader

        Raises
        ------
        ValueError
            不支持的文件格式。
        """
        ext = self.get_extension(path)
        if ext.lower() in self.FITS_EXTENSIONS:
            return self._get_fits_reader()
        if ext.lower() in self.XISF_EXTENSIONS:
            reader = self._get_xisf_reader()
            if reader is None:
                raise ValueError("XISF 读取器不可用")
            return reader
        raise ValueError(f"不支持的文件格式: {ext}")

    def read(self, path: PathLike) -> ImageData:
        """读取图像文件，返回统一 ImageData。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageData
        """
        return self.get_reader(path).read(path)

    def read_info(self, path: PathLike) -> ImageInfo:
        """仅读取图像元数据快照（不读像素数据）。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageInfo
        """
        return self.get_reader(path).read_info(path)

    def read_metadata(self, path: PathLike) -> ImageMetadata:
        """读取完整元数据（不读像素数据）。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageMetadata
        """
        return self.get_reader(path).read_metadata(path)

    def read_wcs(self, path: PathLike) -> Optional[WCSKeywords]:
        """仅读取 WCS 坐标信息。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        WCSKeywords or None
        """
        return self.get_reader(path).read_wcs(path)

    def read_keywords(self, path: PathLike) -> list[FITSKeyword]:
        """仅读取 FITS 关键字列表。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        list of FITSKeyword
        """
        return self.get_reader(path).read_keywords(path)

    @staticmethod
    def is_supported(path: PathLike) -> bool:
        """检查文件格式是否受支持。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        bool
        """
        ext = ImageReaderFactory.get_extension(path)
        return ext.lower() in ImageReaderFactory.FITS_EXTENSIONS \
            or ext.lower() in ImageReaderFactory.XISF_EXTENSIONS
