#!/usr/bin/env python3
"""API-005 测试: Phase3 API 合同 + 拒绝清单与 SCI-P3 同源交叉核对。"""
import os, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
API = os.path.join(REPO, "docs", "api", "PHASE3_API_V1.md")
SCI = os.path.join(REPO, "docs", "science", "PHASE3_HIPS_TO_FITS.md")

class TestPhase3Api(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(API, encoding="utf-8").read()
        cls.sci = open(SCI, encoding="utf-8").read()

    def test_01_lifecycle(self):
        for fn in ("p3_session_create", "p3_session_validate", "p3_session_run",
                   "p3_session_inspect", "p3_session_destroy"):
            self.assertIn(fn, self.s)

    def test_02_request_fields(self):
        for f in ("source.hips_dir", "center", "scale_deg_per_px", "width_px", "height_px",
                  "projection", "sampler", "longitude_parity", "bitpix", "coverage_output", "max_tiles"):
            self.assertIn(f, self.s)

    def test_03_reject_list_matches_sci(self):
        for item in ("TAN", "ICRS", "多通道", "lossy", "int+BLANK", "flux-per-pixel",
                     "ACS_ERR_UNSUPPORTED", "ACS_ERR_PARAM"):
            self.assertIn(item, self.s)
        # 与 SCI-P3 同源: alpha 拒绝项须同时出现在科学合同
        for item in ("多通道", "JPEG/PNG", "int+BLANK", "flux-per-pixel"):
            self.assertIn(item, self.sci, f"SCI-P3 缺 {item} (清单须同源)")

    def test_04_missing_tile_not_error(self):
        self.assertIn("非错误", self.s)
        self.assertIn("coverage=0", self.s)
        self.assertIn("S=NaN+C=1", self.s)

    def test_05_defaults_frozen(self):
        self.assertIn("默认 bilinear", self.s)
        self.assertIn("east_left", self.s)

    def test_06_traceback_anchors(self):
        for a in ("ALG-P3-001", "ALG-P3-002", "ALG-P3-003", "ALG-P3-004", "API-002"):
            self.assertIn(a, self.s)

if __name__ == "__main__":
    unittest.main(verbosity=2)
