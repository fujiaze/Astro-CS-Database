from __future__ import annotations

import os
import logging
import numpy as np
from astropy.io import fits
from datetime import datetime

from ._types import PathLike
from .image_data import ImageData
from .reader import ImageWriter

logger = logging.getLogger(__name__)


class FITSWriter(ImageWriter):
    """FITS 格式写入。

    将 ImageData 写回 FITS 文件，保留关键字并追加校准历史。

    Examples
    --------
    >>> writer = FITSWriter()
    >>> writer.write(calibrated_image, "output/light_001_cal.fits")
    """

    def write(self, image: ImageData, path: PathLike, *, overwrite: bool = True) -> str:
        """写入 ImageData 到 FITS 文件。

        Parameters
        ----------
        image : ImageData
            要写入的图像。
        path : PathLike
            输出文件路径。
        overwrite : bool
            是否覆盖已有文件。

        Returns
        -------
        str
            输出文件路径。
        """
        path_str = str(path)
        os.makedirs(os.path.dirname(path_str) or ".", exist_ok=True)

        header = fits.Header()

        for kw in image.keywords:
            name = kw.name.upper()
            if name not in ("", "COMMENT", "HISTORY", "SIMPLE", "BITPIX",
                           "NAXIS", "NAXIS1", "NAXIS2", "NAXIS3", "EXTEND"):
                try:
                    header[name] = (kw.value, kw.comment)
                except (ValueError, TypeError):
                    header[name] = kw.value

        bunit = image.get_keyword("BUNIT", "ADU")
        header["BUNIT"] = bunit

        if image.metadata is not None and image.metadata.wcs is not None:
            for kw in image.metadata.wcs.to_keywords():
                header[kw.name] = kw.value

        data = image.data.astype(np.float32, copy=False)

        fits.writeto(path_str, data, header, overwrite=overwrite)
        logger.debug(f"写入 FITS: {path_str} ({image.width}x{image.height})")
        return path_str


class XISFWriter(ImageWriter):
    """XISF 格式写入 — placeholder，待 XISF 写入后端确定后实现。"""

    def write(self, image: ImageData, path: PathLike) -> str:
        raise NotImplementedError("XISFWriter 尚未实现")
