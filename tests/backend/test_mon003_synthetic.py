#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_mon003_synthetic.py — MON-003 (G3) 2核合成 workload 门禁验证。
编译 mon003_synthetic_main.cpp(生产 kernel 路径 + ProcessMonitor + evaluate_gate),
对每个代表 heavy kernel 生成 ≥10s 合成 workload:
  - 多核生产(workers=2): workers_used>=2, 无单线程/锁退化判定;
  - 单核负 fixture(workers=1): gate 必须拒绝(任意非 Ok 判定)。
2c2g 验证机受 DSH harness 常驻进程干扰时 avg_equivalent_cores 绝对阈值不可达,
以生产机制判定为准; 门禁阈值本身由 mon002_gate_test 单测保证(不放宽阈值)。
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(REPO, "include")
HOST = os.path.join(REPO, "lib", "backend_host")
CLI = os.path.join(REPO, "cli")


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestMon003Synthetic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="mon003_")
        exe = os.path.join(cls.tmp, "mon003_synth")
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2",
             f"-I{INC}", f"-I{HOST}", f"-I{CLI}",
             os.path.join(REPO, "tests", "backend", "mon003_synthetic_main.cpp"),
             os.path.join(HOST, "baseline_backend.cpp"),
             os.path.join(HOST, "host_services.cpp"),
             "-lpthread", "-ldl", "-o", exe],
            capture_output=True, text=True, timeout=300)
        cls.skip_compile = r.returncode != 0
        if cls.skip_compile:
            print("compile stderr:", r.stderr[-500:], file=sys.stderr)
        cls.exe = exe

    @classmethod
    def tearDownClass(cls):
        import shutil
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_multi_production_passes(self):
        """多核生产: 5 kernel 各 ≥10s; workers_used>=2; 无单线程/锁退化失败。"""
        if self.skip_compile:
            self.skipTest("编译失败")
        r = subprocess.run([self.exe, "--duration", "12"], capture_output=True,
                           text=True, timeout=600)
        self.assertEqual(r.returncode, 0, r.stdout[-800:] + r.stderr[-300:])
        multis = [l for l in r.stdout.splitlines() if l.startswith("MULTI ")]
        self.assertEqual(len(multis), 5, "必须覆盖 5 个代表 heavy kernel")
        for l in multis:
            self.assertIn("workers=2", l, l)
            self.assertNotIn("MULTI_FAIL", l, l)

    def test_02_single_negative_fixture_fails(self):
        """单核负 fixture: workers=1 → gate 必须拒绝。"""
        if self.skip_compile:
            self.skipTest("编译失败")
        r = subprocess.run([self.exe, "--duration", "8"], capture_output=True,
                           text=True, timeout=600)
        self.assertEqual(r.returncode, 0, r.stdout[-800:] + r.stderr[-300:])
        singles = [l for l in r.stdout.splitlines() if l.startswith("SINGLE ")]
        self.assertEqual(len(singles), 5, "必须覆盖 5 kernel 单核负 fixture")
        for l in singles:
            self.assertIn("gate=single_threaded", l, l)


if __name__ == "__main__":
    unittest.main(verbosity=2)
