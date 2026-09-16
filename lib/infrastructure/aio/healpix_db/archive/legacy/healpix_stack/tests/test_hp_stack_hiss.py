# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

# -*- coding: utf-8 -*-
"""
test_hp_stack_hiss.py - 内存 sigma-clip 堆叠测试 (.hiss → .hcsd)

功能: 验证 hp_stack_hiss C++ 函数的端到端流程
用途: 创建 3 个 .hiss 测试帧 → 堆叠 → 读取 .hcsd 验证结果

测试用例:
  1. 基本3帧堆叠: 3 帧相同 ipix, 值接近 → mean ≈ 均值
  2. sigma-clip 剔除: 注入离群值, 验证被剔除
  3. 不同 ipix 集合: 各帧覆盖不同像素 → 合并
"""

import os
import sys
import tempfile
import logging

import numpy as np

# 将 healpix_stack 和 healpix_io 目录加入 sys.path
_this_dir = os.path.dirname(os.path.abspath(__file__))
stack_dir = os.path.normpath(os.path.join(_this_dir, ".."))
hio_dir = os.path.normpath(os.path.join(_this_dir, "..", "..", "healpix_io"))
for d in (stack_dir, hio_dir):
    if d not in sys.path:
        sys.path.insert(0, d)

from healpix_io import HissWriter, HissReader, HcsdReader
from healpix_stack import stack_hiss_files

logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
logger = logging.getLogger(__name__)


# ============================================================================
# 测试 1: 基本3帧堆叠
# ============================================================================

def test_basic_stack():
    """测试 1: 3 帧相同 ipix, 值接近 → mean ≈ 均值"""
    print("=" * 70)
    print("测试 1: 基本3帧堆叠 (3 帧相同 ipix, 值接近)")
    print("=" * 70)

    nside = 8192
    nested = True
    # 5 个像素, 都在 leaf 0
    ipix = np.array([0, 1, 2, 3, 4], dtype=np.uint64)

    tmp_dir = tempfile.mkdtemp(prefix="hps_test_")
    hiss_paths = []

    # 3 帧值: 10, 12, 14 → mean=12
    frame_values = [
        np.array([10.0, 20.0, 30.0, 40.0, 50.0], dtype=np.float32),
        np.array([12.0, 22.0, 32.0, 42.0, 52.0], dtype=np.float32),
        np.array([14.0, 24.0, 34.0, 44.0, 54.0], dtype=np.float32),
    ]

    for i, vals in enumerate(frame_values):
        path = os.path.join(tmp_dir, f"frame_{i}.hiss")
        writer = HissWriter(path, nside, nested)
        meta = {"filter": "Lum", "exposure_s": 600.0,
                "obs_time": f"2025-05-03T0{i}:00:00Z"}
        writer.write(ipix, vals, meta)
        hiss_paths.append(path)
        print(f"  [OK] 写入帧 {i}: {path} (values={vals.tolist()})")

    # 堆叠
    out_hcsd = os.path.join(tmp_dir, "stacked.hcsd")
    print(f"\n  堆叠 {len(hiss_paths)} 帧 → {out_hcsd}")
    ret = stack_hiss_files(hiss_paths, out_hcsd, sigma=3.0, max_iter=5)
    assert ret == 0, f"stack_hiss_files 失败, ret={ret}"
    print(f"  [OK] 堆叠成功: ret={ret}")

    # 读取 .hcsd 验证
    with HcsdReader(out_hcsd) as reader:
        assert reader.nside == nside, f"nside 不匹配: {reader.nside} != {nside}"
        assert reader.nested == nested
        assert reader.n_pix == 5, f"n_pix 不匹配: {reader.n_pix} != 5"
        print(f"  [OK] HcsdReader: nside={reader.nside}, n_pix={reader.n_pix}")

        # 验证 ipix 排序
        expected_ipix = np.array([0, 1, 2, 3, 4], dtype=np.uint64)
        np.testing.assert_array_equal(reader.ipix, expected_ipix, "ipix 不匹配")

        # 验证 mean 值 (10+12+14)/3 = 12, (20+22+24)/3 = 22, ...
        expected_pixel = np.array([12.0, 22.0, 32.0, 42.0, 52.0], dtype=np.float32)
        np.testing.assert_allclose(reader.pixel, expected_pixel, rtol=1e-5,
                                   err_msg="pixel mean 不匹配")
        print(f"  [OK] ipix: {reader.ipix.tolist()}")
        print(f"  [OK] pixel (mean): {reader.pixel.tolist()}")
        print(f"  [OK] 期望 mean:    {expected_pixel.tolist()}")

        # 验证 meta
        meta = reader.meta
        assert meta.get("filter") == "Lum", f"filter 不匹配: {meta}"
        assert meta.get("n_frames") == 3, f"n_frames 不匹配: {meta}"
        assert meta.get("total_exposure_s") == 1800.0, f"total_exposure_s 不匹配: {meta}"
        sc = meta.get("sigma_clip", {})
        assert sc.get("sigma") == 3.0
        assert sc.get("max_iter") == 5
        print(f"  [OK] meta: filter={meta.get('filter')}, n_frames={meta.get('n_frames')}, "
              f"total_exposure_s={meta.get('total_exposure_s')}")
        print(f"  [OK] sigma_clip: {sc}")

    # 清理
    for p in hiss_paths:
        os.remove(p)
    os.remove(out_hcsd)
    os.rmdir(tmp_dir)
    print(f"  [OK] 清理完成")
    print("  测试 1 PASSED\n")
    return True


