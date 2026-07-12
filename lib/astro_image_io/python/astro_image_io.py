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


# ============================================================================
# ahpx 模块 (合并自 ahpx_io.py)
# 通过 astro_image_io.dll 导出的 aio_ahpx_* API 读写 .ahpx 格式
# .ahpx: 单帧图像(像素+SNR+权重+元数据)的自定义二进制存储格式
# ============================================================================

from ctypes import c_char_p, create_string_buffer, pointer

# 权重模式常量 (对应 aio::ahpx::WeightMode)
AHPX_WEIGHT_SCALAR = 0   # 整图统一权重 (标量)
AHPX_WEIGHT_GRID = 1     # 分块网格权重 (gw×gh)
AHPX_WEIGHT_PIXEL = 2    # 逐像素权重 (W×H)

# header JSON 缓冲区容量 (字节)
_AHPX_HEADER_JSON_CAPACITY = 65536

# .ahpx 文件 Magic
_AHPX_MAGIC = b"AHPX"


def _find_ahpx_dll() -> str:
    """查找 astro_image_io.dll (ahpx API 已合并到其中)

    查找顺序: 同目录 → 上级目录 (lib/astro_image_io/)
    """
    dll_name = "astro_image_io.dll"
    base = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base, dll_name),                           # 同目录
        os.path.normpath(os.path.join(base, "..", dll_name)),   # 上级目录
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return os.path.normpath(os.path.join(base, "..", dll_name))


def _load_ahpx_dll(dll_path: str):
    """加载 astro_image_io.dll 并配置 aio_ahpx_* 函数签名 (复用现有 _load_dll)"""
    dll = _load_dll(dll_path)

    # aio_ahpx_write(path, pixels, w, h, c, snr, snr_w, snr_h,
    #                weight_mode, weight_data, grid_w, grid_h,
    #                metadata_json, zstd_level) -> int
    dll.aio_ahpx_write.argtypes = [
        c_char_p,                                   # path
        POINTER(c_float), c_int, c_int, c_int,      # pixels, width, height, channels
        POINTER(c_float), c_int, c_int,             # snr, snr_w, snr_h
        c_int, POINTER(c_float),                    # weight_mode, weight_data
        c_int, c_int,                               # grid_w, grid_h
        c_char_p,                                   # metadata_json
        c_int,                                      # zstd_level
    ]
    dll.aio_ahpx_write.restype = c_int

    # aio_ahpx_read_header(path, metadata_json, capacity) -> int
    dll.aio_ahpx_read_header.argtypes = [c_char_p, c_char_p, c_int]
    dll.aio_ahpx_read_header.restype = c_int

    # aio_ahpx_read_pixels(path, pixels, capacity, &w, &h, &c) -> int
    dll.aio_ahpx_read_pixels.argtypes = [
        c_char_p, POINTER(c_float), c_int,
        POINTER(c_int), POINTER(c_int), POINTER(c_int),
    ]
    dll.aio_ahpx_read_pixels.restype = c_int

    # aio_ahpx_read_snr(path, snr, capacity, &w, &h) -> int
    dll.aio_ahpx_read_snr.argtypes = [
        c_char_p, POINTER(c_float), c_int,
        POINTER(c_int), POINTER(c_int),
    ]
    dll.aio_ahpx_read_snr.restype = c_int

    return dll


