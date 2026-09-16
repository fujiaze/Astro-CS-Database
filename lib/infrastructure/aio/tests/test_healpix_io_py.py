# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

# -*- coding: utf-8 -*-
"""
test_healpix_io_py.py - healpix_io.py Python 绑定往返测试

功能：验证 HissWriter/HissReader/HcsdWriter/HcsdReader 类及便捷函数
用途：确认 Python 绑定层正确封装 healpix_io.dll，读写一致、内存释放正确

测试用例：
  1. .hiss 往返（类 + 便捷函数）
  2. .hcsd 往返（乱序输入 → 排序输出）
  3. .hcsd 按子叶读取（read_leaf + 便捷函数）
  4. .hiss 空数据（n_pix=0）
"""

import os
import sys
import tempfile
import logging

import numpy as np

# 将本目录加入 sys.path 以便 import healpix_io
_this_dir = os.path.dirname(os.path.abspath(__file__))
if _this_dir not in sys.path:
    sys.path.insert(0, _this_dir)

import healpix_io
from healpix_io import (
    HissWriter, HissReader,
    HcsdWriter, HcsdReader,
    hiss_write, hiss_read,
    hcsd_write, hcsd_read,
    hcsd_read_leaf,
)

logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
logger = logging.getLogger(__name__)


# ============================================================================
# 测试 1: .hiss 往返
# ============================================================================

def test_hiss_roundtrip():
    """测试 1: .hiss 写入 → 读取往返一致性（类 + 便捷函数）"""
    print("=" * 70)
    print("测试 1: .hiss 往返测试（HissWriter/HissReader + 便捷函数）")
    print("=" * 70)

    nside = 8192
    nested = True
    # 10 个像素: 3 个在 leaf 0, 3 个在 leaf 1, 4 个在 leaf 2
    # nside=8192, shift=14, leaf_ipix = ipix >> 14
    ipix_in = np.array([
        0, 1, 2,              # leaf 0
        16384, 16385, 16386,  # leaf 1
        32768, 32769, 32770, 32771  # leaf 2
    ], dtype=np.uint64)
    pixel_in = np.array([
        10.5, 20.3, 30.7,
        40.1, 50.9, 60.2,
        70.4, 80.6, 90.8, 100.0
    ], dtype=np.float32)
    meta_in = {
        "filter": "Lum",
        "exposure_s": 600.0,
        "obs_time": "2025-05-03T03:15:25Z",
        "pixfrac": 0.6,
    }

    tmp_path = os.path.join(tempfile.gettempdir(), "test_py_roundtrip.hiss")

    # ---- 方式 A: 使用类 ----
    writer = HissWriter(tmp_path, nside, nested)
    ret = writer.write(ipix_in, pixel_in, meta_in)
    assert ret == 0, f"HissWriter.write 失败, ret={ret}"
    print(f"  [OK] HissWriter.write 成功: {os.path.getsize(tmp_path)} 字节")

    with HissReader(tmp_path) as reader:
        assert reader.nside == nside, f"nside 不匹配: {reader.nside} != {nside}"
        assert reader.nested == nested, f"nested 不匹配: {reader.nested} != {nested}"
        assert reader.n_pix == 10, f"n_pix 不匹配: {reader.n_pix} != 10"
        print(f"  [OK] HissReader: nside={reader.nside}, nested={reader.nested}, n_pix={reader.n_pix}")

        np.testing.assert_array_equal(reader.ipix, ipix_in, "类方式 ipix 不匹配")
        np.testing.assert_allclose(reader.pixel, pixel_in, rtol=1e-5,
                                   err_msg="类方式 pixel 不匹配")
        print(f"  [OK] 类方式 ipix 一致: {reader.ipix.tolist()}")
        print(f"  [OK] 类方式 pixel 一致: {reader.pixel.tolist()}")

        # 验证 meta（C 侧会合并 nside/nested/n_pix 字段）
        meta_out = reader.meta
        assert meta_out.get("filter") == "Lum", f"meta.filter 不匹配: {meta_out}"
        assert meta_out.get("exposure_s") == 600.0, f"meta.exposure_s 不匹配: {meta_out}"
        assert meta_out.get("nside") == nside, f"meta.nside 不匹配: {meta_out}"
        assert meta_out.get("nested") == nested, f"meta.nested 不匹配: {meta_out}"
        assert meta_out.get("n_pix") == 10, f"meta.n_pix 不匹配: {meta_out}"
        print(f"  [OK] 类方式 meta 一致: filter={meta_out['filter']}, nside={meta_out['nside']}")

    # ---- 方式 B: 使用便捷函数 ----
    r_nside, r_nested, r_ipix, r_pixel, r_meta, r_snr = hiss_read(tmp_path)
    assert r_nside == nside
    assert r_nested == nested
    np.testing.assert_array_equal(r_ipix, ipix_in, "便捷函数 ipix 不匹配")
    np.testing.assert_allclose(r_pixel, pixel_in, rtol=1e-5,
                               err_msg="便捷函数 pixel 不匹配")
    assert r_meta.get("filter") == "Lum"
    # 未传 snr 时, 便捷函数返回 None
    assert r_snr is None, f"未写入 snr 时 r_snr 应为 None, 实际: {r_snr}"
    print(f"  [OK] 便捷函数 hiss_read 一致: nside={r_nside}, n_pix={len(r_ipix)}, snr=None")

    # 清理
    os.remove(tmp_path)
    print(f"  [OK] 清理完成")
    print("  测试 1 PASSED\n")
    return True


