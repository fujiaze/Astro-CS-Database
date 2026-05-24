"""XISF 1.0 读取器 — 纯 Python 实现。

基于 XISF 1.0 规范：XISF0100 头 → XML 元数据 → attachment 像素数据。
支持 Float32 / UInt16 像素格式。

从 XML 提取:
  - Image geometry (width:height:channels)
  - sampleFormat (Float32 / UInt16)
  - FITSKeyword 元素 (name/value/comment)
  - location (attachment:offset:size)
"""

from __future__ import annotations

import struct
import os
import re
import logging
from typing import Optional, Any

import numpy as np

from ._types import PathLike
from .image_data import ImageData, ImageGeometry, ImageColor, ImageOptions, ImageInfo, ColorSpace
from .keywords import FITSKeyword
from .image_metadata import ImageMetadata
from .wcs_keywords import WCSKeywords
from .reader import ImageReader

logger = logging.getLogger(__name__)

_MAGIC = b"XISF0100"


def _unpack_sample_format(fmt: str) -> tuple[np.dtype, int]:
    """将 XISF sampleFormat 映射到 numpy dtype。

    Parameters
    ----------
    fmt : str
        XISF sampleFormat 值。

    Returns
    -------
    tuple[np.dtype, int]
        (numpy dtype, bytes_per_element)
    """
    mapping = {
        "Float32": (np.dtype(np.float32), 4),
        "Float64": (np.dtype(np.float64), 8),
        "UInt8":   (np.dtype(np.uint8),   1),
        "UInt16":  (np.dtype(np.uint16),  2),
        "UInt32":  (np.dtype(np.uint32),  4),
        "UInt64":  (np.dtype(np.uint64),  8),
        "Int8":    (np.dtype(np.int8),    1),
        "Int16":   (np.dtype(np.int16),   2),
        "Int32":   (np.dtype(np.int32),   4),
        "Int64":   (np.dtype(np.int64),   8),
    }
    if fmt in mapping:
        return mapping[fmt]
    logger.warning(f"未知 sampleFormat '{fmt}'，回退到 Float32")
    return (np.dtype(np.float32), 4)


