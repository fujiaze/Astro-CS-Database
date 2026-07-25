#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_hiss_hcsd_roundtrip.py - HISS/HCSD 格式 round-trip 测试

任务: P01-003 HISS/HCSD 格式版本与 round-trip
合约: engineering/contracts/hiss_format_v1.md, hcsd_format_v1.md
依赖: lib/astro_image_io/astro_image_io.dll (导出 aio_hiss_*/aio_hcsd_* 函数)

测试流程:
  1. 读取一个 HISS/HCSD 文件，记录所有字段（nside, nested, n_pix, ipix, pixel, snr, meta）
  2. 重新写入一个副本 HISS/HCSD 文件
  3. 重新读取副本，验证所有字段一致
  4. 输出结构化测试报告

退出码:
  0 = 所有测试通过
  1 = 至少一个测试失败
  2 = 环境错误（DLL 加载失败等）

用法:
  python test_hiss_hcsd_roundtrip.py [--output-dir DIR] [FILE...]

  --output-dir DIR   副本文件与报告输出目录（默认: engineering/evidence/P01-003/roundtrip_output）
  FILE...            要测试的 HISS/HCSD 文件路径（默认: 4 个真实数据文件）

注意:
  - 本脚本不修改原文件，仅在输出目录创建副本
  - HCSD 测试会额外验证按子叶读取（aio_hcsd_read_leaf）
  - 报告以 JSON 格式输出到 stdout，人类可读文本输出到 stderr
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import struct
import sys
import time
from dataclasses import dataclass, field, asdict
from typing import Any, Dict, List, Optional, Tuple

# ============================================================================
# 配置
# ============================================================================

# 项目根目录（脚本位于 engineering/tools/）
PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# astro_image_io.dll 路径
AIO_DLL_PATH = os.path.join(PROJECT_ROOT, "lib", "astro_image_io", "astro_image_io.dll")

# mingw64 运行时目录（DLL 依赖）
MINGW_BIN = r"C:\msys64\mingw64\bin"

# 默认测试文件
DEFAULT_HISS_FILES = [
    os.path.join(PROJECT_ROOT, "engineering", "evidence", "P00-003", "output", "stage1_baseline.hiss"),
    os.path.join(PROJECT_ROOT, "lib", "orchestrator", "cpp", "output_hiss_dir", "frame1.hiss"),
    os.path.join(PROJECT_ROOT, "lib", "orchestrator", "cpp", "output_hiss_dir", "frame2.hiss"),
]
DEFAULT_HCSD_FILES = [
    os.path.join(PROJECT_ROOT, "engineering", "evidence", "P00-003", "output", "stage2_baseline.hcsd"),
]

# 默认输出目录
DEFAULT_OUTPUT_DIR = os.path.join(
    PROJECT_ROOT, "engineering", "evidence", "P01-003", "roundtrip_output")

# HISS/HCSD Magic
HISS_MAGIC = b"HISS"
HCSD_MAGIC = b"HCSD"

# 错误码（与 C 侧一致）
HIO_OK = 0
HIO_ERR_PARAM = -1
HIO_ERR_FILE = -2
HIO_ERR_MAGIC = -3
HIO_ERR_ZSTD = -4
HIO_ERR_JSON = -5
HIO_ERR_MEM = -6
HIO_ERR_BOUNDS = -7

ERROR_NAMES = {
    0: "OK", -1: "PARAM", -2: "FILE", -3: "MAGIC",
    -4: "ZSTD", -5: "JSON", -6: "MEM", -7: "BOUNDS",
}


# ============================================================================
# C 结构体定义（对应 aio_healpix_io.h）
# ============================================================================

class HioSnrControlPoint(ctypes.Structure):
    """SNR 控制点 (20 字节, pack=1)"""
    _pack_ = 1
    _fields_ = [
        ("ra", ctypes.c_double),       # 球面赤经 (度)
        ("dec", ctypes.c_double),      # 球面赤纬 (度)
        ("snr_psf", ctypes.c_float),   # (A-B)/mad
    ]


class HioSnrModel(ctypes.Structure):
    """SNR 稀疏控制点模型"""
    _fields_ = [
        ("n_points", ctypes.c_uint32),
        ("points", ctypes.POINTER(HioSnrControlPoint)),
        ("snr_phot", ctypes.c_double),
        ("median_snr", ctypes.c_double),
        ("idw_power", ctypes.c_double),
    ]


# ============================================================================
# DLL 加载与函数签名绑定
# ============================================================================

def _setup_path():
    """配置 DLL 搜索路径"""
    if os.path.isdir(MINGW_BIN):
        os.environ["PATH"] = MINGW_BIN + ";" + os.environ.get("PATH", "")
        try:
            os.add_dll_directory(MINGW_BIN)
        except OSError:
            pass
    aio_dir = os.path.dirname(AIO_DLL_PATH)
    if os.path.isdir(aio_dir):
        try:
            os.add_dll_directory(aio_dir)
        except OSError:
            pass


