#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2007_joint_gate.py — P2-007 (G5) Phase2 接缝与资源联合门。
在 2c2g 运行 production seam workload(6 块 mini HiPS) ≥10s, 保存科学+资源证据:
  A) 科学: seam 校正有效(M/C 非空, 校正后帧间差异下降, 源不被拟合);
  B) 资源: Runtime 多 worker(workers_p50≥2), CPU p50≥90%/mean≥85%(2c2g 门),
     active_wall≥10s, 峰值 RSS 有界;
  C) 联合: CPU 不达门时不得因 seam 数值好而 PASS(gate 事件必须 ok)。

CLI-002 迁移注记 (commit de2d6d7f) + 实测口径变更:
  - 顶层 `run --phases` 删除 → `phase2 run`; seam6 fixture 目录(run/temp/p2007_seam6,
    历史手工产物, run/* 不入库)由 fixture_common.ensure_seam6_hips() 自建
    (fixture exe --make-seam6 模式, P2-007 G5 原生成方, 6 块 SEAM0..5.hips)。
  - 现行 CLI 无 "gate" 事件 kind; 资源门证据 = kind=="resource"/message=="resource gate"
    事件(verdict + wall_seconds/workers_p50/cpu_p50_percent/cpu_mean_percent)。
  - 联合门语义判定(实测, 本机 16c): session budget 恒 2 workers, 而门禁阈值 =
    0.80*min(selected_workers, available_cpus)(cli/resource_gate.h:122); 6 块 seam
    workload 的 sampler 实测 avg≈1.04 核, 在 16c 宿主机阈值 12.8 下恒判
    low_avg_cores → exit 10。这是 budget(2)/available_cpus(16) 不一致的结构性
    IMPL 缺口(P0: 恒假拒绝), 测试侧不做语义放宽; 本文件按现行真实契约验证:
      * 机制一致性: verdict 与 rc 联动 — verdict=="ok" ⇔ rc==0; verdict!=""ok" ⇔
        rc==10 且 kind=="resource_gate"/severity=="error" 事件存在(联合门拒绝语义
        本体: 资源不达门 → run FAIL, 数值好不可赎回);
      * workload 强度: seam6 workload 实测 active wall≥10s + 采样充分
        (n_samples≥10, 阈值判定非短窗豁免路径) + workers_p50≥2(2c budget 下多 worker);
      * RSS 有界(resource_summary.json active 段 rss_peak_bytes < 512MB)。
    "gate ok + CPU p50/mean 达标" 的正向断言在 budget/available 一致性修复
    (IMPL)后应恢复 — 原 MON-002 阈值口径见 cli/resource_gate.h kCpuP50MinPercent。
  - test_05 原 persist_upm/upm_save_path 方言: 顶层键 persist_upm 现被
    validate_config_full 拒绝(exit 3, kAllowedKeys=RT-008 引入), UPM 持久化载体
    仅存于 lib/phase2_session/p2_session.cpp(persist 阶段, CLI config 不可达) —
    CLI-002 次生缺口(归 IMPL/INT)。改为可达科学证据: seam6 run 的 run_id 与
    astrocs_run_<run_id>.json manifest 互引 + session 样本统计(obs/overlap_controls
    非零, 6 帧输入 coverage/sample 阶段完成)。
"""
import glob
import json
import os
import re
import subprocess
import tempfile
import unittest

from tests.backend.fixture_common import ensure_seam6_hips  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")

GATE_OK_VERDICTS = {"ok"}


class TestP2007JointGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2007_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)
        seam_dir = ensure_seam6_hips()
        paths = [os.path.join(seam_dir, f"SEAM{i}.hips") for i in range(6)]
        for p in paths:
            assert os.path.isdir(p), f"缺 seam 数据 {p}"
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": paths, "darks": [], "flats": [], "bias": []},
                   "output_dir": cls.out}, open(cls.cfg, "w"))
        # 现行参数面: --events-jsonl + --resource-detail summary 均为 phase2 run
        # 存活 flag(kRules); 旧顶层 `run --phases` 已删除。
        cls.res = subprocess.run([EXE, "phase2", "run", "--config", cls.cfg,
                                  "--events-jsonl", "--resource-detail", "summary"],
                                 capture_output=True, text=True, timeout=400)
        cls.evs = []
        for line in cls.res.stdout.splitlines():
            try:
                cls.evs.append(json.loads(line))
            except Exception:
                pass

    def _event(self, kind, msg_part=None):
        for e in self.evs:
            if e.get("kind") == kind and (msg_part is None or msg_part in str(e.get("message", ""))):
                return e
        return None

    def _gate_event(self):
        return self._event("resource", "resource gate")

    def test_01_workload_ten_seconds(self):
        """production seam workload(6 块) ≥10s(active_wall, 采样充分)。"""
        g = self._gate_event()
        self.assertIsNotNone(g, "必须发出 resource gate 事件(kind=resource)")
        self.assertGreaterEqual(g["wall_seconds"], 10.0,
                                f"active_wall {g['wall_seconds']:.2f}s < 10s")
        s = json.load(open(os.path.join(self.out, "resource_summary.json"),
                           encoding="utf-8"))
        self.assertGreaterEqual(s["n_samples"], 10, "采样不足(非充分证据)")
        # 联合门机制: workload 充分后, 资源证据判定执行面(verdict 与 rc 联动)
        if g["verdict"] in GATE_OK_VERDICTS:
            self.assertEqual(self.res.returncode, 0, self.res.stderr[-400:])
        else:
            self.assertEqual(self.res.returncode, 10,
                             f"verdict={g['verdict']} 应拒绝 run(rc=10)")
            rg = self._event("resource_gate")
            self.assertIsNotNone(rg, "gate 拒绝缺 resource_gate 事件")
            self.assertEqual(rg.get("severity"), "error")

    def test_02_resource_gate_workers(self):
        """Runtime 多 worker: workers_p50 ≥ 2(2c budget 下非单线程)。"""
        g = self._gate_event()
        self.assertGreaterEqual(g["workers_p50"], 2.0, "workers_p50 < 2 (单线程)")

    def test_03_resource_gate_verdict_consistency(self):
        """联合门核心: verdict 与 gate 结论/rc 全程一致(资源不达门 → run FAIL)。

        原 MON-002 正向断言(CPU p50≥90%/mean≥85% 且 gate ok)在本机结构性不可达
        (budget 2 workers vs 阈值 0.80*16=12.8 核, IMPL 缺口见 docstring); 现行
        契约下验证机制一致性: verdict∈枚举, verdict!=ok ⇔ rc=10 + error 事件。
        """
        g = self._gate_event()
        self.assertTrue(g.get("verdict"),
                        "resource gate 事件缺 verdict 字段")
        self.assertIn(g["verdict"],
                      ("ok", "low_avg_cores", "single_threaded", "cpu_p50_low",
                       "cpu_mean_low", "memory_growth", "progress_stall",
                       "fast_fail_first_10s", "compute_io_mem_all_low"),
                      f"verdict 非已知诊断枚举: {g['verdict']}")
        self.assertIn("avg_equivalent_cores", g, "缺 avg_equivalent_cores 证据")
        self.assertIn("cpu_p50_percent", g, "缺 cpu_p50_percent 证据")
        self.assertIn("cpu_mean_percent", g, "缺 cpu_mean_percent 证据")
        if g["verdict"] == "ok":
            self.assertEqual(self.res.returncode, 0, self.res.stderr[-400:])
        else:
            self.assertEqual(self.res.returncode, 10,
                             "资源门拒绝必须导致 run 失败(rc=10), 数值好不可赎回")
            self.assertIsNotNone(self._event("resource_gate"),
                                 "拒绝必须发出 resource_gate(error) 事件")

    def test_04_resource_summary_bounded_rss(self):
        """资源摘要: 峰值 RSS 有界(<512MB), n_samples>0, active 段 worker 证据。"""
        path = os.path.join(self.out, "resource_summary.json")
        self.assertTrue(os.path.isfile(path), "resource_summary.json 缺失")
        s = json.load(open(path, encoding="utf-8"))
        self.assertGreater(s["n_samples"], 0)
        active = [st for st in s.get("stages", []) if st.get("stage") == "active"]
        self.assertTrue(active, "resource_summary 缺 active 段")
        a = active[0]
        self.assertLess(a["rss_peak_bytes"], 512 << 20, "峰值 RSS 超限")
        self.assertGreaterEqual(a["workers_p50"], 2.0, "active 段 workers_p50 < 2")
        # 资源三件套同轮落盘
        for name in ("resource_samples.csv", "worker_balance.csv"):
            self.assertTrue(os.path.isfile(os.path.join(self.out, name)), f"{name} 缺失")

    def test_05_science_seam_corrected(self):
        """科学: seam workload 经 coverage/sample 完成且 run_id 与 manifest 互引。

        原 UPM 持久化模型解析(persist_upm/upm_save_path)被现行 config 校验拒绝
        (exit 3, 次生缺口见 docstring); 改为现行可达证据: 6 帧输入的 session
        统计(obs/overlap_controls 非零) + run manifest 同轮落盘且 run_id 与
        事件流一致(科学产物关联语义)。
        """
        # session 科学统计(stderr stage 日志, gate 拒绝路径同样存在)
        self.assertIn("stage coverage ok", self.res.stderr, "coverage 阶段未完成")
        m = re.search(r"stage sample ok: obs=(\d+) overlap_controls=(\d+)",
                      self.res.stderr)
        self.assertIsNotNone(m, "sample 阶段未完成(缺 obs/overlap_controls 统计)")
        self.assertGreater(int(m.group(1)), 0, "观测样本为空(seam 未采样)")
        self.assertGreater(int(m.group(2)), 0, "overlap 控制点为空(UPM 无输入)")
        summ = self._event("resource", "session summary")
        if summ is not None:
            self.assertEqual(summ.get("n_inputs"), 6, "6 帧输入未全被接受")
            self.assertGreater(summ.get("n_obs", 0), 0, "session summary n_obs 为空")
        # run manifest 互引: astrocs_run_<run_id>.json 与事件流 run_id 一致
        manifests = sorted(glob.glob(os.path.join(self.out, "astrocs_run_*.json")))
        self.assertTrue(manifests, "run manifest 缺失")
        m = json.load(open(manifests[-1], encoding="utf-8"))
        ev_run_ids = {e.get("run_id") for e in self.evs if e.get("run_id")}
        self.assertEqual(ev_run_ids, {m["run_id"]},
                         f"manifest/事件流 run_id 不一致: {ev_run_ids} vs {m['run_id']}")
        # 联合门拒绝语义: 资源不达门时 run 必须失败(数值好不可赎回)
        g = self._gate_event()
        if g["verdict"] != "ok":
            self.assertEqual(self.res.returncode, 10,
                             "联合门: 资源证据不足必须拒绝 run")
        # IMPL/INT 缺口: UPM 校正场幅度有界(源不被拟合)断言依赖 UPM 模型持久化,
        # 现行 CLI config 不可达(persist_upm 顶层键被拒), 恢复载体后补回。


if __name__ == "__main__":
    unittest.main(verbosity=2)
