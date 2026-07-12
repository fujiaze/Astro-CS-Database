"""
天文图像读写模块 (astro_image_io) Python封装
功能: FITS/XISF格式天文图像读取与FITS写入，封装C++ DLL
用途: 天文图像数据统一IO接口，支持FITS(.fits/.fit/.fts)和XISF(.xisf)格式
"""

from __future__ import annotations

import os
from ctypes import (
    Structure, c_int, c_float, c_double, c_char, c_void_p,
    POINTER, byref, cdll,
)
from dataclasses import dataclass, field
from typing import Optional

import numpy as np


AIO_KEYWORD_NAME_MAX = 72
AIO_KEYWORD_VALUE_MAX = 72
AIO_KEYWORD_COMMENT_MAX = 72
AIO_PATH_MAX = 512
AIO_CTYPE_MAX = 32
AIO_RADESYS_MAX = 32
AIO_FILTER_MAX = 64
AIO_FRAME_TYPE_MAX = 32
AIO_BUNIT_MAX = 32
AIO_OBJECT_MAX = 128
AIO_OBSERVAT_MAX = 64
AIO_DATE_MAX = 64


@dataclass
class FITSKeywordPy:
    name: str = ""
    value: str = ""
    comment: str = ""


@dataclass
class ImageGeometryPy:
    width: int = 0
    height: int = 0
    channels: int = 1


@dataclass
class ImageOptionsPy:
    bits_per_sample: int = 16
    float_sample: bool = False


@dataclass
class WCSKeywordsPy:
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
    radesys: str = "ICRS"
    equinox: Optional[float] = 2000.0
    has_wcs: bool = False

    @property
    def pixel_scale(self) -> float:
        if not self.has_wcs:
            return 0.0
        det = self.cd1_1 * self.cd2_2 - self.cd1_2 * self.cd2_1
        if abs(det) < 1e-30:
            return 0.0
        return float(np.sqrt(abs(det)) * 3600.0)

    @property
    def rotation_deg(self) -> float:
        if not self.has_wcs:
            return 0.0
        import math
        scale_x = np.sqrt(self.cd1_1 ** 2 + self.cd2_1 ** 2)
        if scale_x < 1e-15:
            return 0.0
        return float(math.degrees(math.atan2(self.cd2_1, self.cd1_1)))


@dataclass
class ObservationMetadataPy:
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
    object_name: str = ""


@dataclass
class CalibrationMetadataPy:
    exptime: float = 0.0
    filter_name: str = "Unknown"
    gain: float = 1.0
    ccd_temp: Optional[float] = None
    frame_type: str = ""
    bunit: str = "ADU"


@dataclass
class ImageMetadataPy:
    geometry: ImageGeometryPy = field(default_factory=ImageGeometryPy)
    options: ImageOptionsPy = field(default_factory=ImageOptionsPy)
    wcs: Optional[WCSKeywordsPy] = None
    observation: Optional[ObservationMetadataPy] = None
    calibration: Optional[CalibrationMetadataPy] = None


class _CAIOFITSKeyword(Structure):
    _fields_ = [
        ("name", c_char * AIO_KEYWORD_NAME_MAX),
        ("value", c_char * AIO_KEYWORD_VALUE_MAX),
        ("comment", c_char * AIO_KEYWORD_COMMENT_MAX),
    ]


class _CAIOImageGeometry(Structure):
    _fields_ = [
        ("width", c_int),
        ("height", c_int),
        ("channels", c_int),
    ]


class _CAIOImageOptions(Structure):
    _fields_ = [
        ("bits_per_sample", c_int),
        ("float_sample", c_int),
    ]


class _CAIOWCSKeywords(Structure):
    _fields_ = [
        ("crpix1", c_double), ("crpix2", c_double),
        ("crval1", c_double), ("crval2", c_double),
        ("ctype1", c_char * AIO_CTYPE_MAX),
        ("ctype2", c_char * AIO_CTYPE_MAX),
        ("cd1_1", c_double), ("cd1_2", c_double),
        ("cd2_1", c_double), ("cd2_2", c_double),
        ("cdelt1", c_double), ("cdelt2", c_double),
        ("has_cdelt1", c_int), ("has_cdelt2", c_int),
        ("radesys", c_char * AIO_RADESYS_MAX),
        ("equinox", c_double), ("has_equinox", c_int),
        ("lonpole", c_double), ("latpole", c_double),
        ("has_lonpole", c_int), ("has_latpole", c_int),
        ("has_wcs", c_int),
    ]


