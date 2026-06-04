"""
Star Detector - 天文图像星点检测器 Python封装
功能: 检测天文图像中的星点坐标，支持正常星Moffat4拟合+饱和星半阈值检测
用途: 输入16bit FITS图像，输出星点坐标+flux+饱和标记，可选输出拟合参数
依赖: star_detector.dll
"""

from __future__ import annotations

import os
from ctypes import (
    Structure, c_int, c_float, c_double, c_void_p, c_uint16, c_char_p,
    POINTER, byref, cdll, cast,
)
from dataclasses import dataclass, field
from typing import Optional

import numpy as np
from PIL import Image, ImageDraw


@dataclass
class SDetParamsPy:
    structureLayers: int = 5
    hotPixelFilterRadius: int = 1
    iterativeClipSigma: float = 9.0
    iterativeMaxRounds: int = 5
    medianFilterDetail: int = 1
    maxStars: int = 0
    fitRadius: int = 6
    fwhmClipSigma: float = 3.0
    maxAxisRatio: float = 2.0


@dataclass
class StarDetectionResult:
    """星点检测结果"""
    x: list[float]
    y: list[float]
    flux: list[float]          # 正常星=振幅A，饱和星=-1
    saturated: list[int]       # 0=正常星，1=饱和星
    # 可选额外数据
    extras: dict[str, list[float]] = field(default_factory=dict)

    @property
    def count(self) -> int:
        return len(self.x)

    @property
    def saturated_count(self) -> int:
        return sum(self.saturated)

    @property
    def normal_count(self) -> int:
        return self.count - self.saturated_count


class _CSDetParams(Structure):
    _fields_ = [
        ("structureLayers", c_int),
        ("hotPixelFilterRadius", c_int),
        ("iterativeClipSigma", c_float),
        ("iterativeMaxRounds", c_int),
        ("medianFilterDetail", c_int),
        ("maxStars", c_int),
        ("fitRadius", c_int),
        ("fwhmClipSigma", c_float),
        ("maxAxisRatio", c_float),
    ]


def _params_py_to_c(p: SDetParamsPy) -> _CSDetParams:
    return _CSDetParams(
        structureLayers=p.structureLayers,
        hotPixelFilterRadius=p.hotPixelFilterRadius,
        iterativeClipSigma=p.iterativeClipSigma,
        iterativeMaxRounds=p.iterativeMaxRounds,
        medianFilterDetail=p.medianFilterDetail,
        maxStars=p.maxStars,
        fitRadius=p.fitRadius,
        fwhmClipSigma=p.fwhmClipSigma,
        maxAxisRatio=p.maxAxisRatio,
    )


# 支持的额外输出参数名
EXTRA_NAMES_SUPPORTED = {"fwhm_x", "fwhm_y", "sx", "sy", "theta", "background", "amplitude", "r"}


def _load_dll(dll_path: str):
    mingw_bin = r"C:\msys64\mingw64\bin"
    if os.path.isdir(mingw_bin):
        os.environ["PATH"] = mingw_bin + ";" + os.environ.get("PATH", "")
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
    dll.sdet_create.argtypes = [POINTER(_CSDetParams)]
    dll.sdet_create.restype = c_void_p
    dll.sdet_destroy.argtypes = [c_void_p]
    dll.sdet_destroy.restype = None
    dll.sdet_detect.argtypes = [
        c_void_p, POINTER(c_uint16), c_int, c_int,
        POINTER(POINTER(c_double)), POINTER(POINTER(c_double)), POINTER(c_int),
    ]
    dll.sdet_detect.restype = c_int
    dll.sdet_free_coords.argtypes = [POINTER(c_double)]
    dll.sdet_free_coords.restype = None
    dll.sdet_detect_ex.argtypes = [
        c_void_p, POINTER(c_uint16), c_int, c_int,
        POINTER(POINTER(c_double)), POINTER(POINTER(c_double)),
        POINTER(POINTER(c_float)), POINTER(POINTER(c_int)), POINTER(c_int),
        POINTER(c_char_p), c_int, POINTER(POINTER(POINTER(c_float))),
    ]
    dll.sdet_detect_ex.restype = c_int
    dll.sdet_free_detect_ex.argtypes = [
        POINTER(c_double), POINTER(c_double), POINTER(c_float), POINTER(c_int),
        POINTER(POINTER(c_float)), c_int,
    ]
    dll.sdet_free_detect_ex.restype = None
    dll.sdet_detect_debug.argtypes = [
        c_void_p, POINTER(c_uint16), c_int, c_int,
        POINTER(POINTER(c_double)), POINTER(POINTER(c_double)), POINTER(c_int),
        POINTER(POINTER(c_float)), POINTER(POINTER(c_float)), POINTER(POINTER(c_float)),
        POINTER(c_char_p), c_int, POINTER(POINTER(POINTER(c_float))),
    ]
    dll.sdet_detect_debug.restype = c_int
    dll.sdet_free_debug_maps.argtypes = [POINTER(c_float)]
    dll.sdet_free_debug_maps.restype = None
    return dll


