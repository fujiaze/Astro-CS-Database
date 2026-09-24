#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UT-ABI 采集包装: MOD-001 install/load 闭环验收。

M8-F-001: 本体 mod001_install_load_check.py 不匹配 unittest discover 默认
pattern( test*.py ), 故 UT-ABI 门从未执行它("64/64 PASS" 在 CI 内无重跑)。
本包装把其 main() 纳入 TestCase 采集; 本体直跑入口保留不变。
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mod001_install_load_check as mod001  # noqa: E402


def _real_cmake_build_dir() -> str:
    """选一个真实的 CMake 构建树(有 CMakeCache.txt)。

    M8-F-001: CI 根 build/ 只被 cp 了 acsd + libacsd_runtime.so, 不是
    CMake 树; mod001 需要 build 树里的模块 .so / libastrocs_aio.a / install
    规则。linux-control(eng/ci/steps/linux_build_root_graph.sh 产出)才是完整树,
    故优先之; 本地开发树 build/ 亦为完整 Ninja 树时回退使用。
    """
    for cand in ("build/linux-control", "build"):
        p = os.path.join(mod001.REPO, cand)
        if os.path.isfile(os.path.join(p, "CMakeCache.txt")):
            return p
    return os.path.join(mod001.REPO, "build")


class TestMod001InstallLoadAcceptance(unittest.TestCase):
    def test_acceptance_script_passes(self):
        # 本体 main() 用 argparse.parse_args() 读 sys.argv; unittest 运行器会把
        # 自己的参数留在 sys.argv, 直调会 SystemExit(2)。此处显式给 --build-dir
        # 指向真实 CMake 树, 其余参数不暴露。
        argv = sys.argv
        try:
            sys.argv = [argv[0], "--build-dir", _real_cmake_build_dir()]
            rc = mod001.main()
        finally:
            sys.argv = argv
        self.assertEqual(rc, 0, "MOD-001 install/load 验收脚本返回非零")


if __name__ == "__main__":
    unittest.main(verbosity=2)
