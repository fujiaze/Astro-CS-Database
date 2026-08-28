#!/usr/bin/env python3
"""BENCH-002 测试: Oracle 先行/预热计时顺序/稳健统计/故意错误 backend 被禁用不获胜。"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")


class TestBenchHarness(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="bench_")
        cls.exe = os.path.join(cls.tmp, "bh")
        cls.cheat = os.path.join(cls.tmp, "cheat.so")
        crypto_inc = f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}"
        for src, out, extra in (
            (os.path.join(REPO, "tests", "backend", "bench_harness_main.cpp"), cls.exe,
             [os.path.join(HOST, "baseline_backend.cpp"), os.path.join(HOST, "bench_harness.cpp"),
              os.path.join(HOST, "host_services.cpp"),
              os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"), crypto_inc, "-ldl"]),
            (os.path.join(REPO, "tests", "backend", "cheat_backend.cpp"), cls.cheat, ["-shared", "-fPIC"]),
        ):
            r = subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
                                f"-I{INC}", f"-I{HOST}", src, *extra, "-o", out],
                               capture_output=True, text=True, timeout=180)
            assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, cheat=False):
        args = [self.exe] + (["--cheat", self.cheat] if cheat else [])
        r = subprocess.run(args, capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        results, sel = {}, None
        for line in r.stdout.splitlines():
            if line.startswith("RESULT "):
                t = line.split()
                results[t[1]] = {
                    "verdict": t[2], "median": float(t[3]), "mad": float(t[4]),
                    "p05": float(t[5]), "p95": float(t[6]), "hash": t[7],
                    "reason": " ".join(t[8:]) if len(t) > 8 else ""}
            m = re.match(r"SELECT (\S+)", line)
            if m:
                sel = m.group(1)
        return results, sel

    def test_01_cheat_disabled_and_never_wins(self):
        """验收: 故意错误 backend 被禁用(ORACLE_FAIL)且不得测速获胜。"""
        results, sel = self._run(cheat=True)
        self.assertIn("cheat", results)
        self.assertEqual(results["cheat"]["verdict"], "ORACLE_FAIL", "Oracle 门禁用错误 backend")
        self.assertEqual(results["cheat"]["median"], 0.0, "禁用者无计时数据(先 Oracle 后计时)")
        self.assertIn("got=0.000000", results["cheat"]["reason"])
        self.assertEqual(results["baseline"]["verdict"], "OK")
        self.assertEqual(sel, "baseline", "选择必须是正确 backend(速度不能使错误获胜)")

    def test_02_robust_stats_and_samples(self):
        results, _ = self._run()
        b = results["baseline"]
        self.assertLessEqual(b["p05"], b["median"], "p05 ≤ median")
        self.assertLessEqual(b["median"], b["p95"], "median ≤ p95")
        self.assertGreater(b["mad"], 0, "MAD>0(多样本统计)")
        self.assertRegex(b["hash"], r"^[0-9a-f]{16}$")

    def test_03_correctness_hash_deterministic(self):
        _, sel1 = self._run()
        r1 = self._run()[0]["baseline"]["hash"]
        r2 = self._run()[0]["baseline"]["hash"]
        self.assertEqual(r1, r2, "同数据 correctness hash 必须一致")

    def test_04_baseline_alone_still_selects(self):
        results, sel = self._run()
        self.assertEqual(results["baseline"]["verdict"], "OK")
        self.assertEqual(sel, "baseline")

if __name__ == "__main__":
    unittest.main(verbosity=2)