class StarDetector:
    def __init__(self, dll_path: Optional[str] = None, params: Optional[SDetParamsPy] = None):
        if dll_path is None:
            base = os.path.dirname(os.path.abspath(__file__))
            dll_path = os.path.normpath(os.path.join(base, "..", "star_detector.dll"))
        self._dll = _load_dll(dll_path)
        if params is None:
            params = SDetParamsPy()
        c_params = _params_py_to_c(params)
        self._handle = self._dll.sdet_create(byref(c_params))
        self._closed = False
        if not self._handle:
            raise RuntimeError(f"sdet_create 失败: {dll_path}")

    def detect(self, image: np.ndarray) -> list[tuple[float, float]]:
        if image.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={image.ndim}")
        if image.dtype == np.uint16:
            img = np.ascontiguousarray(image, dtype=np.uint16)
        else:
            img = np.ascontiguousarray(np.clip(image, 0, 65535).astype(np.uint16), dtype=np.uint16)
        height, width = img.shape
        data_ptr = img.ctypes.data_as(POINTER(c_uint16))
        out_x = POINTER(c_double)()
        out_y = POINTER(c_double)()
        out_count = c_int(0)
        ret = self._dll.sdet_detect(
            self._handle, data_ptr, width, height,
            byref(out_x), byref(out_y), byref(out_count),
        )
        if ret != 0:
            raise RuntimeError(f"sdet_detect 返回错误码: {ret}")
        count = out_count.value
        coords = []
        if count > 0 and out_x and out_y:
            for i in range(count):
                coords.append((out_x[i], out_y[i]))
            self._dll.sdet_free_coords(out_x)
            self._dll.sdet_free_coords(out_y)
        return coords

    def detect_ex(self, image: np.ndarray,
                  extra_names: Optional[list[str]] = None) -> StarDetectionResult:
        """检测星点，返回坐标+flux+饱和标记，可选额外参数

        Args:
            image: 16bit 2D数组
            extra_names: 额外输出参数名列表，支持: fwhm_x, fwhm_y, sx, sy, theta, background, amplitude, r

        Returns:
            StarDetectionResult: 包含坐标、flux、饱和标记和可选额外数据
        """
        if image.ndim != 2:
            raise ValueError(f"image 必须为2D数组, 当前 ndim={image.ndim}")
        if image.dtype == np.uint16:
            img = np.ascontiguousarray(image, dtype=np.uint16)
        else:
            img = np.ascontiguousarray(np.clip(image, 0, 65535).astype(np.uint16), dtype=np.uint16)
        height, width = img.shape
        data_ptr = img.ctypes.data_as(POINTER(c_uint16))

        out_x = POINTER(c_double)()
        out_y = POINTER(c_double)()
        out_flux = POINTER(c_float)()
        out_saturated = POINTER(c_int)()
        out_count = c_int(0)

        # 准备额外参数
        c_extra_names = None
        extra_count = 0
        out_extras = POINTER(POINTER(c_float))()

        if extra_names:
            for name in extra_names:
                if name not in EXTRA_NAMES_SUPPORTED:
                    raise ValueError(f"不支持的额外参数名: {name}, 支持: {EXTRA_NAMES_SUPPORTED}")
            extra_count = len(extra_names)
            c_extra_names_arr = (c_char_p * extra_count)()
            for i, name in enumerate(extra_names):
                c_extra_names_arr[i] = name.encode('utf-8')
            c_extra_names = cast(c_extra_names_arr, POINTER(c_char_p))

        ret = self._dll.sdet_detect_ex(
            self._handle, data_ptr, width, height,
            byref(out_x), byref(out_y), byref(out_flux), byref(out_saturated), byref(out_count),
            c_extra_names, extra_count, byref(out_extras),
        )
        if ret != 0:
            raise RuntimeError(f"sdet_detect_ex 返回错误码: {ret}")

        count = out_count.value
        xs = []
        ys = []
        fluxes = []
        saturateds = []
        extras_dict = {name: [] for name in (extra_names or [])}

        if count > 0 and out_x and out_y:
            for i in range(count):
                xs.append(out_x[i])
                ys.append(out_y[i])
                fluxes.append(out_flux[i])
                saturateds.append(out_saturated[i])

            # 读取额外参数
            if extra_count > 0 and out_extras:
                for e in range(extra_count):
                    name = extra_names[e]
                    for i in range(count):
                        extras_dict[name].append(out_extras[e][i])

            self._dll.sdet_free_detect_ex(
                out_x, out_y, out_flux, out_saturated,
                out_extras if extra_count > 0 else None, extra_count,
            )

        return StarDetectionResult(x=xs, y=ys, flux=fluxes, saturated=saturateds, extras=extras_dict)

    def detect_debug_image(self, image: np.ndarray, output_path: str,
                           extra_names: Optional[list[str]] = None) -> StarDetectionResult:
        """检测星点并输出标注图像"""
        result = self.detect_ex(image, extra_names=extra_names)
        p_low, p_high = np.percentile(image, [0.5, 99.5])
        stretched = np.clip((image - p_low) / (p_high - p_low), 0, 1)
        rgb = (stretched * 255).astype(np.uint8)
        rgb = np.stack([rgb, rgb, rgb], axis=-1)
        img_pil = Image.fromarray(rgb)
        draw = ImageDraw.Draw(img_pil)
        # 正常星：绿色十字
        for i in range(result.count):
            if result.saturated[i] == 0:
                ix, iy = int(result.x[i]), int(result.y[i])
                draw.line([(ix - 5, iy), (ix + 5, iy)], fill=(0, 255, 0), width=1)
                draw.line([(ix, iy - 5), (ix, iy + 5)], fill=(0, 255, 0), width=1)
        # 饱和星：红色圆圈（半径=r）
        for i in range(result.count):
            if result.saturated[i] == 1:
                ix, iy = int(result.x[i]), int(result.y[i])
                r = int(result.extras.get("r", [8.0])[i]) if "r" in result.extras else 8
                r = max(r, 3)
                draw.ellipse([(ix - r, iy - r), (ix + r, iy + r)], outline=(255, 0, 0), width=2)
        img_pil.save(output_path)
        return result

    def close(self) -> None:
        if not self._closed and self._handle:
            self._dll.sdet_destroy(self._handle)
            self._handle = None
            self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
