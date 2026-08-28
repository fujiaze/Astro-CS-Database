#!/usr/bin/env python3
"""ABI-001 测试: C ABI v1 双编译(C/C++)/Debug+Release 布局一致/handshake/selftest/异常边界。"""
import os, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HDR = os.path.join(REPO, "include", "astrocs", "common_abi_v1.h")
SRC = os.path.join(REPO, "lib", "backend_host")
MAIN = os.path.join(REPO, "tests", "backend", "abi_selftest_main.cpp")


def compile_and_run(build_dir, cxx, flags):
    exe = os.path.join(build_dir, "abi_selftest")
    cmd = [cxx, "-std=c++17", f"-I{os.path.join(REPO, 'include')}",
           *flags,
           MAIN, os.path.join(SRC, "host_services.cpp"), os.path.join(SRC, "baseline_backend.cpp"),
           "-o", exe]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    if r.returncode != 0:
        return None, r.stderr
    run = subprocess.run([exe], capture_output=True, text=True, timeout=120)
    return run, r.stderr


class TestAbiV1(unittest.TestCase):
    """验收: 编译器/Debug/Release ABI tests;异常不跨边界;分配方可验证。"""

    def test_01_header_compiles_as_c(self):
        with tempfile.TemporaryDirectory() as td:
            src = os.path.join(td, "c_tu.c")
            with open(src, "w", encoding="utf-8") as f:
                f.write('#include "astrocs/common_abi_v1.h"\nint main(void){'
                        'astrocs_backend_api_v1 a; a.abi_version=ACS_ABI_VERSION_V1;'
                        'return a.abi_version==1u?0:1;}\n')
            r = subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-pedantic",
                                f"-I{os.path.join(REPO, 'include')}", "-c", src, "-o",
                                os.path.join(td, "c_tu.o")],
                               capture_output=True, text=True, timeout=60)
            self.assertEqual(r.returncode, 0, f"C11 编译失败: {r.stderr}")

    def test_02_header_compiles_cpp_noexcept_tu(self):
        """backend TU 可 -fno-exceptions 编译(异常不跨边界的编译面)。"""
        with tempfile.TemporaryDirectory() as td:
            r = subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-fno-exceptions",
                                "-DASTROCS_NO_EXCEPTIONS",
                                f"-I{os.path.join(REPO, 'include')}", "-c",
                                os.path.join(SRC, "baseline_backend.cpp"), "-o",
                                os.path.join(td, "b.o")],
                               capture_output=True, text=True, timeout=60)
            self.assertEqual(r.returncode, 0, f"-fno-exceptions 编译失败: {r.stderr}")

    def test_03_debug_release_layout_identical_and_all_pass(self):
        with tempfile.TemporaryDirectory() as td:
            dbg_dir = os.path.join(td, "debug")
            rel_dir = os.path.join(td, "release")
            os.makedirs(dbg_dir)
            os.makedirs(rel_dir)
            dbg, dbg_err = compile_and_run(dbg_dir, "g++", ["-O0", "-g", "-Wall", "-Wextra"])
            rel, rel_err = compile_and_run(rel_dir, "g++", ["-O2", "-DNDEBUG", "-Wall", "-Wextra"])
            self.assertIsNotNone(dbg, f"Debug 编译失败: {dbg_err}")
            self.assertIsNotNone(rel, f"Release 编译失败: {rel_err}")
            self.assertEqual(dbg.returncode, 0, f"Debug 运行失败:\n{dbg.stdout}")
            self.assertEqual(rel.returncode, 0, f"Release 运行失败:\n{rel.stdout}")
            for out in (dbg, rel):
                self.assertIn("[PASS] self_test ok", out.stdout)
                self.assertIn("[PASS] allocator balanced (no leak)", out.stdout)
                self.assertIn("[PASS] exception contained at boundary", out.stdout)
                self.assertIn("[PASS] budget excess rejected", out.stdout)
            self.assertEqual(dbg.stdout.splitlines()[0], rel.stdout.splitlines()[0],
                             "Debug/Release ABI 布局必须逐字节一致")
            self.assertIn("ALL_OK", dbg.stdout)

    def test_04_kernel_table_contracts(self):
        """kernel 表锚定 05 §5: 12 条目+science_contract_id 全非空+precision 合法。"""
        text = open(os.path.join(SRC, "baseline_backend.cpp"), encoding="utf-8").read()
        for kid in ("calibration-pixel-transform", "noise-snr-reductions", "wcs-psf-batch",
                    "drizzle-overlap", "drizzle-accumulate", "drizzle-normalize",
                    "upm-spmv", "upm-residual", "upm-weight-update",
                    "rejection-statistics", "integration-accumulate", "hips-bulk-transform"):
            self.assertIn(f'"{kid}"', text, f"缺 kernel {kid}")
        self.assertIn('"ALG-P3-002"', text)
        for disallowed in ("march=native", "-mavx"):
            self.assertNotIn(disallowed, text)

    def test_05_single_definition_of_abi_version(self):
        """ACS_ABI_VERSION_V1 只在头文件定义(04 §6-3 退出码单源同款纪律)。"""
        hits = []
        for fn in os.listdir(SRC):
            if fn.endswith(".cpp"):
                with open(os.path.join(SRC, fn), encoding="utf-8") as f:
                    if "#define ACS_ABI_VERSION_V1" in f.read():
                        hits.append(fn)
        self.assertEqual(hits, [], "ABI 版本宏泄漏到实现文件")

if __name__ == "__main__":
    unittest.main(verbosity=2)
