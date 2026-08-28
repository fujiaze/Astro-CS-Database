#!/usr/bin/env python3
"""TRACE-001 测试: 追溯 checker 的 mutation 试金石 (stdlib only)。
固定样本表上验证: R2 唯一性/格式、R3 删任一层引用必失败、R4 引用存在、R6 域覆盖。"""
import csv, importlib.util, io, os, shutil, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("ctb", os.path.join(REPO, "tools", "check_traceability.py"))
ctb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ctb)

D, S1, S2, S3 = "docs/VERSIONING.md", "1-唯一版本源", "2-生成接口", "3-同步矩阵"
API_SYM, SRC, TEST_ID, ORC = "tools/gen_version.py::build_report", "tools/gen_version.py", "tests/version/test_version_consistency.py", "ORC-VER-001"

def _row(cid, alg=False, arch=False, api=False, code=False, test=False):
    r = [cid, D, S1, "", "", "", "", "", "", "", "dimensionless", "exact", "ACTIVE"]
    if alg: r[3], r[4] = D, S2
    if arch: r[5] = f"{D}#{S3}"
    if api or code or test: r[6] = API_SYM
    if code or test: r[7] = SRC
    if test: r[8], r[9] = TEST_ID, ORC
    return r

def _seed():
    buf = io.StringIO()
    w = csv.writer(buf, lineterminator="\n")
    w.writerow(ctb.HEADER)
    for r in (_row("SCI-VER-001"), _row("ALG-VER-001", alg=True), _row("CODE-VER-001", alg=True, arch=True, code=True),
              _row("TEST-VER-001", alg=True, arch=True, code=True, test=True)):
        w.writerow(r)
    return buf.getvalue()


class TestTraceability(unittest.TestCase):
    def run_checker(self, content):
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "T.csv")
            open(p, "w", encoding="utf-8").write(content)
            errs = []
            ctb.check_table(p, errs)
            return errs

    def test_01_seed_passes_on_real_repo(self):
        r = __import__("subprocess").run([sys.executable, os.path.join(REPO, "tools", "check_traceability.py")],
                                         capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("TRACEABILITY_PASS claims=6", r.stdout)

    def test_02_delete_any_layer_ref_must_fail(self):
        """mutation: 删除任一层引用 → checker 必须失败。"""
        rows = list(csv.reader(_seed().splitlines()))
        # ALG 行删 algorithm_doc (列3)
        rows[2][3] = ""
        self.assertTrue(any("R3" in e for e in self.run_checker(to_csv(rows))), "删 ALG 引用必须失败")
        # CODE 行删 science_doc (列1)
        rows = list(csv.reader(_seed().splitlines()))
        rows[3][1] = ""
        self.assertTrue(any("R3" in e for e in self.run_checker(to_csv(rows))), "删 CODE 的 SCI 引用必须失败")
        # TEST 行删 test_id (列8)
        rows = list(csv.reader(_seed().splitlines()))
        rows[4][8] = ""
        self.assertTrue(any("R3" in e for e in self.run_checker(to_csv(rows))), "删 TEST 引用必须失败")

    def test_03_duplicate_and_bad_id(self):
        dup = _seed() + _seed().splitlines()[1] + "\n"
        self.assertTrue(any("R2" in e and "重复" in e for e in self.run_checker(dup)))
        bad = _seed().replace("ALG-VER-001", "ALG-ver-001")
        self.assertTrue(any("R2" in e and "非法" in e for e in self.run_checker(bad)))

    def test_04_missing_reference_must_fail(self):
        bad = _seed().replace("docs/VERSIONING.md,2-生成接口", "docs/NO_SUCH.md,2-生成接口")
        self.assertTrue(any("R4" in e and "不存在" in e for e in self.run_checker(bad)))
        bad2 = _seed().replace("tools/gen_version.py::build_report", "tools/gen_version.py::no_such_fn")
        # 符号级: 文件存在但符号缺失 (source_symbol 列的 CODE/TEST 行)
        self.assertTrue(any("no_such_fn" in e for e in self.run_checker(bad2)))

    def test_05_broken_domain_endpoints(self):
        # 去掉 TEST 行 → 域缺 TEST 端点
        rows = [r for r in _seed().splitlines() if not r.startswith("TEST-")]
        self.assertTrue(any("R6" in e for e in self.run_checker("\n".join(rows) + "\n")))

def to_csv(rows):
    import io
    buf = io.StringIO()
    csv.writer(buf, lineterminator="\n").writerows(rows)
    return buf.getvalue()

if __name__ == "__main__":
    unittest.main(verbosity=2)