# ============================================================================
# 测试 2: sigma-clip 剔除离群值
# ============================================================================

def test_sigma_clip():
    """测试 2: 11 帧中注入 1 个离群值, 验证 sigma-clip 剔除

    注: mean+std sigma-clip 需要足够多的正常帧才能有效剔除单个离群值。
    5 帧 (4 正常 + 1 离群) 时, 离群值会把 std 拉得太大导致无法剔除。
    11 帧 (10 正常 + 1 离群) 时, 正常帧占主导, 离群值可被有效剔除。
    """
    print("=" * 70)
    print("测试 2: sigma-clip 剔除离群值 (11 帧: 10 正常 + 1 离群)")
    print("=" * 70)

    nside = 8192
    nested = True
    # 3 个像素
    ipix = np.array([0, 1, 2], dtype=np.uint64)

    tmp_dir = tempfile.mkdtemp(prefix="hps_clip_")
    hiss_paths = []

    # 10 帧正常值 + 1 帧离群值
    # 正常值: 10..19 → mean=14.5, std≈2.87
    # 含离群值后: mean=(145+100)/11≈22.27, std≈24.73, 3σ≈74.19
    # |100-22.27|=77.73 > 74.19 → 被剔除
    # 剔除后 mean=(10+11+...+19)/10 = 14.5
    frame_values = []
    for i in range(10):
        frame_values.append(np.array([10.0 + i, 20.0 + i, 30.0 + i],
                                      dtype=np.float32))
    # 第 11 帧注入离群值
    frame_values.append(np.array([100.0, 200.0, 300.0], dtype=np.float32))

    for i, vals in enumerate(frame_values):
        path = os.path.join(tmp_dir, f"frame_{i}.hiss")
        writer = HissWriter(path, nside, nested)
        meta = {"filter": "Lum", "exposure_s": 600.0}
        writer.write(ipix, vals, meta)
        hiss_paths.append(path)

    print(f"  正常帧 (0-9): values={[v.tolist() for v in frame_values[:10]]}")
    print(f"  离群帧 (10):  values={frame_values[10].tolist()}")
    print(f"  期望: 第11帧 (100/200/300) 被剔除, mean=(10+11+...+19)/10=14.5")

    # 堆叠
    out_hcsd = os.path.join(tmp_dir, "stacked_clip.hcsd")
    ret = stack_hiss_files(hiss_paths, out_hcsd, sigma=3.0, max_iter=5)
    assert ret == 0, f"stack_hiss_files 失败, ret={ret}"

    with HcsdReader(out_hcsd) as reader:
        assert reader.n_pix == 3
        # 期望: (10+11+...+19)/10 = 14.5, (20+21+...+29)/10 = 24.5, ...
        expected_pixel = np.array([14.5, 24.5, 34.5], dtype=np.float32)
        np.testing.assert_allclose(reader.pixel, expected_pixel, rtol=1e-5,
                                   err_msg="sigma-clip 后 mean 不匹配")
        print(f"  [OK] ipix: {reader.ipix.tolist()}")
        print(f"  [OK] pixel (sigma-clip 后): {reader.pixel.tolist()}")
        print(f"  [OK] 期望 mean:             {expected_pixel.tolist()}")
        print(f"  [OK] 离群值已被剔除!")

    # 清理
    for p in hiss_paths:
        os.remove(p)
    os.remove(out_hcsd)
    os.rmdir(tmp_dir)
    print(f"  [OK] 清理完成")
    print("  测试 2 PASSED\n")
    return True


