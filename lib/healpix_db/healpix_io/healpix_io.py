# -*- coding: utf-8 -*-
"""
healpix_io.py - HEALPix 存储格式读写 Python 绑定

功能：封装 healpix_io.dll，提供 .hiss（单帧存储）和 .hcsd（天球数据库）的读写接口
用途：天文图像处理管线中 HEALPix 球面数据的持久化存储

支持格式：
  .hiss - HEALPix Storage System（单帧，稀疏 ipix + pixel 值）
  .hcsd - HEALPix CS Database（天球数据库，含子叶块索引，支持按需加载）

内存管理：
  - C 侧 malloc 分配的 ipix/pixel 数组通过 np.ctypeslib.as_array 零拷贝访问
  - Reader 对象销毁时调用 hio_free 释放 C 内存
  - 建议使用 with 语句或保持 Reader 引用，避免提前释放

调用示例：
  from healpix_io import HissWriter, HissReader
  writer = HissWriter("out.hiss", nside=8192, nested=True)
  writer.write(ipix, pixel, {"filter": "Lum"})
  with HissReader("out.hiss") as reader:
      print(reader.nside, reader.n_pix, reader.ipix, reader.pixel, reader.meta)
"""

from __future__ import annotations

import json
import logging
import os
from ctypes import (
    c_char_p, c_int, c_uint32, c_uint64, c_float, c_void_p,
    POINTER, byref, CDLL, cast,
)
from typing import Optional, Tuple, Dict

import numpy as np

logger = logging.getLogger(__name__)


# ============================================================================
# DLL 加载与签名绑定
# ============================================================================

def _find_dll() -> str:
    """查找 healpix_io.dll 路径（相对于本模块位置推导）"""
    # 本文件: lib/healpix_db/healpix_io/healpix_io.py
    # DLL:    lib/healpix_db/healpix_io/healpix_io.dll
    this_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.join(this_dir, "healpix_io.dll")
    if os.path.isfile(dll_path):
        return dll_path
    raise FileNotFoundError(
        f"healpix_io.dll 未找到，期望路径: {dll_path}\n"
        f"请先运行 build.ps1 编译 DLL")


def _load_dll(dll_path: Optional[str] = None) -> CDLL:
    """加载 DLL 并设置函数签名"""
    if dll_path is None:
        dll_path = _find_dll()
    logger.info("加载 healpix_io.dll: %s", dll_path)
    try:
        dll = CDLL(dll_path)
    except OSError as e:
        raise RuntimeError(
            f"加载 healpix_io.dll 失败: {e}\n"
            f"路径: {dll_path}") from e

    _setup_signatures(dll)
    logger.info("DLL 加载成功，6 个 API 已绑定")
    return dll


def _setup_signatures(dll: CDLL):
    """设置 6 个 C 函数的 argtypes 和 restype"""

    # hiss_write(path, nside, nested, n_pix, ipix, pixel, meta_json) -> int
    dll.hiss_write.argtypes = [
        c_char_p, c_uint32, c_int, c_uint64,
        POINTER(c_uint64), POINTER(c_float), c_char_p,
    ]
    dll.hiss_write.restype = c_int

    # hiss_read(path, nside*, nested*, n_pix*, ipix**, pixel**, meta_json**) -> int
    dll.hiss_read.argtypes = [
        c_char_p,
        POINTER(c_uint32), POINTER(c_int), POINTER(c_uint64),
        POINTER(POINTER(c_uint64)),
        POINTER(POINTER(c_float)),
        POINTER(c_char_p),
    ]
    dll.hiss_read.restype = c_int

    # hcsd_write(path, nside, nested, n_pix, ipix, pixel, meta_json) -> int
    dll.hcsd_write.argtypes = [
        c_char_p, c_uint32, c_int, c_uint64,
        POINTER(c_uint64), POINTER(c_float), c_char_p,
    ]
    dll.hcsd_write.restype = c_int

    # hcsd_read(path, nside*, nested*, n_pix*, ipix**, pixel**, meta_json**) -> int
    dll.hcsd_read.argtypes = [
        c_char_p,
        POINTER(c_uint32), POINTER(c_int), POINTER(c_uint64),
        POINTER(POINTER(c_uint64)),
        POINTER(POINTER(c_float)),
        POINTER(c_char_p),
    ]
    dll.hcsd_read.restype = c_int

    # hcsd_read_leaf(path, leaf_ipix_at_nside64, n_pix*, ipix**, pixel**) -> int
    dll.hcsd_read_leaf.argtypes = [
        c_char_p, c_uint64,
        POINTER(c_uint64),
        POINTER(POINTER(c_uint64)),
        POINTER(POINTER(c_float)),
    ]
    dll.hcsd_read_leaf.restype = c_int

    # hio_free(ptr) -> void
    dll.hio_free.argtypes = [c_void_p]
    dll.hio_free.restype = None


