#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P06-002 verify_snr_weight.py - 读取 HCSD 输出像素值, 验证 SNR² 加权公式"""
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
    dll.aio_hcsd_read.argtypes = [
        c_char_p,
        POINTER(c_uint32), POINTER(c_int), POINTER(c_uint64),
        POINTER(POINTER(c_uint64)),
        POINTER(POINTER(c_float)),
        POINTER(c_char_p),
    ]
    dll.aio_hcsd_read.restype = c_int
    dll.aio_hio_free.argtypes = [ctypes.c_void_p]
    dll.aio_hio_free.restype = None
    return dll


def read_hcsd(dll: CDLL, path: str):
    nside = c_uint32(0)
    nested = c_int(0)
    n_pix = c_uint64(0)
    ipix_ptr = POINTER(c_uint64)()
    pixel_ptr = POINTER(c_float)()
    meta_ptr = c_char_p()
    ret = dll.aio_hcsd_read(path.encode("utf-8"),
                            byref(nside), byref(nested), byref(n_pix),
                            byref(ipix_ptr), byref(pixel_ptr), byref(meta_ptr))
    if ret != 0:
        raise RuntimeError(f"aio_hcsd_read 失败 ret={ret}")
    n = n_pix.value
    ipix = np.ctypeslib.as_array(ipix_ptr, shape=(n,)).copy() if n > 0 else np.array([], dtype=np.uint64)
    pixel = np.ctypeslib.as_array(pixel_ptr, shape=(n,)).copy() if n > 0 else np.array([], dtype=np.float32)
    meta = meta_ptr.value.decode("utf-8") if meta_ptr.value else "{}"
    if ipix_ptr: dll.aio_hio_free(ctypes.cast(ipix_ptr, ctypes.c_void_p))
    if pixel_ptr: dll.aio_hio_free(ctypes.cast(pixel_ptr, ctypes.c_void_p))
    if meta_ptr: dll.aio_hio_free(ctypes.cast(meta_ptr, ctypes.c_void_p))
    return nside.value, nested.value, n, ipix, pixel, meta


from ctypes import byref

def main():
    hcsd_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        PROJECT_ROOT, "engineering", "evidence", "P06-002",
        "test_D_snr_weight", "output", "snr_weighted.hcsd")
    expected_path = os.path.join(
        PROJECT_ROOT, "engineering", "evidence", "P06-002",
        "test_D_snr_weight", "input", "expected_values.json")

    dll = setup_dll()
    nside, nested, n_pix, ipix, pixel, meta = read_hcsd(dll, hcsd_path)
    with open(expected_path, "r", encoding="utf-8") as f:
        expected = json.load(f)

    print(f"[verify_snr_weight] HCSD: {hcsd_path}")
    print(f"[verify_snr_weight] nside={nside} nested={nested} n_pix={n_pix}")
    print(f"[verify_snr_weight] ipix  = {ipix.tolist()}")
    print(f"[verify_snr_weight] pixel = {pixel.tolist()}")
    print(f"[verify_snr_weight] meta  = {meta}")

    expected_weighted = expected["expected_weighted_mean"]
    expected_equal = expected["expected_equal_weight_mean"]
    print(f"[verify_snr_weight] 期望 SNR² 加权均值 = {expected_weighted}")
    print(f"[verify_snr_weight] 期望等权均值 (退化) = {expected_equal}")

    # 检查所有像素是否一致 (输入是均匀的, 输出也应均匀)
    unique_pixels = sorted(set(pixel.tolist()))
    print(f"[verify_snr_weight] 输出唯一像素值: {unique_pixels}")

    if len(unique_pixels) != 1:
        print(f"[verify_snr_weight] FAIL: 输出像素不唯一")
        verdict = "FAIL"
    else:
        out_val = unique_pixels[0]
        # 容差比较 (float32 精度)
        tol = 1e-5
        if abs(out_val - expected_weighted) < tol:
            verdict = "PASS_SNR_WEIGHTED"
            print(f"[verify_snr_weight] 输出={out_val} == 期望加权={expected_weighted} (容差 {tol})")
            print(f"[verify_snr_weight] VERDICT: {verdict} - SNR² 加权真实生效!")
        elif abs(out_val - expected_equal) < tol:
            verdict = "PASS_EQUAL_FALLBACK"
            print(f"[verify_snr_weight] 输出={out_val} == 期望等权={expected_equal} (容差 {tol})")
            print(f"[verify_snr_weight] VERDICT: {verdict} - 等权退化 (G-002 缺口)")
        else:
            verdict = "FAIL"
            print(f"[verify_snr_weight] 输出={out_val} 既非加权({expected_weighted})也非等权({expected_equal})")
            print(f"[verify_snr_weight] VERDICT: {verdict}")

    # 输出 JSON 结果
    result = {
        "hcsd_path": hcsd_path,
        "nside": nside,
        "nested": bool(nested),
        "n_pix": int(n_pix),
        "ipix": ipix.tolist(),
        "pixel": pixel.tolist(),
        "expected_weighted_mean": expected_weighted,
        "expected_equal_weight_mean": expected_equal,
        "actual_pixel_value": unique_pixels[0] if len(unique_pixels) == 1 else None,
        "verdict": verdict,
        "tolerance": 1e-5,
        "meta_json": meta,
    }
    out_json = os.path.join(os.path.dirname(hcsd_path), "snr_weight_verification.json")
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
    print(f"[verify_snr_weight] 结果 JSON: {out_json}")
    return 0 if verdict.startswith("PASS") else 1


if __name__ == "__main__":
    sys.exit(main())