# ============================================================================
# 测试 3: 不同 ipix 集合合并
# ============================================================================

def test_merge_different_ipix():
    """测试 3: 各帧覆盖不同像素 → 合并"""
    print("=" * 70)
    print("测试 3: 不同 ipix 集合合并")
    print("=" * 70)

    nside = 8192
    nested = True

    tmp_dir = tempfile.mkdtemp(prefix="hps_merge_")
    hiss_paths = []

    # 帧 1: ipix [0, 1, 2]
    # 帧 2: ipix [2, 3, 4]  (ipix 2 重叠)
    # 帧 3: ipix [5, 6]     (无重叠)
    frames = [
        (np.array([0, 1, 2], dtype=np.uint64),
         np.array([10.0, 20.0, 30.0], dtype=np.float32)),
        (np.array([2, 3, 4], dtype=np.uint64),
         np.array([40.0, 50.0, 60.0], dtype=np.float32)),
        (np.array([5, 6], dtype=np.uint64),
         np.array([70.0, 80.0], dtype=np.float32)),
    ]

    for i, (ip, vals) in enumerate(frames):
        path = os.path.join(tmp_dir, f"frame_{i}.hiss")
        writer = HissWriter(path, nside, nested)
        meta = {"filter": "Lum", "exposure_s": 300.0}
        writer.write(ip, vals, meta)
        hiss_paths.append(path)
        print(f"  [OK] 帧 {i}: ipix={ip.tolist()}, vals={vals.tolist()}")

    # 堆叠
    out_hcsd = os.path.join(tmp_dir, "merged.hcsd")
    ret = stack_hiss_files(hiss_paths, out_hcsd, sigma=3.0, max_iter=5)
    assert ret == 0, f"stack_hiss_files 失败, ret={ret}"

    with HcsdReader(out_hcsd) as reader:
        # 期望合并后: ipix [0,1,2,3,4,5,6], 共 7 个
        assert reader.n_pix == 7, f"n_pix 不匹配: {reader.n_pix} != 7"
        expected_ipix = np.array([0, 1, 2, 3, 4, 5, 6], dtype=np.uint64)
        np.testing.assert_array_equal(reader.ipix, expected_ipix, "ipix 合并不匹配")

        # ipix 2 有两帧: 30, 40 → mean=35
        # 其他 ipix 各一帧
        expected_pixel = np.array([10.0, 20.0, 35.0, 50.0, 60.0, 70.0, 80.0],
                                  dtype=np.float32)
        np.testing.assert_allclose(reader.pixel, expected_pixel, rtol=1e-5,
                                   err_msg="pixel 合并不匹配")
        print(f"  [OK] 合并后 ipix: {reader.ipix.tolist()}")
        print(f"  [OK] 合并后 pixel: {reader.pixel.tolist()}")
        print(f"  [OK] ipix 2 的 mean = (30+40)/2 = 35 ✓")

        # 验证 total_exposure_s = 300*3 = 900
        meta = reader.meta
        assert meta.get("n_frames") == 3
        assert meta.get("total_exposure_s") == 900.0
        print(f"  [OK] meta: n_frames={meta.get('n_frames')}, "
              f"total_exposure_s={meta.get('total_exposure_s')}")

    # 清理
    for p in hiss_paths:
        os.remove(p)
    os.remove(out_hcsd)
    os.rmdir(tmp_dir)
    print(f"  [OK] 清理完成")
    print("  测试 3 PASSED\n")
    return True


# ============================================================================
# 测试 4: SNR² 加权基本 (有 snr 通道)
# ============================================================================