def load_aio_dll(dll_path: str = AIO_DLL_PATH) -> ctypes.CDLL:
    """加载 astro_image_io.dll 并绑定 HISS/HCSD 函数签名"""
    if not os.path.isfile(dll_path):
        raise FileNotFoundError(f"astro_image_io.dll 未找到: {dll_path}")
    _setup_path()
    dll = ctypes.CDLL(dll_path)

    # aio_hiss_write(path, nside, nested, n_pix, ipix, pixel, snr, meta_json) -> int
    dll.aio_hiss_write.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),  # snr (可为 None)
        ctypes.c_char_p,
    ]
    dll.aio_hiss_write.restype = ctypes.c_int

    # aio_hiss_read(path, nside*, nested*, n_pix*, ipix**, pixel**, snr**, meta_json**) -> int
    dll.aio_hiss_read.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),  # snr**
        ctypes.POINTER(ctypes.c_char_p),
    ]
    dll.aio_hiss_read.restype = ctypes.c_int

    # aio_hiss_write_snr_model(path, nside, nested, n_pix, ipix, pixel, snr_model*, meta_json) -> int
    dll.aio_hiss_write_snr_model.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(HioSnrModel),
        ctypes.c_char_p,
    ]
    dll.aio_hiss_write_snr_model.restype = ctypes.c_int

    # aio_hiss_read_snr_model(path, nside*, nested*, n_pix*, ipix**, pixel**, snr_model**, meta_json**) -> int
    dll.aio_hiss_read_snr_model.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
        ctypes.POINTER(ctypes.POINTER(HioSnrModel)),
        ctypes.POINTER(ctypes.c_char_p),
    ]
    dll.aio_hiss_read_snr_model.restype = ctypes.c_int

    # aio_hio_free_snr_model(snr_model*) -> void
    dll.aio_hio_free_snr_model.argtypes = [ctypes.POINTER(HioSnrModel)]
    dll.aio_hio_free_snr_model.restype = None

    # aio_hcsd_write(path, nside, nested, n_pix, ipix, pixel, meta_json) -> int
    dll.aio_hcsd_write.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_float),
        ctypes.c_char_p,
    ]
    dll.aio_hcsd_write.restype = ctypes.c_int

    # aio_hcsd_read(path, nside*, nested*, n_pix*, ipix**, pixel**, meta_json**) -> int
    dll.aio_hcsd_read.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
        ctypes.POINTER(ctypes.c_char_p),
    ]
    dll.aio_hcsd_read.restype = ctypes.c_int

    # aio_hcsd_read_leaf(path, leaf_ipix_at_nside64, n_pix*, ipix**, pixel**) -> int
    dll.aio_hcsd_read_leaf.argtypes = [
        ctypes.c_char_p, ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ]
    dll.aio_hcsd_read_leaf.restype = ctypes.c_int

    # aio_hio_free(ptr) -> void
    dll.aio_hio_free.argtypes = [ctypes.c_void_p]
    dll.aio_hio_free.restype = None

    return dll


# ============================================================================
# 数据类
# ============================================================================

@dataclass
class HissData:
    """HISS 文件完整数据"""
    nside: int
    nested: bool
    n_pix: int
    ipix: List[int]
    pixel: List[float]
    snr: Optional[List[float]] = None  # snr_format=0
    snr_model: Optional[Dict[str, Any]] = None  # snr_format=1
    meta: Dict[str, Any] = field(default_factory=dict)
    has_snr: bool = False
    snr_format: int = 0
    snr_n_points: int = 0


@dataclass
class HcsdData:
    """HCSD 文件完整数据"""
    nside: int
    nested: bool
    n_pix: int
    ipix: List[int]
    pixel: List[float]
    meta: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TestResult:
    """单个文件的测试结果"""
    file_path: str
    file_type: str  # "HISS" or "HCSD"
    file_size: int
    file_sha256: str
    success: bool
    error: Optional[str] = None
    # 读取统计
    read_nside: Optional[int] = None
    read_nested: Optional[bool] = None
    read_n_pix: Optional[int] = None
    read_has_snr: Optional[bool] = None
    read_snr_format: Optional[int] = None
    read_meta_keys: Optional[List[str]] = None
    # 副本统计
    copy_path: Optional[str] = None
    copy_size: Optional[int] = None
    copy_sha256: Optional[str] = None
    # 验证结果
    json_header_match: Optional[bool] = None
    ipix_match: Optional[bool] = None
    pixel_match: Optional[bool] = None
    snr_match: Optional[bool] = None
    snr_model_match: Optional[bool] = None
    file_size_match: Optional[bool] = None
    # HCSD 专属
    leaf_read_match: Optional[bool] = None
    # 耗时
    read_ms: float = 0.0
    write_ms: float = 0.0
    reread_ms: float = 0.0


# ============================================================================
# HISS 读取/写入（Python 包装 C API）
# ============================================================================

def _decode_meta(meta_ptr) -> Dict[str, Any]:
    """解码 C 返回的 meta_json 字符串为 dict"""
    if not meta_ptr or not meta_ptr.value:
        return {}
    raw = meta_ptr.value.decode("utf-8")
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        return {"_raw": raw}


def _free_ptr(dll, ptr):
    """安全释放 DLL 分配的内存"""
    if ptr:
        dll.aio_hio_free(ctypes.cast(ptr, ctypes.c_void_p))