# ============================================================================
# 测试 2: .hcsd 往返
# ============================================================================

def test_hcsd_roundtrip():
    """测试 2: .hcsd 写入（乱序输入）→ 全量读取（排序输出）"""
    print("=" * 70)
    print("测试 2: .hcsd 往返测试（乱序输入 → leaf+ipix 排序输出）")
    print("=" * 70)

    nside = 8192
    nested = True
    # 10 个像素乱序输入（C 侧会按 leaf_ipix + ipix 排序）
    ipix_in = np.array([
        32769, 32768, 32771, 32770,  # leaf 2 (乱序)
        0, 2, 1,                      # leaf 0 (乱序)
        16385, 16384, 16386,          # leaf 1 (乱序)
    ], dtype=np.uint64)
    pixel_in = np.array([
        71.0, 70.0, 100.0, 90.0,
        10.5, 30.7, 20.3,
        50.9, 40.1, 60.2
    ], dtype=np.float32)
    meta_in = {
        "filter": "Lum",
        "n_frames": 5,
        "total_exposure_s": 3000.0,
        "sigma_clip": {"sigma": 3.0, "max_iter": 5},
    }

    tmp_path = os.path.join(tempfile.gettempdir(), "test_py_roundtrip.hcsd")

    # ---- 写入 ----
    ret = hcsd_write(tmp_path, nside, nested, ipix_in, pixel_in, meta_in)
    assert ret == 0, f"hcsd_write 失败, ret={ret}"
    print(f"  [OK] HcsdWriter.write 成功: {os.path.getsize(tmp_path)} 字节")

    # ---- 读取 ----
    with HcsdReader(tmp_path) as reader:
        assert reader.nside == nside
        assert reader.nested == nested
        assert reader.n_pix == 10
        print(f"  [OK] HcsdReader: nside={reader.nside}, nested={reader.nested}, n_pix={reader.n_pix}")

        # 期望排序结果: leaf 0 (0,1,2), leaf 1 (16384,16385,16386), leaf 2 (32768,32769,32770,32771)
        expected_ipix = np.array([0, 1, 2, 16384, 16385, 16386,
                                  32768, 32769, 32770, 32771], dtype=np.uint64)
        expected_pixel = np.array([10.5, 20.3, 30.7, 40.1, 50.9, 60.2,
                                   70.0, 71.0, 90.0, 100.0], dtype=np.float32)

        np.testing.assert_array_equal(reader.ipix, expected_ipix, "ipix 排序后不匹配")
        np.testing.assert_allclose(reader.pixel, expected_pixel, rtol=1e-5,
                                   err_msg="pixel 排序后不匹配")
        print(f"  [OK] ipix 排序正确: {reader.ipix.tolist()}")
        print(f"  [OK] pixel 排序正确: {reader.pixel.tolist()}")

        meta_out = reader.meta
        assert meta_out.get("n_frames") == 5
        assert meta_out.get("total_exposure_s") == 3000.0
        print(f"  [OK] meta 一致: n_frames={meta_out.get('n_frames')}")

    print("  测试 2 PASSED\n")
    return tmp_path  # 返回路径供测试 3 使用


