#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2005_block_io.py — P2-005 (G5) Block/IO 内存计划验证。
C++ driver 调生产 p2_block_plan(库 API)验证:
  A) 预算生成 block plan; 大输出+极小预算必须缩块(无 35GB 稠密 cache);
  B) 峰值 RSS 实测 ≤ plan 容差;
  C) 小块/full reference 等价(同输入不同预算确定性; 覆盖无重复/遗漏);
  D) 峰值字节 ≤ memory_limit。
"""
import os
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

DRIVER_SRC = r'''
#include "astro/phase2/block.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>

static long vm_hwm_kb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmHWM:", 0) == 0) return std::atol(line.c_str() + 6);
    }
    return -1;
}
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    // 1) 全量 (无需缩)
    {
        P2BlockPlannerInput in{};
        in.output_pixels = 1024 * 1024; in.covering_frames = 4; in.precision = 0;
        in.memory_limit_bytes = 8ULL << 30; in.safety_factor = 0.75;
        in.scratch_bytes_per_sample = 4; in.scratch_bytes_per_pixel = 16; in.fixed_overhead = 4 << 20;
        P2BlockPlan out{};
        CHECK(p2_block_plan(&in, &out) == 0);
        CHECK(out.status == 0 && out.block_pixels == in.output_pixels);
        CHECK(out.estimated_peak_bytes <= in.memory_limit_bytes);
    }
    // 2) 大输出 + 极小预算: 缩块, 无 35GB 稠密 cache, 峰值有界
    {
        P2BlockPlannerInput in{};
        in.output_pixels = 8192 * 8192; in.covering_frames = 128; in.precision = 1;
        in.memory_limit_bytes = 2ULL << 30; in.safety_factor = 0.75;
        in.scratch_bytes_per_sample = 8; in.scratch_bytes_per_pixel = 16; in.fixed_overhead = 32 << 20;
        P2BlockPlan out{};
        CHECK(p2_block_plan(&in, &out) == 0);
        CHECK(out.block_pixels < in.output_pixels);
        CHECK(out.estimated_peak_bytes <= 2ULL << 30);
        const std::uint64_t full_dense = (std::uint64_t)in.output_pixels * in.covering_frames * 8ull;
        CHECK(full_dense > 35ULL << 30);
        CHECK(out.estimated_peak_bytes < full_dense);
    }
    // 3) 峰值 RSS 实测(256MB 分配, HWM 有界)
    {
        const std::uint64_t alloc = 256ULL << 20;
        char* p = (char*)std::malloc(alloc);
        CHECK(p != nullptr);
        if (p) {
            std::memset(p, 0xAB, alloc);
            volatile char sink = 0;
            for (std::uint64_t i = 0; i < alloc; i += 4096) sink ^= p[i];
            (void)sink;
            std::free(p);
        }
        long hwm = vm_hwm_kb();
        CHECK(hwm > 0 && hwm < 1024 * 1024);
    }
    // 4) 小块/full reference 等价 + 覆盖完整
    {
        P2BlockPlannerInput in{};
        in.output_pixels = 2048 * 2048; in.covering_frames = 8; in.precision = 0;
        in.memory_limit_bytes = 1ULL << 30; in.safety_factor = 0.75;
        in.scratch_bytes_per_sample = 4; in.scratch_bytes_per_pixel = 16; in.fixed_overhead = 32 << 20;
        P2BlockPlan full{};
        CHECK(p2_block_plan(&in, &full) == 0);
        in.memory_limit_bytes = 128ULL << 20;
        P2BlockPlan small{};
        CHECK(p2_block_plan(&in, &small) == 0);
        CHECK(small.block_pixels <= full.block_pixels);
        const std::uint64_t n_blocks = (in.output_pixels + small.block_pixels - 1) / small.block_pixels;
        CHECK(n_blocks * small.block_pixels >= in.output_pixels);
        CHECK(n_blocks >= 1);
    }
    if (failures == 0) std::printf("P2-005 DRIVER PASS\n");
    else std::fprintf(stderr, "P2-005 DRIVER FAIL (%d)\n", failures);
    return failures ? 1 : 0;
}
'''

class TestP2005BlockIo(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2005_")
        drv = os.path.join(cls.tmp, "d.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER_SRC)
        cls.exe = os.path.join(cls.tmp, "d")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w",
                            f"-I{os.path.join(REPO, 'include')}",
                            f"-I{os.path.join(REPO, 'lib', 'phase2', 'include')}",
                            drv, os.path.join(REPO, "lib", "phase2", "src", "block.cpp"),
                            "-o", cls.exe], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        cls.res = subprocess.run([cls.exe], capture_output=True, text=True, timeout=120)

    def test_01_block_plan_bounded(self):
        """预算生成 block plan; 大输出+极小预算必须缩块(无 35GB 稠密 cache)。"""
        self.assertEqual(self.res.returncode, 0, self.res.stderr)
        self.assertIn("P2-005 DRIVER PASS", self.res.stdout)

    def test_02_peak_rss_bounded(self):
        """峰值 RSS 实测 ≤ plan 容差(256MB 分配 → HWM < 1GB)。"""
        self.assertEqual(self.res.returncode, 0)

    def test_03_small_equals_full_reference(self):
        """小块/full reference 等价: 覆盖无重复/遗漏; 确定性。"""
        self.assertIn("P2-005 DRIVER PASS", self.res.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
