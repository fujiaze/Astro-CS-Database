from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np

from .keywords import FITSKeyword


@dataclass(slots=True)
class WCSKeywords:
    """WCS 坐标关键字集合，对应 PCL WCSKeywords + FITS WCS Paper II。

    从 FITS/XISF 关键字中提取，提供坐标信息访问。

    Attributes
    ----------
    crpix1, crpix2 : float
        参考像素坐标。
    crval1, crval2 : float
        参考点天球坐标 (度)。
    ctype1, ctype2 : str
        坐标轴类型与投影。
    cd1_1 ~ cd2_2 : float
        CD 变换矩阵元素 (度/像素)。
    cdelt1, cdelt2 : float or None
        像素比例尺 (CD 矩阵不存在时回退)。
    radesys : str
        坐标参考系，默认 "ICRS"。
    equinox : float or None
        历元 (RADESYS=ICRS 时无效)。
    lonpole, latpole : float or None
        天北极本征坐标 (度)。
    """

    crpix1: float = 0.0
    crpix2: float = 0.0
    crval1: float = 0.0
    crval2: float = 0.0
    ctype1: str = ""
    ctype2: str = ""
    cd1_1: float = 0.0
    cd1_2: float = 0.0
    cd2_1: float = 0.0
    cd2_2: float = 0.0
    cdelt1: Optional[float] = None
    cdelt2: Optional[float] = None
    radesys: str = "ICRS"
    equinox: Optional[float] = 2000.0
    lonpole: Optional[float] = None
    latpole: Optional[float] = None

    @property
    def has_wcs(self) -> bool:
        """是否包含有效 WCS 信息。"""
        return (
            self.ctype1 != ""
            and self.ctype2 != ""
            and (abs(self.cd1_1) > 1e-15 or abs(self.cd1_2) > 1e-15
                 or abs(self.cd2_1) > 1e-15 or abs(self.cd2_2) > 1e-15)
        )

    @property
    def cd_matrix(self) -> np.ndarray:
        """CD 变换矩阵 (2, 2)。"""
        return np.array([[self.cd1_1, self.cd1_2],
                         [self.cd2_1, self.cd2_2]], dtype=np.float64)

    @property
    def pixel_scale(self) -> float:
        """像素比例尺 (角秒/像素)，从 CD 矩阵行列式近似计算。

        Returns
        -------
        float
        """
        det = self.cd1_1 * self.cd2_2 - self.cd1_2 * self.cd2_1
        if abs(det) < 1e-30:
            return 0.0
        return float(np.sqrt(abs(det)) * 3600.0)

    @property
    def rotation_deg(self) -> float:
        """图像旋转角 (度)，基于 CD 矩阵近似计算。"""
        import math
        scale_x = np.sqrt(self.cd1_1 ** 2 + self.cd2_1 ** 2)
        if scale_x < 1e-15:
            return 0.0
        return float(math.degrees(math.atan2(self.cd2_1, self.cd1_1)))

    @classmethod
    def from_keywords(cls, keywords: list[FITSKeyword]) -> WCSKeywords:
        """从 FITSKeyword 列表构建 WCSKeywords。

        Parameters
        ----------
        keywords : list of FITSKeyword
            FITS 关键字列表。

        Returns
        -------
        WCSKeywords
        """
        kw_map: dict[str, str] = {kw.name: kw.value for kw in keywords}

        def _float(name: str, default: float = 0.0) -> float:
            val = kw_map.get(name)
            if val is None:
                return default
            try:
                return float(val)
            except (ValueError, TypeError):
                return default

        def _opt(name: str) -> Optional[float]:
            val = kw_map.get(name)
            if val is None:
                return None
            try:
                return float(val)
            except (ValueError, TypeError):
                return None

        return cls(
            crpix1=_float("CRPIX1"),
            crpix2=_float("CRPIX2"),
            crval1=_float("CRVAL1"),
            crval2=_float("CRVAL2"),
            ctype1=kw_map.get("CTYPE1", ""),
            ctype2=kw_map.get("CTYPE2", ""),
            cd1_1=_float("CD1_1", _float("CD001001")),
            cd1_2=_float("CD1_2", _float("CD001002")),
            cd2_1=_float("CD2_1", _float("CD002001")),
            cd2_2=_float("CD2_2", _float("CD002002")),
            cdelt1=_opt("CDELT1"),
            cdelt2=_opt("CDELT2"),
            radesys=kw_map.get("RADESYS", "ICRS"),
            equinox=_opt("EQUINOX"),
            lonpole=_opt("LONPOLE"),
            latpole=_opt("LATPOLE"),
        )

    @classmethod
    def from_header(cls, header) -> WCSKeywords:
        """从 astropy Header 构建 WCSKeywords。

        Parameters
        ----------
        header : astropy.io.fits.Header
            FITS 头。

        Returns
        -------
        WCSKeywords
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
        return cls.from_keywords(keywords)

    def to_keywords(self) -> list[FITSKeyword]:
        """导出为 FITSKeyword 列表。

        Returns
        -------
        list of FITSKeyword
        """
        result: list[FITSKeyword] = [
            FITSKeyword("CRPIX1", str(self.crpix1)),
            FITSKeyword("CRPIX2", str(self.crpix2)),
            FITSKeyword("CRVAL1", str(self.crval1)),
            FITSKeyword("CRVAL2", str(self.crval2)),
            FITSKeyword("CTYPE1", self.ctype1),
            FITSKeyword("CTYPE2", self.ctype2),
            FITSKeyword("CD1_1", str(self.cd1_1)),
            FITSKeyword("CD1_2", str(self.cd1_2)),
            FITSKeyword("CD2_1", str(self.cd2_1)),
            FITSKeyword("CD2_2", str(self.cd2_2)),
            FITSKeyword("RADESYS", self.radesys),
        ]
        if self.equinox is not None:
            result.append(FITSKeyword("EQUINOX", str(self.equinox)))
        if self.lonpole is not None:
            result.append(FITSKeyword("LONPOLE", str(self.lonpole)))
        if self.latpole is not None:
            result.append(FITSKeyword("LATPOLE", str(self.latpole)))
        return result

    def to_astropy_wcs(self):
        """构建 astropy WCS 对象。

        Returns
        -------
        astropy.wcs.WCS
        """
        from astropy.wcs import WCS

        w = WCS(naxis=2)
        w.wcs.crpix = [self.crpix1, self.crpix2]
        w.wcs.crval = [self.crval1, self.crval2]
        w.wcs.ctype = [
            self.ctype1 or "RA---TAN",
            self.ctype2 or "DEC--TAN",
        ]
        w.wcs.cd = self.cd_matrix
        w.wcs.radesys = self.radesys
        if self.equinox is not None:
            w.wcs.equinox = self.equinox
        if self.lonpole is not None:
            w.wcs.lonpole = self.lonpole
        if self.latpole is not None:
            w.wcs.latpole = self.latpole
        w.wcs.set()
        return w
