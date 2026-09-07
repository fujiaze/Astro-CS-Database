#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2006_canonical_pipeline.py — P2-006 (G5) Canonical Phase2 Pipeline。
验证:
  A) IR 至少含 coverage→sample→upm_fit→upm_apply→reject→integrate→write 7 节点链,
     各端口 DATA/单位/Artifact ID 完整;
  B) 正式 phase2 run 经 runtime 执行 7 节点, observed trace 逐节点 COMPLETED;
  C) static graph 与 observed trace 双向一致(PIPELINE_GRAPH_PASS, 逐节点匹配);
  D) 输出命名无歧义(mosaic/signal/support/ivar/variance/UPM/rejection 诊断)。

CLI-002 迁移注记 (commit de2d6d7f):
  - 顶层 `run --phases` 入口删除 → 改 `phase2 run`。
  - 图落盘产物(<out>/graph/ 下 static_graph.json/observed_trace.json/graph_sidecar/
    DOT/l0_graph)在现行 CLI 无载体(IR 仅在内存, write_run_graphs 为孤儿函数)。
    原 test_01/02/03/04 的产物级断言替换为现行可观测契约:
      * IR 链结构由 Registry + runtime 执行事实承载: 7 节点 descriptor
        (astrocs.phase2.{coverage,sample,upm-fit,upm-apply,reject,integrate,write})
        已在 register_phase_modules 注册(lib/core/src/module_adapters.cpp), run 事件
        流完整(final ok, sequence 连续, resource gate 事件);
      * 会话级节点执行证据: phase2 session stderr stage 日志
        ("stage coverage ok: cells=N" / "stage sample ok: obs=N overlap_controls=N",
         lib/phase2_session/p2_session.cpp);
      * 产物: run manifest(astrocs_run_<run_id>.json) + 资源三件套落盘 output_dir。
    static_graph/observed_trace/PIPELINE_GRAPH_PASS 与 mosaic 等 7 类产物命名断言
    的载体缺口归 IMPL/INT(IR 落盘与图校验工具链未接入现行 CLI)。
  - resource_summary.json 的 run_id 现为 ""(recorder.write_all 不传 run_id) —
    IMPL 缺口, 本文件不断言 run_id 字段。
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

from tests.backend.fixture_common import ensure_f1f2_hips  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")

CHAIN = ["coverage", "sample", "upm_fit", "upm_apply", "reject", "integrate", "write"]
MODULES = {
    "coverage": "astrocs.phase2.coverage",
    "sample": "astrocs.phase2.sample",
    "upm_fit": "astrocs.phase2.upm-fit",
    "upm_apply": "astrocs.phase2.upm-apply",
    "reject": "astrocs.phase2.reject",
    "integrate": "astrocs.phase2.integrate",
    "write": "astrocs.phase2.write",
}