# 模块级 DLL 单例（懒加载）
_dll: Optional[CDLL] = None


def _get_dll() -> CDLL:
    """获取模块级 DLL 单例"""
    global _dll
    if _dll is None:
        _dll = _load_dll()
    return _dll


def _free_ptr(ptr):
    """释放 DLL 分配的内存（安全处理 null 指针）

    用 cast 转换为 c_void_p，兼容 c_char_p / POINTER 等指针类型。
    """
    if ptr:
        _get_dll().hio_free(cast(ptr, c_void_p))


# ============================================================================
# 内部辅助函数
# ============================================================================

def _encode_path(path: str) -> bytes:
    """路径转 UTF-8 字节"""
    return path.encode("utf-8")


def _encode_meta(meta: dict) -> bytes:
    """元数据字典转 JSON UTF-8 字节"""
    return json.dumps(meta, ensure_ascii=False).encode("utf-8")


def _decode_meta(meta_ptr) -> dict:
    """解码 C 返回的 meta_json 字符串为 dict"""
    if not meta_ptr or not meta_ptr.value:
        return {}
    raw = meta_ptr.value.decode("utf-8")
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        # C 侧可能返回非标准 JSON，返回原始字符串
        logger.warning("meta_json 解析失败，返回原始字符串: %s", raw)
        return {"_raw": raw}


def _prep_ipix(ipix: np.ndarray) -> np.ndarray:
    """准备 ipix 数组（uint64, contiguous）"""
    arr = np.ascontiguousarray(ipix, dtype=np.uint64)
    if arr.ndim != 1:
        raise ValueError(f"ipix 必须为 1D 数组，实际为 {arr.ndim}D")
    return arr


def _prep_pixel(pixel: np.ndarray) -> np.ndarray:
    """准备 pixel 数组（float32, contiguous）"""
    arr = np.ascontiguousarray(pixel, dtype=np.float32)
    if arr.ndim != 1:
        raise ValueError(f"pixel 必须为 1D 数组，实际为 {arr.ndim}D")
    return arr


# ============================================================================
# HissWriter
# ============================================================================

class HissWriter:
    """ .hiss 文件写入器 """

    def __init__(self, path: str, nside: int, nested: bool):
        """
        Args:
            path: 输出文件路径
            nside: HEALPix nside 参数
            nested: 是否为 nested 排序
        """
        self._path = path
        self._nside = int(nside)
        self._nested = 1 if nested else 0
        self._dll = _get_dll()

    def write(self, ipix: np.ndarray, pixel: np.ndarray, meta: dict) -> int:
        """ 写入像素数据和元数据

        Args:
            ipix: uint64 数组 [n_pix]
            pixel: float32 数组 [n_pix]
            meta: 元数据字典（会序列化为 JSON）

        Returns:
            int: 0=成功，<0=失败

        Raises:
            RuntimeError: DLL 调用失败
        """
        ipix_arr = _prep_ipix(ipix)
        pixel_arr = _prep_pixel(pixel)
        if ipix_arr.size != pixel_arr.size:
            raise ValueError(
                f"ipix 和 pixel 长度不一致: {ipix_arr.size} != {pixel_arr.size}")
        n_pix = ipix_arr.size

        meta_bytes = _encode_meta(meta)
        path_bytes = _encode_path(self._path)

        # 空数据时传 null 指针
        ipix_ptr = ipix_arr.ctypes.data_as(POINTER(c_uint64)) if n_pix > 0 else None
        pixel_ptr = pixel_arr.ctypes.data_as(POINTER(c_float)) if n_pix > 0 else None

        logger.info("hiss_write: path=%s, nside=%d, nested=%d, n_pix=%d",
                    self._path, self._nside, self._nested, n_pix)

        ret = self._dll.hiss_write(
            path_bytes, self._nside, self._nested, n_pix,
            ipix_ptr, pixel_ptr, meta_bytes)

        if ret != 0:
            raise RuntimeError(f"hiss_write 失败，返回码={ret}")
        logger.info("hiss_write 成功: %s", self._path)
        return ret


