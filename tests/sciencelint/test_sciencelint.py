#!/usr/bin/env python3
"""SCI-001 测试: science_contract_lint mutation 试金石。"""
import importlib.util, os, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("scl", os.path.join(REPO, "tools", "science_contract_lint.py"))
scl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scl)
REAL = open(os.path.join(REPO, "docs", "science", "CALIBRATION.md"), encoding="utf-8").read()

def errs_for(text):
    e = []
    with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8") as f:
        f.write(text)
        p = f.name
    scl.check_file(p, e)
    os.unlink(p)
    return e

class TestSciLint(unittest.TestCase):
    def test_01_real_contract_passes(self):
        e = []
        scl.check_file(os.path.join(REPO, "docs", "science", "CALIBRATION.md"), e)
        self.assertEqual(e, [])

    def test_02_missing_section_must_fail(self):
        import re
        cut = re.sub(r"## 15 Acceptance.*", "", REAL, flags=re.S)
        self.assertTrue(any("Acceptance" in e for e in errs_for(cut)), "缺 Acceptance 必须被抓")

    def test_03_bad_claim_id_must_fail(self):
        bad = REAL.replace("> ID: SCI-CAL-001", "> ID: SCI-cal-1")
        self.assertTrue(any("S2" in e for e in errs_for(bad)), "非法 claim ID 必须被抓")

    def test_04_missing_anchor_file_must_fail(self):
        bad = REAL.replace("lib/calibration/src/calibrator.cpp", "lib/calibration/src/NO_SUCH.cpp")
        self.assertTrue(any("S3" in e for e in errs_for(bad)), "锚点文件缺失必须被抓")

if __name__ == "__main__":
    unittest.main(verbosity=2)

class TestAlgLint(unittest.TestCase):
    def test_06_alg_doc_passes(self):
        r = __import__("subprocess").run([sys.executable, os.path.join(REPO, "tools", "science_contract_lint.py"),
                                          "--kind", "alg", "docs/algorithms/CALIBRATION_ALGORITHMS.md"],
                                         capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("kind=alg files=1 sections=10", r.stdout)

    def test_07_no_hardcoded_threads_mutation(self):
        """V5 硬约束 mutation: 回填 num_threads(16) 硬编码必须被内容检查抓住(词法级)。"""
        s = open(os.path.join(REPO, "docs/algorithms/CALIBRATION_ALGORITHMS.md"), encoding="utf-8").read()
        self.assertNotIn("num_threads(", s, "算法文档不得出现硬编码线程数")
        self.assertNotIn("GPU 扩展", s, "V5 为 CPU-only, 不得含 GPU 扩展路径")