class _CAIOObservationMetadata(Structure):
    _fields_ = [
        ("date_obs", c_char * AIO_DATE_MAX),
        ("date_end", c_char * AIO_DATE_MAX),
        ("jd_obs", c_double), ("has_jd_obs", c_int),
        ("longobs", c_double), ("latobs", c_double), ("altobs", c_double),
        ("has_longobs", c_int), ("has_latobs", c_int), ("has_altobs", c_int),
        ("observat", c_char * AIO_OBSERVAT_MAX),
        ("focallen", c_double), ("xpixsz", c_double),
        ("aperture", c_double), ("focal_ratio", c_double),
        ("has_focallen", c_int), ("has_xpixsz", c_int),
        ("has_aperture", c_int), ("has_focal_ratio", c_int),
        ("object_name", c_char * AIO_OBJECT_MAX),
    ]


class _CAIOCalibrationMetadata(Structure):
    _fields_ = [
        ("exptime", c_double),
        ("filter_name", c_char * AIO_FILTER_MAX),
        ("gain", c_double),
        ("ccd_temp", c_double), ("has_ccd_temp", c_int),
        ("frame_type", c_char * AIO_FRAME_TYPE_MAX),
        ("bunit", c_char * AIO_BUNIT_MAX),
    ]


class _CAIOImageMetadata(Structure):
    _fields_ = [
        ("geometry", _CAIOImageGeometry),
        ("options", _CAIOImageOptions),
        ("wcs", _CAIOWCSKeywords),
        ("observation", _CAIOObservationMetadata),
        ("calibration", _CAIOCalibrationMetadata),
    ]


def _kw_c_to_py(c: _CAIOFITSKeyword) -> FITSKeywordPy:
    return FITSKeywordPy(
        name=c.name.decode("utf-8", errors="replace").rstrip("\x00"),
        value=c.value.decode("utf-8", errors="replace").rstrip("\x00"),
        comment=c.comment.decode("utf-8", errors="replace").rstrip("\x00"),
    )


def _geo_c_to_py(c: _CAIOImageGeometry) -> ImageGeometryPy:
    return ImageGeometryPy(width=c.width, height=c.height, channels=c.channels)


def _opts_c_to_py(c: _CAIOImageOptions) -> ImageOptionsPy:
    return ImageOptionsPy(bits_per_sample=c.bits_per_sample, float_sample=bool(c.float_sample))


def _wcs_c_to_py(c: _CAIOWCSKeywords) -> WCSKeywordsPy:
    return WCSKeywordsPy(
        crpix1=c.crpix1, crpix2=c.crpix2,
        crval1=c.crval1, crval2=c.crval2,
        ctype1=c.ctype1.decode("utf-8", errors="replace").rstrip("\x00"),
        ctype2=c.ctype2.decode("utf-8", errors="replace").rstrip("\x00"),
        cd1_1=c.cd1_1, cd1_2=c.cd1_2, cd2_1=c.cd2_1, cd2_2=c.cd2_2,
        radesys=c.radesys.decode("utf-8", errors="replace").rstrip("\x00"),
        equinox=c.equinox if c.has_equinox else None,
        has_wcs=bool(c.has_wcs),
    )


def _obs_c_to_py(c: _CAIOObservationMetadata) -> ObservationMetadataPy:
    return ObservationMetadataPy(
        date_obs=c.date_obs.decode("utf-8", errors="replace").rstrip("\x00") or None,
        date_end=c.date_end.decode("utf-8", errors="replace").rstrip("\x00") or None,
        jd_obs=c.jd_obs if c.has_jd_obs else None,
        longobs=c.longobs if c.has_longobs else None,
        latobs=c.latobs if c.has_latobs else None,
        altobs=c.altobs if c.has_altobs else None,
        observat=c.observat.decode("utf-8", errors="replace").rstrip("\x00") or None,
        focallen=c.focallen if c.has_focallen else None,
        xpixsz=c.xpixsz if c.has_xpixsz else None,
        aperture=c.aperture if c.has_aperture else None,
        focal_ratio=c.focal_ratio if c.has_focal_ratio else None,
        object_name=c.object_name.decode("utf-8", errors="replace").rstrip("\x00"),
    )


