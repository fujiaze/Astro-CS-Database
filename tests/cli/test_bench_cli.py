#!/usr/bin/env python3
"""BENCH-005 golden: hardware/benchmark/doctor CLI + quick/full + profile-output 有效性。"""
import hashlib, json, os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")
VALIDATOR = os.path.join(REPO, "tools", "validate_cpu_profile.py")
COMMIT = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"],
                        capture_output=True, text=True).stdout.strip()


def run(*args, timeout=240):
    return subprocess.run([EXE, *args], capture_output=True, text=True, timeout=timeout)


class TestBenchCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="bench_cli_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_benchmark_quick_profile_output_valid(self):
        out = os.path.join(self.tmp, "profile_quick.json")
        r = run("benchmark", "cpu", "--quick", "--output", out)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), f"{out} PASS")
        v = subprocess.run(["python3", VALIDATOR, out, "--commit", COMMIT],
                           capture_output=True, text=True, timeout=60)
        self.assertIn("VALID", v.stdout, v.stdout + v.stderr)
        d = json.load(open(out, encoding="utf-8"))
        self.assertEqual(d["mode"], "quick")
        self.assertEqual(len(d["kernels"]), 1)
        self.assertEqual(d["kernels"][0]["oracle_status"], "pass")

    def test_02_benchmark_full_twelve_kernels(self):
        out = os.path.join(self.tmp, "profile_full.json")
        r = run("benchmark", "cpu", "--full", "--output", out, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.load(open(out, encoding="utf-8"))
        self.assertEqual(d["mode"], "full")
        self.assertEqual(len(d["kernels"]), 12, "full=全 12 注册 kernel(06 §7)")
        self.assertEqual(d["verdict"], "PASS")
        v = subprocess.run(["python3", VALIDATOR, out, "--commit", COMMIT],
                           capture_output=True, text=True, timeout=60)
        self.assertIn("VALID", v.stdout)

    def test_03_benchmark_mode_flag_required(self):
        r = run("benchmark", "cpu", "--output", os.path.join(self.tmp, "x.json"))
        self.assertEqual(r.returncode, 2)
        r2 = run("benchmark", "cpu", "--quick", "--full")
        self.assertEqual(r2.returncode, 2)

    def test_04_benchmark_bounded_runtime(self):
        """timeout golden: quick 模式在受限时间内完成(单调钟, 无阻塞)。"""
        import time
        out = os.path.join(self.tmp, "bounded.json")
        t0 = time.monotonic()
        r = run("benchmark", "cpu", "--quick", "--output", out, timeout=60)
        elapsed = time.monotonic() - t0
        self.assertEqual(r.returncode, 0)
        self.assertLess(elapsed, 30, f"quick 应 <30s, 实测 {elapsed:.1f}s")

    def test_05_doctor_json_checks(self):
        r = run("doctor", "--json")
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.loads(r.stdout)
        self.assertEqual(d["kind"], "astrocs_doctor")
        self.assertEqual(d["verdict"], "PASS")
        names = [c["name"] for c in d["checks"]]
        self.assertIn("baseline_selftest", names)
        self.assertIn("hardware_sanity", names)
        for c in d["checks"]:
            self.assertIn(c["status"], ("pass", "skipped"), f"{c['name']} 应通过或跳过")

    def test_06_hardware_and_doctor_single_json(self):
        for args in (("hardware", "inspect", "--json"), ("doctor", "--json")):
            r = run(*args)
            json.loads(r.stdout)  # 恰一文档
            self.assertNotIn("CRASH", r.stderr, f"{args} 异常")
            self.assertNotIn("unknown command", r.stderr, f"{args} 参数误判")

    def test_07_cli_sha256_matches_binary(self):
        """profile 内 cli_sha256 = 运行中可执行文件 hash(06 §2)。"""
        out = os.path.join(self.tmp, "p.json")
        run("benchmark", "cpu", "--quick", "--output", out)
        d = json.load(open(out, encoding="utf-8"))
        cli_sha = d["build"]["cli_sha256"]
        bin_path = os.path.realpath(EXE)
        sha = hashlib.sha256(open(bin_path, "rb").read()).hexdigest()
        self.assertEqual(cli_sha, sha, "cli_sha256 必须是可执行文件实测 hash")

if __name__ == "__main__":
    unittest.main(verbosity=2)
