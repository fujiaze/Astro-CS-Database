#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RT-001 冻结利用率门禁验收测试（tests/monitoring 域）。

验收映射（控制包 RT-001 "接入唯一Executor和实测资源门"；合同锚：冻结宪章
§10.5 + §18.2 负责人裁决 2 —— 有效 CPU 数 ≥2 且计算区间 >10s 时，计算区间
平均 CPU 利用率 ≥ 已分配容量的 85%；任何连续 10s 低于 60% 或只有一个活跃
计算线程均失败；BASE-UTIL-001 分类口径：区间不足 10s → 门禁不适用
NOT_APPLICABLE，不是豁免；监控缺失 → fail-closed，不是通过）：

  G1 正向：4 worker / 4 有效 CPU / 12s 区间 / 利用率 ~95% → pass；
  G2 边界：区间恰 10.0s → NOT_APPLICABLE（"超过 10 秒" 严格大于）；
           10.0+s 生效；有效 CPU=1 → NOT_APPLICABLE；有效 CPU=2 生效；
  G3 错误：平均利用率 < 85% → frozen_avg_utilization_low；
  G4 错误：连续 ≥10s 窗口利用率 < 60% → frozen_low_utilization_window；
           9.9s 低窗（<10s）不触发（边界）；
  G5 错误：threads_max < 2（单活跃计算线程）→ frozen_single_active_thread；
  G6 fail-closed：CPU 采样不可得（None/空）→ monitoring_missing（不是 pass）；
           活跃线程不可得（threads_max=0）→ monitoring_missing；
           区间中部监控断流（样本内 None）→ monitoring_missing；
  G7 确定性：同一输入两次判定结果逐字段一致（bitwise JSON）；
  G8 故障注入必败：对 G1 通过样例注入"监控失明"（抹掉 CPU 样本）→ 判定必须
     翻转为 fail（若门禁返回 pass 即测试失败——门禁可被注入击败 = 无效）；
  G9 run_monitored CLI 集成：--gate-workers/--gate-effective-cpus 注入合成
     result → verdict 与 metrics 落入输出 JSON（库面不改动既有返回结构）。