def _cal_c_to_py(c: _CAIOCalibrationMetadata) -> CalibrationMetadataPy:
    return CalibrationMetadataPy(
        exptime=c.exptime,
        filter_name=c.filter_name.decode("utf-8", errors="replace").rstrip("\x00"),
        gain=c.gain,
        ccd_temp=c.ccd_temp if c.has_ccd_temp else None,
        frame_type=c.frame_type.decode("utf-8", errors="replace").rstrip("\x00"),
        bunit=c.bunit.decode("utf-8", errors="replace").rstrip("\x00"),
    )


def _meta_c_to_py(c: _CAIOImageMetadata) -> ImageMetadataPy:
    wcs = _wcs_c_to_py(c.wcs)
    return ImageMetadataPy(
        geometry=_geo_c_to_py(c.geometry),
        options=_opts_c_to_py(c.options),
        wcs=wcs if wcs.has_wcs else None,
        observation=_obs_c_to_py(c.observation),
        calibration=_cal_c_to_py(c.calibration),
    )


def _load_dll(dll_path: str):
    mingw_bin = r"C:\msys64\mingw64\bin"
    if os.path.isdir(mingw_bin):
        # 避免重复追加PATH导致环境变量超长（Windows 32767字符限制）
        current_path = os.environ.get("PATH", "")
        if mingw_bin not in current_path:
            os.environ["PATH"] = mingw_bin + ";" + current_path
        try:
            os.add_dll_directory(mingw_bin)
        except OSError:
            pass
    dll_dir = os.path.dirname(os.path.abspath(dll_path))
    try:
        os.add_dll_directory(dll_dir)
    except OSError:
        pass
    dll = cdll.LoadLibrary(dll_path)

    dll.aio_read.argtypes = [c_void_p]
    dll.aio_read.restype = c_void_p
    dll.aio_read_fits.argtypes = [c_void_p]
    dll.aio_read_fits.restype = c_void_p
    dll.aio_read_xisf.argtypes = [c_void_p]
    dll.aio_read_xisf.restype = c_void_p
    dll.aio_read_header_only.argtypes = [c_void_p]
    dll.aio_read_header_only.restype = c_void_p
    dll.aio_read_metadata.argtypes = [c_void_p]
    dll.aio_read_metadata.restype = _CAIOImageMetadata

    dll.aio_write_fits.argtypes = [c_void_p, c_void_p]
    dll.aio_write_fits.restype = c_int

    dll.aio_get_pixel_data.argtypes = [c_void_p]
    dll.aio_get_pixel_data.restype = POINTER(c_float)
    dll.aio_get_width.argtypes = [c_void_p]
    dll.aio_get_width.restype = c_int
    dll.aio_get_height.argtypes = [c_void_p]
    dll.aio_get_height.restype = c_int
    dll.aio_get_channels.argtypes = [c_void_p]
    dll.aio_get_channels.restype = c_int
    dll.aio_get_geometry.argtypes = [c_void_p]
    dll.aio_get_geometry.restype = _CAIOImageGeometry
    dll.aio_get_options.argtypes = [c_void_p]
    dll.aio_get_options.restype = _CAIOImageOptions
    dll.aio_get_metadata.argtypes = [c_void_p]
    dll.aio_get_metadata.restype = _CAIOImageMetadata

    dll.aio_get_keyword_count.argtypes = [c_void_p]
    dll.aio_get_keyword_count.restype = c_int
    dll.aio_get_keyword.argtypes = [c_void_p, c_int]
    dll.aio_get_keyword.restype = _CAIOFITSKeyword
    dll.aio_get_source_format.argtypes = [c_void_p]
    dll.aio_get_source_format.restype = c_void_p
    dll.aio_get_source_path.argtypes = [c_void_p]
    dll.aio_get_source_path.restype = c_void_p

    dll.aio_wcs_pixel_scale.argtypes = [POINTER(_CAIOWCSKeywords)]
    dll.aio_wcs_pixel_scale.restype = c_double
    dll.aio_wcs_rotation_deg.argtypes = [POINTER(_CAIOWCSKeywords)]
    dll.aio_wcs_rotation_deg.restype = c_double

    dll.aio_free_image_data.argtypes = [c_void_p]
    dll.aio_free_image_data.restype = None

    dll.aio_is_fits.argtypes = [c_void_p]
    dll.aio_is_fits.restype = c_int
    dll.aio_is_xisf.argtypes = [c_void_p]
    dll.aio_is_xisf.restype = c_int

    return dll


