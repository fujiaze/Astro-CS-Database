# test_healpix_io.py - healpix_io.dll 往返测试
# 功能: 验证 .hiss / .hcsd 格式的写入和读取一致性
# 用途: 确认 healpix_io C++ DLL 的 5 个 API 函数正确工作

import ctypes
import os
import sys
import tempfile
import numpy as np

# ============================================================================
# DLL 加载
# ============================================================================

_dll_dir = os.path.dirname(os.path.abspath(__file__))
_dll_path = os.path.join(_dll_dir, "healpix_io.dll")

if not os.path.exists(_dll_path):
    print(f"ERROR: DLL not found: {_dll_path}")
    sys.exit(1)

_dll = ctypes.CDLL(_dll_path)

# ============================================================================
# API 函数签名绑定
# ============================================================================

# hiss_write(path, nside, nested, n_pix, ipix, pixel, meta_json) -> int
_dll.hiss_write.argtypes = [
    ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_float),
    ctypes.c_char_p
]
_dll.hiss_write.restype = ctypes.c_int

# hiss_read(path, nside*, nested*, n_pix*, ipix**, pixel**, meta_json**) -> int
_dll.hiss_read.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_uint64),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ctypes.POINTER(ctypes.c_char_p)
]
_dll.hiss_read.restype = ctypes.c_int

# hcsd_write(path, nside, nested, n_pix, ipix, pixel, meta_json) -> int
_dll.hcsd_write.argtypes = [
    ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_float),
    ctypes.c_char_p
]
_dll.hcsd_write.restype = ctypes.c_int

# hcsd_read(path, nside*, nested*, n_pix*, ipix**, pixel**, meta_json**) -> int
_dll.hcsd_read.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_uint64),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ctypes.POINTER(ctypes.c_char_p)
]
_dll.hcsd_read.restype = ctypes.c_int

# hcsd_read_leaf(path, leaf_ipix, n_pix*, ipix**, pixel**) -> int
_dll.hcsd_read_leaf.argtypes = [
    ctypes.c_char_p, ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint64),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint64)),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float))
]
_dll.hcsd_read_leaf.restype = ctypes.c_int

# hio_free(ptr) -> void
_dll.hio_free.argtypes = [ctypes.c_void_p]
_dll.hio_free.restype = None


def free_ptr(ptr):
    """释放 DLL 分配的内存"""
    if ptr:
        _dll.hio_free(ctypes.cast(ptr, ctypes.c_void_p))


# ============================================================================
# 测试用例
# ============================================================================

