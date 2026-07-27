#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P06-002 make_snr_hiss.py - 构造合成 HISS (has_snr=true, snr_format=0) 验证 SNR² 加权公式

数学公式 (hp_stack_hiss.cpp:224-228):
    w_i = snr_i^2          (has_snr=true)
    w_i = 1.0              (has_snr=false, 等权回退)
    weighted_mean = sum(pixel_i * w_i) / sum(w_i)

构造方法:
  - 2 帧 HISS, ipix 完全重叠 (强制 mean_pixel_count=2.0)
  - frame A: pixel=10.0, snr=2.0  -> w_A = 4.0
  - frame B: pixel=20.0, snr=4.0  -> w_B = 16.0
  - 期望加权均值 = (10*4 + 20*16) / (4+16) = (40+320)/20 = 360/20 = 18.0
  - 等权均值 = (10+20)/2 = 15.0  (对比)
  - 若 stage2 输出 = 18.0 -> SNR² 加权真实生效
  - 若 stage2 输出 = 15.0 -> 退化为等权 (G-002)

退出码: 0=成功, 1=参数错误, 2=DLL 错误
"""
from __future__ import annotations

import os
import sys
import json
import ctypes
from ctypes import (
    c_char_p, c_int, c_uint32, c_uint64, c_float,
    POINTER, CDLL, Structure,
)
from typing import Optional

import numpy as np

PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
AIO_DLL = os.path.join(PROJECT_ROOT, "build", "artifacts", "astro_image_io.dll")
MINGW_BIN = r"C:\msys64\mingw64\bin"


def setup_dll() -> CDLL:
    if not os.path.isfile(AIO_DLL):
        raise FileNotFoundError(f"DLL 不存在: {AIO_DLL}")
    # 同时加入 build/artifacts 与 mingw64/bin 到 DLL 搜索路径
    artifacts_dir = os.path.dirname(AIO_DLL)
    try:
        os.add_dll_directory(artifacts_dir)
    except (AttributeError, OSError):
        pass
    try:
        os.add_dll_directory(MINGW_BIN)
    except (AttributeError, OSError):
        pass
    # 临时把 mingw64/bin 追加到 PATH 末尾 (不前置, 避免覆盖系统 python)
    os.environ["PATH"] = os.environ.get("PATH", "") + os.pathsep + MINGW_BIN + os.pathsep + artifacts_dir
    dll = CDLL(AIO_DLL)
    # 函数名带 aio_ 前缀 (hiss_write 是宏, 实际导出名是 aio_hiss_write)
    dll.aio_hiss_write.argtypes = [
        c_char_p, c_uint32, c_int, c_uint64,
        POINTER(c_uint64), POINTER(c_float),
        POINTER(c_float),
        c_char_p,
    ]
    dll.aio_hiss_write.restype = c_int
    dll.aio_hiss_read.argtypes = [
        c_char_p,
        POINTER(c_uint32), POINTER(c_int), POINTER(c_uint64),
        POINTER(POINTER(c_uint64)),
        POINTER(POINTER(c_float)),
        POINTER(POINTER(c_float)),
        POINTER(c_char_p),
    ]
    dll.aio_hiss_read.restype = c_int
    dll.aio_hio_free.argtypes = [ctypes.c_void_p]
    dll.aio_hio_free.restype = None
    return dll


def write_hiss_with_snr(dll: CDLL, path: str, nside: int, nested: bool,
                        ipix: np.ndarray, pixel: np.ndarray,
                        snr: np.ndarray, meta: dict) -> int:
    """写入带 snr 通道 (snr_format=0) 的 HISS"""
    ipix_arr = np.ascontiguousarray(ipix, dtype=np.uint64)
    pixel_arr = np.ascontiguousarray(pixel, dtype=np.float32)
    snr_arr = np.ascontiguousarray(snr, dtype=np.float32)
    n_pix = ipix_arr.size
    meta_bytes = json.dumps(meta, separators=(",", ":")).encode("utf-8")
    path_bytes = path.encode("utf-8")

    ipix_ptr = ipix_arr.ctypes.data_as(POINTER(c_uint64)) if n_pix > 0 else None
    pixel_ptr = pixel_arr.ctypes.data_as(POINTER(c_float)) if n_pix > 0 else None
    snr_ptr = snr_arr.ctypes.data_as(POINTER(c_float)) if n_pix > 0 else None

    ret = dll.aio_hiss_write(
        path_bytes, nside, 1 if nested else 0, n_pix,
        ipix_ptr, pixel_ptr, snr_ptr, meta_bytes)
    if ret != 0:
        raise RuntimeError(f"aio_hiss_write 失败 ret={ret}: {path}")
    return ret


def main():
    # 输出目录
    out_dir = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P06-002",
                           "test_D_snr_weight", "input")
    os.makedirs(out_dir, exist_ok=True)

    # 准备数据: nside=64 (小 nside, 仅 49152 像素, 速度快)
    # 选 4 个 ipix, 全部位于 leaf 0 (nside=64 时所有 ipix 都是 leaf_ipix)
    nside = 64
    nested = True
    # ipix 在 nside=64 NESTED 下取 4 个连续像素 (确保都属于同一个 leaf)
    ipix = np.array([100, 101, 102, 103], dtype=np.uint64)

    # Frame A: pixel=10.0, snr=2.0 (w=4.0)
    pixel_a = np.array([10.0, 10.0, 10.0, 10.0], dtype=np.float32)
    snr_a   = np.array([2.0,  2.0,  2.0,  2.0],  dtype=np.float32)

    # Frame B: pixel=20.0, snr=4.0 (w=16.0)
    pixel_b = np.array([20.0, 20.0, 20.0, 20.0], dtype=np.float32)
    snr_b   = np.array([4.0,  4.0,  4.0,  4.0],  dtype=np.float32)

    meta_a = {
        "filter": "Lum",
        "exposure_s": 100.0,
        "obs_time": "2026-07-26T00:00:00Z",
        "source": "P06-002 synthetic SNR test frame A",
        "task": "P06-002",
    }
    meta_b = {
        "filter": "Lum",
        "exposure_s": 100.0,
        "obs_time": "2026-07-26T00:00:00Z",
        "source": "P06-002 synthetic SNR test frame B",
        "task": "P06-002",
    }

    dll = setup_dll()

    path_a = os.path.join(out_dir, "frame_snr_A.hiss")
    path_b = os.path.join(out_dir, "frame_snr_B.hiss")
    write_hiss_with_snr(dll, path_a, nside, nested, ipix, pixel_a, snr_a, meta_a)
    write_hiss_with_snr(dll, path_b, nside, nested, ipix, pixel_b, snr_b, meta_b)

    # 期望值计算
    expected_weighted = (10.0 * 4.0 + 20.0 * 16.0) / (4.0 + 16.0)
    expected_equal = (10.0 + 20.0) / 2.0

    print(f"[make_snr_hiss] Frame A: {path_a}")
    print(f"[make_snr_hiss]   pixel=10.0, snr=2.0, w=snr^2=4.0")
    print(f"[make_snr_hiss] Frame B: {path_b}")
    print(f"[make_snr_hiss]   pixel=20.0, snr=4.0, w=snr^2=16.0")
    print(f"[make_snr_hiss] 期望 SNR² 加权均值 = {expected_weighted}")
    print(f"[make_snr_hiss] 期望等权均值 (退化) = {expected_equal}")

    # 输出预期值到 JSON, 供后续验证脚本读取
    expected_json = {
        "nside": nside,
        "nested": nested,
        "ipix": ipix.tolist(),
        "frame_A": {"pixel": pixel_a.tolist(), "snr": snr_a.tolist(),
                    "weight": [s*s for s in snr_a.tolist()]},
        "frame_B": {"pixel": pixel_b.tolist(), "snr": snr_b.tolist(),
                    "weight": [s*s for s in snr_b.tolist()]},
        "expected_weighted_mean": expected_weighted,
        "expected_equal_weight_mean": expected_equal,
        "formula": "weighted_mean = sum(pixel_i * snr_i^2) / sum(snr_i^2)",
        "verdict_rule": {
            "PASS_SNR_WEIGHTED": f"output_pixel == {expected_weighted} (SNR²加权生效)",
            "PASS_EQUAL_FALLBACK": f"output_pixel == {expected_equal} (G-002 退化等权, 代码路径已触发)",
            "FAIL": "output_pixel 既非加权也非等权 (异常)",
        },
    }
    expected_path = os.path.join(out_dir, "expected_values.json")
    with open(expected_path, "w", encoding="utf-8") as f:
        json.dump(expected_json, f, indent=2, ensure_ascii=False)
    print(f"[make_snr_hiss] 期望值 JSON: {expected_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
