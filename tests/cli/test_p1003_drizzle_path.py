#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p1003_drizzle_path.py — P1-003 (G4) Drizzle 路径清理验证。
验证 CLI 生产路径不存在 hp_drizzle_run_hips / spawn_frame_from_fits / 直接 CFITSIO
header 解析直连; drizzle 命令拒绝生产调用(仅测试 preset); wrapper 仅经 preset/Runtime。
"""
import json
import os
import subprocess
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
CLI_DIR = os.path.join(REPO, "cli")


class TestP1003DrizzlePath(unittest.TestCase):
    def test_01_cli_binary_no_drizzle_direct_symbols(self):
        """CLI 二进制不含 hp_drizzle_run_hips/spawn_frame_from_fits 符号(nm 证明不可绕过)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        r = subprocess.run(["nm", "-C", EXE], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0)
        banned = [s for s in ("hp_drizzle_run_hips", "spawn_frame_from_fits") if s in r.stdout]
        self.assertEqual(banned, [], f"CLI 二进制含直连 drizzle 符号: {banned}")

    def test_02_cli_source_no_direct_drizzle_call(self):
        """CLI 源码(commands.cpp)无 hp_drizzle_run_hips / spawn_frame_from_fits 生产调用。"""
        src = os.path.join(CLI_DIR, "commands.cpp")
        if not os.path.isfile(src):
            self.skipTest("commands.cpp 缺失")
        text = open(src, encoding="utf-8").read()
        for sym in ("hp_drizzle_run_hips", "spawn_frame_from_fits"):
            self.assertNotIn(sym, text, f"CLI 源码含 {sym} 直连")

    def test_03_drizzle_command_rejects_production(self):
        """cmd_drizzle 拒绝生产调用(仅测试 preset), 退出码 ARGS(2)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        r = subprocess.run([EXE, "drizzle"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 2, r.stderr)
        self.assertIn("preset", r.stderr)

    def test_04_prod_callgraph_no_hp_drizzle(self):
        """生产可达性检查: CLI 生产路径无 hp_drizzle 直连(REACH_PASS)。"""
        checker = os.path.join(REPO, "tools", "quality", "check_prod_reachability.py")
        if not os.path.isfile(checker):
            self.skipTest("reachability checker 缺失")
        r = subprocess.run(
            ["python3", checker, "--repo", REPO, "--binary", EXE],
            capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("REACH_PASS", r.stdout)
        self.assertIn("acr=0", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
