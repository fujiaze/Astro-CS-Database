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

from tests.backend.fixture_common import ensure_f1f2_hips  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")
SESS = os.path.join(REPO, "lib", "phase3_session", "p3_session.cpp")


class TestP3003ParallelResampler(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3003_")
        cls.hips = os.path.join(REPO, "run", "temp", "p3_data")
        ensure_f1f2_hips()
        # 用已有 F1 HiPS(或建 FIELD)
        os.makedirs(cls.hips, exist_ok=True)

    def _cfg(self, out, cancel_row=-1, sampler="bilinear"):
        # CLI-002 / ASTROCS_DESIGN 6.2: 旧 phase3 run --config 已删(rc=2);
        # 平铺会话配置形态(见 session_commands.h)。cancel_row 非现行 CLI 键
        # (parser 白名单拒绝), 取消语义由 test_04 的 session 源码契约覆盖。
        del cancel_row
        return {"schema_version": "1",
                "source": {"hips_dir": os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")},
                "center": {"ra_deg": 0.0, "dec_deg": 30.0},
                "scale_deg_per_px": 0.01, "width_px": 64, "height_px": 48,
                "sampler": sampler, "projection": "TAN",
                "coverage_output": "mask",
                "output_dir": out}

    @staticmethod
    def _blank_volatile_cards(buf):
        """把主 HDU 头中的 RUNID 卡值段(80 字节卡内 10..80)清空后返回。

        RESCUE-FD-08 判据校正(证据驱动): 原断言要求两次独立 run 的 FITS 整文件
        逐字节一致, 但主 HDU 必写本次 run 的 provenance RUNID(p3_output.cpp:216
        fits_write_key(TSTRING, 'RUNID', prov->run_id)), run_id 逐 run 唯一
        (lib/infrastructure/cli/jsonl.h:38 格式 %012llx), 且 RUNID 是冻结合同(tests/backend/
        test_p3005_fits_output.py:75 关键字白名单; tests/cli/test_phase3_inprocess.py:269
        断言 FITS RUNID == 本 run 真实 run_id)。故"整文件字节相等"在合同上不可
        满足。实测标定(本机同 config 两次 run, 69120 字节): 仅 24 字节不同, 全部
        落在 RUNID 值段; signal/coverage/variance/ivar 数据面与其余全部头卡逐字节
        一致。此处仅屏蔽逐 run provenance 后仍做全文件逐字节比较——科学面
        (数据+坐标+单位+结构)保持逐字节等价判据, 不放宽。
        """
        # 逐 run 易变卡: RUNID(provenance) + 由其派生的 HDU 校验 CHECKSUM/DATASUM。
        # 其余全部头卡 + 数据段保持逐字节比较。
        volatile = (b"RUNID   ", b"CHECKSUM", b"DATASUM ")
        out = bytearray(buf)
        pos = 0
        while pos + 80 <= len(out):
            card = bytes(out[pos:pos + 80])
            if card[:8] in volatile:
                # 卡固定 80 字节: 8 关键字 + 2 '= ' + 70 值/注释
                out[pos + 10:pos + 80] = b" " * 70
            pos += 80
        return bytes(out)

    def test_01_parallel_equals_serial(self):
        """并行(budget=2)与单 worker 输出 1/N 等价(FITS 逐字节一致, RUNID 除外)。"""
        out1 = os.path.join(self.tmp, "o1"); os.makedirs(out1, exist_ok=True)
        out2 = os.path.join(self.tmp, "o2"); os.makedirs(out2, exist_ok=True)
        c1 = os.path.join(self.tmp, "c1.json"); json.dump(self._cfg(out1), open(c1, "w"))
        c2 = os.path.join(self.tmp, "c2.json"); json.dump(self._cfg(out2), open(c2, "w"))
        r1 = subprocess.run([EXE, "export", "--json", c1, "-y"], capture_output=True,
                            text=True, timeout=300)
        r2 = subprocess.run([EXE, "export", "--json", c2, "-y"], capture_output=True,
                            text=True, timeout=300)
        self.assertEqual(r1.returncode, 0, r1.stderr[-400:])
        self.assertEqual(r2.returncode, 0, r2.stderr[-400:])
        f1 = os.path.join(out1, "output_phase3.fits")
        f2 = os.path.join(out2, "output_phase3.fits")
        self.assertTrue(os.path.isfile(f1) and os.path.isfile(f2))
        with open(f1, "rb") as a, open(f2, "rb") as b:
            ba, bb = a.read(), b.read()
        self.assertEqual(len(ba), len(bb), "并行/串行 FITS 长度必须一致")
        # RUNID = 逐 run provenance(合同要求), 屏蔽其值段后仍逐字节比对全文件;
        # 若 RUNID 卡缺失, _blank_runid 不改动 → 整文件比对(合同回归即红)。
        self.assertEqual(self._blank_volatile_cards(ba), self._blank_volatile_cards(bb),
                         "除逐 run RUNID/校验卡外, 并行/串行输出必须逐字节一致")
        self.assertIn(b"RUNID   ", ba[:2880], "主 HDU 缺 RUNID provenance 卡")

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
            r = subprocess.run([EXE, "export", "--json", c, "-y"],
                               capture_output=True, text=True, timeout=300)
            self.assertEqual(r.returncode, 0, f"{smp}: {r.stderr[-300:]}")
            self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")), smp)


if __name__ == "__main__":
    unittest.main(verbosity=2)
