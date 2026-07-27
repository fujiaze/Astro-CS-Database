#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P06-002 make_outlier_hiss.py - 构造含离群值的合成 HISS 验证 sigma-clip 剔除行为

数学公式 (hp_stack_hiss.cpp:263-329):
    weighted_mean = sum(pixel_i * w_i) / sum(w_i)
    weighted_var  = sum(pixel_i^2 * w_i) / sum(w_i) - mean^2
    weighted_std  = sqrt(max(var, 0))
    剔除条件: count >= 2 && std > 0 && |pixel - mean| > sigma * std

构造方法 (3 帧, ipix 完全重叠, has_snr=true):
  - Frame A: pixel=10.0,  snr=2.0  -> w=4.0
  - Frame B: pixel=20.0,  snr=4.0  -> w=16.0
  - Frame C: pixel=100.0, snr=1.0  -> w=1.0  (离群值)

预期行为:
  - weight = 4 + 16 + 1 = 21
  - sum_w = 40 + 320 + 100 = 460
  - mean = 460/21 = 21.905
  - sum_wsq = 400 + 6400 + 10000 = 16800
  - var = 16800/21 - 21.905^2 = 800 - 479.83 = 320.17
  - std = 17.893
  - dev(100) = |100 - 21.905| = 78.095
  - sigma=2.0: threshold=35.79, dev(100)=78.095 > 35.79 -> 剔除 Frame C
  - sigma=3.0: threshold=53.68, dev(100)=78.095 > 53.68 -> 剔除 Frame C
  - sigma=5.0: threshold=89.46, dev(100)=78.095 < 89.46 -> 不剔除