# ============================================================================
# HissReader
# ============================================================================

class HissReader:
    """ .hiss 文件读取器

    注意：C 侧 malloc 分配的 ipix/pixel 内存通过 np.ctypeslib.as_array
    零拷贝访问。Reader 对象销毁时会调用 hio_free 释放 C 内存，
    之后访问 ipix/pixel 属性将失效。建议使用 with 语句或保持 Reader 引用。
    """

    def __init__(self, path: str):
        """
        Args:
            path: .hiss 文件路径

        Raises:
            RuntimeError: DLL 调用失败
        """
        self._path = path
        self._dll = _get_dll()

        # 输出参数
        nside = c_uint32(0)
        nested = c_int(0)
        n_pix = c_uint64(0)
        ipix_ptr = POINTER(c_uint64)()
        pixel_ptr = POINTER(c_float)()
        meta_ptr = c_char_p()

        path_bytes = _encode_path(path)
        logger.info("hiss_read: path=%s", path)

        ret = self._dll.hiss_read(
            path_bytes,
            byref(nside), byref(nested), byref(n_pix),
            byref(ipix_ptr), byref(pixel_ptr), byref(meta_ptr))

        if ret != 0:
            raise RuntimeError(f"hiss_read 失败，返回码={ret}")

        # 保存基本字段
        self._nside = int(nside.value)
        self._nested = bool(nested.value)
        self._n_pix = int(n_pix.value)

        # 解析 meta_json（立即解码并释放 C 字符串）
        self._meta = _decode_meta(meta_ptr)
        _free_ptr(meta_ptr)

        # 保存 C 指针（延迟创建 numpy 视图）
        self._ipix_ptr = ipix_ptr
        self._pixel_ptr = pixel_ptr
        self._closed = False

        logger.info("hiss_read 成功: nside=%d, nested=%d, n_pix=%d",
                    self._nside, self._nested, self._n_pix)

    @property
    def nside(self) -> int:
        return self._nside

    @property
    def nested(self) -> bool:
        return self._nested

    @property
    def n_pix(self) -> int:
        return self._n_pix

    @property
    def ipix(self) -> np.ndarray:
        """ uint64 数组 [n_pix]（零拷贝视图，C 内存由 Reader 管理）"""
        if self._closed:
            raise RuntimeError("Reader 已关闭，数据不可访问")
        if self._n_pix == 0:
            return np.empty(0, dtype=np.uint64)
        return np.ctypeslib.as_array(self._ipix_ptr, shape=(self._n_pix,))

    @property
    def pixel(self) -> np.ndarray:
        """ float32 数组 [n_pix]（零拷贝视图，C 内存由 Reader 管理）"""
        if self._closed:
            raise RuntimeError("Reader 已关闭，数据不可访问")
        if self._n_pix == 0:
            return np.empty(0, dtype=np.float32)
        return np.ctypeslib.as_array(self._pixel_ptr, shape=(self._n_pix,))

    @property
    def meta(self) -> dict:
        """ JSON 解析后的字典 """
        return self._meta

    def close(self):
        """释放 C 侧分配的内存"""
        if not self._closed:
            _free_ptr(self._ipix_ptr)
            _free_ptr(self._pixel_ptr)
            self._closed = True
            logger.info("HissReader 内存已释放: %s", self._path)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


# ============================================================================
# HcsdWriter
# ============================================================================

