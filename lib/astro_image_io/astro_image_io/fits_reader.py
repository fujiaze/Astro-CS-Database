from __future__ import annotations

import os
import logging
from typing import Optional, Tuple

import numpy as np
from astropy.io import fits

from ._types import PathLike
from .image_data import ImageData, ImageGeometry, ImageColor, ImageOptions, ImageInfo, ColorSpace
from .keywords import FITSKeyword
from .image_metadata import ImageMetadata
from .wcs_keywords import WCSKeywords
from .observation_metadata import ObservationMetadata, CalibrationMetadata
from .reader import ImageReader

logger = logging.getLogger(__name__)


class FITSReader(ImageReader):
    """FITS 格式读取适配器。

    基于 astropy.io.fits，将 FITS 数据转化为统一的 ImageData。

    Examples
    --------
    >>> reader = FITSReader()
    >>> img = reader.read("M31_300s_L.fits")
    >>> print(img.width, img.height)
    """

    def read(self, path: PathLike) -> ImageData:
        """读取 FITS 文件，返回 ImageData。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageData
        """
        path_str = str(path)
        with fits.open(path_str) as hdul:
            raw_data = np.asarray(hdul[0].data, dtype=np.float64)
            header = hdul[0].header

        data = raw_data.astype(np.float32, copy=False)

        if data.ndim == 3 and data.shape[0] == 1:
            data = data[0]
        elif data.ndim == 3:
            data = data[0]

        h, w = data.shape

        geometry = ImageGeometry(width=w, height=h, channels=1)
        color = ImageColor(color_space=ColorSpace.GRAY)
        options = ImageOptions(
            bits_per_sample=abs(header.get("BITPIX", 16)),
            float_sample=header.get("BITPIX", 16) < 0,
        )

        keywords = self._extract_keywords(header)
        metadata = ImageMetadata.from_keywords(keywords)

        return ImageData(
            data=data,
            geometry=geometry,
            color=color,
            options=options,
            keywords=keywords,
            metadata=metadata,
            source_format="fits",
            source_path=path_str,
        )

    def read_info(self, path: PathLike) -> ImageInfo:
        """仅读取基本信息快照。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageInfo
        """
        path_str = str(path)
        with fits.open(path_str) as hdul:
            header = hdul[0].header
            w = int(header.get("NAXIS1", 0))
            h = int(header.get("NAXIS2", 0))
            naxis3 = int(header.get("NAXIS3", 1))
        return ImageInfo(
            width=w, height=h, channels=max(naxis3, 1),
            color_space=ColorSpace.GRAY, supported=True,
        )

    def read_keywords(self, path: PathLike) -> list[FITSKeyword]:
        """仅读取 FITS 关键字。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        list of FITSKeyword
        """
        path_str = str(path)
        with fits.open(path_str) as hdul:
            return self._extract_keywords(hdul[0].header)

    def read_metadata(self, path: PathLike) -> ImageMetadata:
        """读取完整元数据。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageMetadata
        """
        keywords = self.read_keywords(path)
        return ImageMetadata.from_keywords(keywords)

    def read_wcs(self, path: PathLike) -> Optional[WCSKeywords]:
        """仅读取 WCS 坐标。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        WCSKeywords or None
        """
        keywords = self.read_keywords(path)
        wcs = WCSKeywords.from_keywords(keywords)
        return wcs if wcs.has_wcs else None

    def read_header_only(self, path: PathLike) -> ImageData:
        """仅读取 header 和元数据，像素数据为零填充。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageData
        """
        info = self.read_info(path)
        keywords = self.read_keywords(path)
        metadata = ImageMetadata.from_keywords(keywords)

        return ImageData(
            data=np.zeros((info.height, info.width), dtype=np.float32),
            geometry=ImageGeometry(width=info.width, height=info.height),
            keywords=keywords,
            metadata=metadata,
            source_format="fits",
            source_path=str(path),
        )

    @staticmethod
    def _extract_keywords(header: fits.Header) -> list[FITSKeyword]:
        """从 astropy Header 提取关键字列表。

        Parameters
        ----------
        header : fits.Header

        Returns
        -------
        list of FITSKeyword
        """
        keywords: list[FITSKeyword] = []
        for key, value in header.items():
            if key in ("", "COMMENT", "HISTORY"):
                continue
            keywords.append(FITSKeyword(
                name=str(key).strip().upper(),
                value=str(value).strip(),
                comment=str(header.comments[key] if key in header.comments else "").strip(),
            ))
        return keywords

    @staticmethod
    def read_raw(file_path: PathLike) -> Tuple[np.ndarray, fits.Header]:
        """低层读取：返回 (data, header) 元组。

        兼容旧 FrameMaker._read_fits() 接口。

        Parameters
        ----------
        file_path : PathLike

        Returns
        -------
        tuple[np.ndarray, fits.Header]
        """
        with fits.open(str(file_path)) as hdul:
            data = np.asarray(hdul[0].data, dtype=np.float32)
            header = hdul[0].header
            if data.ndim == 3:
                data = data[0]
            return data, header