输出: 3 个 .hiss 文件到 test_outlier/input/
"""
from __future__ import annotations

import os
import sys
import json
import ctypes
from ctypes import (
    c_char_p, c_int, c_uint32, c_uint64, c_float,
    POINTER, CDLL,
)
import numpy as np

PROJECT_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
AIO_DLL = os.path.join(PROJECT_ROOT, "build", "artifacts", "astro_image_io.dll")
MINGW_BIN = r"C:\msys64\mingw64\bin"


def setup_dll() -> CDLL:
    artifacts_dir = os.path.dirname(AIO_DLL)
    try: os.add_dll_directory(artifacts_dir)
    except (AttributeError, OSError): pass
    try: os.add_dll_directory(MINGW_BIN)
    except (AttributeError, OSError): pass
    os.environ["PATH"] = os.environ.get("PATH", "") + os.pathsep + MINGW_BIN + os.pathsep + artifacts_dir
    dll = CDLL(AIO_DLL)
    dll.aio_hiss_write.argtypes = [
        c_char_p, c_uint32, c_int, c_uint64,
        POINTER(c_uint64), POINTER(c_float),
        POINTER(c_float),
        c_char_p,
    ]
    dll.aio_hiss_write.restype = c_int
    return dll


def write_hiss_with_snr(dll, path, nside, nested, ipix, pixel, snr, meta):
    ipix_arr = np.ascontiguousarray(ipix, dtype=np.uint64)
    pixel_arr = np.ascontiguousarray(pixel, dtype=np.float32)
    snr_arr = np.ascontiguousarray(snr, dtype=np.float32)
    n_pix = ipix_arr.size
    meta_bytes = json.dumps(meta, separators=(",", ":")).encode("utf-8")
    ipix_ptr = ipix_arr.ctypes.data_as(POINTER(c_uint64)) if n_pix > 0 else None
    pixel_ptr = pixel_arr.ctypes.data_as(POINTER(c_float)) if n_pix > 0 else None
    snr_ptr = snr_arr.ctypes.data_as(POINTER(c_float)) if n_pix > 0 else None
    ret = dll.aio_hiss_write(
        path.encode("utf-8"), nside, 1 if nested else 0, n_pix,
        ipix_ptr, pixel_ptr, snr_ptr, meta_bytes)
    if ret != 0:
        raise RuntimeError(f"aio_hiss_write 失败 ret={ret}: {path}")
    return ret


def main():
    out_dir = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P06-002",
                           "test_outlier", "input")
    os.makedirs(out_dir, exist_ok=True)

    nside = 64
    nested = True
    ipix = np.array([100, 101, 102, 103], dtype=np.uint64)

    # Frame A: pixel=10.0, snr=2.0 (w=4.0)
    pixel_a = np.array([10.0, 10.0, 10.0, 10.0], dtype=np.float32)
    snr_a   = np.array([2.0,  2.0,  2.0,  2.0],  dtype=np.float32)

    # Frame B: pixel=20.0, snr=4.0 (w=16.0)
    pixel_b = np.array([20.0, 20.0, 20.0, 20.0], dtype=np.float32)
    snr_b   = np.array([4.0,  4.0,  4.0,  4.0],  dtype=np.float32)

    # Frame C: pixel=100.0, snr=1.0 (w=1.0) - 离群值
    pixel_c = np.array([100.0, 100.0, 100.0, 100.0], dtype=np.float32)
    snr_c   = np.array([1.0,   1.0,   1.0,   1.0],   dtype=np.float32)

    meta_a = {"filter": "Lum", "exposure_s": 100.0, "obs_time": "2026-07-27T00:00:00Z",
              "source": "P06-002 synthetic outlier test frame A", "task": "P06-002"}
    meta_b = {"filter": "Lum", "exposure_s": 100.0, "obs_time": "2026-07-27T00:00:00Z",
              "source": "P06-002 synthetic outlier test frame B", "task": "P06-002"}
    meta_c = {"filter": "Lum", "exposure_s": 100.0, "obs_time": "2026-07-27T00:00:00Z",
              "source": "P06-002 synthetic outlier test frame C (outlier)", "task": "P06-002"}

    dll = setup_dll()
    path_a = os.path.join(out_dir, "frame_outlier_A.hiss")
    path_b = os.path.join(out_dir, "frame_outlier_B.hiss")
    path_c = os.path.join(out_dir, "frame_outlier_C.hiss")
    write_hiss_with_snr(dll, path_a, nside, nested, ipix, pixel_a, snr_a, meta_a)
    write_hiss_with_snr(dll, path_b, nside, nested, ipix, pixel_b, snr_b, meta_b)
    write_hiss_with_snr(dll, path_c, nside, nested, ipix, pixel_c, snr_c, meta_c)

    # 预期值
    weight = 4.0 + 16.0 + 1.0
    sum_w = 10.0*4.0 + 20.0*16.0 + 100.0*1.0
    mean = sum_w / weight
    sum_wsq = 100.0*4.0 + 400.0*16.0 + 10000.0*1.0
    var = sum_wsq / weight - mean * mean
    std = max(var, 0.0) ** 0.5
    dev_c = abs(100.0 - mean)

    expected = {
        "nside": nside, "nested": nested, "ipix": ipix.tolist(),
        "frame_A": {"pixel": 10.0, "snr": 2.0, "weight": 4.0},
        "frame_B": {"pixel": 20.0, "snr": 4.0, "weight": 16.0},
        "frame_C": {"pixel": 100.0, "snr": 1.0, "weight": 1.0, "role": "outlier"},
        "weighted_mean_before_clip": mean,
        "weighted_std_before_clip": std,
        "dev_outlier": dev_c,
        "expected_rejection": {
            "sigma_2.0": {"threshold": 2.0 * std, "reject": dev_c > 2.0 * std},
            "sigma_3.0": {"threshold": 3.0 * std, "reject": dev_c > 3.0 * std},
            "sigma_5.0": {"threshold": 5.0 * std, "reject": dev_c > 5.0 * std},
        },
        "expected_output_after_clip_sigma3": {
            "pixel": (10.0*4.0 + 20.0*16.0) / (4.0 + 16.0),
            "note": "Frame C (100.0) 被 sigma=3.0 剔除后, 加权均值 = (40+320)/20 = 18.0"
        },
    }
    expected_path = os.path.join(out_dir, "expected_values.json")
    with open(expected_path, "w", encoding="utf-8") as f:
        json.dump(expected, f, indent=2, ensure_ascii=False)

    print(f"[make_outlier_hiss] 3 frames written to {out_dir}")
    print(f"[make_outlier_hiss] weighted_mean={mean:.4f} weighted_std={std:.4f}")
    print(f"[make_outlier_hiss] dev_outlier={dev_c:.4f}")
    print(f"[make_outlier_hiss] sigma=2.0 reject={dev_c > 2.0*std} (threshold={2.0*std:.4f})")
    print(f"[make_outlier_hiss] sigma=3.0 reject={dev_c > 3.0*std} (threshold={3.0*std:.4f})")
    print(f"[make_outlier_hiss] sigma=5.0 reject={dev_c > 5.0*std} (threshold={5.0*std:.4f})")
    print(f"[make_outlier_hiss] expected output after sigma=3.0 clip: {expected['expected_output_after_clip_sigma3']['pixel']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