def test_hiss_roundtrip():
    """测试 1: .hiss 写入 → 读取往返一致性"""
    print("=" * 70)
    print("测试 1: .hiss 往返测试")
    print("=" * 70)

    nside = 8192
    nested = 1
    # 10 个像素: 3 个在 leaf 0, 3 个在 leaf 1, 4 个在 leaf 2
    # nside=8192, shift=14, leaf_ipix = ipix >> 14
    ipix_data = np.array([
        0, 1, 2,              # leaf 0
        16384, 16385, 16386,  # leaf 1 (16384 >> 14 = 1)
        32768, 32769, 32770, 32771  # leaf 2 (32768 >> 14 = 2)
    ], dtype=np.uint64)
    pixel_data = np.array([
        10.5, 20.3, 30.7,
        40.1, 50.9, 60.2,
        70.4, 80.6, 90.8, 100.0
    ], dtype=np.float32)
    n_pix = len(ipix_data)

    meta_json = '{"filter":"Lum","exposure_s":600.0,"obs_time":"2025-05-03T03:15:25Z","pixfrac":0.6}'

    # 写入 .hiss 文件
    tmp_path = os.path.join(tempfile.gettempdir(), "test_roundtrip.hiss")
    path_bytes = tmp_path.encode("utf-8")
    meta_bytes = meta_json.encode("utf-8")

    ipix_ptr = ipix_data.ctypes.data_as(ctypes.POINTER(ctypes.c_uint64))
    pixel_ptr = pixel_data.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

    ret = _dll.hiss_write(path_bytes, nside, nested, n_pix,
                          ipix_ptr, pixel_ptr, meta_bytes)
    assert ret == 0, f"hiss_write 失败, ret={ret}"
    print(f"  [OK] hiss_write 成功: {tmp_path}")
    print(f"       文件大小: {os.path.getsize(tmp_path)} 字节")

    # 读取 .hiss 文件
    out_nside = ctypes.c_uint32(0)
    out_nested = ctypes.c_int(0)
    out_n_pix = ctypes.c_uint64(0)
    out_ipix = ctypes.POINTER(ctypes.c_uint64)()
    out_pixel = ctypes.POINTER(ctypes.c_float)()
    out_meta = ctypes.c_char_p()

    ret = _dll.hiss_read(path_bytes,
                         ctypes.byref(out_nside), ctypes.byref(out_nested),
                         ctypes.byref(out_n_pix), ctypes.byref(out_ipix),
                         ctypes.byref(out_pixel), ctypes.byref(out_meta))
    assert ret == 0, f"hiss_read 失败, ret={ret}"
    print(f"  [OK] hiss_read 成功")

    # 验证数据
    assert out_nside.value == nside, f"nside 不匹配: {out_nside.value} != {nside}"
    print(f"  [OK] nside: {out_nside.value} == {nside}")

    assert out_nested.value == nested, f"nested 不匹配: {out_nested.value} != {nested}"
    print(f"  [OK] nested: {out_nested.value} == {nested}")

    assert out_n_pix.value == n_pix, f"n_pix 不匹配: {out_n_pix.value} != {n_pix}"
    print(f"  [OK] n_pix: {out_n_pix.value} == {n_pix}")

    # 验证 ipix 数组
    read_ipix = np.ctypeslib.as_array(out_ipix, shape=(n_pix,))
    np.testing.assert_array_equal(read_ipix, ipix_data, "ipix 数组不匹配")
    print(f"  [OK] ipix 数组一致: {read_ipix.tolist()}")

    # 验证 pixel 数组
    read_pixel = np.ctypeslib.as_array(out_pixel, shape=(n_pix,))
    np.testing.assert_allclose(read_pixel, pixel_data, rtol=1e-5, err_msg="pixel 数组不匹配")
    print(f"  [OK] pixel 数组一致: {read_pixel.tolist()}")

    # 验证 meta_json (应包含原始字段 + nside/nested/n_pix)
    read_meta = out_meta.value.decode("utf-8")
    assert '"nside":8192' in read_meta, f"meta 中缺少 nside: {read_meta}"
    assert '"nested":true' in read_meta, f"meta 中缺少 nested: {read_meta}"
    assert '"n_pix":10' in read_meta, f"meta 中缺少 n_pix: {read_meta}"
    assert '"filter":"Lum"' in read_meta, f"meta 中缺少 filter: {read_meta}"
    assert '"exposure_s":600.0' in read_meta, f"meta 中缺少 exposure_s: {read_meta}"
    print(f"  [OK] meta_json 一致: {read_meta}")

    # 清理
    free_ptr(out_ipix)
    free_ptr(out_pixel)
    free_ptr(out_meta)
    os.remove(tmp_path)
    print(f"  [OK] 清理完成")

    print("  测试 1 PASSED\n")
    return True