class XISFReaderPy(ImageReader):
    """纯 Python XISF 1.0 单体容器读取器。

    解析流程:
        1. 验证 XISF0100 魔数
        2. 读取 XML header (UTF-8)
        3. 解析 Image 元素 (geometry, sampleFormat, location)
        4. 解析 FITSKeyword 子元素
        5. 从 attachment 偏移量读取原始像素数据
        6. 构建 ImageData 统一容器
    """

    def __init__(self) -> None:
        pass

    # ── 内部解析 ──

    @staticmethod
    def _parse_geometry(geo_str: str) -> tuple[int, int, int]:
        """解析 geometry="4096:4096:1"。"""
        parts = geo_str.strip().split(":")
        w = int(parts[0]) if len(parts) > 0 else 0
        h = int(parts[1]) if len(parts) > 1 else 0
        c = int(parts[2]) if len(parts) > 2 else 1
        return (w, h, c)

    @staticmethod
    def _parse_location(loc_str: str) -> tuple[int, int]:
        """解析 location="attachment:24576:67108864"。

        Returns
        -------
        tuple[int, int]
            (offset, size_bytes)
        """
        parts = loc_str.strip().split(":")
        if len(parts) >= 3 and parts[0] == "attachment":
            return (int(parts[1]), int(parts[2]))
        return (0, 0)

    @staticmethod
    def _normalize_fits_value(value: str) -> str:
        """去除非必须的 FITS 字符串引号。

        XISF 保留 FITS 原始引号表示 ('value')，FITS reader 会自动剥离。
        统一处理以确保读取结果一致。
        """
        v = value
        while len(v) >= 2 and v.startswith("'") and v.endswith("'"):
            inner = v[1:-1]
            if "'" not in inner.replace("''", ""):
                v = inner
            else:
                break
        v = v.replace("''", "'")
        return v

    @staticmethod
    def _parse_fits_keywords(xml_content: str) -> list[FITSKeyword]:
        """从 XML 中提取所有 <FITSKeyword> 元素。"""
        keywords: list[FITSKeyword] = []

        pattern = re.compile(
            r'<FITSKeyword\s+name="([^"]*)"\s+value="([^"]*)"(?:\s+comment="([^"]*)")?\s*/>',
            re.DOTALL,
        )
        for match in pattern.finditer(xml_content):
            raw_value = match.group(2)
            keywords.append(FITSKeyword(
                name=match.group(1),
                value=XISFReaderPy._normalize_fits_value(raw_value),
                comment=match.group(3) or "",
            ))

        return keywords

    @staticmethod
    def _parse_image_elements(xml_content: str) -> list[dict[str, str]]:
        """解析 <Image> 元素，返回属性字典列表。"""
        images: list[dict[str, str]] = []
        # 用非贪婪匹配获取每个 Image 元素
        pattern = re.compile(
            r'<Image\s+([^>]+?)(?:>\s*(.*?)\s*</Image>|/>)',
            re.DOTALL,
        )

        for match in pattern.finditer(xml_content):
            attrs_str = match.group(1)
            inner = match.group(2) or ""

            attrs: dict[str, str] = {}
            attr_pattern = re.compile(r'(\w+)="([^"]*)"')
            for am in attr_pattern.finditer(attrs_str):
                attrs[am.group(1)] = am.group(2)

            attrs["_inner_xml"] = inner
            images.append(attrs)

        return images

    # ── 公开接口 ──

    def read(self, path: PathLike) -> ImageData:
        """读取 XISF 文件为 ImageData。

        XISF 1.0 单体容器结构:
            [0:8]   Magic "XISF0100"
            [8:16]  XML header 长度 (uint64 LE)
            [16:]   XML header (UTF-8, 长度=上述值)
            [...]   零填充至 alignment
            [...]   Attachment 像素数据块

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageData
        """
        path_str = str(path)

        with open(path_str, "rb") as f:
            file_bytes = f.read()

        if file_bytes[:8] != _MAGIC:
            raise ValueError(f"不是有效的 XISF 文件: {path_str}")

        xml_length = int.from_bytes(file_bytes[8:16], "little")
        xml_text = file_bytes[16 : 16 + xml_length].decode("utf-8")

        images = self._parse_image_elements(xml_text)
        if not images:
            raise ValueError(f"XISF 文件中没有 Image 元素: {path_str}")

        first = images[0]
        w, h, channels = self._parse_geometry(
            first.get("geometry", "0:0:1")
        )
        sample_fmt = first.get("sampleFormat", "Float32")
        location = first.get("location", "attachment:0:0")

        dtype, _elem_size = _unpack_sample_format(sample_fmt)
        offset, size_bytes = self._parse_location(location)

        if size_bytes <= 0 or offset <= 0:
            raise ValueError(f"无效的 data location: {location}")

        raw_data = file_bytes[offset : offset + size_bytes]
        expected = channels * h * w * dtype.itemsize
        if len(raw_data) < expected:
            raise ValueError(
                f"像素数据不足: 需要 {expected} 字节, 实际 {len(raw_data)}"
            )

        pixel_data = np.frombuffer(raw_data[:expected], dtype=dtype).reshape(
            (h, w) if channels == 1 else (channels, h, w)
        ).astype(np.float32, copy=False)

        geometry = ImageGeometry(width=w, height=h, channels=channels)
        color = ImageColor(color_space=ColorSpace.GRAY)
        options = ImageOptions(
            bits_per_sample=32 if "32" in sample_fmt else 16,
            float_sample="Float" in sample_fmt,
        )

        inner_xml = first.get("_inner_xml", "") + xml_text
        keywords = self._parse_fits_keywords(inner_xml)
        metadata = ImageMetadata.from_keywords(keywords)

        return ImageData(
            data=pixel_data,
            geometry=geometry,
            color=color,
            options=options,
            keywords=keywords,
            metadata=metadata,
            properties={},
            source_format="xisf",
            source_path=path_str,
        )

    def read_info(self, path: PathLike) -> ImageInfo:
        """仅读取图像信息快照。

        Parameters
        ----------
        path : PathLike

        Returns
        -------
        ImageInfo
        """
        path_str = str(path)
        with open(path_str, "rb") as f:
            file_bytes = f.read()

        if file_bytes[:8] != _MAGIC:
            raise ValueError(f"不是有效的 XISF 文件: {path_str}")

        xml_length = int.from_bytes(file_bytes[8:16], "little")
        xml_text = file_bytes[16 : 16 + xml_length].decode("utf-8")
        images = self._parse_image_elements(xml_text)

        if not images:
            return ImageInfo()

        first = images[0]
        w, h, channels = self._parse_geometry(
            first.get("geometry", "0:0:1")
        )
        return ImageInfo(
            width=w, height=h, channels=channels,
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
        with open(path_str, "rb") as f:
            file_bytes = f.read()

        if file_bytes[:8] != _MAGIC:
            raise ValueError(f"不是有效的 XISF 文件: {path_str}")

        xml_length = int.from_bytes(file_bytes[8:16], "little")
        xml_text = file_bytes[16 : 16 + xml_length].decode("utf-8")
        images = self._parse_image_elements(xml_text)

        if not images:
            return []

        inner = images[0].get("_inner_xml", "") + xml_text
        return self._parse_fits_keywords(inner)

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

    def read_wcs(self, path: PathLike):
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
        """仅读取元数据，像素为零填充。

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
            source_format="xisf",
            source_path=str(path),
        )