def _parse_snr_format_from_meta(meta: Dict[str, Any]) -> Tuple[bool, int, int]:
    """从 meta 中解析 has_snr, snr_format, snr_n_points"""
    has_snr = bool(meta.get("has_snr", False))
    snr_format = int(meta.get("snr_format", 0)) if has_snr else 0
    snr_n_points = int(meta.get("snr_n_points", 0)) if snr_format == 1 else 0
    return has_snr, snr_format, snr_n_points


def hiss_read(dll: ctypes.CDLL, path: str) -> HissData:
    """读取 HISS 文件，返回完整数据"""
    nside = ctypes.c_uint32(0)
    nested = ctypes.c_int(0)
    n_pix = ctypes.c_uint64(0)
    ipix_ptr = ctypes.POINTER(ctypes.c_uint64)()
    pixel_ptr = ctypes.POINTER(ctypes.c_float)()
    snr_ptr = ctypes.POINTER(ctypes.c_float)()
    meta_ptr = ctypes.c_char_p()

    ret = dll.aio_hiss_read(
        path.encode("utf-8"),
        ctypes.byref(nside), ctypes.byref(nested), ctypes.byref(n_pix),
        ctypes.byref(ipix_ptr), ctypes.byref(pixel_ptr),
        ctypes.byref(snr_ptr),
        ctypes.byref(meta_ptr))
    if ret != HIO_OK:
        raise RuntimeError(f"aio_hiss_read 失败 code={ret} ({ERROR_NAMES.get(ret, '?')}): {path}")

    n = int(n_pix.value)
    meta = _decode_meta(meta_ptr)
    _free_ptr(dll, meta_ptr)

    # 拷贝 ipix/pixel
    if n > 0:
        ipix_list = [int(ipix_ptr[i]) for i in range(n)]
        pixel_list = [float(pixel_ptr[i]) for i in range(n)]
    else:
        ipix_list = []
        pixel_list = []

    # 拷贝 snr（仅 snr_format=0 时 snr_ptr 非 null）
    snr_list: Optional[List[float]] = None
    if snr_ptr and n > 0:
        snr_list = [float(snr_ptr[i]) for i in range(n)]

    _free_ptr(dll, ipix_ptr)
    _free_ptr(dll, pixel_ptr)
    _free_ptr(dll, snr_ptr)

    has_snr, snr_format, snr_n_points = _parse_snr_format_from_meta(meta)

    return HissData(
        nside=int(nside.value),
        nested=bool(nested.value),
        n_pix=n,
        ipix=ipix_list,
        pixel=pixel_list,
        snr=snr_list,
        snr_model=None,
        meta=meta,
        has_snr=has_snr,
        snr_format=snr_format,
        snr_n_points=snr_n_points,
    )


def hiss_read_snr_model(dll: ctypes.CDLL, path: str) -> HissData:
    """读取 HISS 文件，含稀疏 SNR 模型（snr_format=1）"""
    nside = ctypes.c_uint32(0)
    nested = ctypes.c_int(0)
    n_pix = ctypes.c_uint64(0)
    ipix_ptr = ctypes.POINTER(ctypes.c_uint64)()
    pixel_ptr = ctypes.POINTER(ctypes.c_float)()
    model_ptr = ctypes.POINTER(HioSnrModel)()
    meta_ptr = ctypes.c_char_p()

    ret = dll.aio_hiss_read_snr_model(
        path.encode("utf-8"),
        ctypes.byref(nside), ctypes.byref(nested), ctypes.byref(n_pix),
        ctypes.byref(ipix_ptr), ctypes.byref(pixel_ptr),
        ctypes.byref(model_ptr),
        ctypes.byref(meta_ptr))
    if ret != HIO_OK:
        raise RuntimeError(
            f"aio_hiss_read_snr_model 失败 code={ret} ({ERROR_NAMES.get(ret, '?')}): {path}")

    n = int(n_pix.value)
    meta = _decode_meta(meta_ptr)
    _free_ptr(dll, meta_ptr)

    if n > 0:
        ipix_list = [int(ipix_ptr[i]) for i in range(n)]
        pixel_list = [float(pixel_ptr[i]) for i in range(n)]
    else:
        ipix_list = []
        pixel_list = []

    _free_ptr(dll, ipix_ptr)
    _free_ptr(dll, pixel_ptr)

    snr_model_dict: Optional[Dict[str, Any]] = None
    if model_ptr:
        m = model_ptr.contents
        n_pts = int(m.n_points)
        pts = []
        for i in range(n_pts):
            cp = m.points[i]
            pts.append({
                "ra": float(cp.ra),
                "dec": float(cp.dec),
                "snr_psf": float(cp.snr_psf),
            })
        snr_model_dict = {
            "n_points": n_pts,
            "points": pts,
            "snr_phot": float(m.snr_phot),
            "median_snr": float(m.median_snr),
            "idw_power": float(m.idw_power),
        }
        dll.aio_hio_free_snr_model(model_ptr)

    has_snr, snr_format, snr_n_points = _parse_snr_format_from_meta(meta)

    return HissData(
        nside=int(nside.value),
        nested=bool(nested.value),
        n_pix=n,
        ipix=ipix_list,
        pixel=pixel_list,
        snr=None,
        snr_model=snr_model_dict,
        meta=meta,
        has_snr=has_snr,
        snr_format=snr_format,
        snr_n_points=snr_n_points,
    )