def test_hcsd_roundtrip():
    """测试 2: .hcsd 写入 → 全量读取往返一致性"""
    print("=" * 70)
    print("测试 2: .hcsd 往返测试 (全量读取)")
    print("=" * 70)

    nside = 8192
    nested = 1
    # 10 个像素: 3 个在 leaf 0, 3 个 in leaf 1, 4 个 in leaf 2
    # 注意: hcsd_write 会按 leaf_ipix + ipix 排序
    ipix_data = np.array([
        32769, 32768, 32771, 32770,  # leaf 2 (乱序)
        0, 2, 1,                      # leaf 0 (乱序)
        16385, 16384, 16386,          # leaf 1 (乱序)
    ], dtype=np.uint64)
    pixel_data = np.array([
        71.0, 70.0, 100.0, 90.0,
        10.5, 30.7, 20.3,
        50.9, 40.1, 60.2
    ], dtype=np.float32)
    n_pix = len(ipix_data)

    meta_json = '{"filter":"Lum","n_frames":5,"total_exposure_s":3000.0,"sigma_clip":{"sigma":3.0,"max_iter":5},"stack_stats":{"mean_pixel_count":12.4,"median_exposure":600.0}}'

    # 写入 .hcsd 文件
    tmp_path = os.path.join(tempfile.gettempdir(), "test_roundtrip.hcsd")
    path_bytes = tmp_path.encode("utf-8")
    meta_bytes = meta_json.encode("utf-8")

    ipix_ptr = ipix_data.ctypes.data_as(ctypes.POINTER(ctypes.c_uint64))
    pixel_ptr = pixel_data.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

    ret = _dll.hcsd_write(path_bytes, nside, nested, n_pix,
                          ipix_ptr, pixel_ptr, meta_bytes)
    assert ret == 0, f"hcsd_write 失败, ret={ret}"
    print(f"  [OK] hcsd_write 成功: {tmp_path}")
    print(f"       文件大小: {os.path.getsize(tmp_path)} 字节")

    # 读取 .hcsd 文件 (全量)
    out_nside = ctypes.c_uint32(0)
    out_nested = ctypes.c_int(0)
    out_n_pix = ctypes.c_uint64(0)
    out_ipix = ctypes.POINTER(ctypes.c_uint64)()
    out_pixel = ctypes.POINTER(ctypes.c_float)()
    out_meta = ctypes.c_char_p()

    ret = _dll.hcsd_read(path_bytes,
                         ctypes.byref(out_nside), ctypes.byref(out_nested),
                         ctypes.byref(out_n_pix), ctypes.byref(out_ipix),
                         ctypes.byref(out_pixel), ctypes.byref(out_meta))
    assert ret == 0, f"hcsd_read 失败, ret={ret}"
    print(f"  [OK] hcsd_read 成功")

    # 验证基本字段
    assert out_nside.value == nside, f"nside 不匹配: {out_nside.value} != {nside}"
    assert out_nested.value == nested, f"nested 不匹配: {out_nested.value} != {nested}"
    assert out_n_pix.value == n_pix, f"n_pix 不匹配: {out_n_pix.value} != {n_pix}"
    print(f"  [OK] nside={out_nside.value}, nested={out_nested.value}, n_pix={out_n_pix.value}")

    # 验证 ipix 和 pixel (应按 leaf_ipix + ipix 排序)
    read_ipix = np.ctypeslib.as_array(out_ipix, shape=(n_pix,))
    read_pixel = np.ctypeslib.as_array(out_pixel, shape=(n_pix,))

    # 期望的排序结果: leaf 0 (ipix 0,1,2), leaf 1 (ipix 16384,16385,16386), leaf 2 (ipix 32768,32769,32770,32771)
    expected_ipix = np.array([0, 1, 2, 16384, 16385, 16386, 32768, 32769, 32770, 32771], dtype=np.uint64)
    expected_pixel = np.array([10.5, 20.3, 30.7, 40.1, 50.9, 60.2, 70.0, 71.0, 90.0, 100.0], dtype=np.float32)

    np.testing.assert_array_equal(read_ipix, expected_ipix, "ipix 排序后不匹配")
    print(f"  [OK] ipix 排序正确: {read_ipix.tolist()}")

    np.testing.assert_allclose(read_pixel, expected_pixel, rtol=1e-5, err_msg="pixel 不匹配")
    print(f"  [OK] pixel 排序正确: {read_pixel.tolist()}")

    # 验证 meta_json
    read_meta = out_meta.value.decode("utf-8")
    assert '"n_frames":5' in read_meta, f"meta 中缺少 n_frames: {read_meta}"
    assert '"total_exposure_s":3000.0' in read_meta, f"meta 中缺少 total_exposure_s: {read_meta}"
    assert '"sigma":3.0' in read_meta, f"meta 中缺少 sigma_clip: {read_meta}"
    print(f"  [OK] meta_json 一致")

    # 清理
    free_ptr(out_ipix)
    free_ptr(out_pixel)
    free_ptr(out_meta)

    print("  测试 2 PASSED\n")
    return tmp_path  # 返回路径供测试 3 使用


