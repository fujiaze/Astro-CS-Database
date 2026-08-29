#!/usr/bin/env python3
"""DOCCHK-002 测试: 六层追溯闭环 + 单位二义性 mutation 试金石。
验收(03 L133): 100% 核心 claims 闭环; 单位 mutation 被抓(修复 V3 漏检)。
- test_traceability 相关(见 tests/traceability/test_traceability.py, TRACE-001 已建);
  本文件新增 DOCCHK-002 范围:
  ① 干净仓库 → check_traceability + check_unit_closure 双双 PASS;
  ② 六层表: 删除任一层引用/删除一行 → check_traceability FAIL;
  ③ 单位: 注入 Drizzle 式无冻结二义("ADU 或 ADU/pixel" / 裸 "ADU/e⁻") → check_unit_closure FAIL;
  ④ 单位: 注入但带冻结语句 → check_unit_closure PASS。
"""
import csv, importlib.util, io, os, re, shutil, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TRACE = os.path.join(REPO, "tools", "check_traceability.py")
UNIT = os.path.join(REPO, "tools", "check_unit_closure.py")
SPEC_TRACE = importlib.util.spec_from_file_location("ctb", TRACE)
ctb = importlib.util.module_from_spec(SPEC_TRACE)
SPEC_TRACE.loader.exec_module(ctb)
SPEC_UNIT = importlib.util.spec_from_file_location("cuc", UNIT)
cuc = importlib.util.module_from_spec(SPEC_UNIT)
SPEC_UNIT.loader.exec_module(cuc)

REAL_TRACE = os.path.join(REPO, "artifacts", "prerelease_v5", "tables", "TRACEABILITY.csv")


class TestDocChk002(unittest.TestCase):
    # ── ① 干净仓库 PASS ──
    def test_01_real_repo_passes(self):
        r = subprocess.run([sys.executable, TRACE], capture_output=True, text=True, cwd=REPO, timeout=120)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("TRACEABILITY_PASS claims=66", r.stdout)
        ru = subprocess.run([sys.executable, UNIT], capture_output=True, text=True, cwd=REPO, timeout=120)
        self.assertEqual(ru.returncode, 0, ru.stdout + ru.stderr)

    # ── ② 六层表 mutation: 删层/删行 → FAIL ──
    def _run_checker(self, content):
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "T.csv")
            open(p, "w", encoding="utf-8").write(content)
            errs = []
            ctb.check_table(p, errs)
            return errs

    def test_02_delete_sci_doc_link_fails(self):
        rows = list(csv.reader(open(REAL_TRACE, encoding="utf-8")))
        rows[1][1] = ""  # 删 SCI-CAL-001 的 science_doc
        sio = io.StringIO(); csv.writer(sio, lineterminator="\n").writerows(rows)
        self.assertTrue(any("R3" in e for e in self._run_checker(sio.getvalue())), "删 SCI 引用必 FAIL")

    def test_03_delete_api_symbol_fails(self):
        rows = list(csv.reader(open(REAL_TRACE, encoding="utf-8")))
        # 删除 API-WCS-001 行的 api_symbol
        for r in rows:
            if r and r[0] == "API-WCS-001":
                r[6] = ""
        sio = io.StringIO(); csv.writer(sio, lineterminator="\n").writerows(rows)
        self.assertTrue(any("R3" in e for e in self._run_checker(sio.getvalue())), "删 API symbol 必 FAIL")

    def test_04_delete_oracle_fails(self):
        rows = list(csv.reader(open(REAL_TRACE, encoding="utf-8")))
        for r in rows:
            if r and r[0] == "TEST-DRZ-001":
                r[3] = ""  # 删 algorithm_doc → 断链
        sio = io.StringIO(); csv.writer(sio, lineterminator="\n").writerows(rows)
        self.assertTrue(any("R3" in e for e in self._run_checker(sio.getvalue())), "删 ALG 引用必 FAIL")

    def test_05_removing_test_endpoint_fails_R6(self):
        rows = list(csv.reader(open(REAL_TRACE, encoding="utf-8")))
        # 删除所有 TEST-INT-* 行 → 域 INT 断链
        rows = [r for r in rows if not (r and r[0].startswith("TEST-INT"))]
        sio = io.StringIO(); csv.writer(sio, lineterminator="\n").writerows(rows)
        self.assertTrue(any("R6" in e for e in self._run_checker(sio.getvalue())), "缺 TEST 端点必 FAIL")

    # ── ③ 单位 mutation ──
    def _unit_errs(self, text, rel="docs/science/N.md"):
        errs = []
        cuc._scan_local = getattr(cuc, "_scan", None)
        # 用 Chk._scan 单文档
        c = cuc.Chk(REPO)
        for e in c._scan(rel, text):
            errs.append(e)
        return errs

    def test_06_unit_ambiguity_injection_fails(self):
        # 注入无冻结语句的 Drizzle 式二义
        t = "# Driz\n## 3 单位\n- `S,F,x`: ADU/e⁻\n"
        self.assertTrue(len(self._unit_errs(t)) > 0, "裸 ADU/e⁻ 单位二义必被抓")

    def test_07_unit_or_ambiguity_injection_fails(self):
        t = "# Cal\n## 3 单位\n- 核心单位: ADU 或 ADU/pixel\n"
        self.assertTrue(len(self._unit_errs(t)) > 0, "'ADU 或 ADU/pixel' 二选一必被抓")

    def test_08_unit_ambiguity_with_freezing_ok(self):
        t = "# Driz\n## 3 单位\n- `S,F,x`: ADU/e⁻（语义固定：以 ADU 为主，e⁻ 为等价标注）\n"
        self.assertEqual(self._unit_errs(t), [], "带冻结语句应放行")


if __name__ == "__main__":
    unittest.main(verbosity=2)