class HcsdWriter:
    """ .hcsd 文件写入器（含子叶块索引构建）

    C 侧会按 leaf_ipix + ipix 升序排序后写入，保证子叶数据连续存储。
    """

    def __init__(self, path: str, nside: int, nested: bool):
        """
        Args:
            path: 输出文件路径
            nside: HEALPix nside 参数
            nested: 是否为 nested 排序
        """
        self._path = path
        self._nside = int(nside)
        self._nested = 1 if nested else 0
        self._dll = _get_dll()

    def write(self, ipix: np.ndarray, pixel: np.ndarray, meta: dict) -> int:
        """ 写入像素数据和元数据（C 侧会按 leaf_ipix + ipix 排序）

        Args:
            ipix: uint64 数组 [n_pix]
            pixel: float32 数组 [n_pix]
            meta: 元数据字典（会序列化为 JSON）

        Returns:
            int: 0=成功，<0=失败

        Raises:
            RuntimeError: DLL 调用失败
        """
        ipix_arr = _prep_ipix(ipix)
        pixel_arr = _prep_pixel(pixel)
        if ipix_arr.size != pixel_arr.size:
            raise ValueError(
                f"ipix 和 pixel 长度不一致: {ipix_arr.size} != {pixel_arr.size}")
        n_pix = ipix_arr.size

        meta_bytes = _encode_meta(meta)
        path_bytes = _encode_path(self._path)

        ipix_ptr = ipix_arr.ctypes.data_as(POINTER(c_uint64)) if n_pix > 0 else None
        pixel_ptr = pixel_arr.ctypes.data_as(POINTER(c_float)) if n_pix > 0 else None

        logger.info("hcsd_write: path=%s, nside=%d, nested=%d, n_pix=%d",
                    self._path, self._nside, self._nested, n_pix)

        ret = self._dll.hcsd_write(
            path_bytes, self._nside, self._nested, n_pix,
            ipix_ptr, pixel_ptr, meta_bytes)

        if ret != 0:
            raise RuntimeError(f"hcsd_write 失败，返回码={ret}")
        logger.info("hcsd_write 成功: %s", self._path)
        return ret


# ============================================================================
# HcsdReader
# ============================================================================