def test_hcsd_read_leaf(tmp_path):
    """测试 3: .hcsd 按子叶读取 (hcsd_read_leaf)"""
    print("=" * 70)
    print("测试 3: .hcsd 按子叶读取 (hcsd_read_leaf)")
    print("=" * 70)

    path_bytes = tmp_path.encode("utf-8")

    # 读取 leaf 0 (应有 3 个像素: ipix 0, 1, 2)
    leaf_n_pix = ctypes.c_uint64(0)
    leaf_ipix = ctypes.POINTER(ctypes.c_uint64)()
    leaf_pixel = ctypes.POINTER(ctypes.c_float)()

    ret = _dll.hcsd_read_leaf(path_bytes, 0,
                              ctypes.byref(leaf_n_pix),
                              ctypes.byref(leaf_ipix),
                              ctypes.byref(leaf_pixel))
    assert ret == 0, f"hcsd_read_leaf(0) 失败, ret={ret}"
    assert leaf_n_pix.value == 3, f"leaf 0 n_pix 不匹配: {leaf_n_pix.value} != 3"
    print(f"  [OK] leaf 0: n_pix={leaf_n_pix.value}")

    read_ipix = np.ctypeslib.as_array(leaf_ipix, shape=(3,))
    read_pixel = np.ctypeslib.as_array(leaf_pixel, shape=(3,))
    np.testing.assert_array_equal(read_ipix, [0, 1, 2], "leaf 0 ipix 不匹配")
    np.testing.assert_allclose(read_pixel, [10.5, 20.3, 30.7], rtol=1e-5, err_msg="leaf 0 pixel 不匹配")
    print(f"  [OK] leaf 0 ipix: {read_ipix.tolist()}")
    print(f"  [OK] leaf 0 pixel: {read_pixel.tolist()}")
    free_ptr(leaf_ipix)
    free_ptr(leaf_pixel)

    # 读取 leaf 1 (应有 3 个像素: ipix 16384, 16385, 16386)
    ret = _dll.hcsd_read_leaf(path_bytes, 1,
                              ctypes.byref(leaf_n_pix),
                              ctypes.byref(leaf_ipix),
                              ctypes.byref(leaf_pixel))
    assert ret == 0, f"hcsd_read_leaf(1) 失败, ret={ret}"
    assert leaf_n_pix.value == 3, f"leaf 1 n_pix 不匹配: {leaf_n_pix.value} != 3"
    print(f"  [OK] leaf 1: n_pix={leaf_n_pix.value}")

    read_ipix = np.ctypeslib.as_array(leaf_ipix, shape=(3,))
    read_pixel = np.ctypeslib.as_array(leaf_pixel, shape=(3,))
    np.testing.assert_array_equal(read_ipix, [16384, 16385, 16386], "leaf 1 ipix 不匹配")
    np.testing.assert_allclose(read_pixel, [40.1, 50.9, 60.2], rtol=1e-5, err_msg="leaf 1 pixel 不匹配")
    print(f"  [OK] leaf 1 ipix: {read_ipix.tolist()}")
    print(f"  [OK] leaf 1 pixel: {read_pixel.tolist()}")
    free_ptr(leaf_ipix)
    free_ptr(leaf_pixel)

    # 读取 leaf 2 (应有 4 个像素: ipix 32768, 32769, 32770, 32771)
    ret = _dll.hcsd_read_leaf(path_bytes, 2,
                              ctypes.byref(leaf_n_pix),
                              ctypes.byref(leaf_ipix),
                              ctypes.byref(leaf_pixel))
    assert ret == 0, f"hcsd_read_leaf(2) 失败, ret={ret}"
    assert leaf_n_pix.value == 4, f"leaf 2 n_pix 不匹配: {leaf_n_pix.value} != 4"
    print(f"  [OK] leaf 2: n_pix={leaf_n_pix.value}")

    read_ipix = np.ctypeslib.as_array(leaf_ipix, shape=(4,))
    read_pixel = np.ctypeslib.as_array(leaf_pixel, shape=(4,))
    np.testing.assert_array_equal(read_ipix, [32768, 32769, 32770, 32771], "leaf 2 ipix 不匹配")
    np.testing.assert_allclose(read_pixel, [70.0, 71.0, 90.0, 100.0], rtol=1e-5, err_msg="leaf 2 pixel 不匹配")
    print(f"  [OK] leaf 2 ipix: {read_ipix.tolist()}")
    print(f"  [OK] leaf 2 pixel: {read_pixel.tolist()}")
    free_ptr(leaf_ipix)
    free_ptr(leaf_pixel)

    # 读取空子叶 (leaf 3, 应返回 n_pix=0)
    ret = _dll.hcsd_read_leaf(path_bytes, 3,
                              ctypes.byref(leaf_n_pix),
                              ctypes.byref(leaf_ipix),
                              ctypes.byref(leaf_pixel))
    assert ret == 0, f"hcsd_read_leaf(3) 失败, ret={ret}"
    assert leaf_n_pix.value == 0, f"leaf 3 应为空, n_pix={leaf_n_pix.value}"
    print(f"  [OK] leaf 3 (空子叶): n_pix=0")

    # 清理
    os.remove(tmp_path)
    print(f"  [OK] 清理完成")

    print("  测试 3 PASSED\n")
    return True