class AhpxReader:
    """.ahpx 文件读取器, 封装 astro_image_io.dll 的 aio_ahpx_read_* API

    新 C API 是一次性调用 (非对象式), 每次 read_* 都会重新打开文件。
    __init__ 中调用 aio_ahpx_read_header 获取并缓存元数据 JSON。
    """

    def __init__(self, path: str, dll_path: Optional[str] = None):
        self._path = path
        self._closed = False
        self._header_json_cache: Optional[str] = None
        self._image_info_cache: Optional[tuple] = None
        if dll_path is None:
            dll_path = _find_ahpx_dll()
        self._dll = _load_ahpx_dll(dll_path)
        # __init__ 中调用 aio_ahpx_read_header 获取元数据
        buf = create_string_buffer(_AHPX_HEADER_JSON_CAPACITY)
        path_bytes = path.encode("utf-8")
        ret = self._dll.aio_ahpx_read_header(path_bytes, buf, _AHPX_HEADER_JSON_CAPACITY)
        if ret != 0:
            raise RuntimeError(f"读取 .ahpx header 失败 (code={ret}): {path}")
        self._header_json_cache = buf.value.decode("utf-8", errors="replace")

    @property
    def header_json(self) -> str:
        """元数据 JSON 字符串 (已解压)"""
        return self._header_json_cache

    @property
    def image_info(self) -> tuple:
        """(width, height, channels) - 从 header JSON 的 image 对象解析"""
        if self._image_info_cache is None:
            import json
            try:
                meta = json.loads(self._header_json_cache) if self._header_json_cache else {}
            except (json.JSONDecodeError, ValueError):
                meta = {}
            img = meta.get("image", {}) if isinstance(meta, dict) else {}
            w = int(img.get("width", 0))
            h = int(img.get("height", 0))
            c = int(img.get("channels", 1))
            self._image_info_cache = (w, h, c)
        return self._image_info_cache

    @property
    def width(self) -> int:
        return self.image_info[0]

    @property
    def height(self) -> int:
        return self.image_info[1]

    @property
    def channels(self) -> int:
        return self.image_info[2]

    def read_pixels(self) -> np.ndarray:
        """读取像素数据, 返回 float32 numpy array (H, W, C)"""
        w, h, c = self.image_info
        if w <= 0 or h <= 0 or c <= 0:
            raise RuntimeError(f"无效图像几何 (w={w} h={h} c={c})")
        n = h * w * c
        buf = (c_float * n)()
        out_w, out_h, out_c = c_int(0), c_int(0), c_int(0)
        ret = self._dll.aio_ahpx_read_pixels(
            self._path.encode("utf-8"),
            buf, n,
            byref(out_w), byref(out_h), byref(out_c),
        )
        if ret != 0:
            raise RuntimeError(f"读取像素数据失败 (code={ret})")
        arr = np.frombuffer(buf, dtype=np.float32, count=n).copy()
        return arr.reshape(h, w, c)

    def read_snr(self) -> np.ndarray:
        """读取 SNR 图, 返回 float32 numpy array (H, W)

        SNR 与图像同尺寸, 从 image_info 推断缓冲区大小。
        """
        w, h, c = self.image_info
        if w <= 0 or h <= 0:
            raise RuntimeError(f"无效图像几何 (w={w} h={h})")
        n = h * w
        buf = (c_float * n)()
        out_w, out_h = c_int(0), c_int(0)
        ret = self._dll.aio_ahpx_read_snr(
            self._path.encode("utf-8"),
            buf, n,
            byref(out_w), byref(out_h),
        )
        if ret != 0:
            raise RuntimeError(f"读取 SNR 数据失败 (code={ret}, 文件可能不包含 SNR)")
        arr = np.frombuffer(buf, dtype=np.float32, count=n).copy()
        return arr.reshape(h, w)

    def close(self) -> None:
        # 新 API 是一次性的, 无需关闭 handle; 仅清理缓存
        self._header_json_cache = None
        self._image_info_cache = None
        self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class AhpxWriter:
    """.ahpx 文件写入器, 封装 astro_image_io.dll 的 aio_ahpx_write API

    新 C API 是一次性写入 (非对象式), Python 端缓存所有数据,
    write() 时一次性调用 aio_ahpx_write。
    """

    def __init__(self, dll_path: Optional[str] = None):
        self._closed = False
        self._metadata_json: Optional[str] = None
        self._pixels: Optional[np.ndarray] = None
        self._pixel_w = 0
        self._pixel_h = 0
        self._pixel_c = 1
        self._snr: Optional[np.ndarray] = None
        self._snr_w = 0
        self._snr_h = 0
        self._weight_mode = AHPX_WEIGHT_SCALAR
        self._weight_scalar = 1.0
        self._weight_grid: Optional[np.ndarray] = None
        self._weight_grid_w = 0
        self._weight_grid_h = 0
        self._weight_pixel: Optional[np.ndarray] = None
        if dll_path is None:
            dll_path = _find_ahpx_dll()
        self._dll = _load_ahpx_dll(dll_path)

    def set_metadata(self, json_str: str) -> None:
        """设置元数据 JSON 字符串"""
        self._metadata_json = json_str

    def set_pixels(self, pixels: np.ndarray, width: int, height: int, channels: int) -> None:
        """设置像素数据 (float32, W×H×C)"""
        arr = np.ascontiguousarray(pixels, dtype=np.float32)
        expected = height * width * channels
        if arr.size != expected:
            raise ValueError(f"像素数据大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._pixels = arr  # 持有引用防止 GC (write 时取指针)
        self._pixel_w = width
        self._pixel_h = height
        self._pixel_c = channels

    def set_snr(self, snr: np.ndarray, width: int, height: int) -> None:
        """设置 SNR 图 (float32, W×H)"""
        arr = np.ascontiguousarray(snr, dtype=np.float32)
        expected = height * width
        if arr.size != expected:
            raise ValueError(f"SNR 数据大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._snr = arr
        self._snr_w = width
        self._snr_h = height

    def set_weight_scalar(self, scalar: float) -> None:
        """设置标量权重 (整图统一)"""
        self._weight_mode = AHPX_WEIGHT_SCALAR
        self._weight_scalar = float(scalar)
        self._weight_grid = None
        self._weight_pixel = None

    def set_weight_grid(self, grid: np.ndarray, gw: int, gh: int) -> None:
        """设置网格权重 (float32, gw×gh)"""
        arr = np.ascontiguousarray(grid, dtype=np.float32)
        expected = gw * gh
        if arr.size != expected:
            raise ValueError(f"权重网格大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._weight_mode = AHPX_WEIGHT_GRID
        self._weight_grid = arr
        self._weight_grid_w = gw
        self._weight_grid_h = gh
        self._weight_pixel = None

    def set_weight_pixel(self, data: np.ndarray, width: int, height: int) -> None:
        """设置逐像素权重 (float32, W×H)"""
        arr = np.ascontiguousarray(data, dtype=np.float32)
        expected = height * width
        if arr.size != expected:
            raise ValueError(f"权重像素数据大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._weight_mode = AHPX_WEIGHT_PIXEL
        self._weight_pixel = arr
        self._weight_grid = None

    def write(self, path: str, zstd_level: int = 5) -> str:
        """写入 .ahpx 文件 (一次性)

        zstd_level: 0=不压缩, 1-22 压缩级别 (推荐 5)
        """
        if self._pixels is None:
            raise RuntimeError("未设置像素数据, 请先调用 set_pixels()")

        path_bytes = path.encode("utf-8")
        pixels_ptr = self._pixels.ctypes.data_as(POINTER(c_float))

        # SNR (可为 None)
        if self._snr is not None:
            snr_ptr = self._snr.ctypes.data_as(POINTER(c_float))
            snr_w = self._snr_w
            snr_h = self._snr_h
        else:
            snr_ptr = None
            snr_w = 0
            snr_h = 0

        # 权重数据指针
        grid_w = 0
        grid_h = 0
        if self._weight_mode == AHPX_WEIGHT_SCALAR:
            scalar_buf = c_float(self._weight_scalar)
            weight_ptr = pointer(scalar_buf)
        elif self._weight_mode == AHPX_WEIGHT_GRID:
            weight_ptr = self._weight_grid.ctypes.data_as(POINTER(c_float))
            grid_w = self._weight_grid_w
            grid_h = self._weight_grid_h
        elif self._weight_mode == AHPX_WEIGHT_PIXEL:
            weight_ptr = self._weight_pixel.ctypes.data_as(POINTER(c_float))
        else:
            raise RuntimeError(f"未知权重模式: {self._weight_mode}")

        # 元数据 JSON (可为 None)
        meta_bytes = self._metadata_json.encode("utf-8") if self._metadata_json else None

        ret = self._dll.aio_ahpx_write(
            path_bytes,
            pixels_ptr, self._pixel_w, self._pixel_h, self._pixel_c,
            snr_ptr, snr_w, snr_h,
            self._weight_mode, weight_ptr,
            grid_w, grid_h,
            meta_bytes,
            zstd_level,
        )
        if ret != 0:
            raise RuntimeError(f"写入 .ahpx 文件失败 (code={ret}): {path}")
        return path

    def close(self) -> None:
        self._pixels = None
        self._snr = None
        self._weight_grid = None
        self._weight_pixel = None
        self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def is_ahpx(path: str, dll_path: Optional[str] = None) -> bool:
    """检查文件是否为 .ahpx 格式 (检查文件头 Magic 'AHPX')

    直接读取文件头前 4 字节, 不依赖 DLL。
    保留 dll_path 参数仅为 API 兼容, 实际不使用。
    """
    try:
        with open(path, "rb") as f:
            magic = f.read(4)
        return magic == _AHPX_MAGIC
    except (OSError, IOError):
        return False


# ============================================================================
# Pipeline 管线引擎模块
# 封装 astro_image_io.dll 的 aio_pipeline_engine_* / aio_pipeline_frame_* API
# ============================================================================

from ctypes import (
    CFUNCTYPE, c_int64, c_size_t, cast as c_cast,
)

# 阶段枚举常量 (对应 PipelineStage)
STAGE_CALIBRATE = 0
STAGE_PLATESOLVE = 1
STAGE_PHOTOMETRIC = 2
STAGE_DRIZZLE = 3
STAGE_STACK = 4

# 调试导出阶段位掩码
DEBUG_AFTER_CALIBRATE = 1 << STAGE_CALIBRATE
DEBUG_AFTER_PLATESOLVE = 1 << STAGE_PLATESOLVE
DEBUG_AFTER_PHOTOMETRIC = 1 << STAGE_PHOTOMETRIC
DEBUG_AFTER_DRIZZLE = 1 << STAGE_DRIZZLE
DEBUG_AFTER_STACK = 1 << STAGE_STACK
DEBUG_AFTER_ALL = -1


# 块类型常量 (对应 AioBlockType 枚举)
AIO_BLOCK_FLOAT32 = 0
AIO_BLOCK_FLOAT64 = 1
AIO_BLOCK_INT32   = 2
AIO_BLOCK_INT64   = 3
AIO_BLOCK_STRING  = 4
AIO_BLOCK_KV      = 5
AIO_BLOCK_RAW     = 6

# numpy dtype -> AioBlockType 映射
_NUMPY_TO_BLOCK_TYPE = {
    np.dtype(np.float32): AIO_BLOCK_FLOAT32,
    np.dtype(np.float64): AIO_BLOCK_FLOAT64,
    np.dtype(np.int32):   AIO_BLOCK_INT32,
    np.dtype(np.int64):   AIO_BLOCK_INT64,
    np.dtype(np.uint8):   AIO_BLOCK_RAW,
}

# AioBlockType -> numpy dtype 映射
_BLOCK_TYPE_TO_NUMPY = {
    AIO_BLOCK_FLOAT32: np.float32,
    AIO_BLOCK_FLOAT64: np.float64,
    AIO_BLOCK_INT32:   np.int32,
    AIO_BLOCK_INT64:   np.int64,
    AIO_BLOCK_RAW:     np.uint8,
}


class _CAioKVEntry(Structure):
    """AioKVEntry C 结构体 (对应 aio_pipeline.h)"""
    _fields_ = [
        ("key", c_char * 64),
        ("value", c_char * 256),
    ]


class _CAioBlock(Structure):
    """AioBlock C 结构体 (对应 aio_pipeline.h)"""
    _fields_ = [
        ("name", c_char * 64),
        ("type", c_int),
        ("data", c_void_p),
        ("count", c_int64),
        ("dims", c_int * 4),
        ("n_dims", c_int),
        ("description", c_char * 128),
    ]


class _CPipelineFrame(Structure):
    """PipelineFrame C 结构体 (命名块容器, 对应 aio_pipeline.h)"""
    _fields_ = [
        ("blocks", POINTER(_CAioBlock)),
        ("n_blocks", c_int),
        ("blocks_capacity", c_int),
        ("stages_completed", c_int),
    ]


# 阶段处理函数类型: int (*)(PipelineFrame* frame, const void* params, char* error_msg, int error_capacity)
PipelineStageHandlerC = CFUNCTYPE(
    c_int,                  # 返回值
    POINTER(_CPipelineFrame),  # frame
    c_void_p,               # params
    c_char_p,               # error_msg
    c_int,                  # error_capacity
)


def _load_pipeline_dll(dll_path: str):
    """加载 astro_image_io.dll 并配置 pipeline 相关函数签名 (命名块容器版)"""
    dll = _load_dll(dll_path)

    # 帧生命周期
    dll.aio_pipeline_frame_create.argtypes = []
    dll.aio_pipeline_frame_create.restype = POINTER(_CPipelineFrame)

    dll.aio_pipeline_frame_destroy.argtypes = [POINTER(_CPipelineFrame)]
    dll.aio_pipeline_frame_destroy.restype = None

    dll.aio_pipeline_frame_memory_usage.argtypes = [POINTER(_CPipelineFrame)]
    dll.aio_pipeline_frame_memory_usage.restype = c_size_t

    # 块管理 API
    dll.aio_frame_add_block.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_int,
        c_void_p, c_int64,
        POINTER(c_int), c_int,
        c_char_p,
    ]
    dll.aio_frame_add_block.restype = c_int

    dll.aio_frame_add_block_move.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_int,
        c_void_p, c_int64,
        POINTER(c_int), c_int,
        c_char_p,
    ]
    dll.aio_frame_add_block_move.restype = c_int

    dll.aio_frame_get_block.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_get_block.restype = POINTER(_CAioBlock)

    dll.aio_frame_get_block_data.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_get_block_data.restype = c_void_p

    dll.aio_frame_get_block_count.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_get_block_count.restype = c_int64

    dll.aio_frame_get_block_type.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_get_block_type.restype = c_int

    dll.aio_frame_remove_block.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_remove_block.restype = c_int

    dll.aio_frame_has_block.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_has_block.restype = c_int

    dll.aio_frame_list_blocks.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_int, POINTER(c_int)
    ]
    dll.aio_frame_list_blocks.restype = c_int

    # KV 操作 API
    dll.aio_frame_kv_set.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p, c_char_p
    ]
    dll.aio_frame_kv_set.restype = c_int

    dll.aio_frame_kv_get.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p
    ]
    dll.aio_frame_kv_get.restype = c_char_p

    dll.aio_frame_kv_set_double.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p, c_double
    ]
    dll.aio_frame_kv_set_double.restype = c_int

    dll.aio_frame_kv_get_double.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p, c_double
    ]
    dll.aio_frame_kv_get_double.restype = c_double

    # 缓存文件
    dll.aio_frame_save_cache.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_save_cache.restype = c_int

    dll.aio_frame_load_cache.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_load_cache.restype = c_int

    # 调试导出
    dll.aio_frame_export_block_fits.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p
    ]
    dll.aio_frame_export_block_fits.restype = c_int

    dll.aio_frame_export_block_xml.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p
    ]
    dll.aio_frame_export_block_xml.restype = c_int

    dll.aio_frame_export_all_xml.argtypes = [POINTER(_CPipelineFrame), c_char_p]
    dll.aio_frame_export_all_xml.restype = c_int

    # 旧版兼容 (export_xml -> export_all_xml)
    dll.aio_pipeline_export_xml.argtypes = [
        POINTER(_CPipelineFrame), c_char_p, c_char_p
    ]
    dll.aio_pipeline_export_xml.restype = c_int

    # 引擎 API
    dll.aio_pipeline_engine_create.argtypes = []
    dll.aio_pipeline_engine_create.restype = c_void_p

    dll.aio_pipeline_engine_destroy.argtypes = [c_void_p]
    dll.aio_pipeline_engine_destroy.restype = None

    dll.aio_pipeline_engine_register.argtypes = [
        c_void_p, c_int, PipelineStageHandlerC, c_void_p
    ]
    dll.aio_pipeline_engine_register.restype = c_int

    dll.aio_pipeline_engine_set_debug.argtypes = [c_void_p, c_char_p, c_int, c_int]
    dll.aio_pipeline_engine_set_debug.restype = c_int

    dll.aio_pipeline_engine_set_auto_free.argtypes = [c_void_p, c_int]
    dll.aio_pipeline_engine_set_auto_free.restype = c_int

    dll.aio_pipeline_engine_set_block_drop.argtypes = [c_void_p, c_int, c_char_p]
    dll.aio_pipeline_engine_set_block_drop.restype = c_int

    dll.aio_pipeline_engine_run_single.argtypes = [
        c_void_p, POINTER(_CPipelineFrame), c_int, c_int, c_char_p, c_int
    ]
    dll.aio_pipeline_engine_run_single.restype = c_int

    dll.aio_pipeline_engine_run_batch.argtypes = [
        c_void_p, POINTER(POINTER(_CPipelineFrame)), c_int, c_int, c_int, c_int, c_char_p, c_int
    ]
    dll.aio_pipeline_engine_run_batch.restype = c_int

    dll.aio_pipeline_stage_name.argtypes = [c_int]
    dll.aio_pipeline_stage_name.restype = c_char_p

    return dll