class HcsdReader:
    """ .hcsd 文件读取器（支持全量读取和按子叶读取）

    全量读取的 ipix/pixel 内存由 Reader 管理（同 HissReader）。
    read_leaf 返回的数组是独立拷贝（C 内存已立即释放）。
    """

    def __init__(self, path: str):
        """
        Args:
            path: .hcsd 文件路径

        Raises:
            RuntimeError: DLL 调用失败
        """
        self._path = path
        self._dll = _get_dll()

        nside = c_uint32(0)
        nested = c_int(0)
        n_pix = c_uint64(0)
        ipix_ptr = POINTER(c_uint64)()
        pixel_ptr = POINTER(c_float)()
        meta_ptr = c_char_p()

        path_bytes = _encode_path(path)
        logger.info("hcsd_read: path=%s", path)

        ret = self._dll.hcsd_read(
            path_bytes,
            byref(nside), byref(nested), byref(n_pix),
            byref(ipix_ptr), byref(pixel_ptr), byref(meta_ptr))

        if ret != 0:
            raise RuntimeError(f"hcsd_read 失败，返回码={ret}")

        self._nside = int(nside.value)
        self._nested = bool(nested.value)
        self._n_pix = int(n_pix.value)

        self._meta = _decode_meta(meta_ptr)
        _free_ptr(meta_ptr)

        self._ipix_ptr = ipix_ptr
        self._pixel_ptr = pixel_ptr
        self._closed = False

        logger.info("hcsd_read 成功: nside=%d, nested=%d, n_pix=%d",
                    self._nside, self._nested, self._n_pix)

    @property
    def nside(self) -> int:
        return self._nside

    @property
    def nested(self) -> bool:
        return self._nested

    @property
    def n_pix(self) -> int:
        return self._n_pix

    @property
    def ipix(self) -> np.ndarray:
        """ uint64 数组 [n_pix]（零拷贝视图，C 内存由 Reader 管理）"""
        if self._closed:
            raise RuntimeError("Reader 已关闭，数据不可访问")
        if self._n_pix == 0:
            return np.empty(0, dtype=np.uint64)
        return np.ctypeslib.as_array(self._ipix_ptr, shape=(self._n_pix,))

    @property
    def pixel(self) -> np.ndarray:
        """ float32 数组 [n_pix]（零拷贝视图，C 内存由 Reader 管理）"""
        if self._closed:
            raise RuntimeError("Reader 已关闭，数据不可访问")
        if self._n_pix == 0:
            return np.empty(0, dtype=np.float32)
        return np.ctypeslib.as_array(self._pixel_ptr, shape=(self._n_pix,))

    @property
    def meta(self) -> dict:
        """ JSON 解析后的字典 """
        return self._meta

    def read_leaf(self, leaf_ipix_at_nside64: int) -> Tuple[np.ndarray, np.ndarray]:
        """ 按需读取指定子叶的数据

        Args:
            leaf_ipix_at_nside64: nside=64 层的子叶 ipix

        Returns:
            (ipix, pixel) 元组，均为 numpy 数组（独立拷贝，C 内存已释放）

        Raises:
            RuntimeError: DLL 调用失败
        """
        if self._closed:
            raise RuntimeError("Reader 已关闭")

        leaf_n_pix = c_uint64(0)
        leaf_ipix_ptr = POINTER(c_uint64)()
        leaf_pixel_ptr = POINTER(c_float)()

        path_bytes = _encode_path(self._path)
        logger.info("hcsd_read_leaf: path=%s, leaf_ipix=%d",
                    self._path, leaf_ipix_at_nside64)

        ret = self._dll.hcsd_read_leaf(
            path_bytes, c_uint64(leaf_ipix_at_nside64),
            byref(leaf_n_pix),
            byref(leaf_ipix_ptr), byref(leaf_pixel_ptr))

        if ret != 0:
            raise RuntimeError(
                f"hcsd_read_leaf 失败，返回码={ret}, leaf_ipix={leaf_ipix_at_nside64}")

        n = int(leaf_n_pix.value)
        logger.info("hcsd_read_leaf 成功: leaf_ipix=%d, n_pix=%d",
                    leaf_ipix_at_nside64, n)

        if n == 0:
            _free_ptr(leaf_ipix_ptr)
            _free_ptr(leaf_pixel_ptr)
            return np.empty(0, dtype=np.uint64), np.empty(0, dtype=np.float32)

        # 零拷贝视图，然后拷贝为独立数组（因为要立即释放 C 内存）
        ipix_view = np.ctypeslib.as_array(leaf_ipix_ptr, shape=(n,))
        pixel_view = np.ctypeslib.as_array(leaf_pixel_ptr, shape=(n,))
        ipix_out = ipix_view.copy()
        pixel_out = pixel_view.copy()

        _free_ptr(leaf_ipix_ptr)
        _free_ptr(leaf_pixel_ptr)

        return ipix_out, pixel_out

    def close(self):
        """释放全量读取的 C 内存"""
        if not self._closed:
            _free_ptr(self._ipix_ptr)
            _free_ptr(self._pixel_ptr)
            self._closed = True
            logger.info("HcsdReader 内存已释放: %s", self._path)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


# ============================================================================
# 便捷函数
# ============================================================================

def hiss_write(path: str, nside: int, nested: bool,
               ipix: np.ndarray, pixel: np.ndarray, meta: dict) -> int:
    """ 便捷写入 .hiss 文件

    Args:
        path: 输出文件路径
        nside: HEALPix nside 参数
        nested: 是否为 nested 排序
        ipix: uint64 数组 [n_pix]
        pixel: float32 数组 [n_pix]
        meta: 元数据字典

    Returns:
        int: 0=成功
    """
    writer = HissWriter(path, nside, nested)
    return writer.write(ipix, pixel, meta)


def hiss_read(path: str) -> Tuple[int, bool, np.ndarray, np.ndarray, dict]:
    """ 便捷读取 .hiss 文件

    Args:
        path: .hiss 文件路径

    Returns:
        (nside, nested, ipix, pixel, meta) 元组
        ipix/pixel 为独立拷贝的 numpy 数组（C 内存已释放）

    Raises:
        RuntimeError: DLL 调用失败
    """
    with HissReader(path) as reader:
        # 拷贝数组，确保 Reader 关闭后数据仍可用
        ipix = reader.ipix.copy() if reader.n_pix > 0 else np.empty(0, dtype=np.uint64)
        pixel = reader.pixel.copy() if reader.n_pix > 0 else np.empty(0, dtype=np.float32)
        return reader.nside, reader.nested, ipix, pixel, reader.meta


