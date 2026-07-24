from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional

from .image_data import ImageInfo, ImageOptions
from .keywords import FITSKeyword
from .wcs_keywords import WCSKeywords
from .observation_metadata import ObservationMetadata, CalibrationMetadata


@dataclass
class ImageMetadata:
    """图像完整元数据，ImageInfo + ImageOptions + WCS + 观测 + 校准的并集。

    对应 PCL ImageDescription + WCSKeywords + AstrometricMetadata 的合并。

    Attributes
    ----------
    info : ImageInfo
        基本图像信息。
    options : ImageOptions
        样本属性。
    wcs : WCSKeywords or None
        WCS 坐标信息。
    observation : ObservationMetadata or None
        观测元数据。
    calibration : CalibrationMetadata or None
        校准元数据。
    """

    info: ImageInfo = field(default_factory=ImageInfo)
    options: ImageOptions = field(default_factory=ImageOptions)
    wcs: Optional[WCSKeywords] = None
    observation: Optional[ObservationMetadata] = None
    calibration: Optional[CalibrationMetadata] = None

    @classmethod
    def from_keywords(cls, keywords: list[FITSKeyword]) -> ImageMetadata:
        """从 FITSKeyword 列表构建完整元数据。

        Parameters
        ----------
        keywords : list of FITSKeyword

        Returns
        -------
        ImageMetadata
        """
        kw_map: dict[str, str] = {kw.name: kw.value for kw in keywords}

        def _get(name: str, default: str = "") -> str:
            return kw_map.get(name, default)

        def _int(name: str, default: int = 0) -> int:
            val = kw_map.get(name)
            if val is None:
                return default
            try:
                return int(float(val))
            except (ValueError, TypeError):
                return default

        info = ImageInfo(
            width=_int("NAXIS1"),
            height=_int("NAXIS2"),
            channels=_int("NAXIS3", 1),
        )

        from .image_data import ColorSpace
        options = ImageOptions(
            bits_per_sample=abs(int(float(_get("BITPIX", "16")))),
            float_sample=int(float(_get("BITPIX", "16"))) < 0,
        )

        wcs = WCSKeywords.from_keywords(keywords)
        wcs_obj = wcs if wcs.has_wcs else None

        obs = ObservationMetadata.from_keywords(keywords)
        calib = CalibrationMetadata.from_keywords(keywords)

        return cls(
            info=info,
            options=options,
            wcs=wcs_obj,
            observation=obs,
            calibration=calib,
        )

    @classmethod
    def from_properties(cls, properties: dict[str, Any]) -> ImageMetadata:
        """从 XISF 属性字典构建元数据。

        Parameters
        ----------
        properties : dict
            XISF 属性字典。

        Returns
        -------
        ImageMetadata
        """
        return cls()

    def to_keywords(self) -> list[FITSKeyword]:
        """导出为 FITSKeyword 列表。

        Returns
        -------
        list of FITSKeyword
        """
        result: list[FITSKeyword] = [
            FITSKeyword("NAXIS", "2"),
            FITSKeyword("NAXIS1", str(self.info.width)),
            FITSKeyword("NAXIS2", str(self.info.height)),
        ]
        if self.info.channels > 1:
            result.append(FITSKeyword("NAXIS3", str(self.info.channels)))
        result.append(FITSKeyword("BITPIX",
            str(self.options.bits_per_sample if not self.options.float_sample
                else -self.options.bits_per_sample)))

        if self.wcs is not None:
            result.extend(self.wcs.to_keywords())
        if self.observation is not None:
            result.extend(self.observation.to_keywords())
        if self.calibration is not None:
            result.extend(self.calibration.to_keywords())

        return result

    def to_properties(self) -> dict[str, Any]:
        """导出为 XISF 属性字典。

        Returns
        -------
        dict
        """
        return {}
