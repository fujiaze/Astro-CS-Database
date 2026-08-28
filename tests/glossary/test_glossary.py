#!/usr/bin/env python3
"""DOC-001 测试: 词典检查器 mutation 试金石。"""
import importlib.util, os, re, shutil, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("cg", os.path.join(REPO, "tools", "check_glossary.py"))
cg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cg)
REAL = open(os.path.join(REPO, "docs", "GLOSSARY.md"), encoding="utf-8").read()

def errs_for(text, roots=()):
    e = []
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "G.md")
        open(p, "w", encoding="utf-8").write(text)
        cg.parse_glossary(e, path=p)
        cg.check_banned_aliases(e, roots=list(roots) and None or list(roots))
    return e

class TestGlossary(unittest.TestCase):
    def test_01_real_repo_pass(self):
        r = __import__("subprocess").run([sys.executable, os.path.join(REPO, "tools", "check_glossary.py")],
                                         capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("GLOSSARY_PASS terms=18/18", r.stdout)

    def test_02_duplicate_term_must_fail(self):
        dup = REAL + "|adu | 伪造重复 | 单位 | - | docs/science/CALIBRATION.md#27 |\n"
        self.assertTrue(any("G2" in e for e in errs_for(dup)), "重复术语必须被抓")

    def test_03_ambiguity_phrase_must_fail(self):
        # 在含义列注入二选一表述
        amb = REAL.replace("逐像素随机方差,Drizzle 传播", "逐像素随机方差(或 TBD 二选一),Drizzle 传播", 1)
        self.assertTrue(any("G3" in e for e in errs_for(amb)), "二义表述必须被抓")

    def test_04_alias_collision_must_fail(self):
        coll = REAL + "|flux_conflict | x | y | coverage → flux | docs/science/DRIZZLE.md#46 |\n"
        self.assertTrue(any("G4" in e and "coverage" in e for e in errs_for(coll)), "alias 冲突映射必须被抓")

    def test_05_banned_alias_dn_in_science_doc_must_fail(self):
        with tempfile.TemporaryDirectory() as td:
            scan = os.path.join(td, "docs_scan")
            os.makedirs(scan)
            open(os.path.join(scan, "bad.md"), "w", encoding="utf-8").write("信号单位用 DN 表示\n")
            e = []
            cg.check_banned_aliases(e, roots=[scan])
            self.assertTrue(any("G6" in e2 and "DN" in e2 for e2 in e), "裸 DN 必须被抓")

if __name__ == "__main__":
    unittest.main(verbosity=2)
