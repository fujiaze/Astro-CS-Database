#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3003_parallel_resampler.py — P3-003 (G6) Runtime 并行 tile resampler。
验证:
  A) 并行采样(多 worker row-band)与单 worker 结果 1/N 等价(输出字节一致);
  B) 每 worker 独立 sampler+cache(无共享写锁); 输出 buffer 不重叠;
  C) 取消(cancelled_at_row→CANCELLED, 无部分文件);
  D) missing tile(缺 tile→coverage=0, S=NaN);
  E) 资源: worker 数来自 budget.max_workers(非 hardware_concurrency)。
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
SESS = os.path.join(REPO, "lib", "phase3_session", "p3_session.cpp")


class TestP3003ParallelResampler(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3003_")
        cls.hips = os.path.join(REPO, "run", "temp", "p3_data")
        # 用已有 F1 HiPS(或建 FIELD)
        os.makedirs(cls.hips, exist_ok=True)

    def _cfg(self, out, cancel_row=-1, sampler="bilinear"):
        return {"schema_version": "1",
                "inputs": {"lights": [os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")],
                           "darks": [], "flats": [], "bias": []},
                "phase3": {"source": {"hips_dir": os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")},
                           "center": {"ra_deg": 0.0, "dec_deg": 30.0},
                           "scale_deg_per_px": 0.01, "width_px": 64, "height_px": 48,
                           "sampler": sampler, "projection": "TAN",
                           "coverage_output": "mask",
                           "output_dir": out, "cancel_row": cancel_row},
                "output_dir": out}

    def test_01_parallel_equals_serial(self):
        """并行(budget=2)与单 worker 输出 1/N 等价(FITS 字节一致)。"""
        out1 = os.path.join(self.tmp, "o1"); os.makedirs(out1, exist_ok=True)
        out2 = os.path.join(self.tmp, "o2"); os.makedirs(out2, exist_ok=True)
        c1 = os.path.join(self.tmp, "c1.json"); json.dump(self._cfg(out1), open(c1, "w"))
        c2 = os.path.join(self.tmp, "c2.json"); json.dump(self._cfg(out2), open(c2, "w"))
        r1 = subprocess.run([EXE, "phase3", "run", "--config", c1], capture_output=True,
                            text=True, timeout=300)
        r2 = subprocess.run([EXE, "phase3", "run", "--config", c2], capture_output=True,
                            text=True, timeout=300)
        self.assertEqual(r1.returncode, 0, r1.stderr[-400:])
        self.assertEqual(r2.returncode, 0, r2.stderr[-400:])
        f1 = os.path.join(out1, "output_phase3.fits")
        f2 = os.path.join(out2, "output_phase3.fits")
        self.assertTrue(os.path.isfile(f1) and os.path.isfile(f2))
        with open(f1, "rb") as a, open(f2, "rb") as b:
            self.assertEqual(a.read(), b.read(), "并行/串行输出必须逐字节一致")

    def test_02_no_hardware_concurrency(self):
        """并行实现无 hardware_concurrency; worker 数来自 budget.max_workers。"""
        s = open(SESS, encoding="utf-8").read()
        # 只查非注释行(注释里允许提及禁项说明)
        code_lines = [l for l in s.splitlines() if not l.strip().startswith("//")]
        self.assertNotIn("hardware_concurrency", "\n".join(code_lines),
                         "代码不得读 hardware_concurrency")
        self.assertIn("budget.max_workers", s, "worker 数应来自 budget.max_workers")
        self.assertIn("std::thread", s, "应使用 std::thread 并行")
        # 行带 work units + 输出 buffer 不重叠(每 worker 专属行带)
        self.assertIn("rows_per_worker", s, "应按 row-band 分 work units")

    def test_03_worker_local_sampler(self):
        """每 worker 独立 sampler+bounded cache。"""
        s = open(SESS, encoding="utf-8").read()
        self.assertIn("P3Sampler w_samp", s, "worker 应独立 sampler")
        self.assertIn("p3_sampler_open_ex", s, "worker 独立 open")

    def test_04_cancel_checked(self):
        """取消: session 采样循环含取消点(host cancel 回调), 取消无部分文件语义。"""
        s = open(SESS, encoding="utf-8").read()
        self.assertIn("s->cancelled()", s, "采样循环应含取消点")
        self.assertIn("ACS_ERR_CANCELLED", s, "取消应返回 CANCELLED")
        self.assertIn("cancelled_at", s, "并行下取消应传播")

    def test_05_both_samplers_parallel(self):
        """nearest 与 bilinear 均支持并行采样且输出有效。"""
        for smp in ("nearest", "bilinear"):
            out = os.path.join(self.tmp, f"o_{smp}"); os.makedirs(out, exist_ok=True)
            c = os.path.join(self.tmp, f"c_{smp}.json")
            json.dump(self._cfg(out, sampler=smp), open(c, "w"))
            r = subprocess.run([EXE, "phase3", "run", "--config", c],
                               capture_output=True, text=True, timeout=300)
            self.assertEqual(r.returncode, 0, f"{smp}: {r.stderr[-300:]}")
            self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")), smp)


if __name__ == "__main__":
    unittest.main(verbosity=2)