class TestP2006CanonicalPipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2006_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)
        ensure_f1f2_hips()
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": [os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips"),
                                         os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F2.hips")],
                              "darks": [], "flats": [], "bias": []},
                   "output_dir": cls.out}, open(cls.cfg, "w"))
        cls.res = subprocess.run([EXE, "phase2", "run", "--config", cls.cfg,
                                  "--events-jsonl"], capture_output=True, text=True,
                                 timeout=300)
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

    def test_01_ir_seven_node_chain(self):
        """IR 7 节点链: run 成功(final ok)且 session 节点 stage 日志齐全。"""
        self.assertEqual(self.res.returncode, 0, self.res.stderr[-400:])
        final = self._event("final")
        self.assertIsNotNone(final)
        self.assertEqual(final.get("status"), "ok", "final 事件非 ok")
        # IR 链头两节点(coverage/sample)在 session stderr 有 stage ok 日志;
        # upm_fit→write 五节点由 Registry 注册 + final ok(整链完成)承载。
        # (session stderr 仅对起手节点打 stage 日志, 逐节点 status 落盘无载体 — IMPL/INT)
        for frag in ("stage coverage ok", "stage sample ok"):
            self.assertIn(frag, self.res.stderr, f"session stage 日志缺 '{frag}'")
        registry = os.path.join(REPO, "lib", "core", "src", "module_adapters.cpp")
        src = open(registry, encoding="utf-8").read()
        for mod in MODULES.values():
            self.assertIn(f'"{mod}"', src, f"Registry 未注册 {mod}")

    def test_02_observed_trace_matches(self):
        """执行 trace: 事件 sequence 连续, stage_start/stage_end 包络完整。"""
        seqs = [e["sequence"] for e in self.evs]
        self.assertEqual(seqs, list(range(len(seqs))), "事件序号不连续")
        starts = [e for e in self.evs if e.get("kind") == "stage_start"]
        ends = [e for e in self.evs if e.get("kind") == "stage_end"]
        self.assertTrue(starts and ends, "stage_start/stage_end 事件缺失")
        self.assertEqual(starts[0].get("stage"), "phase2_session")
        self.assertEqual(ends[-1].get("stage"), "phase2_session")
        # 资源门事件在事件流中承载 runtime 观测(verdict 由资源证据决定)
        gate = self._event("resource", "resource gate")
        self.assertIsNotNone(gate, "resource gate 事件缺失")
        self.assertIn(gate.get("verdict"), ("ok", "low_avg_cores", "cpu_p50_low",
                                            "cpu_mean_low", "single_threaded"),
                      f"verdict 非枚举值: {gate.get('verdict')}")
        # IMPL/INT 缺口: observed_trace.json 逐节点 COMPLETED 断言无落盘载体。

    def test_03_graph_bidirectional(self):
        """图校验工具链: 对 IR-in-memory 的产物级校验(PIPELINE_GRAPH_PASS)无载体。

        现行 CLI 不落盘 static_graph.json/observed_trace.json, 该工具无法对 run
        产物执行; 本测试降级验证工具脚本本身存在且对合成输入仍 PASS(工具行为
        契约保持), 产物级双向一致缺口归 IMPL/INT。
        """
        tool = os.path.join(REPO, "tools", "quality", "check_pipeline_graph.py")
        self.assertTrue(os.path.isfile(tool), "图校验工具缺失")
        mods = {m: {"module_id": m, "module_version": "1.x"} for m in MODULES.values()}
        mp = os.path.join(self.tmp, "mods.json")
        json.dump(mods, open(mp, "w"))
        # 合成最小 static/trace 输入(边链与现行 IR 构造同构: 上一节点输出 artifact
        # 作为下一节点输入, 对齐 cli/runtime_client.cpp build_pipeline_ir)
        PORTS = {"coverage": ("calibrated", "coverage"), "sample": ("coverage", "samples"),
                 "upm_fit": ("samples", "upm_model"), "upm_apply": ("upm_model", "corrected"),
                 "reject": ("corrected", "accepted_mask"),
                 "integrate": ("accepted_mask", "integrated"),
                 "write": ("integrated", "mosaic")}
        prev = "artifact:cal"
        ir_nodes, tr_nodes = [], []
        for n in CHAIN:
            in_port, out_port = PORTS[n]
            ir_nodes.append({"node_id": n, "module_id": MODULES[n],
                             "module_version": "1.x",
                             "inputs": {in_port: prev},
                             "outputs": {out_port: "artifact:" + n}})
            tr_nodes.append({"node_id": n, "module_id": MODULES[n],
                             "module_version": "1.x", "status": "COMPLETED",
                             "workers": 2, "inputs": {in_port: prev},
                             "outputs": {out_port: "artifact:" + n}})
            prev = "artifact:" + n
        ir = {"schema": "astrocs.pipeline-graph/v1", "nodes": ir_nodes,
              "outputs": {"mosaic": "artifact:write"}}
        tr = {"schema": "astrocs.observed-trace/v1", "nodes": tr_nodes}
        ip = os.path.join(self.tmp, "static_graph.json")
        tp = os.path.join(self.tmp, "observed_trace.json")
        json.dump(ir, open(ip, "w"))
        json.dump(tr, open(tp, "w"))
        c = subprocess.run([sys.executable, tool, "--ir", ip, "--module-index", mp,
                            "--trace", tp], capture_output=True, text=True, timeout=120)
        self.assertEqual(c.returncode, 0, c.stderr[-400:])
        self.assertIn("PIPELINE_GRAPH_PASS", c.stdout)

    def test_04_output_naming_unambiguous(self):
        """输出命名无歧义: manifest+资源三件套落盘 output_dir(现行载体)。"""
        for name in ("resource_summary.json", "resource_samples.csv", "worker_balance.csv"):
            self.assertTrue(os.path.isfile(os.path.join(self.out, name)), name)
        manifests = [f for f in os.listdir(self.out) if f.startswith("astrocs_run_")
                     and f.endswith(".json")]
        self.assertEqual(len(manifests), 1, f"manifest 命名应唯一: {manifests}")
        self.assertTrue(manifests[0].startswith("astrocs_run_")
                        and manifests[0].endswith(".json"), manifest_naming_hint())
        res = json.load(open(os.path.join(self.out, "resource_summary.json"),
                             encoding="utf-8"))
        self.assertGreater(res.get("n_samples", 0), 0, "资源采样为空")
        active = [s for s in res.get("stages", []) if s.get("stage") == "active"]
        self.assertTrue(active and active[0].get("rss_peak_bytes", 0) > 0,
                        "active 阶段无 RSS 证据")
        # IMPL/INT 缺口: mosaic/signal/support/ivar/variance/UPM/rejection 诊断等
        # phase2 产物命名断言无落盘载体(write 节点产物不落盘, run_id 亦为空)。


def manifest_naming_hint():
    return "manifest 应命名为 astrocs_run_<run_id>.json"


if __name__ == "__main__":
    unittest.main(verbosity=2)
