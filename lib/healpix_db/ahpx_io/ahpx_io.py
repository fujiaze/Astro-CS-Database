"""
ahpx_io.py - .ahpx 单帧存储格式 Python 绑定

功能：通过 ctypes 调用 ahpx_io.dll，读写 .ahpx 自定义二进制格式
用途：单帧图像(像素+SNR+权重+WCS+元数据)的存储与读取

使用示例：
    from ahpx_io import AhpxReader, AhpxWriter

    # 读取
    reader = AhpxReader("frame.ahpx")
    pixels = reader.read_pixels()  # numpy array (H, W, C)
    snr = reader.read_snr()        # numpy array (H, W)
    header = reader.header_json     # 元数据 JSON 字符串
    reader.close()

    # 写入
    writer = AhpxWriter()
    writer.set_metadata(metadata_json)
    writer.set_pixels(pixels, width, height, channels)
    writer.set_snr(snr, width, height)
    writer.set_weight_scalar(1.0)
    writer.write("output.ahpx", zstd_level=5)
"""

from __future__ import annotations

import os
from ctypes import (
    c_int, c_float, c_void_p, c_char_p,
    POINTER, byref, cdll,
)
from typing import Optional, Union

import numpy as np


# 权重模式常量 (对应 ahpx::WeightMode)
AHPX_WEIGHT_SCALAR = 0   # 整图统一权重 (标量)
AHPX_WEIGHT_GRID = 1     # 分块网格权重 (gw×gh)
AHPX_WEIGHT_PIXEL = 2    # 逐像素权重 (W×H)


def _find_dll() -> str:
    """查找 ahpx_io.dll: 同目录 → 上级目录 → lib/healpix_db/ahpx_io/"""
    dll_name = "ahpx_io.dll"
    base = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base, dll_name),                                              # 同目录
        os.path.normpath(os.path.join(base, "..", dll_name)),                      # 上级目录
        os.path.normpath(os.path.join(base, "..", "..", "..", "lib",               # lib/healpix_db/ahpx_io/
                                      "healpix_db", "ahpx_io", dll_name)),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return os.path.join(base, dll_name)  # 默认同目录


def _load_dll(dll_path: str):
    """加载 ahpx_io.dll 并配置 C API 函数签名"""
    mingw_bin = r"C:\msys64\mingw64\bin"
    if os.path.isdir(mingw_bin):
        # 避免重复追加 PATH 导致环境变量超长 (Windows 32767 字符限制)
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

    # ===== 读取 API =====
    dll.ahpx_open.argtypes = [c_char_p]
    dll.ahpx_open.restype = c_void_p
    dll.ahpx_get_header_json.argtypes = [c_void_p]
    dll.ahpx_get_header_json.restype = c_char_p
    dll.ahpx_get_image_info.argtypes = [c_void_p, POINTER(c_int), POINTER(c_int), POINTER(c_int)]
    dll.ahpx_get_image_info.restype = c_int
    dll.ahpx_read_pixels.argtypes = [c_void_p]
    dll.ahpx_read_pixels.restype = POINTER(c_float)
    dll.ahpx_read_snr.argtypes = [c_void_p]
    dll.ahpx_read_snr.restype = POINTER(c_float)
    dll.ahpx_read_weight.argtypes = [c_void_p, POINTER(c_int), POINTER(c_int),
                                     POINTER(c_int), POINTER(c_int)]
    dll.ahpx_read_weight.restype = POINTER(c_float)
    dll.ahpx_close.argtypes = [c_void_p]
    dll.ahpx_close.restype = None

    # ===== 写入 API =====
    dll.ahpx_writer_new.argtypes = []
    dll.ahpx_writer_new.restype = c_void_p
    dll.ahpx_writer_set_metadata.argtypes = [c_void_p, c_char_p]
    dll.ahpx_writer_set_metadata.restype = None
    dll.ahpx_writer_set_pixels.argtypes = [c_void_p, POINTER(c_float), c_int, c_int, c_int]
    dll.ahpx_writer_set_pixels.restype = None
    dll.ahpx_writer_set_snr.argtypes = [c_void_p, POINTER(c_float), c_int, c_int]
    dll.ahpx_writer_set_snr.restype = None
    dll.ahpx_writer_set_weight_scalar.argtypes = [c_void_p, c_float]
    dll.ahpx_writer_set_weight_scalar.restype = None
    dll.ahpx_writer_set_weight_grid.argtypes = [c_void_p, POINTER(c_float), c_int, c_int]
    dll.ahpx_writer_set_weight_grid.restype = None
    dll.ahpx_writer_set_weight_pixel.argtypes = [c_void_p, POINTER(c_float), c_int, c_int]
    dll.ahpx_writer_set_weight_pixel.restype = None
    dll.ahpx_writer_write.argtypes = [c_void_p, c_char_p, c_int]
    dll.ahpx_writer_write.restype = c_int
    dll.ahpx_writer_free.argtypes = [c_void_p]
    dll.ahpx_writer_free.restype = None

    # ===== 工具函数 =====
    dll.ahpx_is_ahpx.argtypes = [c_char_p]
    dll.ahpx_is_ahpx.restype = c_int
    dll.ahpx_free.argtypes = [c_void_p]
    dll.ahpx_free.restype = None

    return dll


