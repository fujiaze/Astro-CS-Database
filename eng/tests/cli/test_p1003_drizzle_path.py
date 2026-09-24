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

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# DISPATCH 附录 H（构建隔离）: 被测构建树 = 被测二进制所在目录; ASTROCS_CLI_BIN 覆盖。
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "acsd"))
BUILD = os.path.dirname(os.path.abspath(EXE))
# ROOT-008: CLI 命令层源在 lib/infrastructure/cli/（旧 cli/ 已退役）。
CLI_DIR = os.path.join(REPO, "lib", "infrastructure", "cli")

# FIX-UTCLI-HYGIENE: 子进程 cwd / 证据落点统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402


class TestP1003DrizzlePath(unittest.TestCase):
    # P1-003 真实判据（CLI-002 重锚）: 「CLI 生产路径不得直连 drizzle」。
    # 旧判据「exe 内不含 hp_drizzle_run_hips」在 ROOT-008 单 exe 形态下不可满足 —— 模块
    # wrapper（lib/algorithms/drizzle/src/module_entry.cpp）本就编入同一 exe，符号必然
    # 存在；判据与架构互斥（真缺陷判据错误，非实现缺陷）。现行判据拆三面，任一破即红：
    #   1. 禁用内部符号 spawn_frame_from_fits 不得出现在 exe；
    #   2. CLI 目标文件（nm -u 逐 TU）不得引用 hp_drizzle_run_hips/spawn_frame_from_fits；
    #   3. 该入口符号的**定义者**必须是 drizzle 算法库（模块 wrapper 面），不得由 CLI 面定义。
    DRIZZLE_ENTRY = "hp_drizzle_run_hips"
    INTERNAL_BANNED = ("spawn_frame_from_fits",)

    def _cli_objects(self):
        root = os.path.join(BUILD, "CMakeFiles", "astrocs.dir")
        objs = []
        for dirpath, _dirs, files in os.walk(root):
            objs += [os.path.join(dirpath, f) for f in files if f.endswith(".o")]
        return objs

    def test_01_cli_binary_no_drizzle_direct_symbols(self):
        """CLI 生产路径无 drizzle 直连：内部符号不在 exe + CLI 目标文件零引用 + 定义者非 CLI。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        r = subprocess.run(["nm", "-C", EXE], capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        banned = [s for s in self.INTERNAL_BANNED if s in r.stdout]
        self.assertEqual(banned, [], f"CLI 二进制含禁用内部符号: {banned}")
        # 2) 逐 TU: CLI 目标文件不得有对 drizzle 入口/内部符号的未定义引用
        objs = self._cli_objects()
        self.assertTrue(objs, "未找到 CLI 目标文件（构建树布局变化，判据需重锚）")
        for obj in objs:
            u = subprocess.run(["nm", "-C", "-u", obj], capture_output=True, text=True,
                               timeout=120)
            self.assertEqual(u.returncode, 0, u.stderr[-200:])
            for sym in (self.DRIZZLE_ENTRY,) + self.INTERNAL_BANNED:
                self.assertNotIn(sym, u.stdout, f"{obj} 直连 {sym}")
            self.assertNotIn(f" T {self.DRIZZLE_ENTRY}", subprocess.run(
                ["nm", "-C", obj], capture_output=True, text=True, timeout=120).stdout,
                f"{obj} 定义了 drizzle 入口符号（越界实现）")
        # 3) 定义者归属: 入口符号定义必须来自 drizzle 算法库（模块 wrapper 面）
        if self.DRIZZLE_ENTRY in r.stdout:
            import glob
            defs = []
            for lib in glob.glob(os.path.join(BUILD, "**", "*.a"), recursive=True):
                a = subprocess.run(["nm", "-A", lib], capture_output=True, text=True,
                                   timeout=300)
                defs += [l for l in a.stdout.splitlines()
                         if l.rstrip().endswith(f" T {self.DRIZZLE_ENTRY}")]
            self.assertTrue(defs, "exe 含该符号但无静态库定义者（判据失效）")
            self.assertTrue(all("drizzle" in d for d in defs),
                            f"drizzle 入口由非 drizzle 库定义: {defs}")

    def test_02_cli_source_no_direct_drizzle_call(self):
        """CLI 命令层全部源文件无 hp_drizzle_run_hips / spawn_frame_from_fits 直连。"""
        srcs = [os.path.join(CLI_DIR, f) for f in sorted(os.listdir(CLI_DIR))
                if f.endswith((".cpp", ".h"))]
        self.assertTrue(srcs, "CLI 源目录缺失或为空")
        for src in srcs:
            text = open(src, encoding="utf-8").read()
            for sym in (self.DRIZZLE_ENTRY,) + self.INTERNAL_BANNED:
                self.assertNotIn(sym, text, f"{os.path.basename(src)} 含 {sym} 直连")

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
        self.assertIn("acsd normalize", r.stderr, "错误面必须给出 §6.2 命令树")

    def test_04_prod_callgraph_no_hp_drizzle(self):
        """生产可达性检查: CLI 生产路径无 hp_drizzle 直连(REACH_PASS)。"""
        checker = os.path.join(REPO, "eng", "tools", "quality", "check_prod_reachability.py")
        if not os.path.isfile(checker):
            self.skipTest("reachability checker 缺失")
        # checker 需要 compile_commands.json 做 TU 级调用图; 纯 CMake 构建产物
        # 不含 CMAKE_EXPORT_COMPILE_COMMANDS 时该环境证据缺失, skip 而非 fail
        # (可达性本身由 test_01/02 的 nm/源码断言独立覆盖)。
        cc = os.path.join(BUILD, "compile_commands.json")
        if not os.path.isfile(cc):
            # 跨域缺口（不在 CLI-002 改动面）: ① CI 构建步未开
            # CMAKE_EXPORT_COMPILE_COMMANDS（登记面 V17-N-03 / 归属 CI-001）；
            # ② eng/tools/quality/check_prod_reachability.py:92 锚点仍指向已退役 cli/
            # （tools 域）。二者修好后本用例自动转为实跑；此处不放宽判据。
            self.skipTest("compile_commands.json 缺失：CI 构建步未开 "
                          "CMAKE_EXPORT_COMPILE_COMMANDS (V17-N-03/CI-001)")
        # FIX-UTCLI-HYGIENE: checker 把可达图证据硬写到
        # <repo>/evidence/v6_1_rework/tasks/CHK-001/（tracked 受控文件，
        # eng/tools/quality/check_prod_reachability.py:140）。UT-CLI 以
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
