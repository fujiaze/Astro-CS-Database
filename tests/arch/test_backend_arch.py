#!/usr/bin/env python3
"""ARCH-003 测试: backend 架构覆盖 05 全条目 + 禁止项机器门。"""
import os, re, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "architecture", "CPU_BACKEND_ARCH.md")

class TestBackendArch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()

    def test_01_covers_05_all_sections(self):
        for k in ("设计结论", "编译隔离", "CPU/OS", "C ABI v1", "Kernel 注册粒度", "失败与回退", "发布检查"):
            self.assertIn(k, self.s, f"05 条目 {k} 缺失")

    def test_02_cpp_abi_banned(self):
        self.assertIn("C++ STL", self.s) and self.assertIn("异常", self.s)
        self.assertIn("禁私有线程池", self.s)

    def test_03_no_plugin_injection(self):
        self.assertIn("LD_LIBRARY_PATH", self.s) and self.assertIn("禁止任意", self.s)

    def test_04_kernel_granularity(self):
        self.assertIn("禁一个全局", self.s, "必须声明禁止全局 AVX2 模式")
        for k in ("calibration", "noise-SNR", "drizzle", "UPM", "rejection", "integration", "HiPS"):
            self.assertIn(k, self.s, f"kernel 类别 {k} 缺失")
        self.assertIn("science_contract_id", self.s)

    def test_05_fallback_rules(self):
        self.assertIn("禁静默换 backend", self.s)
        self.assertIn("baseline 自检失败", self.s)

if __name__ == "__main__":
    unittest.main(verbosity=2)
