#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P26 T1 外挂资源监控自检(阴性对照)。

覆盖:
  * 真实采样: samples.csv/threads.csv/summary.json/三张 curve_*.png 落盘且口径自洽;
  * --pid 挂载: 完全不依赖被测程序配合;
  * --timeout: 超时透传 124;
  * --judge 阴性对照(证明判定不是恒真/恒假):
      - 健康并行合成曲线 → PASS;
      - 高于工作量下限但有效核数=1 的饥饿曲线 → FAIL(含 single_busy_compute_thread);
      - 低于工作量下限的饥饿曲线 → INSUFFICIENT(只记录不裁决, 证明下限非空);
      - 均值临界带(85~90) → WARN;
      - 均值 70% → FAIL(证明不是恒 PASS)。
  多档结论并存即排除"判定恒真/恒假"。
"""
from __future__ import annotations

import json
import os
import statistics
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools" / "quality" / "resource_monitor.py"
sys.path.insert(0, str(REPO / "tools" / "quality"))
import resource_monitor as rm  # noqa: E402


def _rec(t, dt, cap_pct, eff, busy, rss_mb, n_threads=8):
    return {
        "t": t, "iso_utc": "2026-09-15T00:00:00.000000Z", "dt": dt,
        "cpu_ticks_total": int(t * 100), "cpu_pct_core": eff * 100.0,
        "cpu_pct_capacity": cap_pct, "effective_cores": eff,
        "busy_threads": busy, "n_threads": n_threads, "n_procs": 5,
        "rss_bytes": int(rss_mb * 1024 * 1024), "pss_bytes": int(rss_mb * 1024 * 1024),
        "rss_growth_mb_per_s": 0.1, "read_bytes": int(t * 1e6),
        "write_bytes": int(t * 2e6), "read_bytes_per_s": 1e6, "write_bytes_per_s": 2e6,
        "io_wait_ms": 0.0,
    }


def _synth(interval, cap_pct_values, eff_values, capacity_cores, out):
    recs = []
    for i, pct in enumerate(cap_pct_values):
        recs.append(_rec(i * interval, interval if i else 0.0, pct, eff_values[i],
                         8 if eff_values[i] >= 2 else 1, 100.0))
    summary = rm.facts(recs, capacity_cores, 60.0, 10.0)
    summary.update({"capacity_cores": capacity_cores, "wall_seconds_total":
                    summary["wall_seconds"]})
    out.mkdir(parents=True, exist_ok=True)
    rm.write_csv(out / "samples.csv", recs, rm.CSV_COLUMNS)
    (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2),
                                      encoding="utf-8")
    return summary


class TestResourceMonitor(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="p26_mon_"))

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp, ignore_errors=True)

    # ---------------------------------------------------------------- 真实采样
    def test_01_real_sample_products(self):
        out = self.tmp / "real"
        rc = rm.main(["--out", str(out), "--interval", "0.3", "--timeout", "30", "--",
                      sys.executable, "-c", "import time; time.sleep(1.2)"])
        self.assertEqual(rc, 0)
        for name in ("samples.csv", "threads.csv", "summary.json",
                     "curve_cpu.png", "curve_mem.png", "curve_io.png"):
            self.assertTrue((out / name).is_file(), name + " missing")
        summary = json.loads((out / "summary.json").read_text(encoding="utf-8"))
        self.assertGreaterEqual(summary["n_samples"], 2)
        for key in ("cpu_pct_capacity", "effective_cores", "busy_threads",
                    "memory_growth_mb_per_s", "io", "facts"):
            self.assertIn(key, summary)
        self.assertIn("has_low_util_continuous_run_ge_threshold", summary["facts"])
        self.assertIn("single_busy_thread", summary["facts"])
        for name in ("curve_cpu.png", "curve_mem.png", "curve_io.png"):
            blob = (out / name).read_bytes()
            self.assertEqual(blob[:8], b"\x89PNG\r\n\x1a\n", name + " not PNG")
            self.assertGreater(len(blob), 1000, name + " too small/empty")

    def test_02_attach_without_cooperation(self):
        out = self.tmp / "attach"
        proc = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(1.5)"])
        try:
            rc = rm.main(["--out", str(out), "--interval", "0.3", "--pid", str(proc.pid)])
        finally:
            proc.wait(timeout=10)
        self.assertEqual(rc, 0)
        summary = json.loads((out / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(summary["mode"], "attached")
        self.assertGreaterEqual(summary["n_samples"], 2)

    def test_03_timeout_exit_code(self):
        out = self.tmp / "timeout"
        rc = rm.main(["--out", str(out), "--interval", "0.3", "--timeout", "1.0", "--",
                      sys.executable, "-c", "import time; time.sleep(10)"])
        self.assertEqual(rc, 124)
        summary = json.loads((out / "summary.json").read_text(encoding="utf-8"))
        self.assertTrue(summary["timed_out"])

    # ------------------------------------------------- judge 阴性对照(非恒真/恒假)
    def _judge(self, summary_dir, extra=()):
        out = self.tmp / ("judge_" + summary_dir.name)
        rc = rm.main(["--judge", "--from", str(summary_dir), "--out", str(out), *extra])
        self.assertIn(rc, (0, 3))
        return json.loads((out / "judge.json").read_text(encoding="utf-8"))

    def test_04_judge_is_not_vacuous(self):
        cap = 8.0
        interval = 1.0
        n = 20
        healthy = self.tmp / "healthy"
        _synth(interval, [95.0] * n, [7.6] * n, cap, healthy)
        starved = self.tmp / "starved"
        _synth(interval, [12.0] * n, [0.96] * n, cap, starved)
        below = self.tmp / "below_floor"
        _synth(interval, [12.0] * 3, [0.96] * 3, cap, below)
        lowmean = self.tmp / "low_mean"
        _synth(interval, [70.0] * n, [5.6] * n, cap, lowmean)
        near = self.tmp / "near_min"
        _synth(interval, [87.0] * n, [6.96] * n, cap, near)

        j_ok = self._judge(healthy)
        j_fail = self._judge(starved)
        j_floor = self._judge(below)
        j_low = self._judge(lowmean)
        j_near = self._judge(near)

        # 阳性: 健康并行曲线 PASS
        self.assertEqual(j_ok["verdict"], "PASS", j_ok)
        # 阴性方向 1: 饥饿(高于下限)必须 FAIL, 且给出"只有一个活跃计算线程"事实
        self.assertEqual(j_fail["verdict"], "FAIL", j_fail)
        self.assertIn("single_busy_compute_thread", j_fail["failed_checks"])
        self.assertIn("mean_utilization_below_min", j_fail["failed_checks"])
        # 阴性方向 2: 低于工作量下限 → 不裁决(只记录), 证明下限不是空条件
        self.assertEqual(j_floor["verdict"], "INSUFFICIENT", j_floor)
        self.assertEqual(j_floor["reason"], "below_workload_floor")
        self.assertLess(j_floor["work_core_seconds"],
                        j_floor["workload_floor_core_seconds"])
        # 阴性方向 3: 均值 70% → FAIL(不是恒 PASS); 临界带 → WARN(有中间档)
        self.assertEqual(j_low["verdict"], "FAIL", j_low)
        self.assertEqual(j_near["verdict"], "WARN", j_near)
        # 结论集合不是单值 → 判定不恒真/恒假
        self.assertGreaterEqual(
            len({j_ok["verdict"], j_fail["verdict"], j_floor["verdict"],
                 j_low["verdict"], j_near["verdict"]}), 4)

    def test_05_low_util_continuous_and_single_thread_facts(self):
        cap = 4.0
        # 连续 12s 低于 60% 且有效核数<=1.2 → 两条事实都为真
        vals = [30.0] * 13
        summ = _synth(1.0, vals, [1.0] * 13, cap, self.tmp / "lowrun")
        self.assertTrue(summ["facts"]["has_low_util_continuous_run_ge_threshold"])
        self.assertTrue(summ["facts"]["single_busy_thread"])
        self.assertGreaterEqual(summ["facts"]["low_util_continuous_runs_ge_threshold"][0]
                                ["duration_s"], 10.0)
        # 高利用曲线 → 两条事实都为假(对照)
        summ2 = _synth(1.0, [95.0] * 13, [3.8] * 13, cap, self.tmp / "hibrun")
        self.assertFalse(summ2["facts"]["has_low_util_continuous_run_ge_threshold"])
        self.assertFalse(summ2["facts"]["single_busy_thread"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
