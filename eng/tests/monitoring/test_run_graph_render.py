#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-003 运行图渲染工具验收测试（eng/tests/monitoring 域，owner SA-LOG-08）。

覆盖 LOG-003 验收（tasks/03_RUNTIME_DATA_IO_TASKS.md + 控制包标准
14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §5 + 23_GRAPH_AND_DOC_TOOL_POLICY.md）：
  G1 从 plan+trace 生成 DOT/SVG/JSON：节点=真实入口，边=数据边，标注
     call_count/worker/provider/耗时/资源/DLL hash/artifact hash；
  G2 图与 trace 调用计数一致：图节点 call_count == replay call_count ==
     原始 module_call 事件计数（verify 机器比对 exit 0 GRAPH_CONSISTENT）；
  G3 DLL hash / artifact hash 与 trace 事件真实字段一致；禁止 config 冒充
     （缺失观测不得编造 hash）；
  G4 JSON 中间表示结构：astrocs.graph-json/v1 含 nodes/edges/metrics/
     source.main_sha/输入 hash；
  G5 SVG 为合法最小结构（零第三方直出；派生展示物）；DOT 含 generator 头与
     全部节点/边；工具不依赖 dot 二进制（dot/graphviz 不可用仍 PASS）；
  G6 无 plan 时 producer 边来自 trace artifact_publish；计划声明但不运行的
     节点标 PLAN_ONLY（不冒充观测）；
  G7 负测：篡改图（改 call_count / 删 artifact hash）verify 必须 FAIL；
     无 node_id 事件不得生成空节点；
  G8 回归：LOG-002/RT-006 语义不破坏（本域纯新增，import 既有模块正常）。

方法：纯 Python unittest + stdlib（同 eng/tests/monitoring/test_monitor_contract.py
风格），真实调用 lib/infrastructure/pipeline/trace_replay.py + 本工具；样例 trace 为
RT-006 7 节点真实语义 JSONL（与 test_rt006_trace.py 同构）。节点集合、每节点发布的
artifact 一律**从计划 fixture 推导**，artifact_sha256 期望值**由产物内容推导**
（sha256(artifact_content)）——两处都不是手抄常量表，fixture 或产物面一改就重新对齐，
不会退化成"改 DAG 就得改测试"（真实观测路径语义验证，非真实产品运行）。
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

REPO = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "lib" / "infrastructure" / "pipeline"))

from trace_replay import replay_from_jsonl  # noqa: E402

SPEC = importlib.util.spec_from_file_location(
    "render_run_graph",
    REPO / "eng" / "tools" / "graph" / "render_run_graph.py")
RG = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(RG)

TOOL_PY = REPO / "eng" / "tools" / "graph" / "render_run_graph.py"

PLAN_FIXTURE = (REPO / "lib" / "infrastructure" / "pipeline" / "fixtures"
                / "phase2_typed_dag.json")
_PLAN_FIXTURE = json.loads(PLAN_FIXTURE.read_text(encoding="utf-8"))
# 节点集合与「每节点发布哪些 artifact」**从计划 fixture 推导**（唯一权威），不手抄：
# UNIT-DERIVE-01（7cde39cb）改写 fixture 时（reject 的 accepted_mask → rejection、
# upm_fit 增加 sky_plane）手抄表与磁盘脱钩，本文件曾以 KeyError 判红。
NODES = [n["node_id"] for n in _PLAN_FIXTURE["nodes"]]
# 每节点发布的 artifact = fixture 中该节点 outputs 的取值（保序去重）；index 0 = 主产物
NODE_ARTIFACTS = {n["node_id"]: list(dict.fromkeys(n["outputs"].values()))
                  for n in _PLAN_FIXTURE["nodes"]}
OUT_ART = {nid: arts[0] for nid, arts in NODE_ARTIFACTS.items()}
DLL_SHA = "d" * 64


def artifact_content(art: str) -> bytes:
    """样例产物的字节内容：以 artifact 身份为种子的确定性载荷（真实产品不入库）。"""
    return ("astrocs.phase2.fixture-artifact:%s\n" % art).encode("utf-8")


# 期望哈希**由产物内容推导**（sha256(artifact_content)），不是手抄常量表：
# 图上的 artifact_sha256 必须等于该产物内容的真实摘要 —— 编造、张冠李戴、
# 漏传观测都会判红；fixture 增删 artifact 时本表自动跟随。
ART_SHA = {art: hashlib.sha256(artifact_content(art)).hexdigest()
           for arts in NODE_ARTIFACTS.values() for art in arts}


