#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-002 — WCS 闭环诊断工具单元测试

覆盖:
  1. match_pairs_kdtree: 空输入, 正常匹配, 双向校验, 边界距离
  2. compute_residuals: 空匹配, 已知残差
  3. compute_stats: 空输入, 正常, 四象限, 边缘
  4. pixel_sky_pixel_closure / sky_pixel_sky_closure: 合成 WCS 闭环
  5. build_astropy_wcs_from_header: 合成 header
  6. project_gaia_to_pixel: 合成 WCS 投影
  7. 工具独立性检查: 不依赖 PlateSolve IpvWcsResult 字段做 transform

运行:
    python test_wcs_closure.py
"""

import json
import os
import sys
import unittest
from pathlib import Path

import numpy as np

# 添加脚本路径
SCRIPT_DIR = Path(__file__).parent.resolve()
sys.path.insert(0, str(SCRIPT_DIR))

# 导入被测模块
from wcs_closure_diagnostic import (
    match_pairs_kdtree,
    _match_pairs_numpy,
    compute_residuals,
    compute_stats,
    pixel_sky_pixel_closure,
    sky_pixel_sky_closure,
    build_astropy_wcs_from_header,
    project_gaia_to_pixel,
    generate_residual_plot,
    generate_quadrant_plot,
)


class TestMatchPairs(unittest.TestCase):
    """测试 kd-tree 匹配"""

    def test_empty_detected(self):
        det = np.zeros((0, 2))
        pred = np.array([[10, 10], [20, 20]])
        m = match_pairs_kdtree(det, pred, 3.0)
        self.assertEqual(m, [])

    def test_empty_predicted(self):
        det = np.array([[10, 10], [20, 20]])
        pred = np.zeros((0, 2))
        m = match_pairs_kdtree(det, pred, 3.0)
        self.assertEqual(m, [])

    def test_both_empty(self):
        m = match_pairs_kdtree(np.zeros((0, 2)), np.zeros((0, 2)), 3.0)
        self.assertEqual(m, [])

    def test_perfect_match(self):
        # 完全重合
        det = np.array([[10, 10], [20, 20], [30, 30]], dtype=float)
        pred = det.copy()
        m = match_pairs_kdtree(det, pred, 3.0)
        self.assertEqual(len(m), 3)
        for det_idx, pred_idx, dist in m:
            self.assertAlmostEqual(dist, 0.0, places=6)

    def test_within_threshold(self):
        # 距离 1.0 应该匹配
        det = np.array([[10, 10]], dtype=float)
        pred = np.array([[11, 10]], dtype=float)
        m = match_pairs_kdtree(det, pred, 3.0)
        self.assertEqual(len(m), 1)
        self.assertAlmostEqual(m[0][2], 1.0, places=6)

    def test_outside_threshold(self):
        # 距离 5.0 不应该匹配
        det = np.array([[10, 10]], dtype=float)
        pred = np.array([[15, 10]], dtype=float)
        m = match_pairs_kdtree(det, pred, 3.0)
        self.assertEqual(m, [])

    def test_bidirectional_rejection(self):
        # 一对多: 一个检测星点附近有两个预测, 反向最近应只匹配一个
        det = np.array([[10, 10]], dtype=float)
        pred = np.array([[11, 10], [12, 10]], dtype=float)
        # max_dist=2.0: 两个 pred 都在范围内, 但反向最近只选一个
        m = match_pairs_kdtree(det, pred, 2.0)
        # 只有 pred[0] 反向最近是 det[0]
        self.assertEqual(len(m), 1)
        self.assertEqual(m[0][1], 0)  # pred_idx

    def test_numpy_fallback(self):
        # 测试 numpy 暴力匹配
        det = np.array([[10, 10], [20, 20]], dtype=float)
        pred = np.array([[11, 10], [21, 21]], dtype=float)
        m = _match_pairs_numpy(det, pred, 3.0)
        self.assertEqual(len(m), 2)


class TestComputeResiduals(unittest.TestCase):
    """测试残差计算"""

    def test_empty_matches(self):
        r, md, td = compute_residuals(np.zeros((0, 2)), np.zeros((0, 2)), [])
        self.assertEqual(r.shape, (0, 2))
        self.assertEqual(len(md), 0)
        self.assertEqual(len(td), 0)

    def test_known_residuals(self):
        det = np.array([[10, 10], [20, 20]], dtype=float)
        pred = np.array([[11, 9], [22, 18]], dtype=float)
        matches = [(0, 0, np.sqrt(2)), (1, 1, np.sqrt(8))]
        r, md, td = compute_residuals(det, pred, matches)
        # 残差 = det - pred
        np.testing.assert_allclose(r[0], [-1, 1], atol=1e-10)
        np.testing.assert_allclose(r[1], [-2, 2], atol=1e-10)
        # 总距离
        np.testing.assert_allclose(td[0], np.sqrt(2), atol=1e-10)
        np.testing.assert_allclose(td[1], np.sqrt(8), atol=1e-10)


class TestComputeStats(unittest.TestCase):
    """测试统计计算"""

    def test_empty(self):
        s = compute_stats(np.zeros((0, 2)), np.zeros(0), 100, 100, np.zeros((0, 2)))
        self.assertEqual(s["n"], 0)
        self.assertEqual(s["dist_median_px"], 0.0)
        self.assertEqual(s["quadrant_counts"], {"Q1": 0, "Q2": 0, "Q3": 0, "Q4": 0})

    def test_normal_stats(self):
        residuals = np.array([[1, 1], [-1, -1], [2, 0], [0, 2]], dtype=float)
        total_dists = np.sqrt(residuals[:, 0] ** 2 + residuals[:, 1] ** 2)
        # 检测星点分布在 4 个象限
        det_xy = np.array([[80, 20], [20, 20], [20, 80], [80, 80]], dtype=float)
        s = compute_stats(residuals, total_dists, 100, 100, det_xy)
        self.assertEqual(s["n"], 4)
        self.assertAlmostEqual(s["dist_median_px"], np.median(total_dists))
        # 四象限各 1 个
        self.assertEqual(s["quadrant_counts"]["Q1"], 1)  # (80, 20)
        self.assertEqual(s["quadrant_counts"]["Q2"], 1)  # (20, 20)
        self.assertEqual(s["quadrant_counts"]["Q3"], 1)  # (20, 80)
        self.assertEqual(s["quadrant_counts"]["Q4"], 1)  # (80, 80)

    def test_gate_thresholds(self):
        # 验证门限: median<=0.75, p90<=1.5, p99<=3.0
        residuals = np.array([[0.1, 0.1], [0.2, 0.2], [0.3, 0.3]], dtype=float)
        total_dists = np.sqrt(residuals[:, 0] ** 2 + residuals[:, 1] ** 2)
        s = compute_stats(residuals, total_dists, 100, 100, np.zeros((0, 2)))
        self.assertLessEqual(s["dist_median_px"], 0.75)
        self.assertLessEqual(s["dist_p90_px"], 1.5)
        self.assertLessEqual(s["dist_p99_px"], 3.0)


class TestClosure(unittest.TestCase):
    """测试双向闭环 (用合成 WCS)"""

    @classmethod
    def setUpClass(cls):
        # 构建一个简单的 TAN WCS (无 SIP)
        from astropy.wcs import WCS

        wcs = WCS(naxis=2)
        wcs.wcs.crpix = [50.5, 50.5]
        wcs.wcs.crval = [10.0, 20.0]
        wcs.wcs.cd = [[1e-5, 0], [0, 1e-5]]  # 0.036 arcsec/px
        wcs.wcs.ctype = ["RA---TAN", "DEC--TAN"]
        cls.wcs = wcs

    def test_pixel_sky_pixel_closure(self):
        # 完美闭环: pixel→sky→pixel 误差应该接近 0
        xy = np.array([[10, 10], [50, 50], [90, 90]], dtype=float)
        result = pixel_sky_pixel_closure(self.wcs, xy, n_samples=3)
        self.assertEqual(result["n_samples"], 3)
        # astropy WCS 闭环误差应该 < 1e-6 px
        self.assertLess(result["closure_err_median_px"], 1e-6)
        self.assertLess(result["closure_err_max_px"], 1e-6)

    def test_sky_pixel_sky_closure(self):
        # 完美闭环: sky→pixel→sky 误差应该接近 0
        ra = np.array([10.0, 10.001, 9.999])
        dec = np.array([20.0, 20.001, 19.999])
        result = sky_pixel_sky_closure(self.wcs, ra, dec, n_samples=3)
        self.assertEqual(result["n_samples"], 3)
        # astropy WCS 闭环误差应该 < 1e-8 度
        self.assertLess(result["closure_err_median_deg"], 1e-8)

    def test_empty_closure(self):
        result = pixel_sky_pixel_closure(self.wcs, np.zeros((0, 2)), n_samples=100)
        self.assertEqual(result["n_samples"], 0)

        result = sky_pixel_sky_closure(self.wcs, np.zeros(0), np.zeros(0), n_samples=100)
        self.assertEqual(result["n_samples"], 0)


class TestBuildAstropyWCS(unittest.TestCase):
    """测试从 FITS header 构建 WCS"""

    def test_build_tan_wcs(self):
        # 构造合成 FITS header
        from astropy.io.fits import Header

        h = Header()
        h["CTYPE1"] = "RA---TAN"
        h["CTYPE2"] = "DEC--TAN"
        h["CRVAL1"] = 10.0
        h["CRVAL2"] = 20.0
        h["CRPIX1"] = 50.5
        h["CRPIX2"] = 50.5
        h["CD1_1"] = 1e-5
        h["CD1_2"] = 0
        h["CD2_1"] = 0
        h["CD2_2"] = 1e-5

        wcs, summary = build_astropy_wcs_from_header(h)
        self.assertEqual(summary["ctype"], ["RA---TAN", "DEC--TAN"])
        self.assertFalse(summary["has_sip"])
        self.assertEqual(summary["sip_order"], 0)
        self.assertAlmostEqual(summary["crval"][0], 10.0)
        self.assertAlmostEqual(summary["crval"][1], 20.0)

    def test_build_tan_sip_wcs(self):
        from astropy.io.fits import Header

        h = Header()
        h["CTYPE1"] = "RA---TAN-SIP"
        h["CTYPE2"] = "DEC--TAN-SIP"
        h["CRVAL1"] = 10.0
        h["CRVAL2"] = 20.0
        h["CRPIX1"] = 50.5
        h["CRPIX2"] = 50.5
        h["CD1_1"] = 1e-5
        h["CD1_2"] = 0
        h["CD2_1"] = 0
        h["CD2_2"] = 1e-5
        h["A_ORDER"] = 2
        h["A_0_0"] = 0.0
        h["A_1_0"] = 0.0
        h["A_0_1"] = 0.0
        h["A_2_0"] = 1e-10
        h["A_1_1"] = 0.0
        h["A_0_2"] = 1e-10
        h["B_ORDER"] = 2
        h["B_0_0"] = 0.0
        h["B_1_0"] = 0.0
        h["B_0_1"] = 0.0
        h["B_2_0"] = 1e-10
        h["B_1_1"] = 0.0
        h["B_0_2"] = 1e-10

        wcs, summary = build_astropy_wcs_from_header(h)
        self.assertTrue(summary["has_sip"])
        self.assertEqual(summary["sip_order"], 2)


class TestProjectGaiaToPixel(unittest.TestCase):
    """测试 Gaia→pixel 投影"""

    @classmethod
    def setUpClass(cls):
        from astropy.wcs import WCS

        wcs = WCS(naxis=2)
        wcs.wcs.crpix = [50.5, 50.5]
        wcs.wcs.crval = [10.0, 20.0]
        wcs.wcs.cd = [[1e-5, 0], [0, 1e-5]]
        wcs.wcs.ctype = ["RA---TAN", "DEC--TAN"]
        cls.wcs = wcs

    def test_project_center(self):
        # CRVAL 投影应该接近 CRPIX
        ra = np.array([10.0])
        dec = np.array([20.0])
        pred_xy, valid = project_gaia_to_pixel(self.wcs, ra, dec)
        self.assertEqual(len(pred_xy), 1)
        self.assertTrue(valid[0])
        # CRVAL→CRPIX (1-based → 0-based 偏差应在 0.5)
        # astropy WCS 返回 0-based, 所以 CRVAL 应该投影到 CRPIX-1 = 49.5
        self.assertAlmostEqual(pred_xy[0, 0], 49.5, delta=0.5)
        self.assertAlmostEqual(pred_xy[0, 1], 49.5, delta=0.5)

    def test_project_empty(self):
        pred_xy, valid = project_gaia_to_pixel(self.wcs, np.zeros(0), np.zeros(0))
        self.assertEqual(pred_xy.shape, (0, 2))
        self.assertEqual(len(valid), 0)


class TestToolIndependence(unittest.TestCase):
    """验证工具独立性: 不依赖 PlateSolve 内部 transform

    通过 grep 源码确认工具脚本中:
      - 不导入 PlateSolve 的 WCS transform 函数
      - 不读取 IpvWcsResult 的 CD/CRPIX/CRVAL/SIP 字段做 transform
      - 仅用 astropy.wcs.WCS 做 pixel↔sky 转换
    """

    @classmethod
    def setUpClass(cls):
        cls.tool_path = SCRIPT_DIR / "wcs_closure_diagnostic.py"
        with open(cls.tool_path, "r", encoding="utf-8") as f:
            cls.source = f.read()

    def test_no_import_platesolve_transform(self):
        # 不应该导入 to_astropy_wcs (PlateSolve 的 WCS 转换)
        self.assertNotIn("from ipv_solver import to_astropy_wcs", self.source)
        self.assertNotIn("import to_astropy_wcs", self.source)

    def test_no_use_ipvwcsresult_for_transform(self):
        # 不应该读取 wcs_result.cd / wcs_result.crval / wcs_result.sip_a 等做 transform
        # 但允许读取 wcs_result.success / n_pairs / trans_order 等元信息
        # 注意: trans_order 是阶数 (1/2/3), 不是 transform 数据
        forbidden_patterns = [
            "wcs_result.cd", "wcs_result.crval", "wcs_result.crpix",
            "wcs_result.sip_a", "wcs_result.sip_b",
            "wcs_result.sip_ap", "wcs_result.sip_bp",
            "wcs_result.ctype", "wcs_result.ctype1", "wcs_result.ctype2",
        ]
        for p in forbidden_patterns:
            self.assertNotIn(p, self.source, f"发现禁止的模式: {p}")

    def test_only_astropy_wcs_for_transform(self):
        # pixel_to_world / world_to_pixel 应该通过 astropy WCS 调用
        self.assertIn("wcs.pixel_to_world", self.source)
        self.assertIn("wcs.world_to_pixel", self.source)

    def test_no_ipv_solver_to_astropy_wcs(self):
        # 不应该调用 ipv_solver.to_astropy_wcs()
        self.assertNotIn("to_astropy_wcs(", self.source)

    def test_wcs_only_from_header(self):
        # build_astropy_wcs_from_header 应该从 header 构建, 不从 IpvWcsResult
        self.assertIn("WCS(header)", self.source)


class TestPlotGeneration(unittest.TestCase):
    """测试图生成 (不验证图像内容, 只验证不抛异常)"""

    def test_residual_plot_empty(self):
        import tempfile

        with tempfile.TemporaryDirectory() as td:
            out = os.path.join(td, "test.png")
            generate_residual_plot(
                np.zeros((0, 2)), np.zeros(0), out, "test_frame",
            )
            self.assertTrue(os.path.exists(out))

    def test_residual_plot_normal(self):
        import tempfile

        with tempfile.TemporaryDirectory() as td:
            out = os.path.join(td, "test.png")
            residuals = np.random.randn(50, 2) * 0.5
            total_dists = np.sqrt(residuals[:, 0] ** 2 + residuals[:, 1] ** 2)
            generate_residual_plot(residuals, total_dists, out, "test_frame")
            self.assertTrue(os.path.exists(out))

    def test_quadrant_plot(self):
        import tempfile

        with tempfile.TemporaryDirectory() as td:
            out = os.path.join(td, "test.png")
            det = np.random.rand(100, 2) * 100
            mask = np.random.rand(100) > 0.5
            generate_quadrant_plot(det, mask, 100, 100, out, "test_frame")
            self.assertTrue(os.path.exists(out))


class TestGateCheck(unittest.TestCase):
    """测试门限检查"""

    def test_pass_gate(self):
        # median=0.3, p90=0.6, p99=0.9 应该通过
        residuals = np.array([[0.3, 0.0], [0.6, 0.0], [0.9, 0.0]], dtype=float)
        total_dists = np.sqrt(residuals[:, 0] ** 2 + residuals[:, 1] ** 2)
        s = compute_stats(residuals, total_dists, 100, 100, np.zeros((0, 2)))
        self.assertLessEqual(s["dist_median_px"], 0.75)
        self.assertLessEqual(s["dist_p90_px"], 1.5)
        self.assertLessEqual(s["dist_p99_px"], 3.0)

    def test_fail_gate(self):
        # median=1.0 应该不通过
        residuals = np.array([[1.0, 0.0], [1.0, 0.0], [1.0, 0.0]], dtype=float)
        total_dists = np.sqrt(residuals[:, 0] ** 2 + residuals[:, 1] ** 2)
        s = compute_stats(residuals, total_dists, 100, 100, np.zeros((0, 2)))
        self.assertGreater(s["dist_median_px"], 0.75)


def run_all_tests():
    """运行全部测试"""
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return result.wasSuccessful()


if __name__ == "__main__":
    print("=" * 70)
    print("P11-002 WCS 闭环诊断工具 — 单元测试")
    print("=" * 70)
    success = run_all_tests()
    print("=" * 70)
    if success:
        print("ALL TESTS PASSED")
    else:
        print("SOME TESTS FAILED")
    print("=" * 70)
    sys.exit(0 if success else 1)
