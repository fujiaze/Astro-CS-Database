#!/usr/bin/env python3
"""API-003 测试: Phase1 API 登记表 ↔ 头文件签名一致性 (doc-symbol-signature checker 合同)。"""
import os, re, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DOC = os.path.join(REPO, "docs", "api", "PHASE1_API_V1.md")

HEADERS = {
    "astro_calibration.h": ["ac_generate_master_bias", "ac_generate_master_dark", "ac_generate_master_flat",
                            "ac_calibrate_frame", "ac_correct_frame", "ac_calibrate_frame_f64",
                            "ac_set_num_threads", "ac_version"],
    "star_detector.h": ["sdet_detect", "sdet_detect_ex", "sdet_destroy", "sdet_free_coords"],
    "dynamic_psf.h": ["dpsf_fit", "dpsf_fit_batch", "dpsf_fit_batch_f", "dpsf_free_results"],
    "ipv_api.h": ["ipv_solve_create", "ipv_solve_destroy", "ipv_solve", "ipv_solve_from_memory"],
    "photometric_calib.h": ["pc_calibrate_simple"],
    "snr_estimator.h": ["snr_noise_model_v1", "snr_noise_model_v1_f64", "snr_noise_model_v1_fill",
                        "snr_noise_model_v1_free", "snr_noise_gain_variance"],
}
HDR_PATHS = {
    "astro_calibration.h": "lib/algorithms/calibration/include/astro_calibration.h",
    "star_detector.h": "lib/algorithms/star_detection/include/star_detector.h",
    "dynamic_psf.h": "lib/algorithms/psf/include/dynamic_psf.h",
    "ipv_api.h": "lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h",
    "photometric_calib.h": "lib/algorithms/photometry/cpp/include/photometric_calib.h",
    "snr_estimator.h": "lib/algorithms/noise_snr/cpp/include/snr_estimator.h",
}

def find_header(name):
    for p, rel in HDR_PATHS.items():
        if p == name and os.path.isfile(os.path.join(REPO, rel)):
            return os.path.join(REPO, rel)
    for dirpath, dirs, files in os.walk(os.path.join(REPO, "lib")):
        if name in files:
            return os.path.join(dirpath, name)
    return None

class TestPhase1Api(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()

    def test_01_documented_symbols_exist_in_headers(self):
        for hdr, fns in HEADERS.items():
            hp = find_header(hdr)
            self.assertIsNotNone(hp, f"{hdr} 未找到")
            text = open(hp, encoding="utf-8", errors="replace").read()
            for fn in fns:
                self.assertIn(fn, text, f"{hdr} 缺符号 {fn}")

    def test_02_lifecycle_five_functions(self):
        for fn in ("p1_session_create", "p1_session_validate", "p1_session_run",
                   "p1_session_inspect", "p1_session_destroy"):
            self.assertIn(fn, self.s)
        self.assertIn("create→validate→run→inspect", self.s)

    def test_03_every_row_has_test_id_and_concurrency(self):
        rows = [l for l in self.s.splitlines() if (l.startswith("| `") or l.startswith("| drizzle")) and "—" not in l.split("|")[-2]]
        self.assertGreaterEqual(len(rows), 10, f"登记行不足: {len(rows)}")
        for l in rows:
            self.assertRegex(l, r"(TST-|TB-)", f"行缺 test/checker ID: {l[:60]}")
            self.assertTrue(any(k in l for k in ("yes", "no")), f"行缺 reentrant/threadsafe: {l[:60]}")

    def test_04_set_num_threads_superseded_by_budget_injection(self):
        """ac_set_num_threads 的现行去向：由 p1 budget 注入取代、ABI-001 收编，
        私有线程数设置不再是可调面（内部并行列与取消点列均为 —）。

        GAP_AUDIT G2-4 / D-8：旧断言 "迁移整改点" 这一措辞在 tracked 权威文档中不存在
        （RELEASE-04 换版后 PHASE1_API_V1.md §2 表已改写为现行措辞）。此处按现行合同
        逐字断言，并解析表格行而不是全文子串，避免「换个地方出现同一串」的假绿。
        """
        rows = [l for l in self.s.splitlines() if l.startswith("| `ac_set_num_threads")]
        self.assertEqual(1, len(rows), "PHASE1_API_V1.md §2 必须恰有一行登记 ac_set_num_threads")
        row = rows[0]
        cells = [c.strip() for c in row.strip("|").split("|")]
        self.assertEqual(6, len(cells), "§2 表列数漂移: %r" % row)
        fn, reentrant, threadsafe, internal_parallel, cancel_point, test_id = cells
        self.assertIn("ac_set_num_threads", fn)
        self.assertEqual("yes", reentrant, "ac_set_num_threads 必须 reentrant")
        self.assertEqual("yes", threadsafe, "ac_set_num_threads 必须 threadsafe")
        self.assertEqual("—", internal_parallel, "ac_set_num_threads 不得再声明内部并行（已由 budget 注入取代）")
        self.assertEqual("—", cancel_point, "ac_set_num_threads 无取消点（短任务）")
        self.assertIn("TB-ARCH-004", test_id, "必须保留 checker 管控 ID")
        self.assertIn("p1 budget 注入取代", test_id, "必须声明由 p1 budget 注入取代")
        self.assertIn("ABI-001 收编", test_id, "必须声明收编归属 ABI-001")

    def test_05_units_reference_glossary(self):
        for k in ("ADU", "0-based", "ICRS", "host allocator"):
            self.assertIn(k, self.s)

    def test_06_stages_match_call_paths(self):
        r = open(os.path.join(REPO, "docs/architecture/production_call_paths_stage1.csv"), encoding="utf-8").read()
        n = len([l for l in r.splitlines() if l.strip()]) - 1
        self.assertIn(f"{n} 路径", self.s, f"文档须声明 stage1 {n} 路径")

if __name__ == "__main__":
    unittest.main(verbosity=2)
