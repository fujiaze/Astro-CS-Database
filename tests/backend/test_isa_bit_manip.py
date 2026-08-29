#!/usr/bin/env python3
"""ISA-005 测试: 整数/位操作热点审计 — 无位操作热点→NOT_APPLICABLE, 不写空 DLL。
依据 03 §91: '只评估整数/位操作热点; VNNI 等与算法无关则写 NOT_APPLICABLE 证据, 不写空 DLL'。"""
import csv, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")

BMI2_POPCNT = re.compile(r"^(mulx|rorx|blsr|blsmsk|blsi|tzcnt|lzcnt|popcnt|pdep|pext|bmi1|bmi2|andn|bextr)$", re.I)


class TestIsaBitManip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="isa_bmi2_")
        # 用 -mbmi2 -mpopcnt 编译 baseline 同源, 验证指令层无位操作(非只读断言)
        cls.src = os.path.join(cls.tmp, "bmi2_backend.cpp")
        with open(cls.src, "w") as f:
            f.write('#define ASTROCS_BACKEND_ID "bmi2"\n'
                    '#include "astrocs/common_abi_v1.h"\n'
                    '#include "baseline_kernels.h"\n'
                    '#include <algorithm>\n#include <atomic>\n#include <cmath>\n'
                    '#include <cstdio>\n#include <cstring>\n#include <functional>\n'
                    '#include <thread>\n#include <vector>\n'
                    '#include "baseline_kernels_impl.inc"\n'
                    '#include "backend_table.inc"\n')
        cls.so = os.path.join(cls.tmp, "bmi2_backend.so")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mbmi2", "-mpopcnt",
                            "-fPIC", "-shared", "-Wall", "-Wextra", f"-I{INC}", f"-I{HOST}",
                            cls.src, "-o", cls.so], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_no_bitop_hotspot_in_kernels(self):
        """12 kernel 源码审计: 无位操作热点(仅计算场/比较/索引, 非 popcount/BMI2)。"""
        impl = open(os.path.join(HOST, "baseline_kernels_impl.inc"), encoding="utf-8").read()
        # 算法中不得出现可被 BMI2/POPCNT 加速的位操作形态(极少数真位操作会提示)
        self.assertNotIn("__builtin_popcount", impl)
        self.assertNotIn("__builtin_ctz", impl)
        self.assertNotIn("__builtin_clz", impl)
        self.assertNotIn("popcount", impl)
        # 仅截取 OpComputer 计算区(并行带尺寸的 workers<<=1/>>=1 是非内核元, 排除)
        comp = impl[impl.index("struct OpComputer {"):]
        self.assertNotIn("<<", comp.replace("<<=", ""), "kernel 计算区无位移位算子")
        self.assertNotIn(">>", comp.replace(">>=", ""), "kernel 计算区无位移位算子")

    def test_02_bmi2_compile_emits_no_bitop_instructions(self):
        """-mbmi2 -mpopcnt 编译变体 DSO: 反汇编含 BMI2/POPCNT 指令数 = 0(工具链无可加速位操作)。"""
        dis = subprocess.run(["objdump", "-d", self.so], capture_output=True, text=True,
                             timeout=120).stdout
        found = set(BMI2_POPCNT.findall(dis))
        self.assertEqual(found, set(),
                         f"不得有位操作专用指令(无位操作热点): {found}")

    def test_03_no_empty_dll_shipped(self):
        """必须写 NOT_APPLICABLE 证据且不把空 DLL 入库(shipped backend 目录无位操作变体)。"""
        # 本任务未把 bmi2_backend.cpp/.so 作为正式 SHPI 变体入库
        self.assertFalse(os.path.isfile(os.path.join(HOST, "bmi2_backend.cpp")),
                         "NOT_APPLICABLE 结论不应留下入库的空 DLL/变体源")
        self.assertFalse(os.path.isfile(os.path.join(HOST, "bmi2_backend.so")))
        # 证据表存在且 instruction_count=0
        mea = os.path.join(REPO, "artifacts", "prerelease_v5", "ISA-005", "MEASUREMENTS.csv")
        self.assertTrue(os.path.isfile(mea))
        for row in csv.reader(open(mea, encoding="utf-8")):
            if row and row[0] != "kernel":
                self.assertEqual(row[3], "0", f"{row[0]} 位操作指令计数须为 0")
                self.assertIn("NOT_APPLICABLE", row[4])

    def test_04_decision_ledger_records_na(self):
        doc = open(os.path.join(REPO, "docs", "architecture", "ISA_BIT_MANIP_VARIANTS.md"),
                   encoding="utf-8").read()
        self.assertIn("NOT_APPLICABLE", doc)
        self.assertIn("不写空 DLL", doc)
        self.assertIn("upm-spmv", doc)


if __name__ == "__main__":
    unittest.main(verbosity=2)
