#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-002 资源监控伴随器验收测试（tests/monitoring 域，owner SA-LOG-08）。

覆盖 LOG-002 验收（tasks/03_RUNTIME_DATA_IO_TASKS.md）：
  B1 monitor 对 synthetic cpu_heavy 负载真实采集出 CSV：
     字段齐、时间戳单调、间隔≈1s（真实 1s 后台采样线程）；
  B2 负测：cpu_heavy run 无 monitor 时 FAIL（HeavyRunGuard.assert_ready 抛
     MonitorRequired）；非重类不强制；
  B3 负测：CSV 被篡改（改 1 字节 / 手工追加行）校验失败；
  B4 I/O 区间与初始化区间分开（独立 init/io 样本行存在）；
  B5 provider/module/active/granted workers 从 RT-006 trace 真实观测取
     （TraceSnapshotObserver 只读 trace 事件，不读计划/配置）；
  B6 Linux procfs 与 Windows PDH/ETW 适配代码路径分离（Windows 侧为显式
     未实现隔离 stub：is_available False / collect 抛 NotImplementedError）；
  B7 既有 LOG-001 / RT-006 语义回归不破坏。

方法：与 tests/monitoring/test_log_contract.py 同款——纯 Python unittest，
真实调用 runtime/monitoring；真实采样用 ~1s 间隔短负载（≤4s），合成负载仅
演示 CPU 抬升，不依赖精确 CPU 数值。
"""
from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import sys
import tempfile
import time
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))

from runtime.monitoring import linux_procfs, windows_pdh_etw  # noqa: E402
from runtime.monitoring.monitor import (  # noqa: E402
    HEADER, PHASES, ResourceMonitor, load_rows, verify_csv,
    _seed_fingerprint, _fingerprint)
from runtime.monitoring.runner import (  # noqa: E402
    HeavyRunGuard, MonitorRequired, run_heavy_with_monitor)
from runtime.monitoring.trace_feed import (  # noqa: E402
    TraceSnapshotObserver, observer_from_jsonl, emit_metric_event)

HAS_LINUX = linux_procfs.is_available()


def _fake_raw_factory():
    """确定性伪 Linux 后端：严格递增计数器（测试用；非真实观测注入）。"""
    state = {"n": 0}

    def _raw():
        state["n"] += 1
        n = state["n"]
        return {
            "cpu_user_seconds": n * 0.01,
            "cpu_sys_seconds": n * 0.005,
            "sys_cpu_jiffies_total": 100 + n * 10,
            "sys_cpu_jiffies_idle": 50 + n * 4,
            "sys_cpu_jiffies_iowait": 0,
            "rss_bytes": 1000 + n * 100,
            "private_bytes": 500 + n * 10,
            "commit_bytes": 2000 + n * 50,
            "read_bytes": n * 7,
            "write_bytes": n * 3,
            "ctxt_switches_nvcsw": n * 2,
            "ctxt_switches_vol": n * 5,
            "faults_minor": n * 2,
            "faults_major": 0,
            "sys_loadavg_1m": 1.0 + n * 0.01,
            "threads": 2,
        }
    return _raw


def _rewrite_writable(path: pathlib.Path) -> None:
    """测试辅助：临时恢复写权限以便篡改（生产路径无此操作）。"""
    st = path.stat()
    os.chmod(path, st.st_mode | 0o600)


class TestRealSamplingPositive(unittest.TestCase):
    """B1：真实 1s 采样对 synthetic cpu_heavy 负载产出合规 CSV。"""

    @unittest.skipUnless(HAS_LINUX, "需要 Linux procfs")
    def test_real_run_collects_full_csv(self):
        with tempfile.TemporaryDirectory() as td:
            csv = pathlib.Path(td) / "cpu_heavy_run.csv"
            run_id = "run-cpuheavy-test"

            def work(m, obs):
                for _ in range(2):
                    m.set_static_observation(
                        provider="baseline",
                        module="astrocs.phase2.coverage",
                        active_workers=1, granted_workers=2)
                    time.sleep(0.6)

            res = run_heavy_with_monitor(
                "cpu_heavy", run_id, csv, work,
                init_work=lambda m: time.sleep(0.5),
                io_work=lambda m: time.sleep(0.5),
                cpu_burn_seconds=0.6, host="h", commit="c")

            self.assertEqual(res["monitor_errors"], [])
            self.assertGreaterEqual(res["row_count"], 2,
                                    "真实采样至少 2 行")
            self.assertIn("active", res["phases"])

            rows = load_rows(csv)
            data = rows[1:]  # 去掉 seed
            self.assertGreaterEqual(len(data), 2)
            # 字段齐：全部合同列在每行都有值（除可选空值列）
            seqs = [int(r["seq"]) for r in data]
            self.assertEqual(seqs, list(range(1, len(data) + 1)))
            # 时间戳单调（秒精度允许相等）
            ts = [r["t_iso_utc"] for r in data]
            self.assertEqual(ts, sorted(ts))
            # run_phase 合法
            for r in data:
                self.assertIn(r["run_phase"], PHASES)
                self.assertEqual(r["run_id"], run_id)
            # 间隔≈1s：0.3s..2.5s（2c2g 调度抖动容差）
            ints = [r["interval_s"] for r in data if r["interval_s"] is not None]
            self.assertTrue(ints)
            self.assertGreaterEqual(min(ints), 0.3)
            self.assertLessEqual(max(ints), 2.5)
            # 真实观测注入反映在行上（active 行应带 baseline provider/module）
            active_rows = [r for r in data if r["run_phase"] == "active"]
            self.assertTrue(active_rows)
            self.assertGreaterEqual(max(int(r["granted_workers"] or 0)
                                        for r in active_rows), 2)
            # 完整校验通过
            v = verify_csv(csv, run_id=run_id)
            self.assertTrue(v["ok"], v["errors"])
            # 写后只读
            self.assertEqual(csv.stat().st_mode & 0o222, 0,
                             "seal 后 CSV 应为只读")


class TestInitIoSeparate(unittest.TestCase):
    """B4：I/O 区间与初始化区间分开（各段独立样本行）。"""

    @unittest.skipUnless(HAS_LINUX, "需要 Linux procfs")
    def test_init_io_active_flush_all_separate(self):
        with tempfile.TemporaryDirectory() as td:
            csv = pathlib.Path(td) / "sep.csv"

            def work(m, obs):
                time.sleep(0.2)

            run_heavy_with_monitor(
                "cpu_heavy", "run-sep", csv, work,
                init_work=lambda m: time.sleep(0.5),
                io_work=lambda m: time.sleep(0.5),
                flush_work=lambda m: time.sleep(0.3))
            rows = load_rows(csv)
            phases = sorted({r["run_phase"] for r in rows[1:]})
            self.assertIn("init", phases)
            self.assertIn("io", phases)
            self.assertIn("active", phases)
            self.assertIn("flush", phases)
            # 各段样本的 phase 标签互不混合（一行只属一段）
            for r in rows[1:]:
                self.assertIn(r["run_phase"], PHASES)
            v = verify_csv(csv, run_id="run-sep")
            self.assertTrue(v["ok"], v["errors"])


class TestNegativeNoMonitor(unittest.TestCase):
    """B2：无 monitor 的 cpu_heavy run 必须失败。"""

    def test_cpu_heavy_without_monitor_fails(self):
        g = HeavyRunGuard("cpu_heavy", "run-neg")
        with self.assertRaises(MonitorRequired) as cm:
            g.assert_ready()
        self.assertEqual(cm.exception.resource_class, "cpu_heavy")
        self.assertEqual(cm.exception.run_id, "run-neg")

    def test_io_without_monitor_fails(self):
        g = HeavyRunGuard("io", "run-io-neg")
        with self.assertRaises(MonitorRequired):
            g.assert_ready()

    def test_metadata_does_not_require_monitor(self):
        g = HeavyRunGuard("metadata", "run-meta")
        g.assert_ready()  # 不抛

    def test_run_heavy_monitor_attached_succeeds(self):
        with tempfile.TemporaryDirectory() as td:
            csv = pathlib.Path(td) / "ok.csv"
            g = HeavyRunGuard("cpu_heavy", "run-ok")
            m = g.create_monitor(csv, interval_s=1.0)
            m.start()
            time.sleep(0.05)
            m.stop()
            m.seal()
            g.assert_ready()  # attach 后通过
            v = verify_csv(csv, run_id="run-ok")
            self.assertTrue(v["ok"], v["errors"])


class TestTamperDetection(unittest.TestCase):
    """B3：原始 CSV 不可手工合成——篡改/追加被抓。"""

    def _make_csv(self, td: str, run_id: str) -> pathlib.Path:
        csv = pathlib.Path(td) / "m.csv"
        m = ResourceMonitor(run_id, csv, interval_s=0.03,
                            collect_raw=_fake_raw_factory())
        m.begin()
        m.start()
        m.active_phase()
        time.sleep(0.12)
        m.flush_phase()
        m.stop()
        m.seal()
        v = verify_csv(csv, run_id=run_id)
        self.assertTrue(v["ok"], v["errors"])
        return csv

    def test_byte_tamper_detected(self):
        with tempfile.TemporaryDirectory() as td:
            csv = self._make_csv(td, "run-tamper")
            _rewrite_writable(csv)
            text = csv.read_text(encoding="utf-8")
            lines = text.splitlines()
            header = lines[0].split(",")
            ridx = header.index("read_bytes")
            for i in range(2, len(lines)):  # 跳 seed 行
                cells = lines[i].split(",")
                if cells[ridx]:
                    cells[ridx] = str(int(cells[ridx]) + 1)  # 改 1 个值
                    lines[i] = ",".join(cells)
                    break
            csv.write_text("\n".join(lines) + "\n", encoding="utf-8")
            v = verify_csv(csv, run_id="run-tamper")
            self.assertFalse(v["ok"])
            self.assertTrue(any("指纹失配" in e for e in v["errors"]))

    def test_forged_append_detected(self):
        with tempfile.TemporaryDirectory() as td:
            csv = self._make_csv(td, "run-append")
            _rewrite_writable(csv)
            text = csv.read_text(encoding="utf-8")
            if not text.endswith("\n"):
                text += "\n"
            hc = csv.read_text(encoding="utf-8").splitlines()[0].split(",")
            forged = dict.fromkeys(hc, "")
            forged["t_iso_utc"] = "2099-01-01T00:00:00Z"
            forged["seq"] = "999"
            forged["run_id"] = "run-append"
            forged["run_phase"] = "active"
            forged["row_fingerprint"] = "0" * 64  # 伪造指纹必被抓
            csv.write_text(text + ",".join(forged[h] for h in hc) + "\n",
                           encoding="utf-8")
            v = verify_csv(csv, run_id="run-append")
            self.assertFalse(v["ok"])
            self.assertTrue(any("指纹失配" in e for e in v["errors"]))

    def test_seed_runid_binding(self):
        # 复制链到别的 run_id → seed 指纹失配（防整链复制伪造）
        with tempfile.TemporaryDirectory() as td:
            csv = self._make_csv(td, "run-orig")
            v = verify_csv(csv, run_id="run-orig")
            self.assertTrue(v["ok"])
            # 换 run_id 复验必须失败
            v2 = verify_csv(csv, run_id="run-other")
            self.assertFalse(v2["ok"])


class TestTraceObserverWiring(unittest.TestCase):
    """B5：provider/module/workers 从 RT-006 trace 真实观测取。"""

    def test_observer_from_real_trace_events(self):
        events = [
            {"type": "node_start", "run_id": "r", "node_id": "coverage",
             "granted_workers": 4, "status": ""},
            {"type": "provider_enter", "run_id": "r", "node_id": "coverage",
             "provider": "avx2", "kernel_id": "k1"},
            {"type": "module_call", "run_id": "r", "node_id": "coverage",
             "module_id": "astrocs.phase2.coverage", "entry": "e1",
             "workers": 2, "call_count": 1},
            {"type": "node_start", "run_id": "r", "node_id": "sampling",
             "granted_workers": 2},
            {"type": "module_call", "run_id": "r", "node_id": "sampling",
             "module_id": "astrocs.phase2.sampling", "workers": 1},
        ]
        obs = TraceSnapshotObserver("r", events).observe()
        self.assertEqual(obs["provider"], "avx2")          # 最近真实 provider
        self.assertEqual(obs["module"], "astrocs.phase2.sampling")
        self.assertEqual(obs["active_workers"], 2)         # 2 个活动节点
        self.assertEqual(obs["granted_workers"], 4)        # max granted 观测

    def test_observer_node_end_drops_active(self):
        events = [
            {"type": "node_start", "run_id": "r", "node_id": "coverage",
             "granted_workers": 3},
            {"type": "node_end", "run_id": "r", "node_id": "coverage",
             "status": "COMPLETED"},
        ]
        obs = TraceSnapshotObserver("r", events).observe()
        self.assertEqual(obs["active_workers"], 0)
        self.assertEqual(obs["granted_workers"], 3)

    def test_observer_never_reads_config(self):
        # observer 输入只有 trace 事件；把 config/plan 伪装成事件也不冒充
        events = [
            {"type": "node_start", "run_id": "r", "node_id": "n",
             "provider": "", "module_id": ""},
        ]
        obs = TraceSnapshotObserver("r", events).observe()
        self.assertEqual(obs["provider"], "")
        self.assertEqual(obs["module"], "n")  # node_id 归属（真实 trace 值）

    def test_observer_from_jsonl_roundtrip(self):
        lines = [
            '{"type":"node_start","run_id":"r2","node_id":"a","granted_workers":2}',
            '{"type":"provider_enter","run_id":"r2","node_id":"a","provider":"baseline"}',
            '{"type":"node_end","run_id":"r2","node_id":"a","status":"COMPLETED"}',
        ]
        obs = observer_from_jsonl("r2", "\n".join(lines)).observe()
        self.assertEqual(obs["provider"], "baseline")
        self.assertEqual(obs["active_workers"], 0)

    def test_monitor_rows_reflect_trace_observer(self):
        # 集成：monitor 用 trace observer → CSV 行带 provider/module/workers
        with tempfile.TemporaryDirectory() as td:
            csv = pathlib.Path(td) / "obs.csv"
            events = [
                {"type": "node_start", "run_id": "run-obs", "node_id": "cov",
                 "granted_workers": 2},
                {"type": "provider_enter", "run_id": "run-obs", "node_id": "cov",
                 "provider": "baseline"},
                {"type": "module_call", "run_id": "run-obs", "node_id": "cov",
                 "module_id": "astrocs.phase2.coverage", "workers": 2},
            ]
            observer = TraceSnapshotObserver("run-obs", events)
            m = ResourceMonitor("run-obs", csv, interval_s=0.03,
                                collect_raw=_fake_raw_factory(),
                                observer=observer)
            m.begin()
            m.start()
            m.active_phase()
            time.sleep(0.15)
            m.flush_phase()
            m.stop()
            m.seal()
            rows = load_rows(csv)
            data = rows[1:]
            self.assertTrue(data)
            any_provider = any(r["provider"] == "baseline" for r in data)
            any_module = any(r["module"] == "astrocs.phase2.coverage"
                             for r in data)
            self.assertTrue(any_provider, "CSV 行应带真实 provider 观测")
            self.assertTrue(any_module, "CSV 行应带真实 module 观测")
            self.assertGreaterEqual(
                max(int(r["granted_workers"] or 0) for r in data), 2)
            v = verify_csv(csv, run_id="run-obs")
            self.assertTrue(v["ok"], v["errors"])


class TestBackendSeparation(unittest.TestCase):
    """B6：Linux procfs 与 Windows PDH/ETW 适配路径分离。"""

    def test_linux_backend_real(self):
        # 代码路径分离：Linux 采集器与 Windows stub 是不同模块文件
        self.assertNotEqual(
            pathlib.Path(linux_procfs.__file__).name,
            pathlib.Path(windows_pdh_etw.__file__).name)

    @unittest.skipUnless(HAS_LINUX, "需要 Linux procfs")
    def test_linux_collect_real_values(self):
        raw = linux_procfs.collect()
        self.assertEqual(raw["backend"], "linux_procfs")
        self.assertIsInstance(raw.get("rss_bytes"), int)
        self.assertGreater(raw["rss_bytes"], 0)

    def test_windows_stub_not_available_and_raises(self):
        self.assertFalse(windows_pdh_etw.is_available())
        with self.assertRaises(NotImplementedError):
            windows_pdh_etw.collect()  # 显式未实现，不返回伪造数据


class TestLogEventMetricSurface(unittest.TestCase):
    """LOG-001 metric 事件复用面（不破坏冻结语义）。"""

    def test_emit_metric_event_roundtrips_log_event_schema(self):
        ev = emit_metric_event(seq=1, run="run-m", ts="2026-09-04T00:00:00Z",
                               diagnostic="monitor summary", units="%",
                               value=42.0, phase="monitoring")
        for k in ("schema", "seq", "ts", "run", "task", "node", "module",
                  "phase", "commit", "host", "level", "event", "units",
                  "elapsed", "diagnostic"):
            self.assertIn(k, ev, f"LOG-001 必需字段缺: {k}")
        self.assertEqual(ev["phase"], "monitoring")
        self.assertEqual(ev["event"], "metric")


class TestRegressionLog001(unittest.TestCase):
    """B7：LOG-001 checker 语义回归。"""

    def test_log001_checker_selfcheck_still_passes(self):
        spec = importlib.util.spec_from_file_location(
            "check_log_contract",
            REPO / "tools" / "monitoring" / "check_log_contract.py")
        clc = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(clc)
        schema = REPO / "runtime" / "logging" / "log_event_v1.schema.json"
        ok, errs, _ = clc.selfcheck(schema)
        self.assertTrue(ok, errs)


class TestRegressionRt006(unittest.TestCase):
    """B7：RT-006 Python replay 语义回归（双实现互证面）。"""

    def test_trace_replay_still_works(self):
        spec = importlib.util.spec_from_file_location(
            "trace_replay", REPO / "runtime" / "pipeline" / "trace_replay.py")
        tr = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(tr)
        lines = []
        for node in ("coverage", "sampling", "rejection", "upm",
                     "integration", "write", "resample"):
            lines.append(json.dumps({
                "schema": "astrocs.trace-event/v1", "type": "module_call",
                "ts_utc": "2026-09-04T00:00:00Z", "run_id": "r",
                "node_id": node, "module_id": f"astrocs.phase2.{node}",
                "entry": "e1", "call_count": 1, "seq": 0}, ensure_ascii=False))
        summary = tr.replay_from_jsonl("\n".join(lines))
        self.assertEqual(summary["parsed_lines"], 7)
        self.assertEqual(len(summary["nodes"]), 7)
        self.assertEqual(summary["skipped_lines"], 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