def test_hiss_empty():
    """测试 4: .hiss 空数据 (n_pix=0)"""
    print("=" * 70)
    print("测试 4: .hiss 空数据测试 (n_pix=0)")
    print("=" * 70)

    nside = 8192
    nested = 1
    n_pix = 0
    meta_json = '{"filter":"Lum"}'

    tmp_path = os.path.join(tempfile.gettempdir(), "test_empty.hiss")
    path_bytes = tmp_path.encode("utf-8")
    meta_bytes = meta_json.encode("utf-8")

    # 写入空数据 (ipix/pixel 为 null)
    ret = _dll.hiss_write(path_bytes, nside, nested, n_pix,
                          None, None, meta_bytes)
    assert ret == 0, f"hiss_write(空) 失败, ret={ret}"
    print(f"  [OK] hiss_write(空) 成功: {os.path.getsize(tmp_path)} 字节")

    # 读取
    out_nside = ctypes.c_uint32(0)
    out_nested = ctypes.c_int(0)
    out_n_pix = ctypes.c_uint64(0)
    out_ipix = ctypes.POINTER(ctypes.c_uint64)()
    out_pixel = ctypes.POINTER(ctypes.c_float)()
    out_meta = ctypes.c_char_p()

    ret = _dll.hiss_read(path_bytes,
                         ctypes.byref(out_nside), ctypes.byref(out_nested),
                         ctypes.byref(out_n_pix), ctypes.byref(out_ipix),
                         ctypes.byref(out_pixel), ctypes.byref(out_meta))
    assert ret == 0, f"hiss_read(空) 失败, ret={ret}"
    assert out_n_pix.value == 0, f"n_pix 应为 0: {out_n_pix.value}"
    assert not out_ipix, "ipix 应为 null"
    assert not out_pixel, "pixel 应为 null"
    print(f"  [OK] hiss_read(空) 成功: n_pix=0, ipix=null, pixel=null")

    free_ptr(out_meta)
    os.remove(tmp_path)

    print("  测试 4 PASSED\n")
    return True


def main():
    print("\n" + "=" * 70)
    print("healpix_io.dll 往返测试")
    print("=" * 70 + "\n")

    tests = [
        ("test_hiss_roundtrip", test_hiss_roundtrip),
        ("test_hcsd_roundtrip", test_hcsd_roundtrip),
        ("test_hcsd_read_leaf", test_hcsd_read_leaf),
        ("test_hiss_empty", test_hiss_empty),
    ]

    results = []
    hcsd_path = None

    for name, test in tests:
        try:
            if name == "test_hcsd_read_leaf" and hcsd_path:
                result = test(hcsd_path)
            elif name == "test_hcsd_roundtrip":
                hcsd_path = test()
                result = True
            else:
                result = test()
            results.append((name, result))
        except Exception as e:
            print(f"  测试 {name} FAILED: {e}\n")
            import traceback
            traceback.print_exc()
            results.append((name, False))

    # 汇总
    print("=" * 70)
    print("测试汇总")
    print("=" * 70)
    n_pass = sum(1 for _, r in results if r)
    n_total = len(results)
    for name, result in results:
        status = "PASS" if result else "FAIL"
        print(f"  {name}: {status}")
    print(f"\n  总计: {n_pass}/{n_total} 通过")
    print("=" * 70)

    return 0 if n_pass == n_total else 1


if __name__ == "__main__":
    sys.exit(main())