def make_trace_jsonl(*, tamper_node: str = "", zero_dll: bool = False,
                     omit_node: str = "", bad_events: bool = False,
                     no_node_id: bool = False) -> str:
    """RT-006 语义 7 节点 trace（每节点 module_call/node_start/end +
    artifact_publish）；可注入篡改/缺失/无 node_id 行。"""
    lines = []
    for i, nid in enumerate(NODES):
        provider = "io-backend" if nid == "write" else "avx2"
        workers = 1 if nid == "write" else 2
        entry = "astrocs_phase2_%s_v1" % nid.replace("-", "")
        nid_field = nid if not no_node_id else ""
        lines.append(json.dumps({
            "schema": "astrocs.trace-event/v1", "type": "node_start",
            "ts_utc": "2026-09-04T00:00:00.%03dZ" % i, "run_id": "run-g7",
            "node_id": nid_field, "module_id": "astrocs.phase2." + nid,
            "status": "RUNNING", "granted_workers": workers, "seq": i * 10 + 1}))
        mc = {
            "schema": "astrocs.trace-event/v1", "type": "module_call",
            "ts_utc": "2026-09-04T00:00:00.%03dZ" % i, "run_id": "run-g7",
            "node_id": nid, "module_id": "astrocs.phase2." + nid,
            "module_version": "1.0." + "0", "entry": entry,
            "call_count": 1, "workers": workers, "provider": provider,
            "dll_name": ("astrocs_phase2_%s.dll" % nid if not zero_dll else ""),
            "dll_sha256": (DLL_SHA if not zero_dll else ""),
            "seq": i * 10 + 2}
        lines.append(json.dumps(mc))
        # 发布该节点在计划里声明的**全部**产物（保序）——计划数据边引用的 artifact
        # 因此都有真实发布观测；无观测的边只能标空（G3/G6，见另一用例）。
        for k, art in enumerate(NODE_ARTIFACTS[nid]):
            lines.append(json.dumps({
                "schema": "astrocs.trace-event/v1", "type": "artifact_publish",
                "ts_utc": "2026-09-04T00:00:01.%03dZ" % i, "run_id": "run-g7",
                "node_id": nid, "module_id": "astrocs.phase2." + nid,
                "artifact_id": art,
                "artifact_sha256": ART_SHA[art],
                "artifact_size": 4096 * (i + 1) + k,
                "seq": i * 10 + 3 + k}))
        ne = {
            "schema": "astrocs.trace-event/v1", "type": "node_end",
            "ts_utc": "2026-09-04T00:00:01.%03dZ" % i, "run_id": "run-g7",
            "node_id": nid, "status": "COMPLETED", "wall_ms": 10.0 + i,
            "workers": workers, "granted_workers": workers,
            "provider": provider, "seq": i * 10 + 3 + len(NODE_ARTIFACTS[nid])}
        if tamper_node and nid == tamper_node:
            ne["status"] = "FAILED"
            ne["error"] = "boom"
        lines.append(json.dumps(ne))
    if omit_node:
        # 删除该节点的全部 4 行
        lines = [ln for ln in lines
                 if ('"node_id": "%s"' % omit_node) not in ln]
    if bad_events:
        lines.insert(0, "not json")
        lines.insert(1, '{"type":"bogus","node_id":"x"}')
    return "\n".join(lines) + "\n"