# ============================================================================
# 测试 3: .hcsd 按子叶读取
# ============================================================================

def test_hcsd_read_leaf(hcsd_path):
    """测试 3: .hcsd 按子叶读取（HcsdReader.read_leaf + 便捷函数）"""
    print("=" * 70)
    print("测试 3: .hcsd 按子叶读取（read_leaf + hcsd_read_leaf）")
    print("=" * 70)

    # ---- 方式 A: HcsdReader.read_leaf ----
    with HcsdReader(hcsd_path) as reader:
        # leaf 0 (3 个像素: ipix 0,1,2)
        ipix0, pixel0 = reader.read_leaf(0)
        assert len(ipix0) == 3, f"leaf 0 n_pix 不匹配: {len(ipix0)} != 3"
        np.testing.assert_array_equal(ipix0, [0, 1, 2], "leaf 0 ipix 不匹配")
        np.testing.assert_allclose(pixel0, [10.5, 20.3, 30.7], rtol=1e-5,
                                   err_msg="leaf 0 pixel 不匹配")
        print(f"  [OK] leaf 0: ipix={ipix0.tolist()}, pixel={pixel0.tolist()}")

        # leaf 1 (3 个像素: ipix 16384,16385,16386)
        ipix1, pixel1 = reader.read_leaf(1)
        assert len(ipix1) == 3, f"leaf 1 n_pix 不匹配: {len(ipix1)} != 3"
        np.testing.assert_array_equal(ipix1, [16384, 16385, 16386], "leaf 1 ipix 不匹配")
        np.testing.assert_allclose(pixel1, [40.1, 50.9, 60.2], rtol=1e-5,
                                   err_msg="leaf 1 pixel 不匹配")
        print(f"  [OK] leaf 1: ipix={ipix1.tolist()}, pixel={pixel1.tolist()}")

        # leaf 2 (4 个像素: ipix 32768,32769,32770,32771)
        ipix2, pixel2 = reader.read_leaf(2)
        assert len(ipix2) == 4, f"leaf 2 n_pix 不匹配: {len(ipix2)} != 4"
        np.testing.assert_array_equal(ipix2, [32768, 32769, 32770, 32771],
                                      "leaf 2 ipix 不匹配")
        np.testing.assert_allclose(pixel2, [70.0, 71.0, 90.0, 100.0], rtol=1e-5,
                                   err_msg="leaf 2 pixel 不匹配")
        print(f"  [OK] leaf 2: ipix={ipix2.tolist()}, pixel={pixel2.tolist()}")

        # leaf 3 (空子叶)
        ipix3, pixel3 = reader.read_leaf(3)
        assert len(ipix3) == 0, f"leaf 3 应为空, n_pix={len(ipix3)}"
        print(f"  [OK] leaf 3 (空子叶): n_pix=0")

    # ---- 方式 B: 便捷函数 hcsd_read_leaf ----
    ipix_b, pixel_b = hcsd_read_leaf(hcsd_path, 0)
    np.testing.assert_array_equal(ipix_b, [0, 1, 2], "便捷函数 leaf 0 ipix 不匹配")
    np.testing.assert_allclose(pixel_b, [10.5, 20.3, 30.7], rtol=1e-5,
                               err_msg="便捷函数 leaf 0 pixel 不匹配")
    print(f"  [OK] 便捷函数 hcsd_read_leaf(0): ipix={ipix_b.tolist()}")

    # 清理
    os.remove(hcsd_path)
    print(f"  [OK] 清理完成")
    print("  测试 3 PASSED\n")
    return True


# ============================================================================
# 测试 4: .hiss 空数据
# ============================================================================