def test_snr_weighted_basic():
    """测试 4: SNR² 加权堆叠

    帧1: v=[10], snr=[10] → weight = 10² = 100
    帧2: v=[20], snr=[20] → weight = 20² = 400
    期望加权平均: (10*100 + 20*400) / (100+400) = 9000/500 = 18.0
    """
    print("=" * 70)
    print("测试 4: SNR² 加权基本 (2 帧, snr 通道)")
    print("=" * 70)

    nside = 8192
    nested = True
    ipix = np.array([0], dtype=np.uint64)

    tmp_dir = tempfile.mkdtemp(prefix="hps_snr_")
    hiss_paths = []

    # 帧1: v=10, snr=10 → w=100
    # 帧2: v=20, snr=20 → w=400
    frames = [
        (np.array([10.0], dtype=np.float32), np.array([10.0], dtype=np.float32)),
        (np.array([20.0], dtype=np.float32), np.array([20.0], dtype=np.float32)),
    ]

    for i, (vals, snr) in enumerate(frames):
        path = os.path.join(tmp_dir, f"frame_{i}.hiss")
        writer = HissWriter(path, nside, nested)
        meta = {"filter": "Lum", "exposure_s": 600.0}
        writer.write(ipix, vals, meta, snr=snr)
        hiss_paths.append(path)
        print(f"  [OK] 写入帧 {i}: v={vals.tolist()}, snr={snr.tolist()}, "
              f"w={snr[0]**2:.0f}")

    out_hcsd = os.path.join(tmp_dir, "stacked_snr.hcsd")
    ret = stack_hiss_files(hiss_paths, out_hcsd, sigma=3.0, max_iter=5)
    assert ret == 0, f"stack_hiss_files 失败, ret={ret}"

    with HcsdReader(out_hcsd) as reader:
        assert reader.n_pix == 1
        # 期望: (10*100 + 20*400) / 500 = 18.0
        expected = 18.0
        np.testing.assert_allclose(reader.pixel, [expected], rtol=1e-5,
                                   err_msg="SNR² 加权 mean 不匹配")
        print(f"  [OK] pixel (SNR² 加权): {reader.pixel.tolist()}")
        print(f"  [OK] 期望:             {expected}")
        print(f"  [OK] SNR² 加权正确! "
              f"(w1=100, w2=400, weighted_mean=18.0)")

    for p in hiss_paths:
        os.remove(p)
    os.remove(out_hcsd)
    os.rmdir(tmp_dir)
    print(f"  [OK] 清理完成")
    print("  测试 4 PASSED\n")
    return True


# ============================================================================
# 测试 5: 向后兼容 (无 snr 通道, 等权)
# ============================================================================

def test_snr_backward_compat():
    """测试 5: 向后兼容 - 无 snr 通道时等权堆叠

    帧1: v=[10], 无 snr → weight = 1.0
    帧2: v=[20], 无 snr → weight = 1.0
    期望等权平均: (10+20)/2 = 15.0
    """
    print("=" * 70)
    print("测试 5: 向后兼容 (无 snr 通道, 等权)")
    print("=" * 70)

    nside = 8192
    nested = True
    ipix = np.array([0], dtype=np.uint64)

    tmp_dir = tempfile.mkdtemp(prefix="hps_nosnr_")
    hiss_paths = []

    # 2 帧无 snr (向后兼容, weight=1.0)
    frame_values = [
        np.array([10.0], dtype=np.float32),
        np.array([20.0], dtype=np.float32),
    ]

    for i, vals in enumerate(frame_values):
        path = os.path.join(tmp_dir, f"frame_{i}.hiss")
        writer = HissWriter(path, nside, nested)
        meta = {"filter": "Lum", "exposure_s": 600.0}
        writer.write(ipix, vals, meta)  # 不传 snr
        hiss_paths.append(path)
        print(f"  [OK] 写入帧 {i}: v={vals.tolist()}, 无 snr (w=1.0)")

    out_hcsd = os.path.join(tmp_dir, "stacked_nosnr.hcsd")
    ret = stack_hiss_files(hiss_paths, out_hcsd, sigma=3.0, max_iter=5)
    assert ret == 0, f"stack_hiss_files 失败, ret={ret}"

    with HcsdReader(out_hcsd) as reader:
        assert reader.n_pix == 1
        # 期望等权: (10+20)/2 = 15.0
        expected = 15.0
        np.testing.assert_allclose(reader.pixel, [expected], rtol=1e-5,
                                   err_msg="等权 mean 不匹配")
        print(f"  [OK] pixel (等权): {reader.pixel.tolist()}")
        print(f"  [OK] 期望:         {expected}")
        print(f"  [OK] 向后兼容正确! (无 snr 时 weight=1.0)")

    for p in hiss_paths:
        os.remove(p)
    os.remove(out_hcsd)
    os.rmdir(tmp_dir)
    print(f"  [OK] 清理完成")
    print("  测试 5 PASSED\n")
    return True


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("\n" + "=" * 70)
    print("hp_stack_hiss 内存 sigma-clip 堆叠测试 (.hiss → .hcsd)")
    print("=" * 70 + "\n")

    tests = [
        ("test_basic_stack", test_basic_stack),
        ("test_sigma_clip", test_sigma_clip),
        ("test_merge_different_ipix", test_merge_different_ipix),
        ("test_snr_weighted_basic", test_snr_weighted_basic),
        ("test_snr_backward_compat", test_snr_backward_compat),
    ]

    results = []
    for name, test in tests:
        try:
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