class AhpxReader:
    """.ahpx 文件读取器, 封装 ahpx_reader C API"""

    def __init__(self, path: str, dll_path: Optional[str] = None):
        self._handle = None
        self._closed = False
        self._header_json_cache: Optional[str] = None
        self._image_info_cache: Optional[tuple] = None
        if dll_path is None:
            dll_path = _find_dll()
        self._dll = _load_dll(dll_path)
        self._c_free = self._dll.ahpx_free  # 用 DLL 导出的 ahpx_free 释放内存
        handle = self._dll.ahpx_open(path.encode("utf-8"))
        if not handle:
            raise RuntimeError(f"打开 .ahpx 文件失败: {path}")
        self._handle = handle

    @property
    def header_json(self) -> str:
        """元数据 JSON 字符串 (已解压)"""
        if self._header_json_cache is None:
            ptr = self._dll.ahpx_get_header_json(self._handle)
            if ptr is None:
                raise RuntimeError("获取 header JSON 失败")
            # restype=c_char_p: ctypes 自动复制为 bytes
            self._header_json_cache = ptr.decode("utf-8", errors="replace")
        return self._header_json_cache

    @property
    def image_info(self) -> tuple:
        """(width, height, channels)"""
        if self._image_info_cache is None:
            w, h, c = c_int(0), c_int(0), c_int(0)
            ret = self._dll.ahpx_get_image_info(self._handle, byref(w), byref(h), byref(c))
            if not ret:
                raise RuntimeError("获取图像几何信息失败")
            self._image_info_cache = (w.value, h.value, c.value)
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

    def _free_ptr(self, ptr) -> None:
        """释放 DLL malloc 分配的指针"""
        if ptr and self._c_free is not None:
            try:
                self._c_free(ptr)
            except Exception:
                pass

    def read_pixels(self) -> np.ndarray:
        """读取像素数据, 返回 float32 numpy array (H, W, C)"""
        w, h, c = self.image_info
        ptr = self._dll.ahpx_read_pixels(self._handle)
        if not ptr:
            raise RuntimeError("读取像素数据失败")
        try:
            n = h * w * c
            arr = np.ctypeslib.as_array(ptr, shape=(n,)).copy()
            return arr.reshape(h, w, c)
        finally:
            self._free_ptr(ptr)

    def read_snr(self) -> np.ndarray:
        """读取 SNR 图, 返回 float32 numpy array (H, W)"""
        w, h, c = self.image_info
        ptr = self._dll.ahpx_read_snr(self._handle)
        if not ptr:
            raise RuntimeError("读取 SNR 数据失败")
        try:
            n = h * w
            arr = np.ctypeslib.as_array(ptr, shape=(n,)).copy()
            return arr.reshape(h, w)
        finally:
            self._free_ptr(ptr)

    def read_weight(self) -> Union[float, np.ndarray]:
        """读取权重: SCALAR 返回 float, GRID 返回 (gh, gw), PIXEL 返回 (h, w)"""
        w, h, c = self.image_info
        mode = c_int(0)
        gw = c_int(0)
        gh = c_int(0)
        count = c_int(0)
        ptr = self._dll.ahpx_read_weight(self._handle,
                                         byref(mode), byref(gw), byref(gh), byref(count))
        if not ptr:
            raise RuntimeError("读取权重数据失败")
        try:
            n = count.value
            if n <= 0:
                return 0.0
            arr = np.ctypeslib.as_array(ptr, shape=(n,)).copy()
            if mode.value == AHPX_WEIGHT_SCALAR:
                return float(arr[0])
            elif mode.value == AHPX_WEIGHT_GRID:
                return arr.reshape(gh.value, gw.value)
            elif mode.value == AHPX_WEIGHT_PIXEL:
                return arr.reshape(h, w)
            return arr
        finally:
            self._free_ptr(ptr)

    def close(self) -> None:
        if not self._closed and self._handle:
            self._dll.ahpx_close(self._handle)
            self._handle = None
            self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class AhpxWriter:
    """.ahpx 文件写入器, 封装 ahpx_writer C API"""

    def __init__(self, dll_path: Optional[str] = None):
        self._handle = None
        self._closed = False
        self._pixels_ref: Optional[np.ndarray] = None
        self._snr_ref: Optional[np.ndarray] = None
        self._weight_ref: Optional[np.ndarray] = None
        if dll_path is None:
            dll_path = _find_dll()
        self._dll = _load_dll(dll_path)
        handle = self._dll.ahpx_writer_new()
        if not handle:
            raise RuntimeError("创建 AhpxWriter 失败")
        self._handle = handle

    def set_metadata(self, json_str: str) -> None:
        """设置元数据 JSON 字符串"""
        self._dll.ahpx_writer_set_metadata(self._handle, json_str.encode("utf-8"))

    def set_pixels(self, pixels: np.ndarray, width: int, height: int, channels: int) -> None:
        """设置像素数据 (float32, W×H×C)"""
        arr = np.ascontiguousarray(pixels, dtype=np.float32)
        expected = height * width * channels
        if arr.size != expected:
            raise ValueError(f"像素数据大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._pixels_ref = arr  # 持有引用防止 GC 回收 (C++ 端保存指针)
        self._dll.ahpx_writer_set_pixels(self._handle,
                                         arr.ctypes.data_as(POINTER(c_float)),
                                         width, height, channels)

    def set_snr(self, snr: np.ndarray, width: int, height: int) -> None:
        """设置 SNR 图 (float32, W×H)"""
        arr = np.ascontiguousarray(snr, dtype=np.float32)
        expected = height * width
        if arr.size != expected:
            raise ValueError(f"SNR 数据大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._snr_ref = arr
        self._dll.ahpx_writer_set_snr(self._handle,
                                      arr.ctypes.data_as(POINTER(c_float)),
                                      width, height)

    def set_weight_scalar(self, scalar: float) -> None:
        """设置标量权重"""
        self._dll.ahpx_writer_set_weight_scalar(self._handle, scalar)
        self._weight_ref = None

    def set_weight_grid(self, grid: np.ndarray, gw: int, gh: int) -> None:
        """设置网格权重 (float32, gw×gh)"""
        arr = np.ascontiguousarray(grid, dtype=np.float32)
        expected = gw * gh
        if arr.size != expected:
            raise ValueError(f"权重网格大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._weight_ref = arr
        self._dll.ahpx_writer_set_weight_grid(self._handle,
                                              arr.ctypes.data_as(POINTER(c_float)),
                                              gw, gh)

    def set_weight_pixel(self, data: np.ndarray, width: int, height: int) -> None:
        """设置逐像素权重 (float32, W×H)"""
        arr = np.ascontiguousarray(data, dtype=np.float32)
        expected = height * width
        if arr.size != expected:
            raise ValueError(f"权重像素数据大小不匹配: 期望 {expected}, 实际 {arr.size}")
        self._weight_ref = arr
        self._dll.ahpx_writer_set_weight_pixel(self._handle,
                                               arr.ctypes.data_as(POINTER(c_float)),
                                               width, height)

    def write(self, path: str, zstd_level: int = 5) -> str:
        """写入 .ahpx 文件, zstd_level: 0=不压缩, 1-22 压缩级别 (推荐 5)"""
        ret = self._dll.ahpx_writer_write(self._handle, path.encode("utf-8"), zstd_level)
        if not ret:
            raise RuntimeError(f"写入 .ahpx 文件失败: {path}")
        return path

    def close(self) -> None:
        if not self._closed and self._handle:
            self._dll.ahpx_writer_free(self._handle)
            self._handle = None
            self._closed = True
            self._pixels_ref = None
            self._snr_ref = None
            self._weight_ref = None

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def is_ahpx(path: str, dll_path: Optional[str] = None) -> bool:
    """检查文件是否为 .ahpx 格式 (检查 Magic)"""
    if dll_path is None:
        dll_path = _find_dll()
    dll = _load_dll(dll_path)
    return bool(dll.ahpx_is_ahpx(path.encode("utf-8")))