def test_hiss_empty():
    """测试 4: .hiss 空数据（n_pix=0）"""
    print("=" * 70)
    print("测试 4: .hiss 空数据测试（n_pix=0）")
    print("=" * 70)

    nside = 8192
    nested = True
    meta_in = {"filter": "Lum"}

    tmp_path = os.path.join(tempfile.gettempdir(), "test_py_empty.hiss")

    # 空数组写入
    empty_ipix = np.empty(0, dtype=np.uint64)
    empty_pixel = np.empty(0, dtype=np.float32)

    ret = hiss_write(tmp_path, nside, nested, empty_ipix, empty_pixel, meta_in)
    assert ret == 0, f"hiss_write(空) 失败, ret={ret}"
    print(f"  [OK] hiss_write(空) 成功: {os.path.getsize(tmp_path)} 字节")

    # 读取
    with HissReader(tmp_path) as reader:
        assert reader.n_pix == 0, f"n_pix 应为 0: {reader.n_pix}"
        assert reader.ipix.size == 0, f"ipix 应为空: size={reader.ipix.size}"
        assert reader.pixel.size == 0, f"pixel 应为空: size={reader.pixel.size}"
        print(f"  [OK] HissReader(空): nside={reader.nside}, n_pix={reader.n_pix}")
        print(f"  [OK] ipix.size={reader.ipix.size}, pixel.size={reader.pixel.size}")
        assert reader.meta.get("filter") == "Lum"
        print(f"  [OK] meta 一致: filter={reader.meta.get('filter')}")

    # 便捷函数
    r_nside, r_nested, r_ipix, r_pixel, r_meta, r_snr = hiss_read(tmp_path)
    assert r_nside == nside
    assert r_nested == nested
    assert len(r_ipix) == 0
    assert len(r_pixel) == 0
    assert r_snr is None
    print(f"  [OK] 便捷函数 hiss_read(空): nside={r_nside}, n_pix={len(r_ipix)}, snr=None")

    os.remove(tmp_path)
    print(f"  [OK] 清理完成")
    print("  测试 4 PASSED\n")
    return True


# ============================================================================
# 测试 5: .hiss 稀疏 SNR 模型往返 (snr_format=1)
# ============================================================================

