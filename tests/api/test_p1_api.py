#!/usr/bin/env python3
"""API-003 测试: Phase1 API 登记表 ↔ 头文件签名一致性 (doc-symbol-signature checker 合同)。"""
import os, re, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
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
    "astro_calibration.h": "lib/calibration/include/astro_calibration.h",
    "star_detector.h": "lib/star_detector/include/star_detector.h",
    "dynamic_psf.h": "lib/dynamic_psf/include/dynamic_psf.h",
    "ipv_api.h": "lib/plate_solve/cpp/ipv/include/ipv_api.h",
    "photometric_calib.h": "lib/photometric_calib/cpp/include/photometric_calib.h",
    "snr_estimator.h": "lib/snr_estimator/cpp/include/snr_estimator.h",
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

    def test_04_set_num_threads_marked_migration(self):
        self.assertIn("迁移整改点", self.s)
        self.assertIn("TB-ARCH-004", self.s)

    def test_05_units_reference_glossary(self):
        for k in ("ADU", "0-based", "ICRS", "host allocator"):
            self.assertIn(k, self.s)

    def test_06_stages_match_call_paths(self):
        r = open(os.path.join(REPO, "docs/architecture/production_call_paths_stage1.csv"), encoding="utf-8").read()
        n = len([l for l in r.splitlines() if l.strip()]) - 1
        self.assertIn(f"{n} 路径", self.s, f"文档须声明 stage1 {n} 路径")

if __name__ == "__main__":
    unittest.main(verbosity=2)