def hiss_write(dll: ctypes.CDLL, path: str, data: HissData):
    """写入 HISS 文件（根据 snr_format 选择写入函数）"""
    n = len(data.ipix)
    if n != len(data.pixel):
        raise ValueError(f"ipix/pixel 长度不一致: {n} != {len(data.pixel)}")

    # 准备 C 数组
    ipix_arr = (ctypes.c_uint64 * n)(*data.ipix) if n > 0 else None
    pixel_arr = (ctypes.c_float * n)(*data.pixel) if n > 0 else None

    meta_bytes = json.dumps(data.meta, ensure_ascii=False).encode("utf-8")

    if data.snr_format == 1 and data.snr_model is not None:
        # snr_format=1: 稀疏控制点
        model = HioSnrModel()
        n_pts = data.snr_model["n_points"]
        model.n_points = n_pts
        pts_arr = (HioSnrControlPoint * n_pts)()
        for i, p in enumerate(data.snr_model["points"]):
            pts_arr[i].ra = p["ra"]
            pts_arr[i].dec = p["dec"]
            pts_arr[i].snr_psf = p["snr_psf"]
        model.points = pts_arr
        model.snr_phot = data.snr_model["snr_phot"]
        model.median_snr = data.snr_model["median_snr"]
        model.idw_power = data.snr_model["idw_power"]
        # 必须保持 pts_arr 引用直到调用完成
        ret = dll.aio_hiss_write_snr_model(
            path.encode("utf-8"), data.nside, 1 if data.nested else 0, n,
            ipix_arr, pixel_arr, ctypes.byref(model), meta_bytes)
    else:
        # snr_format=0 或无 SNR
        snr_arr = None
        if data.snr is not None and n > 0:
            snr_arr = (ctypes.c_float * n)(*data.snr)
        ret = dll.aio_hiss_write(
            path.encode("utf-8"), data.nside, 1 if data.nested else 0, n,
            ipix_arr, pixel_arr, snr_arr, meta_bytes)

    if ret != HIO_OK:
        raise RuntimeError(
            f"aio_hiss_write 失败 code={ret} ({ERROR_NAMES.get(ret, '?')}): {path}")


def hcsd_read(dll: ctypes.CDLL, path: str) -> HcsdData:
    """读取 HCSD 文件（全量）"""
    nside = ctypes.c_uint32(0)
    nested = ctypes.c_int(0)
    n_pix = ctypes.c_uint64(0)
    ipix_ptr = ctypes.POINTER(ctypes.c_uint64)()
    pixel_ptr = ctypes.POINTER(ctypes.c_float)()
    meta_ptr = ctypes.c_char_p()

    ret = dll.aio_hcsd_read(
        path.encode("utf-8"),
        ctypes.byref(nside), ctypes.byref(nested), ctypes.byref(n_pix),
        ctypes.byref(ipix_ptr), ctypes.byref(pixel_ptr), ctypes.byref(meta_ptr))
    if ret != HIO_OK:
        raise RuntimeError(f"aio_hcsd_read 失败 code={ret} ({ERROR_NAMES.get(ret, '?')}): {path}")

    n = int(n_pix.value)
    meta = _decode_meta(meta_ptr)
    _free_ptr(dll, meta_ptr)

    if n > 0:
        ipix_list = [int(ipix_ptr[i]) for i in range(n)]
        pixel_list = [float(pixel_ptr[i]) for i in range(n)]
    else:
        ipix_list = []
        pixel_list = []

    _free_ptr(dll, ipix_ptr)
    _free_ptr(dll, pixel_ptr)

    return HcsdData(
        nside=int(nside.value),
        nested=bool(nested.value),
        n_pix=n,
        ipix=ipix_list,
        pixel=pixel_list,
        meta=meta,
    )


def hcsd_write(dll: ctypes.CDLL, path: str, data: HcsdData):
    """写入 HCSD 文件（内部会重新排序）"""
    n = len(data.ipix)
    if n != len(data.pixel):
        raise ValueError(f"ipix/pixel 长度不一致: {n} != {len(data.pixel)}")

    ipix_arr = (ctypes.c_uint64 * n)(*data.ipix) if n > 0 else None
    pixel_arr = (ctypes.c_float * n)(*data.pixel) if n > 0 else None
    meta_bytes = json.dumps(data.meta, ensure_ascii=False).encode("utf-8")

    ret = dll.aio_hcsd_write(
        path.encode("utf-8"), data.nside, 1 if data.nested else 0, n,
        ipix_arr, pixel_arr, meta_bytes)
    if ret != HIO_OK:
        raise RuntimeError(f"aio_hcsd_write 失败 code={ret} ({ERROR_NAMES.get(ret, '?')}): {path}")


