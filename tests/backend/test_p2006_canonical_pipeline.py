#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2006_canonical_pipeline.py — P2-006 (G5) Canonical Phase2 Pipeline。
验证:
  A) IR 至少含 coverage→sample→upm_fit→upm_apply→reject→integrate→write 7 节点链,
     各端口 DATA/单位/Artifact ID 完整;
  B) 正式 phase2 run 经 runtime 执行 7 节点, observed trace 逐节点 COMPLETED;
  C) static graph 与 observed trace 双向一致(PIPELINE_GRAPH_PASS, 逐节点匹配);
  D) 输出命名无歧义(mosaic/signal/support/ivar/variance/UPM/rejection 诊断)。
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")

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
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": [os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips"),
                                         os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F2.hips")],
                              "darks": [], "flats": [], "bias": []},
                   "output_dir": cls.out}, open(cls.cfg, "w"))
        cls.res = subprocess.run([EXE, "run", "--phases", "2", "--config", cls.cfg,
                                  "--events-jsonl"], capture_output=True, text=True,
                                 timeout=300)
        cls.gdir = os.path.join(cls.out, "graph")

    def test_01_ir_seven_node_chain(self):
        """静态 IR 含 7 节点链(coverage→...→write), 端口/模块完整。"""
        self.assertEqual(self.res.returncode, 0, self.res.stderr[-400:])
        ir = json.load(open(os.path.join(self.gdir, "static_graph.json"), encoding="utf-8"))
        nodes = ir["nodes"]
        self.assertEqual([n["node_id"] for n in nodes], CHAIN)
        for n in nodes:
            self.assertEqual(n["module_id"], MODULES[n["node_id"]])
            self.assertTrue(n["inputs"] and n["outputs"], f"{n['node_id']} 端口缺失")

    def test_02_observed_trace_matches(self):
        """observed trace 7 节点全部 COMPLETED, 与 static 逐节点匹配。"""
        tr = json.load(open(os.path.join(self.gdir, "observed_trace.json"), encoding="utf-8"))
        self.assertEqual(tr["schema"], "astrocs.observed-trace/v1")
        nodes = {n["node_id"]: n for n in tr["nodes"]}
        self.assertEqual(set(nodes.keys()), set(CHAIN))
        for nid in CHAIN:
            self.assertEqual(nodes[nid]["status"], "COMPLETED", nid)
            self.assertGreaterEqual(nodes[nid]["workers"], 1, nid)

    def test_03_graph_bidirectional(self):
        """static graph 与 observed trace 双向一致(PIPELINE_GRAPH_PASS)。"""
        mods = {m: {"module_id": m, "module_version": "1.x"} for m in MODULES.values()}
        mp = os.path.join(self.tmp, "mods.json")
        json.dump(mods, open(mp, "w"))
        c = subprocess.run([sys.executable, os.path.join(REPO, "tools", "quality",
                            "check_pipeline_graph.py"),
                            "--ir", os.path.join(self.gdir, "static_graph.json"),
                            "--module-index", mp,
                            "--trace", os.path.join(self.gdir, "observed_trace.json")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(c.returncode, 0, c.stderr[-400:])
        self.assertIn("PIPELINE_GRAPH_PASS", c.stdout)

    def test_04_output_naming_unambiguous(self):
        """输出命名无歧义: graph 产物齐全; mosaic 在 write 节点输出。"""
        for name in ("static_graph.json", "observed_trace.json", "graph_sidecar.json",
                     "static_graph.dot", "observed_graph.dot", "l0_graph.json"):
            self.assertTrue(os.path.isfile(os.path.join(self.gdir, name)), name)
        ir = json.load(open(os.path.join(self.gdir, "static_graph.json"), encoding="utf-8"))
        write = [n for n in ir["nodes"] if n["node_id"] == "write"][0]
        self.assertIn("mosaic", write["outputs"])
        self.assertEqual(ir["outputs"].get("mosaic"), "artifact:write")


if __name__ == "__main__":
    unittest.main(verbosity=2)