class PipelineFramePy:
    """PipelineFrame Python 封装 (命名块容器模型)

    用法:
        frame = PipelineFramePy()
        frame.add_block("data", pixels_array, description="图像像素")
        frame.kv_set("header", "EXPTIME", "60.0")
        frame.kv_set("header", "FILTER", "L")
        frame.kv_set_double("header", "CD1_1", 0.000123)
        # ... 传递给 PipelineEngine 执行
    """

    def __init__(self, dll_path: Optional[str] = None):
        if dll_path is None:
            dll_path = _find_ahpx_dll()
        self._dll = _load_pipeline_dll(dll_path)
        self._frame = self._dll.aio_pipeline_frame_create()
        if not self._frame:
            raise RuntimeError("aio_pipeline_frame_create 失败")
        self._closed = False
        self._owns_memory = True  # __init__ 创建的帧拥有内存，close 时释放
        self._keep_alive = []  # 持有 numpy 数组引用防止 GC (用于 add_block_move)

    @classmethod
    def from_c_ptr(cls, c_frame_ptr, dll=None):
        """包装现有 C 指针，不分配新内存（用于引擎回调场景）

        c_frame_ptr: POINTER(_CPipelineFrame) 或 c_void_p
        dll: 已加载的 pipeline DLL（None 时自动加载）
        """
        if dll is None:
            dll = _load_pipeline_dll(_find_ahpx_dll())
        obj = cls.__new__(cls)
        obj._dll = dll
        # 如果传入 c_void_p 或整数，转换为 POINTER(_CPipelineFrame)
        if isinstance(c_frame_ptr, int):
            obj._frame = c_cast(c_frame_ptr, POINTER(_CPipelineFrame))
        else:
            obj._frame = c_frame_ptr
        obj._closed = False
        obj._owns_memory = False  # 不拥有内存，close 时不调用 frame_destroy
        obj._keep_alive = []
        return obj

    # --- 块管理 ---

    def add_block(self, name: str, data, description: str = "") -> None:
        """添加块 (拷贝数据到 C 端)

        name: 块名 (如 "data", "header", "psf")
        data: numpy 数组 / 字符串 / dict (KV)
            - numpy 数组: 按 dtype 自动推断块类型 (FLOAT32/FLOAT64/INT32/INT64/RAW)
            - str: STRING 块
            - dict: KV 块 (key-value 对)
        description: 人类可读描述
        """
        if isinstance(data, dict):
            self.add_block_kv(name, data, description)
            return
        if isinstance(data, str):
            self.add_block_string(name, data, description)
            return
        if not isinstance(data, np.ndarray):
            data = np.asarray(data)
        arr = np.ascontiguousarray(data)
        block_type = _NUMPY_TO_BLOCK_TYPE.get(arr.dtype)
        if block_type is None:
            raise ValueError(f"不支持的 numpy dtype: {arr.dtype}")
        count = arr.size
        # 构建 dims (补齐到 4 维, 不足部分填 0)
        shape = list(arr.shape)
        n_dims = len(shape)
        if n_dims > 4:
            raise ValueError(f"维度数超过 4: {n_dims}")
        shape_padded = shape + [0] * (4 - n_dims)
        dims_arr = (c_int * 4)(*shape_padded)
        ret = self._dll.aio_frame_add_block(
            self._frame,
            name.encode("utf-8"),
            c_int(block_type),
            arr.ctypes.data_as(c_void_p),
            c_int64(count),
            dims_arr,
            c_int(n_dims),
            description.encode("utf-8"),
        )
        if ret != 0:
            raise RuntimeError(f"add_block 失败 (code={ret}): {name}")

    def add_block_string(self, name: str, value: str, description: str = "") -> None:
        """添加字符串块"""
        b = value.encode("utf-8")
        count = len(b)
        dims_arr = (c_int * 4)(count, 0, 0, 0)
        ret = self._dll.aio_frame_add_block(
            self._frame,
            name.encode("utf-8"),
            c_int(AIO_BLOCK_STRING),
            b if count > 0 else None,
            c_int64(count),
            dims_arr,
            c_int(1),
            description.encode("utf-8"),
        )
        if ret != 0:
            raise RuntimeError(f"add_block_string 失败 (code={ret}): {name}")

    def add_block_kv(self, name: str, kv_dict: dict, description: str = "") -> None:
        """添加 KV 块 (字典 → KV 块)"""
        # 先创建空 KV 块
        for key, value in kv_dict.items():
            self.kv_set(name, key, str(value))
        # 设置 description (通过重新创建块实现 - 这里简化处理，description 只在首次创建时有效)

    def get_block_data(self, name: str) -> Optional[np.ndarray]:
        """获取数值块数据为零拷贝 numpy 数组 (直接映射 C 内存)

        返回 shape 按 dims 重塑的 numpy 数组，或 None
        注意: 返回的数组与 C 内存共享，修改数组会修改块数据
        """
        from ctypes import c_uint8
        blk_ptr = self._dll.aio_frame_get_block(self._frame, name.encode("utf-8"))
        if not blk_ptr:
            return None
        blk = blk_ptr.contents
        if blk.type == AIO_BLOCK_KV:
            return None  # KV 块请用 get_block_kv
        if blk.type == AIO_BLOCK_STRING:
            return None  # STRING 块请用 get_block_string
        np_dtype = _BLOCK_TYPE_TO_NUMPY.get(blk.type)
        if np_dtype is None:
            return None
        if blk.count <= 0 or not blk.data:
            return np.zeros(0, dtype=np_dtype)
        # 零拷贝: 用 uint8 指针映射原始字节, 再 view 为目标 dtype
        elem_bytes = np.dtype(np_dtype).itemsize
        total_bytes = int(blk.count) * elem_bytes
        ptr = c_cast(blk.data, POINTER(c_uint8))
        raw = np.ctypeslib.as_array(ptr, shape=(total_bytes,))
        arr = raw.view(np_dtype)  # 零拷贝 reinterpret
        # 按 dims 重塑
        if blk.n_dims > 0:
            shape = tuple(blk.dims[i] for i in range(blk.n_dims))
            try:
                arr = arr.reshape(shape)
            except Exception:
                pass  # reshape 失败则返回 1D
        return arr

    def get_block_string(self, name: str) -> Optional[str]:
        """获取字符串块内容"""
        from ctypes import c_uint8
        blk_ptr = self._dll.aio_frame_get_block(self._frame, name.encode("utf-8"))
        if not blk_ptr:
            return None
        blk = blk_ptr.contents
        if blk.type != AIO_BLOCK_STRING:
            return None
        if not blk.data or blk.count <= 0:
            return ""
        # 读取字节
        ptr = c_cast(blk.data, POINTER(c_uint8))
        b = bytes(np.ctypeslib.as_array(ptr, shape=(blk.count,)))
        return b.decode("utf-8", errors="replace")

    def get_block_kv(self, name: str) -> Optional[dict]:
        """获取 KV 块为字典"""
        blk_ptr = self._dll.aio_frame_get_block(self._frame, name.encode("utf-8"))
        if not blk_ptr:
            return None
        blk = blk_ptr.contents
        if blk.type != AIO_BLOCK_KV:
            return None
        if not blk.data or blk.count <= 0:
            return {}
        # 读取 KV 数组
        ptr = c_cast(blk.data, POINTER(_CAioKVEntry))
        result = {}
        for i in range(blk.count):
            entry = ptr[i]
            key = entry.key.decode("utf-8", errors="replace").rstrip("\x00")
            value = entry.value.decode("utf-8", errors="replace").rstrip("\x00")
            result[key] = value
        return result

    def get_block_info(self, name: str) -> Optional[dict]:
        """获取块元数据 (不读数据)"""
        blk_ptr = self._dll.aio_frame_get_block(self._frame, name.encode("utf-8"))
        if not blk_ptr:
            return None
        blk = blk_ptr.contents
        return {
            "name": blk.name.decode("utf-8", errors="replace").rstrip("\x00"),
            "type": blk.type,
            "count": blk.count,
            "dims": list(blk.dims)[:blk.n_dims] if blk.n_dims > 0 else [],
            "n_dims": blk.n_dims,
            "description": blk.description.decode("utf-8", errors="replace").rstrip("\x00"),
        }

    def remove_block(self, name: str) -> bool:
        """移除块 (返回 True=成功, False=不存在)"""
        ret = self._dll.aio_frame_remove_block(self._frame, name.encode("utf-8"))
        return ret == 0

    def has_block(self, name: str) -> bool:
        """检查块是否存在"""
        return self._dll.aio_frame_has_block(self._frame, name.encode("utf-8")) == 1

    def list_blocks(self) -> list:
        """列出所有块名"""
        n_out = c_int(0)
        # 先查询数量
        self._dll.aio_frame_list_blocks(self._frame, None, 0, byref(n_out))
        n = n_out.value
        if n <= 0:
            return []
        # 分配缓冲区
        buf = create_string_buffer(64 * n)
        self._dll.aio_frame_list_blocks(self._frame, buf, n, byref(n_out))
        result = []
        for i in range(n):
            name_bytes = buf.raw[i * 64:(i + 1) * 64]
            name = name_bytes.split(b"\x00")[0].decode("utf-8", errors="replace")
            if name:
                result.append(name)
        return result

    # --- KV 操作 ---

    def kv_set(self, block_name: str, key: str, value: str) -> None:
        """设置 KV 块中某个 key 的 value (字符串形式)"""
        ret = self._dll.aio_frame_kv_set(
            self._frame,
            block_name.encode("utf-8"),
            key.encode("utf-8"),
            value.encode("utf-8"),
        )
        if ret != 0:
            raise RuntimeError(f"kv_set 失败 (code={ret}): {block_name}/{key}")

    def kv_get(self, block_name: str, key: str) -> Optional[str]:
        """获取 KV 块中某个 key 的 value"""
        ptr = self._dll.aio_frame_kv_get(
            self._frame,
            block_name.encode("utf-8"),
            key.encode("utf-8"),
        )
        if ptr:
            return ptr.decode("utf-8", errors="replace")
        return None

    def kv_set_double(self, block_name: str, key: str, value: float) -> None:
        """设置 KV 块中某个 key 的 value (double 自动转字符串)"""
        ret = self._dll.aio_frame_kv_set_double(
            self._frame,
            block_name.encode("utf-8"),
            key.encode("utf-8"),
            c_double(value),
        )
        if ret != 0:
            raise RuntimeError(f"kv_set_double 失败 (code={ret}): {block_name}/{key}")

    def kv_get_double(self, block_name: str, key: str, default: float = 0.0) -> float:
        """获取 KV 块中某个 key 的 value (字符串转 double)"""
        return self._dll.aio_frame_kv_get_double(
            self._frame,
            block_name.encode("utf-8"),
            key.encode("utf-8"),
            c_double(default),
        )

    # --- 缓存文件 ---

    def save_cache(self, path: str) -> None:
        """保存所有块到缓存文件 (.aio)"""
        ret = self._dll.aio_frame_save_cache(self._frame, path.encode("utf-8"))
        if ret != 0:
            raise RuntimeError(f"save_cache 失败 (code={ret}): {path}")

    def load_cache(self, path: str) -> None:
        """从缓存文件加载所有块 (清除现有块后加载)"""
        ret = self._dll.aio_frame_load_cache(self._frame, path.encode("utf-8"))
        if ret != 0:
            raise RuntimeError(f"load_cache 失败 (code={ret}): {path}")

    # --- 调试导出 ---

    def export_block_fits(self, name: str, path: str) -> None:
        """导出单个块为 FITS 文件"""
        ret = self._dll.aio_frame_export_block_fits(
            self._frame, name.encode("utf-8"), path.encode("utf-8")
        )
        if ret != 0:
            raise RuntimeError(f"export_block_fits 失败 (code={ret}): {name} -> {path}")

    def export_block_xml(self, name: str, path: str) -> None:
        """导出单个块为 XML 文件"""
        ret = self._dll.aio_frame_export_block_xml(
            self._frame, name.encode("utf-8"), path.encode("utf-8")
        )
        if ret != 0:
            raise RuntimeError(f"export_block_xml 失败 (code={ret}): {name} -> {path}")

    def export_all_xml(self, path: str) -> None:
        """导出所有块为 XML 文件"""
        ret = self._dll.aio_frame_export_all_xml(self._frame, path.encode("utf-8"))
        if ret != 0:
            raise RuntimeError(f"export_all_xml 失败 (code={ret}): {path}")

    def export_xml(self, path: str, comment: str = "") -> int:
        """旧版兼容 (导出所有块为 XML)"""
        return self._dll.aio_pipeline_export_xml(
            self._frame, path.encode("utf-8"), comment.encode("utf-8")
        )

    # --- 属性 ---

    @property
    def handle(self) -> int:
        """C 端 PipelineFrame 指针 (用于传递给引擎)"""
        return c_cast(self._frame, c_void_p).value

    @property
    def c_frame(self):
        """直接访问 C PipelineFrame 结构体"""
        return self._frame

    @property
    def memory_usage(self) -> int:
        """当前帧内存占用 (字节)"""
        return self._dll.aio_pipeline_frame_memory_usage(self._frame)

    @property
    def stages_completed(self) -> int:
        return self._frame.contents.stages_completed

    @property
    def n_blocks(self) -> int:
        return self._frame.contents.n_blocks

    def close(self) -> None:
        if not self._closed and self._frame:
            if self._owns_memory:
                self._dll.aio_pipeline_frame_destroy(self._frame)
            self._frame = None
            self._closed = True
            self._keep_alive.clear()

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class PipelineEngine:
    """管线编排引擎, 串联 CALIBRATE → PLATESOLVE → PHOTOMETRIC → DRIZZLE → STACK

    用法:
        engine = PipelineEngine()
        engine.register(STAGE_DRIZZLE, drizzle_handler, drizzle_params)
        engine.register(STAGE_STACK, stack_handler, stack_params)
        frame = PipelineFramePy()
        frame.set_pixels(...)
        frame.set_wcs(...)
        engine.run_single(frame, STAGE_DRIZZLE, STAGE_DRIZZLE)
    """

    def __init__(self, dll_path: Optional[str] = None):
        if dll_path is None:
            dll_path = _find_ahpx_dll()
        self._dll = _load_pipeline_dll(dll_path)
        self._engine = self._dll.aio_pipeline_engine_create()
        if not self._engine:
            raise RuntimeError("aio_pipeline_engine_create 失败")
        self._handlers = []  # 持有 CFUNCTYPE 引用防止 GC
        self._closed = False

    def register(self, stage: int, handler: PipelineStageHandlerC,
                 params: Optional[int] = None) -> None:
        """注册阶段处理函数

        stage: STAGE_CALIBRATE ~ STAGE_STACK
        handler: PipelineStageHandlerC 类型的回调函数
        params: 阶段参数指针 (c_void_p 的 value, 或 None)
        """
        param_ptr = params if params is not None else None
        ret = self._dll.aio_pipeline_engine_register(
            self._engine, stage, handler, param_ptr
        )
        if ret != 0:
            raise RuntimeError(f"注册阶段 {stage} 失败 (code={ret})")
        self._handlers.append(handler)  # 防止 GC

    def set_debug(self, dir_path: str, stage_mask: int = DEBUG_AFTER_ALL,
                  skip_pixels: bool = False) -> None:
        """设置调试导出

        dir_path: 导出目录
        stage_mask: 导出阶段位掩码 (DEBUG_AFTER_*)
        skip_pixels: True=跳过像素数据只导出元数据
        """
        ret = self._dll.aio_pipeline_engine_set_debug(
            self._engine, dir_path.encode("utf-8"), stage_mask,
            1 if skip_pixels else 0
        )
        if ret != 0:
            raise RuntimeError(f"set_debug 失败 (code={ret})")

    def set_auto_free(self, auto_free: bool) -> None:
        """设置是否自动丢弃中间块 (默认 True)
        默认策略: PLATESOLVE 后丢弃 weight; PHOTOMETRIC 后丢弃 star_det/gaia_cat/psf;
        DRIZZLE 后丢弃 data/snr/weight/photo_stats; STACK 后丢弃 healpix
        """
        self._dll.aio_pipeline_engine_set_auto_free(
            self._engine, 1 if auto_free else 0
        )

    def set_block_drop(self, stage: int, block_names: Optional[str]) -> None:
        """自定义某阶段后要丢弃的块 (覆盖默认策略)

        stage: STAGE_CALIBRATE ~ STAGE_STACK
        block_names: 逗号分隔的块名列表 (如 "weight,psf"), None 或空串表示不丢弃
        """
        if block_names is None:
            block_names_bytes = None
        else:
            block_names_bytes = block_names.encode("utf-8")
        ret = self._dll.aio_pipeline_engine_set_block_drop(
            self._engine, stage, block_names_bytes
        )
        if ret != 0:
            raise RuntimeError(f"set_block_drop 失败 (code={ret})")

    def run_single(self, frame: PipelineFramePy, from_stage: int, to_stage: int) -> int:
        """单帧执行

        frame: PipelineFramePy 实例
        from_stage/to_stage: 起始/结束阶段
        返回: 0=成功, 非0=失败 (抛出 RuntimeError)
        """
        err_buf = create_string_buffer(512)
        ret = self._dll.aio_pipeline_engine_run_single(
            self._engine, frame.c_frame, from_stage, to_stage,
            err_buf, 512
        )
        if ret != 0:
            err_msg = err_buf.value.decode("utf-8", errors="replace")
            raise RuntimeError(f"管线执行失败 (code={ret}): {err_msg}")
        return ret

    def run_batch(self, frames: list, n_threads: int = 16,
                  from_stage: int = STAGE_CALIBRATE,
                  to_stage: int = STAGE_DRIZZLE) -> int:
        """批量并行执行

        frames: PipelineFramePy 列表
        n_threads: 线程数 (默认 16)
        from_stage/to_stage: 起始/结束阶段
        返回: 成功帧数
        """
        n = len(frames)
        if n == 0:
            return 0
        # 构建 C 端 PipelineFrame* 数组
        frame_arr = (POINTER(_CPipelineFrame) * n)()
        for i, f in enumerate(frames):
            frame_arr[i] = f.c_frame

        err_buf = create_string_buffer(512)
        ret = self._dll.aio_pipeline_engine_run_batch(
            self._engine, frame_arr, n, n_threads,
            from_stage, to_stage, err_buf, 512
        )
        if ret < n:
            err_msg = err_buf.value.decode("utf-8", errors="replace")
            import sys
            print(f"[PipelineEngine] 部分帧失败: {ret}/{n} 成功. {err_msg}", file=sys.stderr)
        return ret

    @staticmethod
    def stage_name(stage: int) -> str:
        """获取阶段名称"""
        from ctypes import string_at
        dll = _load_pipeline_dll(_find_ahpx_dll())
        ptr = dll.aio_pipeline_stage_name(stage)
        if ptr:
            return string_at(ptr).decode("utf-8", errors="replace")
        return "unknown"

    def close(self) -> None:
        if not self._closed and self._engine:
            self._dll.aio_pipeline_engine_destroy(self._engine)
            self._engine = None
            self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
