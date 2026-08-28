#!/usr/bin/env python3
"""API-001 测试: 公共 ABI 基础层文档机器门 + 头独立编译试金石(头文件实现属 ABI-001)。"""
import os, re, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOC = os.path.join(REPO, "docs", "api", "COMMON_ABI_V1.md")

HEADER = r"""
#include <stdint.h>
typedef struct { uint32_t struct_size, abi_version; } acs_head;
typedef struct acs_span_f32 { float* data; uint64_t count; } acs_span_f32;
typedef enum { ACS_OK=0, ACS_ERR_PARAM=1 } acs_status;
uint64_t acs_span_count(const acs_span_f32* s){ return s->count; }
uint32_t acs_status_ok(void){ return (uint32_t)ACS_OK; }
"""

class TestCommonAbi(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = open(DOC, encoding="utf-8").read()

    def test_01_all_host_services_defined(self):
        for k in ("acs_allocator", "acs_logger", "acs_cancel", "acs_thread_budget",
                  "acs_status", "acs_span_f32", "acs_handle"):
            self.assertIn(k, self.s, f"缺 {k}")

    def test_02_struct_size_handshake(self):
        self.assertEqual(self.s.count("struct_size, abi_version"), 5, "五个结构必须带 handshake")

    def test_03_units_and_ownership_annotated(self):
        self.assertIn("count=元素数", self.s)
        self.assertIn("分配方释放", self.s)
        self.assertIn("available_cpus", self.s)
        self.assertIn("affinity", self.s)

    def test_04_header_compiles_as_c_and_cpp(self):
        with tempfile.TemporaryDirectory() as td:
            hp = os.path.join(td, "h.c")
            open(hp, "w").write(HEADER)
            for std in (["gcc", "-x", "c", "-std=c11"], ["g++", "-x", "c++", "-std=c++17"]):
                out = os.path.join(td, "out.o")
                r = subprocess.run(std + ["-c", hp, "-o", out], capture_output=True, text=True, timeout=60)
                self.assertEqual(r.returncode, 0, f"{std[0]} 编译失败: {r.stderr}")

    def test_05_contract_template_fields(self):
        for k in ("reentrant", "threadsafe", "internal_parallel", "aliasing", "取消点粒度"):
            self.assertIn(k, self.s)

if __name__ == "__main__":
    unittest.main(verbosity=2)
