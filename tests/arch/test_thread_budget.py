#!/usr/bin/env python3
"""ARCH-004 测试: thread budget 静态 checker 试金石。"""
import importlib.util, os, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("ctb", os.path.join(REPO, "tools", "arch", "check_thread_budget.py"))
ctb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ctb)

class TestThreadBudget(unittest.TestCase):
    def test_01_real_repo_passes(self):
        import subprocess
        r = subprocess.run([sys.executable, os.path.join(REPO, "tools", "arch", "check_thread_budget.py")],
                           capture_output=True, text=True, cwd=REPO, timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("未登记线程创建=0 硬编码线程数=0", r.stdout)

    def test_02_unregistered_thread_must_fail(self):
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "lib", "x"))
            open(os.path.join(td, "lib", "x", "rogue.cpp"), "w").write(
                "void f(){ std::thread t([](){}); t.join(); }\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertTrue(any("rogue.cpp" in e and "未登记" in e for e in errors))

    def test_03_omp_set_unregistered_must_fail(self):
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "lib", "y"))
            open(os.path.join(td, "lib", "y", "bad.cpp"), "w").write(
                "#include <omp.h>\nvoid g(){ omp_set_num_threads(8); }\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertTrue(any("bad.cpp" in e and "omp_set_num_threads" in e for e in errors))

    def test_04_registered_rows_carry_annotation(self):
        for k, v in ctb.REGISTERED.items():
            self.assertTrue(v.strip(), f"登记 {k} 必须带整改/豁免注记")

    def test_05_hardcoded_literal_always_fails(self):
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "lib", "z"))
            open(os.path.join(td, "lib", "z", "lit.cpp"), "w").write(
                "#pragma omp parallel for num_threads(16)\nint main(){}\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertTrue(any("lit.cpp" in e and "字面量线程数" in e for e in errors))

if __name__ == "__main__":
    unittest.main(verbosity=2)