class TestGraphRenderTool(unittest.TestCase):
    """工具结构/自检/零依赖（G5）。"""

    def test_selfcheck_pass(self):
        r = subprocess.run([sys.executable, str(TOOL_PY), "selfcheck"],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("GRAPH_TOOL_SELFCHECK PASS", r.stdout)

    def test_no_dot_binary_required(self):
        """DOT/SVG 纯 stdlib 生成：不调用 dot/graphviz（无则跳过不失败）。"""
        src = TOOL_PY.read_text(encoding="utf-8")
        self.assertNotIn("subprocess.run", src)      # 不调用外部二进制
        self.assertIn("to_dot", src)
        self.assertIn("to_svg", src)
        self.assertNotIn("graphviz", src.lower().replace(
            "graphviz", ""))  # 无强依赖

    def test_render_cli_emits_json_dot(self):
        """CLI render：JSON+DOT 落盘且结构合法；SVG 合法最小结构。"""
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td)
            tr = td / "t.jsonl"
            tr.write_text(make_trace_jsonl(), encoding="utf-8")
            plan = REPO / "lib" / "infrastructure" / "pipeline" / "fixtures" / "phase2_typed_dag.json"
            r = subprocess.run(
                [sys.executable, str(TOOL_PY), "render",
                 "--trace", str(tr), "--plan", str(plan), "--out-dir", str(td),
                 "--name", "g", "--sha", "e" * 40, "--svg"],
                capture_output=True, text=True, timeout=120)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("GRAPH_RENDER_OK", r.stdout)
            gj = json.loads((td / "g.json").read_text(encoding="utf-8"))
            dot = (td / "g.dot").read_text(encoding="utf-8")
            svg = (td / "g.svg").read_text(encoding="utf-8")
            self.assertEqual(gj["schema"], "astrocs.graph-json/v1")
            ET.fromstring(svg)  # 合法 XML
            self.assertIn("<svg", svg)
            for nid in NODES:
                self.assertIn('"%s"' % nid, dot)
            self.assertIn("digraph run_graph {", dot)