class ImageData:
    """统一图像数据容器，封装C++ DLL的AIOImageData。"""

    def __init__(self, handle, dll):
        self._handle = handle
        self._dll = dll
        self._closed = False
        self._data_cache = None
        self._keywords_cache = None
        self._metadata_cache = None

    @property
    def width(self) -> int:
        return self._dll.aio_get_width(self._handle)

    @property
    def height(self) -> int:
        return self._dll.aio_get_height(self._handle)

    @property
    def channels(self) -> int:
        return self._dll.aio_get_channels(self._handle)

    @property
    def shape(self) -> tuple:
        return (self.height, self.width)

    @property
    def data(self) -> np.ndarray:
        if self._data_cache is None:
            ptr = self._dll.aio_get_pixel_data(self._handle)
            w = self.width
            h = self.height
            if ptr and w > 0 and h > 0:
                self._data_cache = np.ctypeslib.as_array(ptr, shape=(h * w,)).copy().reshape(h, w)
            else:
                self._data_cache = np.zeros((h, w), dtype=np.float32)
        return self._data_cache

    @property
    def keywords(self) -> list[FITSKeywordPy]:
        if self._keywords_cache is None:
            count = self._dll.aio_get_keyword_count(self._handle)
            kws = []
            for i in range(count):
                c_kw = self._dll.aio_get_keyword(self._handle, i)
                kws.append(_kw_c_to_py(c_kw))
            self._keywords_cache = kws
        return self._keywords_cache

    @property
    def metadata(self) -> ImageMetadataPy:
        if self._metadata_cache is None:
            c_meta = self._dll.aio_get_metadata(self._handle)
            self._metadata_cache = _meta_c_to_py(c_meta)
        return self._metadata_cache

    @property
    def wcs(self) -> Optional[WCSKeywordsPy]:
        return self.metadata.wcs

    @property
    def has_wcs(self) -> bool:
        meta = self.metadata
        return meta.wcs is not None and meta.wcs.has_wcs

    @property
    def pixel_scale_arcsec(self) -> float:
        if self.has_wcs and self.wcs:
            return self.wcs.pixel_scale
        return 0.0

    @property
    def source_format(self) -> str:
        ptr = self._dll.aio_get_source_format(self._handle)
        if ptr:
            from ctypes import string_at
            return string_at(ptr).decode("utf-8", errors="replace")
        return ""

    @property
    def source_path(self) -> str:
        ptr = self._dll.aio_get_source_path(self._handle)
        if ptr:
            from ctypes import string_at
            return string_at(ptr).decode("utf-8", errors="replace")
        return ""

    def get_keyword(self, name: str, default: Optional[str] = None) -> Optional[str]:
        for kw in self.keywords:
            if kw.name.upper() == name.upper():
                return kw.value
        return default

    def get_keyword_float(self, name: str, default: float = 0.0) -> float:
        val = self.get_keyword(name)
        if val is None:
            return default
        try:
            return float(val)
        except (ValueError, TypeError):
            return default

    def get_keyword_int(self, name: str, default: int = 0) -> int:
        val = self.get_keyword(name)
        if val is None:
            return default
        try:
            return int(float(val))
        except (ValueError, TypeError):
            return default

    def to_numpy(self) -> np.ndarray:
        return self.data.astype(np.float32, copy=False)

    def to_numpy_f64(self) -> np.ndarray:
        return self.data.astype(np.float64, copy=False)

    def close(self):
        if not self._closed and self._handle:
            self._dll.aio_free_image_data(self._handle)
            self._handle = None
            self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class ImageReader:
    """天文图像读取器，自动检测FITS/XISF格式。"""

    def __init__(self, dll_path: Optional[str] = None):
        if dll_path is None:
            base = os.path.dirname(os.path.abspath(__file__))
            dll_path = os.path.normpath(os.path.join(base, "..", "astro_image_io.dll"))
        self._dll = _load_dll(dll_path)

    def read(self, path: str) -> ImageData:
        path_bytes = path.encode("utf-8")
        handle = self._dll.aio_read(path_bytes)
        if not handle:
            raise RuntimeError(f"读取失败: {path}")
        return ImageData(handle, self._dll)

    def read_fits(self, path: str) -> ImageData:
        path_bytes = path.encode("utf-8")
        handle = self._dll.aio_read_fits(path_bytes)
        if not handle:
            raise RuntimeError(f"FITS读取失败: {path}")
        return ImageData(handle, self._dll)

    def read_xisf(self, path: str) -> ImageData:
        path_bytes = path.encode("utf-8")
        handle = self._dll.aio_read_xisf(path_bytes)
        if not handle:
            raise RuntimeError(f"XISF读取失败: {path}")
        return ImageData(handle, self._dll)

    def read_header_only(self, path: str) -> ImageData:
        path_bytes = path.encode("utf-8")
        handle = self._dll.aio_read_header_only(path_bytes)
        if not handle:
            raise RuntimeError(f"Header读取失败: {path}")
        return ImageData(handle, self._dll)

    def read_metadata(self, path: str) -> ImageMetadataPy:
        path_bytes = path.encode("utf-8")
        c_meta = self._dll.aio_read_metadata(path_bytes)
        return _meta_c_to_py(c_meta)

    def is_fits(self, path: str) -> bool:
        return bool(self._dll.aio_is_fits(path.encode("utf-8")))

    def is_xisf(self, path: str) -> bool:
        return bool(self._dll.aio_is_xisf(path.encode("utf-8")))