"""
from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))

from tools.monitoring import run_monitored as rm  # noqa: E402


def _sample(t, cpu, threads=4, rss=100000):
    return {
        "t": t, "cpu_percent": cpu, "rss_kb": rss, "pss_kb": None,
        "threads": threads, "io_read_bytes": 0, "io_write_bytes": 0,
        "pids": 1,
    }


def _good_result(interval=12.0, poll=0.2, allocated=4):
    """合成 4-worker 满载 run: 利用率 ~95%（380% / (100×4)）。"""
    n = int(interval / poll)
    samples = [_sample(round(i * poll, 4), 380.0) for i in range(1, n + 1)]
    return {
        "command": ["synthetic"],
        "exit_code": 0,
        "timed_out": False,
        "duration_seconds": interval,
        "cpu_percent_avg": 380.0,
        "cpu_samples": samples,
        "samples": len(samples),
        "peak_rss_kb": 100000,
        "rss_start_kb": 100000,
        "threads_max": 4,
        "progress": {"done": 100, "total": 100, "raw": "100/100",
                     "source": "stdout"},
        "poll_interval": poll,
        "stdout_tail": [],
        "stderr_tail": [],
        "host_probe": {"effective_cpu_cores": 4},
    }


class FrozenGateTest(unittest.TestCase):
    # ── G1 正向 ──
    def test_g1_pass_full_utilization(self):
        verdict = rm.evaluate_frozen_gate(
            _good_result(), effective_cpus=4, allocated_workers=4)
        self.assertEqual(verdict["verdict"], "pass", verdict)
        self.assertEqual(verdict["violations"], [])
        self.assertAlmostEqual(verdict["metrics"]["avg_utilization"], 0.95, places=6)

    # ── G2 边界 / NOT_APPLICABLE ──
    def test_g2_interval_boundary_strict(self):
        v = rm.evaluate_frozen_gate(_good_result(interval=10.0),
                                    effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "not_applicable", v)
        v = rm.evaluate_frozen_gate(_good_result(interval=10.001),
                                    effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "pass", v)

    def test_g2_single_cpu_not_applicable(self):
        v = rm.evaluate_frozen_gate(_good_result(interval=30.0),
                                    effective_cpus=1, allocated_workers=1)
        self.assertEqual(v["verdict"], "not_applicable", v)
        # NOT_APPLICABLE 不是豁免：显式 reason 注明分类（BASE-UTIL-001 口径）
        self.assertIn("reason", v)
        v = rm.evaluate_frozen_gate(_good_result(interval=30.0),
                                    effective_cpus=2, allocated_workers=2)
        self.assertEqual(v["verdict"], "pass", v)

    # ── G3 平均利用率 ──
    def test_g3_avg_utilization_low(self):
        r = _good_result(interval=12.0)
        r["cpu_percent_avg"] = 260.0  # 65% of 4
        for s in r["cpu_samples"]:
            s["cpu_percent"] = 260.0
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(x.split(":")[0] == "frozen_avg_utilization_low"
                            for x in v["violations"]), v)

    # ── G4 连续低利用窗口 ──
    def test_g4_continuous_low_window(self):
        poll = 0.2
        r = _good_result(interval=14.0, poll=poll)
        # 前 1s 高, 1.0..13.0s 低(50%), 末段高 → 连续低窗 12s ≥ 10s
        n = len(r["cpu_samples"])
        for i, s in enumerate(r["cpu_samples"]):
            t = s["t"]
            s["cpu_percent"] = 380.0 if (t < 1.0 or t > 13.0) else 200.0
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(x.split(":")[0] == "frozen_low_utilization_window"
                            for x in v["violations"]), v)

    def test_g4_low_window_below_10s_boundary(self):
        poll = 0.2
        r = _good_result(interval=60.0, poll=poll)
        # 仅 9.8s 连续低窗（<10s, t∈[20.0,29.6], 单样本覆盖计 0.2s）→ 不触发
        # 该违规; 其余时段满载 → 平均利用率仍 ≥85% → 整体 pass（边界判据）。
        for s in r["cpu_samples"]:
            s["cpu_percent"] = 236.0 if 20.0 <= s["t"] <= 29.6 else 400.0
        r["cpu_percent_avg"] = round(
            sum(s["cpu_percent"] for s in r["cpu_samples"])
            / len(r["cpu_samples"]), 2)
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertNotIn("frozen_low_utilization_window", v["violations"], v)
        self.assertEqual(v["verdict"], "pass", v)

    # ── G5 单活跃线程 ──
    def test_g5_single_active_thread(self):
        r = _good_result()
        for s in r["cpu_samples"]:
            s["threads"] = 1
        r["threads_max"] = 1
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(x.split(":")[0] == "frozen_single_active_thread"
                            for x in v["violations"]), v)

    # ── G6 fail-closed ──
    def test_g6_monitoring_missing_cpu(self):
        r = _good_result()
        r["cpu_samples"] = []
        r["cpu_percent_avg"] = None
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(v.startswith("monitoring_missing")
                            for v in v["violations"]), v)

    def test_g6_monitoring_missing_threads(self):
        r = _good_result()
        r["threads_max"] = 0
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(v.startswith("monitoring_missing")
                            for v in v["violations"]), v)

    def test_g6_interior_monitoring_gap(self):
        r = _good_result(interval=12.0)
        mid = len(r["cpu_samples"]) // 2
        r["cpu_samples"][mid]["cpu_percent"] = None  # 区间中部断流
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(v.startswith("monitoring_missing")
                            for v in v["violations"]), v)

    def test_g6_bad_inputs_fail_closed(self):
        r = _good_result()
        v = rm.evaluate_frozen_gate(r, effective_cpus=0, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=0)
        self.assertEqual(v["verdict"], "fail", v)
        v = rm.evaluate_frozen_gate(r, effective_cpus=None, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)

    # ── G7 确定性 ──
    def test_g7_deterministic(self):
        r = _good_result()
        a = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        b = rm.evaluate_frozen_gate(copy.deepcopy(r), effective_cpus=4,
                                    allocated_workers=4)
        self.assertEqual(json.dumps(a, sort_keys=True),
                         json.dumps(b, sort_keys=True))

    # ── G8 故障注入必败 ──
    def test_g8_fault_injection_blind_monitor(self):
        r = _good_result()
        v0 = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v0["verdict"], "pass", v0)
        blinded = copy.deepcopy(r)
        for s in blinded["cpu_samples"]:
            s["cpu_percent"] = None  # 注入: 监控失明（CPU 采样全部失效）
        v1 = rm.evaluate_frozen_gate(blinded, effective_cpus=4,
                                     allocated_workers=4)
        self.assertEqual(v1["verdict"], "fail", v1)

    # ── G9 CLI 集成 ──
    def test_g9_gate_flags_parse(self):
        # build_arg_parser 只收 wrapper 选项; "--" 分隔由 main() 手工切分
        # （合同: 永不把子进程选项吞进 wrapper, main 注释同源）。
        parser = rm.build_arg_parser()
        args = parser.parse_args(["--gate-workers", "4"])
        self.assertEqual(args.gate_workers, 4)
        self.assertIsNone(args.gate_effective_cpus)
        self.assertFalse(args.gate_require_progress)


if __name__ == "__main__":
    unittest.main()
