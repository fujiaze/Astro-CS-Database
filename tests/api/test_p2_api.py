#!/usr/bin/env python3
"""API-004 测试: Phase2 API 登记 ↔ 头文件符号实跑核对 + 所有权/预算机器门。"""
import os, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "api", "PHASE2_API_V1.md")
P2HDR = os.path.join(REPO, "lib", "phase2", "include", "astro", "phase2")

SYMBOLS = ["p2_coverage_build", "p2_coverage_free", "p2_sample_controls", "p2_sample_controls_cached",
           "p2_upm_build", "p2_upm_build_geo", "p2_upm_calibrate_block", "p2_upm_close", "p2_upm_open", "p2_upm_evaluate_c",
           "p2_reject_plan_resolve", "p2_eligibility_filter", "p2_collect_candidate_stack",
           "p2_validate_candidate_weights", "p2_reject_stack", "p2_reject_stack_ex",
           "p2_integrate_pixel", "p2_large_scale_apply", "p2_frame_id",
           "p2_stats_median", "p2_stats_mad", "p2_acr_block_eligible", "p2_block_plan"]

class TestPhase2Api(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()
        cls.hdr_text = ""
        for fn in os.listdir(P2HDR):
            if fn.endswith(".h"):
                cls.hdr_text += open(os.path.join(P2HDR, fn), encoding="utf-8", errors="replace").read()

    def test_01_every_symbol_in_headers(self):
        missing = [s for s in SYMBOLS if s not in self.hdr_text]
        self.assertEqual(missing, [], f"头文件缺符号: {missing}")

    def test_02_every_symbol_documented(self):
        missing = [s for s in SYMBOLS if s not in self.s]
        self.assertEqual(missing, [], f"文档缺登记: {missing}")

    def test_03_ownership_table_complete(self):
        for obj in ("Coverage", "ControlObservation", "UPM Model", "CandidateStack",
                    "P2PixelStack", "async_io 队列"):
            self.assertIn(obj, self.s)
        self.assertIn("p2_upm_close", self.s)

    def test_04_no_hidden_globals(self):
        self.assertIn("无隐藏全局状态", self.s)
        self.assertIn("g_model_floor", self.s)

    def test_05_budget_binding(self):
        self.assertIn("Σ≤全局 budget", self.s)
        self.assertIn("sampler=1(串行 reference)", self.s)

    def test_06_error_mapping_declared(self):
        self.assertIn("ACS_ERR_STATE", self.s)
        self.assertIn("UNDERDETERMINED", self.s)

if __name__ == "__main__":
    unittest.main(verbosity=2)