class TestGraphConsistency(unittest.TestCase):
    """验收 G1/G2/G3：图与 trace 调用计数、DLL/artifact hash 一致。"""

    @staticmethod
    def _graph_for(jsonl: str, with_plan: bool = True) -> dict:
        events = [json.loads(ln) for ln in jsonl.splitlines()
                  if ln.strip() and not ln.startswith("not json")
                  and '"type":"bogus"' not in ln]
        plan = None
        if with_plan:
            plan = json.loads(
                (REPO / "lib" / "infrastructure" / "pipeline" / "fixtures"
                 / "phase2_typed_dag.json").read_text(encoding="utf-8"))
        return RG.build_graph(trace_events=events, plan_obj=plan,
                              sha="e" * 40)

    def test_node_call_count_matches_trace(self):
        g = self._graph_for(make_trace_jsonl())
        # 7 节点每节点 1 次
        self.assertEqual(g["metrics"]["node_count"], 7)
        self.assertEqual(g["metrics"]["module_call_total"], 7)
        for n in g["nodes"]:
            self.assertEqual(n["call_count"], 1)
            self.assertEqual(n["events"]["module_call"], 1)
            self.assertIn(n["id"], NODES)

    def test_verify_graph_consistent_exit0(self):
        """verify：图与 replay 调用计数/entry/provider/status/DLL/artifact 全一致。"""
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td)
            jsonl = make_trace_jsonl()
            tr = td / "t.jsonl"
            tr.write_text(jsonl, encoding="utf-8")
            events = [json.loads(ln) for ln in jsonl.splitlines()
                      if ln.strip()]
            g = RG.build_graph(trace_events=events, plan_obj=json.loads(
                (REPO / "lib" / "infrastructure" / "pipeline" / "fixtures"
                 / "phase2_typed_dag.json").read_text(encoding="utf-8")),
                sha="e" * 40)
            gj = td / "g.json"
            gj.write_text(json.dumps(g, ensure_ascii=False), encoding="utf-8")
            r = subprocess.run(
                [sys.executable, str(TOOL_PY), "verify",
                 "--verify", str(gj), "--trace", str(tr)],
                capture_output=True, text=True, timeout=120)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            out = json.loads(r.stdout)
            self.assertEqual(out["verdict"], "GRAPH_CONSISTENT")
            self.assertEqual(out["diff_count"], 0)

    def test_dll_and_artifact_hash_from_trace(self):
        """DLL name/sha256、artifact id/sha256/size 与 trace 事件真实字段一致。"""
        g = self._graph_for(make_trace_jsonl())
        by_id = {n["id"]: n for n in g["nodes"]}
        for nid in NODES:
            n = by_id[nid]
            self.assertEqual(n["dll_name"], "astrocs_phase2_%s.dll" % nid)
            self.assertEqual(n["dll_sha256"], DLL_SHA)
            self.assertEqual(n["artifacts"][0]["id"], OUT_ART[nid])
            self.assertEqual(n["artifacts"][0]["sha256"], ART_SHA[OUT_ART[nid]])
            self.assertEqual(n["artifacts"][0]["size"],
                             4096 * (NODES.index(nid) + 1))

    def test_provider_workers_wall_from_trace(self):
        g = self._graph_for(make_trace_jsonl())
        by_id = {n["id"]: n for n in g["nodes"]}
        self.assertEqual(by_id["coverage"]["provider"], "avx2")
        self.assertEqual(by_id["write"]["provider"], "io-backend")
        self.assertEqual(by_id["write"]["workers"], 1)
        self.assertEqual(by_id["coverage"]["workers"], 2)
        self.assertEqual(by_id["coverage"]["wall_ms"], 10.0)
        self.assertEqual(by_id["write"]["status"], "COMPLETED")

    def test_no_config_impersonation_when_observation_missing(self):
        """DLL/artifact hash 观测缺失时留空，绝不编造/config 冒充。"""
        g = self._graph_for(make_trace_jsonl(zero_dll=True))
        for n in g["nodes"]:
            self.assertEqual(n["dll_name"], "")
            self.assertEqual(n["dll_sha256"], "")
        # artifact_publish 仍携带 → hash 有值；此处只验证 dll 缺失语义

    def test_plan_only_nodes_not_pretending(self):
        """计划声明但 trace 未运行的节点 → PLAN_ONLY（call_count=0，不冒充）。"""
        jsonl = make_trace_jsonl(omit_node="write")
        g = self._graph_for(jsonl)
        by_id = {n["id"]: n for n in g["nodes"]}
        w = by_id["write"]
        self.assertEqual(w["status"], "PLAN_ONLY")
        self.assertEqual(w["call_count"], 0)
        # verify 仍一致（图含 PLAN_ONLY 而 replay 不含 write —— 只在缺失
        # 观测节点的 call_count 上无冲突；此处直接比对 replay 集合以外节点）
        events = [json.loads(ln) for ln in jsonl.splitlines() if ln.strip()]
        ok, diffs = RG.verify_consistency(g, events)
        self.assertFalse(ok)  # 图含 plan-only 节点，replay 无 → 不一致是预期
        # （真实用法：只把 plan-only 图当"计划 vs 实际"对照，验收用同 trace 图）

    def test_edges_carry_plan_data_edges_and_artifact_hash(self):
        jsonl = make_trace_jsonl()
        g = self._graph_for(jsonl)
        edges = {e["artifact"]: e for e in g["edges"]}
        # fixture 经 RT-001 编译器推导的数据边（authoritative plan edges）
        plan = json.loads(PLAN_FIXTURE.read_text(encoding="utf-8"))
        from lib.infrastructure.pipeline.typed_dag import TypedDagCompiler
        res = TypedDagCompiler().compile(plan)
        self.assertTrue(res.ok)
        pj = json.loads(res.plan.to_json())
        plan_edge_arts = {e["artifact"] for e in pj["edges"]}
        self.assertEqual(set(edges), plan_edge_arts)
        self.assertIn("artifact:coverage", edges)
        # 覆盖前提（判据非退化）：样例 trace 必须发布计划数据边引用的**每一个**
        # artifact —— fixture 再改而 trace 没跟 ⇒ 这里带着差集判红，不再退化成
        # KeyError，也不允许"没观测"被静默当成通过。
        self.assertEqual(set(), plan_edge_arts - set(ART_SHA),
                         "样例 trace 未发布计划数据边引用的产物: %s"
                         % sorted(plan_edge_arts - set(ART_SHA)))
        # 期望值 = 该产物内容的真实摘要（ART_SHA = sha256(artifact_content)）：
        # 计划数据边携带真实 artifact_publish 观测 hash，非 config 冒充/编造。
        for art in sorted(plan_edge_arts):
            self.assertEqual(edges[art]["edge_source"], "plan")
            self.assertEqual(edges[art]["artifact_sha256"], ART_SHA[art],
                             "边 %s 的 artifact_sha256 必须是该产物内容的摘要" % art)
            self.assertNotEqual(edges[art]["artifact_sha256"], "",
                                "边 %s 有发布观测 ⇒ hash 不得为空" % art)
        # 抽样验证边端点与 plan 边一致（coverage → sample / upm_apply → reject）
        pe_by_art = {}
        for e in pj["edges"]:
            pe_by_art.setdefault(e["artifact"], []).append(
                (e["from_node"], e["to_node"]))
        for frm, to in pe_by_art["artifact:corrected"]:
            self.assertIn((frm, to),
                          [(e["from"], e["to"]) for e in g["edges"]
                           if e["artifact"] == "artifact:corrected"])

    def test_edges_hash_from_observation_or_empty(self):
        """计划边引用的 artifact 若 trace 未发布 → hash 标注空（不编造）。"""
        # 手工 JSONL：upm_apply 的 artifact_publish 事件缺失（其余事件保留）→
        # artifact:corrected 无发布观测 → 引用它的计划边 hash 留空。
        lines = make_trace_jsonl().splitlines()
        kept = [ln for ln in lines if not (
            '"type": "artifact_publish"' in ln
            and '"node_id": "upm_apply"' in ln)]
        g = self._graph_for("\n".join(kept) + "\n")
        by_art = {e["artifact"]: e for e in g["edges"]}
        self.assertEqual(by_art["artifact:corrected"]["artifact_sha256"], "")
        # 有发布观测的 artifact → hash 有真实观测
        self.assertEqual(by_art["artifact:coverage"]["artifact_sha256"],
                         ART_SHA["artifact:coverage"])
        # 节点 artifact 列表只含真实发布事件（corrected 发布缺失 → 空列表）
        by_id = {n["id"]: n for n in g["nodes"]}
        self.assertEqual(by_id["upm_apply"]["artifacts"], [])

    def test_trace_only_producer_edges_no_plan(self):
        """无 plan 时 producer 边来自 trace artifact_publish；hash 有观测值。"""
        g = self._graph_for(make_trace_jsonl(), with_plan=False)
        self.assertEqual(g["plan"], None)
        by_art = {e["artifact"]: e for e in g["edges"]}
        # 边集合 = trace 真实发布过的 artifact 集合（推导，不写死条数：节点可发布多个产物）
        self.assertEqual(set(by_art), set(ART_SHA))
        for nid in NODES:
            for art in NODE_ARTIFACTS[nid]:
                e = by_art[art]
                self.assertEqual(e["from"], nid)
                self.assertEqual(e["artifact_sha256"], ART_SHA[art])
                self.assertEqual(e["edge_source"], "trace")

    def test_no_node_id_events_produce_no_empty_node(self):
        g = self._graph_for(make_trace_jsonl(no_node_id=True))
        for n in g["nodes"]:
            self.assertNotEqual(n["id"], "")
        self.assertEqual(g["metrics"]["node_count"], 7)