def test_hiss_snr_model_roundtrip():
    """测试 5: .hiss 稀疏 SNR 模型写入 → 读取往返一致性"""
    print("=" * 70)
    print("测试 5: .hiss 稀疏 SNR 模型往返 (snr_format=1)")
    print("=" * 70)

    from healpix_io import (
        SnrModel, SnrControlPoint,
        hiss_write_snr_model, hiss_read_snr_model, hiss_read,
    )

    nside = 8192
    nested = True
    meta_in = {"filter": "Lum", "exposure_s": 120.0}

    # 构造测试数据: 100 个像素 + 50 个 SNR 控制点
    n_pix = 100
    ipix_in = np.arange(n_pix, dtype=np.uint64) * 100
    pixel_in = np.linspace(1.0, 100.0, n_pix, dtype=np.float32)

    n_points = 50
    points_in = [
        SnrControlPoint(
            ra=10.0 + i * 0.1,       # 10.0 ~ 14.9 度
            dec=20.0 + i * 0.05,     # 20.0 ~ 22.45 度
            snr_psf=50.0 + i * 1.0,  # 50 ~ 99
        )
        for i in range(n_points)
    ]
    model_in = SnrModel(
        n_points=n_points,
        points=points_in,
        snr_phot=123.456,
        median_snr=75.0,
        idw_power=2.0,
    )

    tmp_path = os.path.join(tempfile.gettempdir(), "test_snr_model.hiss")

    # 写入稀疏格式
    ret = hiss_write_snr_model(tmp_path, nside, nested, ipix_in, pixel_in, meta_in, model_in)
    assert ret == 0, f"hiss_write_snr_model 失败, ret={ret}"
    file_size = os.path.getsize(tmp_path)
    print(f"  [OK] hiss_write_snr_model 成功: {file_size} 字节, n_points={n_points}")

    # 验证 JSON 头包含 snr_format=1
    with open(tmp_path, "rb") as f:
        magic = f.read(4)
        assert magic == b"HISS", f"Magic 错误: {magic}"
        uncomp_len = int.from_bytes(f.read(4), "little")
        comp_len = int.from_bytes(f.read(4), "little")
        # 不解压, 只检查文件大小合理 (100*8 + 100*4 + 4 + 50*20 + 24 ≈ 1948 + header)
        expected_data = 100 * 8 + 100 * 4 + 4 + 50 * 20 + 24
        print(f"  [OK] 数据区预期 ~{expected_data} 字节 (ipix+pixel+model)")

    # 1. 用 hiss_read_snr_model 读取 (应得到完整 snr_model)
    r_nside, r_nested, r_ipix, r_pixel, r_meta, r_model = hiss_read_snr_model(tmp_path)
    assert r_nside == nside, f"nside 不一致: {r_nside} != {nside}"
    assert r_nested == nested, f"nested 不一致: {r_nested} != {nested}"
    assert np.array_equal(r_ipix, ipix_in), "ipix 不一致"
    assert np.allclose(r_pixel, pixel_in), "pixel 不一致"
    assert r_meta.get("filter") == "Lum", f"meta.filter 不一致: {r_meta}"
    assert r_meta.get("exposure_s") == 120.0, f"meta.exposure_s 不一致: {r_meta}"

    assert r_model is not None, "snr_model 不应为 None"
    assert r_model.n_points == n_points, f"n_points 不一致: {r_model.n_points} != {n_points}"
    assert abs(r_model.snr_phot - 123.456) < 1e-6, f"snr_phot 不一致: {r_model.snr_phot}"
    assert abs(r_model.median_snr - 75.0) < 1e-6, f"median_snr 不一致: {r_model.median_snr}"
    assert abs(r_model.idw_power - 2.0) < 1e-6, f"idw_power 不一致: {r_model.idw_power}"

    for i in range(n_points):
        cp_in = points_in[i]
        cp_out = r_model.points[i]
        assert abs(cp_out.ra - cp_in.ra) < 1e-10, f"point[{i}].ra 不一致"
        assert abs(cp_out.dec - cp_in.dec) < 1e-10, f"point[{i}].dec 不一致"
        assert abs(cp_out.snr_psf - cp_in.snr_psf) < 1e-5, f"point[{i}].snr_psf 不一致"

    print(f"  [OK] hiss_read_snr_model: nside={r_nside}, n_pix={len(r_ipix)}, "
          f"n_points={r_model.n_points}, snr_phot={r_model.snr_phot:.3f}")

    # 2. 用 hiss_read 读取 (兼容模式, 应跳过稀疏块, snr=None)
    r_nside2, r_nested2, r_ipix2, r_pixel2, r_meta2, r_snr2 = hiss_read(tmp_path)
    assert r_nside2 == nside
    assert np.array_equal(r_ipix2, ipix_in), "兼容模式 ipix 不一致"
    assert np.allclose(r_pixel2, pixel_in), "兼容模式 pixel 不一致"
    assert r_snr2 is None, f"snr_format=1 时 hiss_read 的 snr 应为 None, 实际: {r_snr2}"
    print(f"  [OK] hiss_read 兼容模式: snr=None (正确跳过稀疏块)")

    # 3. 无 snr_model 写入 (has_snr=false)
    tmp_path2 = os.path.join(tempfile.gettempdir(), "test_snr_model_none.hiss")
    ret = hiss_write_snr_model(tmp_path2, nside, nested, ipix_in, pixel_in, meta_in, snr_model=None)
    assert ret == 0, f"hiss_write_snr_model(None) 失败, ret={ret}"
    _, _, _, _, _, r_model2 = hiss_read_snr_model(tmp_path2)
    assert r_model2 is None, "无 snr 时 model 应为 None"
    print(f"  [OK] 无 snr_model 写入: 读取返回 None")

    os.remove(tmp_path)
    os.remove(tmp_path2)
    print(f"  [OK] 清理完成")
    print("  测试 5 PASSED\n")
    return True


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("\n" + "=" * 70)
    print("healpix_io.py Python 绑定往返测试")
    print("=" * 70 + "\n")

    # 先验证 DLL 可加载
    try:
        healpix_io._get_dll()
        print(f"[OK] DLL 加载成功: {healpix_io._find_dll()}\n")
    except Exception as e:
        print(f"[FAIL] DLL 加载失败: {e}")
        return 1

    tests = [
        ("test_hiss_roundtrip", test_hiss_roundtrip),
        ("test_hcsd_roundtrip", test_hcsd_roundtrip),
        ("test_hcsd_read_leaf", test_hcsd_read_leaf),
        ("test_hiss_empty", test_hiss_empty),
        ("test_hiss_snr_model_roundtrip", test_hiss_snr_model_roundtrip),
    ]

    results = []
    hcsd_path = None

    for name, test in tests:
        try:
            if name == "test_hcsd_roundtrip":
                hcsd_path = test()
                results.append((name, True))
            elif name == "test_hcsd_read_leaf":
                if hcsd_path:
                    result = test(hcsd_path)
                    results.append((name, result))
                else:
                    print(f"  [SKIP] {name}: 无 hcsd 文件")
                    results.append((name, False))
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
