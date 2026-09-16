#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G-RES-01 重计算负载资源门验收测试（tests/monitoring 域）。

判据权威 = docs/plugins/infrastructure/21_observability.md §8；阈值唯一数值源 =
contracts/resource_gate_v1.json（GATE-FIX-RES / R-4 D-11..D-15 落地）。

硬失败三条（契约 hard_fail_criteria；fail → 退出码 10）：
  ① 单活跃计算线程（活跃线程数 **p50** < 2；峰值只回显/样本<2 时回落——峰值
     为判据会放过"绝大多数时间单线程、偶发并发"的 run）；
  ② 任何连续 ≥10s 窗口利用率 <60% **且队列有工作**（runnable p50 ≥2；
     无 runnable 证据时沿用无前置判据）；
  ③ 无界内存增长（C++ 侧 alloc report；本实现只做事实回显）。

record_and_justify（必须记录 + 超标须登记，**不改变退出码**）：
  平均利用率 <85%、p50 <90%、逐样本 ≥85% 占比 <0.70
  —— 依据：16-worker 真负载实测仅 65.09%（reports/v6/performance/
  PERF-SCALE-001.md），未标定前不得硬失败。

验收映射：

  G1 正向：4 worker / 4 有效 CPU / 12s 区间 / 利用率 ~95% → pass，无记录项；
  G2 边界：区间恰 10.0s → NOT_APPLICABLE（"超过 10 秒" 严格大于）；
           10.0+s 生效；有效 CPU=1 → NOT_APPLICABLE；有效 CPU=2 生效；
  G3 记录项：平均利用率 < 85% → recorded 含 frozen_avg_utilization_low，
           且 verdict 仍为 pass（record_and_justify，不改退出码）；
  G4 硬失败：连续 ≥10s 窗口利用率 < 60% → frozen_low_utilization_window
           （无 runnable 证据 → 沿用无前置判据）；9.9s 低窗不触发（边界）；
  G5 硬失败：活跃线程 p50 < 2（单活跃计算线程）→ frozen_single_active_thread；
  G6 fail-closed：CPU 采样不可得（None/空）→ monitoring_missing（不是 pass）；
           活跃线程不可得（threads_max=0）→ monitoring_missing；
           区间中部监控断流（样本内 None）→ monitoring_missing；
           非法 allocated_workers（负数/非整数）→ monitoring_missing；
  G7 确定性：同一输入两次判定结果逐字段一致（bitwise JSON）；
  G8 故障注入必败：对 G1 通过样例注入"监控失明"（抹掉 CPU 样本）→ 判定必须
     翻转为 fail（若门禁返回 pass 即测试失败——门禁可被注入击败 = 无效）；
  G9 CLI 集成：--gate-workers/--gate-selected-workers/--gate-effective-cpus
     旗标解析（库面不改动既有返回结构）；
  G10 分母纠错（本任务核心缺陷）：2 核满核 run 在 granted=2 下利用率 100%
     （无记录项），在机器有效核 16 下才被判低 —— 旧实现把机器有效核当已分配
     容量 ⇒ 结构性误报；
  G11 哨兵：granted/selected 皆哨兵 → allocated=0 → 利用率判据不参与裁决
     （utilization_evaluated=False + recorded 显式登记），不得用机器核冒充；
  G12 队列前置：低利用窗内 runnable p50 ≥2 → 硬失败；runnable p50 =1（纯
     串行/等待）→ 记入 recorded，不判 CPU 饥饿；
  G13 三分量回显与数值唯一源：metrics 回显 selected/available/granted；
     模块常量 == contracts/resource_gate_v1.json。
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

    # ── G3 平均利用率（record_and_justify：记录但不改退出码） ──
    def test_g3_avg_utilization_low_recorded_not_fail(self):
        r = _good_result(interval=12.0)
        r["cpu_percent_avg"] = 260.0  # 65% of 4
        for s in r["cpu_samples"]:
            s["cpu_percent"] = 260.0
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "pass", v)
        self.assertEqual(v["violations"], [], v)
        names = [x.split(":")[0] for x in v["recorded"]]
        self.assertIn("frozen_avg_utilization_low", names, v)
        self.assertIn("frozen_p50_utilization_low", names, v)
        self.assertIn("frozen_sample_utilization_low", names, v)

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
        # allocated_workers 语义（契约 denominator）：0/None = **未声明哨兵**
        # （利用率类判据不成立 → recorded，见 G11）；**负数/非整数**才是非法输入
        # → fail-closed。
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=-1)
        self.assertEqual(v["verdict"], "fail", v)
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers="4")
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
        self.assertIsNone(args.gate_selected_workers)
        self.assertFalse(args.gate_require_progress)

    # ── G10 分母纠错（本任务核心缺陷） ──
    def test_g10_denominator_granted_not_machine_cores(self):
        """2 核满核 run：granted=2 → 利用率 100%（无记录项）；机器核 16 → 低。

        旧实现把机器有效核当已分配容量 ⇒ 任何 worker 数 < 机器核数的并行任务
        结构性判红（R-4 E7b：2 线程满核 14s → U=6.3% → rc=10）。
        """
        r = _good_result(interval=14.0)
        r["cpu_percent_avg"] = 200.0  # 2 核满载（16 核机器）
        for s in r["cpu_samples"]:
            s["cpu_percent"] = 200.0
            s["threads"] = 3
        r["threads_max"] = 3
        granted = rm.resolve_allocated_capacity(
            granted_workers=2, selected_workers=16, available_cpus=16)
        self.assertEqual(granted, 2)
        v = rm.evaluate_frozen_gate(
            r, effective_cpus=16, allocated_workers=granted,
            selected_workers=16, granted_workers=2)
        self.assertEqual(v["verdict"], "pass", v)
        self.assertEqual(v["recorded"], [], v)
        self.assertAlmostEqual(v["metrics"]["avg_utilization"], 1.0, places=3)
        # 同一实测输入、错误分母（机器核）→ 才会被判低利用率
        v_bad = rm.evaluate_frozen_gate(r, effective_cpus=16, allocated_workers=16)
        self.assertAlmostEqual(v_bad["metrics"]["avg_utilization"], 0.125, places=3)
        self.assertTrue(any(x.startswith("frozen_avg_utilization_low")
                            for x in v_bad["recorded"]), v_bad)

    # ── G11 哨兵：未声明已分配容量 → 利用率不参与裁决 ──
    def test_g11_undeclared_capacity_not_judged(self):
        r = _good_result(interval=14.0)
        r["cpu_percent_avg"] = 200.0
        v = rm.evaluate_frozen_gate(
            r, effective_cpus=16,
            allocated_workers=rm.resolve_allocated_capacity(
                granted_workers=0, selected_workers=0, available_cpus=16))
        self.assertEqual(v["verdict"], "pass", v)
        self.assertEqual(v["metrics"]["allocated"], 0)
        self.assertFalse(v["metrics"]["utilization_evaluated"])
        self.assertTrue(any(x.startswith("allocated_capacity_undeclared")
                            for x in v["recorded"]), v)

    # ── G12 队列前置（CPU 饥饿 vs 并行宽度不足） ──
    def _low_window_result(self, runnable):
        poll = 0.2
        r = _good_result(interval=14.0, poll=poll)
        for s in r["cpu_samples"]:
            s["cpu_percent"] = 40.0     # 10% of 4 核
            s["runnable"] = runnable(s["t"])
        r["cpu_percent_avg"] = 40.0
        return r

    def test_g12_queued_work_makes_window_hard_fail(self):
        # 4 核配额、8 个就绪线程长期抢 0.4 核 → 队列有工作 → 硬失败
        r = self._low_window_result(lambda t: 8)
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "fail", v)
        self.assertTrue(any(x.startswith("frozen_low_utilization_window")
                            for x in v["violations"]), v)

    def test_g12_no_queue_is_recorded_not_fail(self):
        # 4 核配额、仅 1 个就绪线程（纯等待/串行）→ 不判 CPU 饥饿
        r = self._low_window_result(lambda t: 1)
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=4)
        self.assertEqual(v["verdict"], "pass", v)
        self.assertTrue(any(x.startswith(
            "frozen_low_utilization_window_no_queue") for x in v["recorded"]), v)

    # ── G13 三分量回显 + 数值唯一源 ──
    def test_g13_three_component_echo_and_single_numeric_source(self):
        r = _good_result()
        v = rm.evaluate_frozen_gate(r, effective_cpus=4, allocated_workers=2,
                                    selected_workers=4, granted_workers=2)
        m = v["metrics"]
        for key in ("selected_workers", "available_cpus", "granted_workers",
                    "allocated", "allocated_source"):
            self.assertIn(key, m, v)
        self.assertEqual(m["allocated_source"], "granted_workers")
        contract = rm.RESOURCE_GATE_CONTRACT
        self.assertEqual(contract["schema"], "astrocs.resource-gate/v1")
        comp = contract["compute"]
        self.assertEqual(rm.FROZEN_GATE_MIN_AVG_UTILIZATION,
                         comp["mean_utilization_min_percent"] / 100.0)
        self.assertEqual(rm.FROZEN_GATE_MIN_P50_UTILIZATION,
                         comp["p50_utilization_min_percent"] / 100.0)
        self.assertEqual(rm.FROZEN_GATE_MIN_SAMPLE_PASS_FRACTION,
                         comp["per_sample_pass_fraction_min"])
        self.assertEqual(rm.FROZEN_GATE_WINDOW_MIN_UTILIZATION,
                         comp["queue_low_utilization_percent"] / 100.0)
        self.assertEqual(rm.FROZEN_GATE_WINDOW_SECONDS,
                         comp["queue_low_window_seconds_min"])
        self.assertEqual(rm.FROZEN_GATE_MIN_ACTIVE_THREADS,
                         comp["min_active_compute_threads"])
        self.assertEqual(contract["memory"]["growth_limit_mib_per_s"], 32.0)
        self.assertEqual(contract["memory"]["growth_predicate"], ">=")
        # 契约不可得 → fail-closed（不得静默回落内置默认值）
        with self.assertRaises(RuntimeError):
            rm.load_resource_gate_contract("/nonexistent/resource_gate_v1.json")


if __name__ == "__main__":
    unittest.main()