def hcsd_read_leaf(dll: ctypes.CDLL, path: str, leaf_ipix: int) -> Tuple[List[int], List[float]]:
    """按子叶读取 HCSD"""
    n_pix = ctypes.c_uint64(0)
    ipix_ptr = ctypes.POINTER(ctypes.c_uint64)()
    pixel_ptr = ctypes.POINTER(ctypes.c_float)()

    ret = dll.aio_hcsd_read_leaf(
        path.encode("utf-8"), ctypes.c_uint64(leaf_ipix),
        ctypes.byref(n_pix), ctypes.byref(ipix_ptr), ctypes.byref(pixel_ptr))
    if ret != HIO_OK:
        raise RuntimeError(
            f"aio_hcsd_read_leaf 失败 code={ret} leaf={leaf_ipix}: {path}")

    n = int(n_pix.value)
    if n > 0:
        ipix_list = [int(ipix_ptr[i]) for i in range(n)]
        pixel_list = [float(pixel_ptr[i]) for i in range(n)]
    else:
        ipix_list = []
        pixel_list = []

    _free_ptr(dll, ipix_ptr)
    _free_ptr(dll, pixel_ptr)
    return ipix_list, pixel_list


# ============================================================================
# 辅助函数
# ============================================================================

def sha256_file(path: str) -> str:
    """计算文件 SHA-256"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def detect_file_type(path: str) -> str:
    """根据 magic 检测文件类型"""
    with open(path, "rb") as f:
        magic = f.read(4)
    if magic == HISS_MAGIC:
        return "HISS"
    elif magic == HCSD_MAGIC:
        return "HCSD"
    else:
        raise ValueError(f"未知 magic: {magic!r} (期望 HISS 或 HCSD): {path}")


def read_file_header_info(path: str) -> Dict[str, Any]:
    """读取文件头部信息（magic + uncomp/comp json len）"""
    with open(path, "rb") as f:
        magic = f.read(4)
        uncomp_len, comp_len = struct.unpack("<2I", f.read(8))
    return {
        "magic": magic.decode("ascii", errors="replace"),
        "uncomp_json_len": uncomp_len,
        "comp_json_len": comp_len,
    }


def floats_bitwise_equal(a: List[float], b: List[float]) -> bool:
    """float32 位级比较（NaN 视为相等当且仅当双方都是 NaN）"""
    if len(a) != len(b):
        return False
    for x, y in zip(a, b):
        # 用 struct 转为 float32 位模式比较
        bx = struct.pack("<f", x)
        by = struct.pack("<f", y)
        if bx != by:
            # NaN 处理：struct.pack 把 NaN 转为 0x7FC00000，所以同是 NaN 时位相同
            return False
    return True


# ============================================================================
# Round-trip 测试
# ============================================================================

def test_hiss_roundtrip(dll: ctypes.CDLL, file_path: str, output_dir: str) -> TestResult:
    """HISS round-trip 测试"""
    file_name = os.path.basename(file_path)
    copy_path = os.path.join(output_dir, file_name.replace(".hiss", ".roundtrip.hiss"))

    result = TestResult(
        file_path=file_path,
        file_type="HISS",
        file_size=os.path.getsize(file_path),
        file_sha256=sha256_file(file_path),
        success=False,
    )

    # 读取文件头信息（用于报告）
    hdr_info = read_file_header_info(file_path)
    result.read_meta_keys = [f"{hdr_info['magic']}/uncomp={hdr_info['uncomp_json_len']}/comp={hdr_info['comp_json_len']}"]

    try:
        # 1. 第一次读取（根据 snr_format 选择读取函数）
        # 先用 hiss_read 读一次（兼容格式 0/1），获取 meta 中的 snr_format
        t0 = time.perf_counter()
        data1 = hiss_read(dll, file_path)
        t1 = time.perf_counter()
        result.read_ms = (t1 - t0) * 1000
        result.read_nside = data1.nside
        result.read_nested = data1.nested
        result.read_n_pix = data1.n_pix
        result.read_has_snr = data1.has_snr
        result.read_snr_format = data1.snr_format

        # 若 snr_format=1，需要用 hiss_read_snr_model 重新读取以获取稀疏模型
        if data1.snr_format == 1:
            t0 = time.perf_counter()
            data1 = hiss_read_snr_model(dll, file_path)
            t1 = time.perf_counter()
            result.read_ms = (t1 - t0) * 1000

        # 2. 写入副本
        t0 = time.perf_counter()
        hiss_write(dll, copy_path, data1)
        t1 = time.perf_counter()
        result.write_ms = (t1 - t0) * 1000
        result.copy_path = copy_path
        result.copy_size = os.path.getsize(copy_path)
        result.copy_sha256 = sha256_file(copy_path)

        # 3. 第二次读取副本
        t0 = time.perf_counter()
        if data1.snr_format == 1:
            data2 = hiss_read_snr_model(dll, copy_path)
        else:
            data2 = hiss_read(dll, copy_path)
        t1 = time.perf_counter()
        result.reread_ms = (t1 - t0) * 1000

        # 4. 验证字段一致性
        # 4.1 JSON 头字段
        result.json_header_match = (
            data1.nside == data2.nside and
            data1.nested == data2.nested and
            data1.n_pix == data2.n_pix and
            data1.has_snr == data2.has_snr and
            data1.snr_format == data2.snr_format and
            data1.snr_n_points == data2.snr_n_points and
            data1.meta == data2.meta
        )

        # 4.2 ipix 数组
        result.ipix_match = (data1.ipix == data2.ipix)

        # 4.3 pixel 数组（位级比较）
        result.pixel_match = floats_bitwise_equal(data1.pixel, data2.pixel)

        # 4.4 SNR 通道
        if data1.snr_format == 0 and data1.has_snr:
            if data1.snr is not None and data2.snr is not None:
                result.snr_match = floats_bitwise_equal(data1.snr, data2.snr)
            else:
                result.snr_match = (data1.snr is None and data2.snr is None)
        elif data1.snr_format == 1 and data1.has_snr:
            if data1.snr_model is not None and data2.snr_model is not None:
                m1 = data1.snr_model
                m2 = data2.snr_model
                result.snr_model_match = (
                    m1["n_points"] == m2["n_points"] and
                    m1["points"] == m2["points"] and
                    m1["snr_phot"] == m2["snr_phot"] and
                    m1["median_snr"] == m2["median_snr"] and
                    m1["idw_power"] == m2["idw_power"]
                )
            else:
                result.snr_model_match = (data1.snr_model is None and data2.snr_model is None)
        else:
            # 无 SNR 通道
            result.snr_match = (data1.snr is None and data2.snr is None)
            result.snr_model_match = (data1.snr_model is None and data2.snr_model is None)

        # 4.5 文件大小（zstd 压缩可能不完全确定）
        result.file_size_match = (result.file_size == result.copy_size)

        # 总体成功判定
        all_match = (
            result.json_header_match and
            result.ipix_match and
            result.pixel_match and
            result.snr_match and
            result.snr_model_match
            # 注意：file_size_match 不计入成功判定（zstd 可能非确定性）
        )
        result.success = all_match

        if not result.success:
            failures = []
            if not result.json_header_match:
                failures.append("json_header")
            if not result.ipix_match:
                failures.append("ipix")
            if not result.pixel_match:
                failures.append("pixel")
            if not result.snr_match:
                failures.append("snr")
            if not result.snr_model_match:
                failures.append("snr_model")
            result.error = "字段不匹配: " + ",".join(failures)

    except Exception as e:
        result.error = f"{type(e).__name__}: {e}"

    return result


def test_hcsd_roundtrip(dll: ctypes.CDLL, file_path: str, output_dir: str) -> TestResult:
    """HCSD round-trip 测试（含按子叶读取验证）"""
    file_name = os.path.basename(file_path)
    copy_path = os.path.join(output_dir, file_name.replace(".hcsd", ".roundtrip.hcsd"))

    result = TestResult(
        file_path=file_path,
        file_type="HCSD",
        file_size=os.path.getsize(file_path),
        file_sha256=sha256_file(file_path),
        success=False,
    )

    hdr_info = read_file_header_info(file_path)
    result.read_meta_keys = [f"{hdr_info['magic']}/uncomp={hdr_info['uncomp_json_len']}/comp={hdr_info['comp_json_len']}"]

    try:
        # 1. 第一次读取
        t0 = time.perf_counter()
        data1 = hcsd_read(dll, file_path)
        t1 = time.perf_counter()
        result.read_ms = (t1 - t0) * 1000
        result.read_nside = data1.nside
        result.read_nested = data1.nested
        result.read_n_pix = data1.n_pix
        result.read_has_snr = bool(data1.meta.get("has_snr", False))
        result.read_snr_format = int(data1.meta.get("snr_format", 0))

        # 2. 写入副本
        t0 = time.perf_counter()
        hcsd_write(dll, copy_path, data1)
        t1 = time.perf_counter()
        result.write_ms = (t1 - t0) * 1000
        result.copy_path = copy_path
        result.copy_size = os.path.getsize(copy_path)
        result.copy_sha256 = sha256_file(copy_path)

        # 3. 第二次读取副本
        t0 = time.perf_counter()
        data2 = hcsd_read(dll, copy_path)
        t1 = time.perf_counter()
        result.reread_ms = (t1 - t0) * 1000

        # 4. 验证字段一致性
        # 4.1 JSON 头字段
        result.json_header_match = (
            data1.nside == data2.nside and
            data1.nested == data2.nested and
            data1.n_pix == data2.n_pix and
            data1.meta == data2.meta
        )

        # 4.2 ipix 集合（HCSD 写入会重新排序，所以比较集合而非数组顺序）
        # 同时验证排序后数组是否一致（更强的不变量）
        # 由于 aio_hcsd_write 排序输入，data2 应已排序；data1 来自原文件，也应已排序
        # 但为安全起见，比较集合 + 排序后的数组
        result.ipix_match = (
            len(data1.ipix) == len(data2.ipix) and
            sorted(data1.ipix) == sorted(data2.ipix)
        )

        # 4.3 pixel 值（按 ipix 索引后比较）
        if len(data1.ipix) == len(data2.ipix):
            # 建立 ipix -> pixel 映射
            map1 = dict(zip(data1.ipix, data1.pixel))
            map2 = dict(zip(data2.ipix, data2.pixel))
            if set(map1.keys()) == set(map2.keys()):
                result.pixel_match = all(
                    struct.pack("<f", map1[k]) == struct.pack("<f", map2[k])
                    for k in map1.keys()
                )
            else:
                result.pixel_match = False
        else:
            result.pixel_match = False

        # 4.4 SNR 通道（HCSD 强制 has_snr=false）
        result.snr_match = True  # HCSD 无 SNR
        result.snr_model_match = True

        # 4.5 文件大小
        result.file_size_match = (result.file_size == result.copy_size)

        # 4.6 按子叶读取验证
        # 计算 nside 对应的 shift
        shift = 0
        temp_nside = data1.nside
        while temp_nside > 64:
            shift += 2
            temp_nside >>= 1

        # 找出非空子叶（从 data1.ipix 计算所有 leaf_ipix）
        leaf_set = set()
        for ipix in data1.ipix:
            leaf_set.add(ipix >> shift)

        # 取前若干个非空子叶做验证（避免对 49152 个子叶全测）
        test_leaves = sorted(leaf_set)[:min(10, len(leaf_set))]

        leaf_all_match = True
        leaf_failures = []
        for leaf_ipix in test_leaves:
            try:
                # 原文件的子叶数据
                orig_ipix, orig_pixel = hcsd_read_leaf(dll, file_path, leaf_ipix)
                # 副本的子叶数据
                copy_ipix, copy_pixel = hcsd_read_leaf(dll, copy_path, leaf_ipix)

                if len(orig_ipix) != len(copy_ipix):
                    leaf_all_match = False
                    leaf_failures.append(f"leaf={leaf_ipix} size {len(orig_ipix)}!={len(copy_ipix)}")
                    continue

                # 比较 ipix 数组（按 leaf 内排序）
                if sorted(orig_ipix) != sorted(copy_ipix):
                    leaf_all_match = False
                    leaf_failures.append(f"leaf={leaf_ipix} ipix set mismatch")
                    continue

                # 比较 pixel 值
                omap = dict(zip(orig_ipix, orig_pixel))
                cmap = dict(zip(copy_ipix, copy_pixel))
                if set(omap.keys()) != set(cmap.keys()):
                    leaf_all_match = False
                    leaf_failures.append(f"leaf={leaf_ipix} pixel keys mismatch")
                    continue

                for k in omap.keys():
                    if struct.pack("<f", omap[k]) != struct.pack("<f", cmap[k]):
                        leaf_all_match = False
                        leaf_failures.append(f"leaf={leaf_ipix} pixel value mismatch at ipix={k}")
                        break

                # 也验证子叶数据与全量读取一致
                expected_ipix = [ip for ip in data1.ipix if (ip >> shift) == leaf_ipix]
                if len(orig_ipix) != len(expected_ipix):
                    leaf_all_match = False
                    leaf_failures.append(f"leaf={leaf_ipix} size {len(orig_ipix)} != full-read {len(expected_ipix)}")

            except Exception as e:
                leaf_all_match = False
                leaf_failures.append(f"leaf={leaf_ipix} error: {e}")

        result.leaf_read_match = leaf_all_match

        # 总体成功判定
        all_match = (
            result.json_header_match and
            result.ipix_match and
            result.pixel_match and
            result.snr_match and
            result.snr_model_match and
            result.leaf_read_match
        )
        result.success = all_match

        if not result.success:
            failures = []
            if not result.json_header_match:
                failures.append("json_header")
            if not result.ipix_match:
                failures.append("ipix")
            if not result.pixel_match:
                failures.append("pixel")
            if not result.snr_match:
                failures.append("snr")
            if not result.snr_model_match:
                failures.append("snr_model")
            if not result.leaf_read_match:
                failures.append(f"leaf_read({len(leaf_failures)} failures: {leaf_failures[:3]})")
            result.error = "字段不匹配: " + ",".join(failures)

    except Exception as e:
        result.error = f"{type(e).__name__}: {e}"

    return result


# ============================================================================
# 主流程
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="HISS/HCSD round-trip 测试")
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR,
                        help=f"副本文件与报告输出目录（默认: {DEFAULT_OUTPUT_DIR}）")
    parser.add_argument("--dll-path", default=AIO_DLL_PATH,
                        help=f"astro_image_io.dll 路径（默认: {AIO_DLL_PATH}）")
    parser.add_argument("--json-report", default=None,
                        help="结构化 JSON 报告输出路径（默认: <output-dir>/roundtrip_report.json）")
    parser.add_argument("files", nargs="*",
                        help="要测试的 HISS/HCSD 文件路径（默认: 4 个真实数据文件）")
    args = parser.parse_args()

    # 配置测试文件
    if args.files:
        test_files = args.files
    else:
        test_files = DEFAULT_HISS_FILES + DEFAULT_HCSD_FILES

    # 创建输出目录
    os.makedirs(args.output_dir, exist_ok=True)
    json_report_path = args.json_report or os.path.join(args.output_dir, "roundtrip_report.json")

    # 打印头部信息到 stderr
    print("=" * 72, file=sys.stderr)
    print("HISS/HCSD Round-trip 测试 (P01-003)", file=sys.stderr)
    print("=" * 72, file=sys.stderr)
    print(f"DLL: {args.dll_path}", file=sys.stderr)
    print(f"输出目录: {args.output_dir}", file=sys.stderr)
    print(f"测试文件数: {len(test_files)}", file=sys.stderr)
    print(f"Python: {sys.version.split()[0]}", file=sys.stderr)
    print("", file=sys.stderr)

    # 加载 DLL
    try:
        dll = load_aio_dll(args.dll_path)
        print(f"[OK] DLL 加载成功: {args.dll_path}", file=sys.stderr)
    except Exception as e:
        print(f"[FAIL] DLL 加载失败: {e}", file=sys.stderr)
        report = {
            "task": "P01-003",
            "test": "hiss_hcsd_roundtrip",
            "status": "ENV_ERROR",
            "error": f"DLL 加载失败: {e}",
            "dll_path": args.dll_path,
        }
        with open(json_report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=False, indent=2)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 2

    # 运行测试
    results: List[TestResult] = []
    all_success = True

    for file_path in test_files:
        print(f"\n--- 测试: {file_path} ---", file=sys.stderr)

        if not os.path.isfile(file_path):
            print(f"  [SKIP] 文件不存在", file=sys.stderr)
            results.append(TestResult(
                file_path=file_path, file_type="UNKNOWN",
                file_size=0, file_sha256="",
                success=False, error="文件不存在"))
            all_success = False
            continue

        try:
            file_type = detect_file_type(file_path)
        except Exception as e:
            print(f"  [FAIL] 文件类型检测失败: {e}", file=sys.stderr)
            results.append(TestResult(
                file_path=file_path, file_type="UNKNOWN",
                file_size=os.path.getsize(file_path),
                file_sha256=sha256_file(file_path),
                success=False, error=f"magic 检测失败: {e}"))
            all_success = False
            continue

        print(f"  类型: {file_type}, 大小: {os.path.getsize(file_path)} 字节", file=sys.stderr)

        if file_type == "HISS":
            result = test_hiss_roundtrip(dll, file_path, args.output_dir)
        elif file_type == "HCSD":
            result = test_hcsd_roundtrip(dll, file_path, args.output_dir)
        else:
            result = TestResult(
                file_path=file_path, file_type=file_type,
                file_size=os.path.getsize(file_path),
                file_sha256=sha256_file(file_path),
                success=False, error=f"不支持的文件类型: {file_type}")

        results.append(result)

        # 打印结果
        if result.success:
            print(f"  [PASS] round-trip 成功", file=sys.stderr)
        else:
            print(f"  [FAIL] {result.error}", file=sys.stderr)
            all_success = False

        print(f"  读取: nside={result.read_nside} nested={result.read_nested} "
              f"n_pix={result.read_n_pix} has_snr={result.read_has_snr} "
              f"snr_format={result.read_snr_format}", file=sys.stderr)
        print(f"  耗时: read={result.read_ms:.1f}ms write={result.write_ms:.1f}ms "
              f"reread={result.reread_ms:.1f}ms", file=sys.stderr)
        if result.copy_path:
            print(f"  副本: {result.copy_path}", file=sys.stderr)
            print(f"        大小={result.copy_size} 原大小={result.file_size} "
                  f"大小一致={result.file_size_match}", file=sys.stderr)
            print(f"        SHA-256: {result.copy_sha256}", file=sys.stderr)
            print(f"        原 SHA-256: {result.file_sha256}", file=sys.stderr)
        print(f"  验证: json_header={result.json_header_match} ipix={result.ipix_match} "
              f"pixel={result.pixel_match} snr={result.snr_match} "
              f"snr_model={result.snr_model_match}", file=sys.stderr)
        if result.leaf_read_match is not None:
            print(f"        leaf_read={result.leaf_read_match}", file=sys.stderr)

    # 汇总
    n_total = len(results)
    n_pass = sum(1 for r in results if r.success)
    n_fail = n_total - n_pass

    print("\n" + "=" * 72, file=sys.stderr)
    print(f"汇总: {n_pass}/{n_total} 通过, {n_fail} 失败", file=sys.stderr)
    print("=" * 72, file=sys.stderr)

    # 生成 JSON 报告
    report = {
        "task": "P01-003",
        "test": "hiss_hcsd_roundtrip",
        "status": "PASS" if all_success else "FAIL",
        "summary": {
            "total": n_total,
            "passed": n_pass,
            "failed": n_fail,
        },
        "dll": {
            "path": args.dll_path,
        },
        "environment": {
            "python": sys.version.split()[0],
            "platform": sys.platform,
        },
        "results": [asdict(r) for r in results],
    }

    with open(json_report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(f"\nJSON 报告已写入: {json_report_path}", file=sys.stderr)

    # stdout 输出 JSON（供程序化处理）
    print(json.dumps(report, ensure_ascii=False, indent=2))

    return 0 if all_success else 1


if __name__ == "__main__":
    sys.exit(main())