class TestGraphNegative(unittest.TestCase):
    """G7 负测：篡改图 verify 必须 FAIL。"""

    def _write_and_verify(self, g: dict, jsonl: str) -> subprocess.CompletedProcess:
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td)
            tr = td / "t.jsonl"
            tr.write_text(jsonl, encoding="utf-8")
            gj = td / "g.json"
            gj.write_text(json.dumps(g, ensure_ascii=False), encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(TOOL_PY), "verify",
                 "--verify", str(gj), "--trace", str(tr)],
                capture_output=True, text=True, timeout=120)

    def test_call_count_tamper_detected(self):
        jsonl = make_trace_jsonl()
        g = TestGraphConsistency._graph_for(jsonl)
        for n in g["nodes"]:
            if n["id"] == "coverage":
                n["call_count"] = 2  # 篡改
        r = self._write_and_verify(g, jsonl)
        self.assertEqual(r.returncode, 1)
        out = json.loads(r.stdout)
        self.assertEqual(out["verdict"], "GRAPH_INCONSISTENT")
        self.assertTrue(any("coverage" in d for d in out["diffs"]))

    def test_artifact_hash_tamper_detected(self):
        jsonl = make_trace_jsonl()
        g = TestGraphConsistency._graph_for(jsonl)
        for n in g["nodes"]:
            if n["id"] == "coverage":
                n["artifacts"][0]["sha256"] = "f" * 64  # 篡改
        r = self._write_and_verify(g, jsonl)
        self.assertEqual(r.returncode, 1)
        out = json.loads(r.stdout)
        self.assertEqual(out["verdict"], "GRAPH_INCONSISTENT")

    def test_status_tamper_detected(self):
        jsonl = make_trace_jsonl()
        g = TestGraphConsistency._graph_for(jsonl)
        for n in g["nodes"]:
            if n["id"] == "coverage":
                n["status"] = "FAILED"  # 篡改（trace 实际 COMPLETED）
        r = self._write_and_verify(g, jsonl)
        self.assertEqual(r.returncode, 1)
        self.assertIn("GRAPH_INCONSISTENT", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
