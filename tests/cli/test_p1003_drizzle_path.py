#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p1003_drizzle_path.py — P1-003 (G4) Drizzle 路径清理验证。
验证 CLI 生产路径不存在 hp_drizzle_run_hips / spawn_frame_from_fits / 直接 CFITSIO
header 解析直连; drizzle 命令拒绝生产调用(仅测试 preset); wrapper 仅经 preset/Runtime。
"""
import json
import os
import shutil
import subprocess
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")
CLI_DIR = os.path.join(REPO, "cli")

# FIX-UTCLI-HYGIENE: 子进程 cwd / 证据落点统一落 run/（gitignore），见 cli_test_hygiene.py
from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402


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
        """drizzle 用户命令已删除（ASTROCS_DESIGN §6.2 唯一命令树; CLI-001 rc 矩阵）:
        作为生产命令不可达 ⇒ 未知命令 rc=2(ARGS), 错误面指向新命令树。
        旧断言 "stderr 含 preset"（旧 cmd_drizzle 仅测试 preset 的语义）已随命令删除;
        本用例保留其真实意图「drizzle 不得作为生产命令运行」并加强为"命令不存在"。
        """
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        r = subprocess.run([EXE, "drizzle"], capture_output=True, text=True, timeout=60,
                           cwd=run_cwd())
        self.assertEqual(r.returncode, 2, r.stderr)
        self.assertIn("unknown command 'drizzle'", r.stderr)
        self.assertIn("astrocs normalize", r.stderr, "错误面必须给出 §6.2 命令树")

    def test_04_prod_callgraph_no_hp_drizzle(self):
        """生产可达性检查: CLI 生产路径无 hp_drizzle 直连(REACH_PASS)。"""
        checker = os.path.join(REPO, "tools", "quality", "check_prod_reachability.py")
        if not os.path.isfile(checker):
            self.skipTest("reachability checker 缺失")
        # checker 需要 compile_commands.json 做 TU 级调用图; 纯 CMake 构建产物
        # 不含 CMAKE_EXPORT_COMPILE_COMMANDS 时该环境证据缺失, skip 而非 fail
        # (可达性本身由 test_01/02 的 nm/源码断言独立覆盖)。
        cc = os.path.join(REPO, "build", "compile_commands.json")
        if not os.path.isfile(cc):
            self.skipTest("compile_commands.json 缺失(checker 依赖)")
        # FIX-UTCLI-HYGIENE: checker 把可达图证据硬写到
        # <repo>/evidence/v6_1_rework/tasks/CHK-001/（tracked 受控文件，
        # tools/quality/check_prod_reachability.py:140）。UT-CLI 以
        # mutates_workspace=false 执行，重写 tracked evidence 即 dirty 违规，而
        # checker 无输出目录开关（产品域禁改）。测试侧用 run/ 下 scratch repo 视图：
        # 只读符号链接 lib/infrastructure/cli/include/lib（checker 扫描面与真 repo 逐字节一致）+
        # 本地 evidence/ 输出目录；checker 的读取与判定完全不变，仅证据落点进入
        # gitignore 的 run/。
        scratch = os.path.join(run_cwd(), "reach_scratch")
        shutil.rmtree(scratch, ignore_errors=True)
        os.makedirs(scratch)
        for sub in ("cli", "include", "lib"):
            os.symlink(os.path.join(REPO, sub), os.path.join(scratch, sub))
        r = subprocess.run(
            ["python3", checker, "--repo", scratch, "--binary", EXE,
             "--compile-commands", cc],
            capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("REACH_PASS", r.stdout)
        self.assertIn("acr=0", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
