from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .keywords import FITSKeyword


@dataclass(slots=True)
class ObservationMetadata:
    """观测元数据，对应 PCL AstrometricMetadata 观测部分。

    Attributes
    ----------
    date_obs : str or None
        观测开始时间 (ISO 8601, DATE-OBS)。
    date_end : str or None
        观测结束时间 (DATE-END)。
    jd_obs : float or None
        儒略日 (JD)。
    longobs : float or None
        观测地经度 (度, 东正)。
    latobs : float or None
        观测地纬度 (度, 北正)。
    altobs : float or None
        海拔 (米)。
    observat : str or None
        观测站名称。
    focallen : float or None
        焦距 (毫米)。
    xpixsz : float or None
        像素尺寸 (微米)。
    aperture : float or None
        口径 (毫米)。
    focal_ratio : float or None
        焦比 (f/)。
    fov_x : float or None
        视场宽度 (角分)。
    fov_y : float or None
        视场高度 (角分)。
    object_name : str
        目标名称 (OBJECT)。
    """

    date_obs: Optional[str] = None
    date_end: Optional[str] = None
    jd_obs: Optional[float] = None
    longobs: Optional[float] = None
    latobs: Optional[float] = None
    altobs: Optional[float] = None
    observat: Optional[str] = None
    focallen: Optional[float] = None
    xpixsz: Optional[float] = None
    aperture: Optional[float] = None
    focal_ratio: Optional[float] = None
    fov_x: Optional[float] = None
    fov_y: Optional[float] = None
    object_name: str = ""

    @classmethod
    def from_keywords(cls, keywords: list[FITSKeyword]) -> ObservationMetadata:
        kw_map: dict[str, str] = {kw.name: kw.value for kw in keywords}

        def _opt(name: str) -> Optional[str]:
            return kw_map.get(name)

        def _opt_f(name: str) -> Optional[float]:
            val = kw_map.get(name)
            if val is None:
                return None
            try:
                return float(val)
            except (ValueError, TypeError):
                return None

        return cls(
            date_obs=_opt("DATE-OBS"),
            date_end=_opt("DATE-END"),
            jd_obs=_opt_f("JD"),
            longobs=_opt_f("LONG-OBS"),
            latobs=_opt_f("LAT-OBS"),
            altobs=_opt_f("ALT-OBS"),
            observat=_opt("OBSERVAT"),
            focallen=_opt_f("FOCALLEN"),
            xpixsz=_opt_f("XPIXSZ"),
            aperture=_opt_f("APERTURE"),
            focal_ratio=_opt_f("FOCAL_RATIO"),
            object_name=kw_map.get("OBJECT", ""),
        )

    def to_keywords(self) -> list[FITSKeyword]:
        result: list[FITSKeyword] = []
        if self.object_name:
            result.append(FITSKeyword("OBJECT", self.object_name))
        for name, value in [
            ("DATE-OBS", self.date_obs), ("DATE-END", self.date_end),
            ("OBSERVAT", self.observat),
        ]:
            if value is not None:
                result.append(FITSKeyword(name, str(value)))
        for name, value in [
            ("JD", self.jd_obs), ("LONG-OBS", self.longobs),
            ("LAT-OBS", self.latobs), ("ALT-OBS", self.altobs),
            ("FOCALLEN", self.focallen), ("XPIXSZ", self.xpixsz),
            ("APERTURE", self.aperture), ("FOCAL_RATIO", self.focal_ratio),
        ]:
            if value is not None:
                result.append(FITSKeyword(name, str(value)))
        return result


@dataclass(slots=True)
class CalibrationMetadata:
    """校准相关元数据。

    Attributes
    ----------
    exptime : float
        曝光时间 (秒, EXPTIME)。
    filter_name : str
        滤镜名称 (FILTER)。
    gain : float
        增益 (e⁻/ADU, GAIN)。
    ccd_temp : float or None
        CCD 温度 (℃, CCD-TEMP 优先于 TEMP)。
    frame_type : str
        帧类型 (IMAGETYP): BIAS/DARK/FLAT/LIGHT。
    bunit : str
        单位 (BUNIT): ADU / e⁻/s。
    """

    exptime: float = 0.0
    filter_name: str = "Unknown"
    gain: float = 1.0
    ccd_temp: Optional[float] = None
    frame_type: str = ""
    bunit: str = "ADU"

    @classmethod
    def from_keywords(cls, keywords: list[FITSKeyword]) -> CalibrationMetadata:
        kw_map: dict[str, str] = {kw.name: kw.value for kw in keywords}

        def _float(name: str, default: float = 0.0) -> float:
            val = kw_map.get(name)
            if val is None:
                return default
            try:
                return float(val)
            except (ValueError, TypeError):
                return default

        def _opt_f(name: str) -> Optional[float]:
            val = kw_map.get(name)
            if val is None:
                return None
            try:
                return float(val)
            except (ValueError, TypeError):
                return None

        ccd_temp = _opt_f("CCD-TEMP")
        if ccd_temp is None:
            ccd_temp = _opt_f("TEMP")

        return cls(
            exptime=_float("EXPTIME"),
            filter_name=kw_map.get("FILTER", "Unknown"),
            gain=_float("GAIN", 1.0),
            ccd_temp=ccd_temp,
            frame_type=kw_map.get("IMAGETYP", "").strip(),
            bunit=kw_map.get("BUNIT", "ADU"),
        )

    def to_keywords(self) -> list[FITSKeyword]:
        result: list[FITSKeyword] = [
            FITSKeyword("EXPTIME", str(self.exptime)),
            FITSKeyword("FILTER", self.filter_name),
            FITSKeyword("GAIN", str(self.gain)),
            FITSKeyword("BUNIT", self.bunit),
        ]
        if self.frame_type:
            result.append(FITSKeyword("IMAGETYP", self.frame_type))
        if self.ccd_temp is not None:
            result.append(FITSKeyword("CCD-TEMP", str(self.ccd_temp)))
        return result