class FITSWriter:
    """FITS格式写入器。"""

    def __init__(self, dll_path: Optional[str] = None):
        if dll_path is None:
            base = os.path.dirname(os.path.abspath(__file__))
            dll_path = os.path.normpath(os.path.join(base, "..", "astro_image_io.dll"))
        self._dll = _load_dll(dll_path)

    def write(self, image_data: np.ndarray, path: str,
              keywords: Optional[list[FITSKeywordPy]] = None,
              float_sample: bool = True) -> str:
        img = np.ascontiguousarray(image_data, dtype=np.float32)
        if img.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={img.ndim}")
        h, w = img.shape

        from ctypes import create_string_buffer
        c_keywords = None
        c_kw_count = 0
        if keywords:
            c_kw_count = len(keywords)
            c_keywords = (_CAIOFITSKeyword * c_kw_count)()
            for i, kw in enumerate(keywords):
                c_keywords[i].name = kw.name.encode("utf-8")[:AIO_KEYWORD_NAME_MAX - 1]
                c_keywords[i].value = kw.value.encode("utf-8")[:AIO_KEYWORD_VALUE_MAX - 1]
                c_keywords[i].comment = kw.comment.encode("utf-8")[:AIO_KEYWORD_COMMENT_MAX - 1]

        class _TempImage(Structure):
            _fields_ = [
                ("data", POINTER(c_float)),
                ("width", c_int),
                ("height", c_int),
                ("channels", c_int),
                ("bits_per_sample", c_int),
                ("float_sample", c_int),
                ("source_format", c_char * 16),
                ("source_path", c_char * AIO_PATH_MAX),
                ("keywords", POINTER(_CAIOFITSKeyword)),
                ("keyword_count", c_int),
                ("metadata", _CAIOImageMetadata),
            ]

        temp = _TempImage()
        temp.data = img.ctypes.data_as(POINTER(c_float))
        temp.width = w
        temp.height = h
        temp.channels = 1
        temp.bits_per_sample = 32 if float_sample else 16
        temp.float_sample = 1 if float_sample else 0
        temp.source_format = b"fits"
        temp.source_path = b""
        temp.keywords = c_keywords
        temp.keyword_count = c_kw_count

        path_bytes = path.encode("utf-8")
        ret = self._dll.aio_write_fits(byref(temp), path_bytes)
        if ret != 0:
            raise RuntimeError(f"FITS写入失败: {path}")
        return path