def hcsd_write(path: str, nside: int, nested: bool,
               ipix: np.ndarray, pixel: np.ndarray, meta: dict) -> int:
    """ 便捷写入 .hcsd 文件

    Args:
        path: 输出文件路径
        nside: HEALPix nside 参数
        nested: 是否为 nested 排序
        ipix: uint64 数组 [n_pix]（C 侧会按 leaf+ipix 排序）
        pixel: float32 数组 [n_pix]
        meta: 元数据字典

    Returns:
        int: 0=成功
    """
    writer = HcsdWriter(path, nside, nested)
    return writer.write(ipix, pixel, meta)


def hcsd_read(path: str) -> Tuple[int, bool, np.ndarray, np.ndarray, dict]:
    """ 便捷读取 .hcsd 文件

    Args:
        path: .hcsd 文件路径

    Returns:
        (nside, nested, ipix, pixel, meta) 元组
        ipix/pixel 为独立拷贝的 numpy 数组（C 内存已释放）

    Raises:
        RuntimeError: DLL 调用失败
    """
    with HcsdReader(path) as reader:
        ipix = reader.ipix.copy() if reader.n_pix > 0 else np.empty(0, dtype=np.uint64)
        pixel = reader.pixel.copy() if reader.n_pix > 0 else np.empty(0, dtype=np.float32)
        return reader.nside, reader.nested, ipix, pixel, reader.meta


def hcsd_read_leaf(path: str, leaf_ipix_at_nside64: int) -> Tuple[np.ndarray, np.ndarray]:
    """ 按需读取 .hcsd 文件中指定子叶

    Args:
        path: .hcsd 文件路径
        leaf_ipix_at_nside64: nside=64 层的子叶 ipix

    Returns:
        (ipix, pixel) 元组，均为 numpy 数组（独立拷贝，C 内存已释放）

    Raises:
        RuntimeError: DLL 调用失败
    """
    dll = _get_dll()

    leaf_n_pix = c_uint64(0)
    leaf_ipix_ptr = POINTER(c_uint64)()
    leaf_pixel_ptr = POINTER(c_float)()

    path_bytes = _encode_path(path)
    logger.info("hcsd_read_leaf: path=%s, leaf_ipix=%d", path, leaf_ipix_at_nside64)

    ret = dll.hcsd_read_leaf(
        path_bytes, c_uint64(leaf_ipix_at_nside64),
        byref(leaf_n_pix),
        byref(leaf_ipix_ptr), byref(leaf_pixel_ptr))

    if ret != 0:
        raise RuntimeError(
            f"hcsd_read_leaf 失败，返回码={ret}, leaf_ipix={leaf_ipix_at_nside64}")

    n = int(leaf_n_pix.value)
    logger.info("hcsd_read_leaf 成功: leaf_ipix=%d, n_pix=%d",
                leaf_ipix_at_nside64, n)

    if n == 0:
        _free_ptr(leaf_ipix_ptr)
        _free_ptr(leaf_pixel_ptr)
        return np.empty(0, dtype=np.uint64), np.empty(0, dtype=np.float32)

    ipix_view = np.ctypeslib.as_array(leaf_ipix_ptr, shape=(n,))
    pixel_view = np.ctypeslib.as_array(leaf_pixel_ptr, shape=(n,))
    ipix_out = ipix_view.copy()
    pixel_out = pixel_view.copy()

    _free_ptr(leaf_ipix_ptr)
    _free_ptr(leaf_pixel_ptr)

    return ipix_out, pixel_out


# ============================================================================
# 模块自测
# ============================================================================

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
    print("=" * 60)
    print("healpix_io.py 模块自测（DLL 加载验证）")
    print("=" * 60)
    try:
        dll = _get_dll()
        print(f"[OK] DLL 加载成功: {_find_dll()}")
        print("[OK] 6 个函数已绑定: hiss_write/hiss_read/hcsd_write/"
              "hcsd_read/hcsd_read_leaf/hio_free")
    except Exception as e:
        print(f"[FAIL] {e}")
        import sys
        sys.exit(1)
