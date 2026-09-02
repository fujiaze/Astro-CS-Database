#!/usr/bin/env python3
"""RT-001 typed DAG 计划图生成正测（合同骨架，无执行）。

覆盖:
  1. 每节点唯一 operation 绑定; 计划图每 node 含 entry/module_id/operation/phase。
  2. 计划图 edges 已做类型校验(data_schema_id/unit/coordinate/scalar/shape 一致)。
  3. acyclic=true; 无重复 producer; 顶层 outputs 全部被产出。
  4. 同一 module_id 不可出现两次(node 绑定表唯一)。
  5. module_ports.registry.json 中聚合 Session 模块(astrocs.phase2.resample /
     astrocs.phase3.resample)不可入图(IRF-0007)。
  6. CLI 模式: python3 runtime/pipeline/typed_dag.py fixtures/phase2_typed_dag.json
     输出 plan-graph JSON(exit 0)。
  7. 隐式文件路径等负测由 tests/runtime/test_typed_dag_negative.py 覆盖。
"""
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "pipeline"))

from typed_dag import (  # noqa: E402
    PLAN_SCHEMA_CONST,
    Registry,
    TypedDagCompiler,
)

FIXTURE = REPO / "runtime" / "pipeline" / "fixtures" / "phase2_typed_dag.json"
COMPILER = REPO / "runtime" / "pipeline" / "typed_dag.py"

OPS = {
    "coverage": "compute_coverage", "sample": "sample_frames",
    "upm_fit": "fit_upm", "upm_apply": "apply_upm",
    "reject": "reject_outliers", "integrate": "integrate_frames",
    "write": "write_mosaic",
}
CHAIN = [
    ("coverage", "astrocs.phase2.coverage", "calibrated", "coverage"),
    ("sample", "astrocs.phase2.sample", "coverage", "samples"),
    ("upm_fit", "astrocs.phase2.upm-fit", "samples", "upm_model"),
    ("upm_apply", "astrocs.phase2.upm-apply", "upm_model", "corrected"),
    ("reject", "astrocs.phase2.reject", "corrected", "accepted_mask"),
    ("integrate", "astrocs.phase2.integrate", "accepted_mask", "integrated"),
    ("write", "astrocs.phase2.write", "integrated", "mosaic"),
]
EDGES = {
    "coverage": {"calibrated": "artifact:cal"},
    "sample": {"coverage": "artifact:coverage"},
    "upm_fit": {"samples": "artifact:samples"},
    "upm_apply": {"upm_model": "artifact:upm_model", "calibrated_frames": "artifact:cal"},
    "reject": {"corrected": "artifact:corrected"},
    "integrate": {"accepted_mask": "artifact:accepted_mask", "corrected": "artifact:corrected"},
    "write": {"integrated": "artifact:integrated"},
}
OUT = {
    "coverage": "artifact:coverage", "sample": "artifact:samples",
    "upm_fit": "artifact:upm_model", "upm_apply": "artifact:corrected",
    "reject": "artifact:accepted_mask", "integrate": "artifact:integrated",
    "write": "artifact:mosaic",
}


OUT_PORT = {
    "coverage": "coverage", "sample": "samples", "upm_fit": "upm_model",
    "upm_apply": "corrected", "reject": "accepted_mask",
    "integrate": "integrated", "write": "mosaic",
}


def ir_doc() -> dict:
    nodes = []
    for (nid, mid, _ip, _op) in CHAIN:
        n = {
            "node_id": nid, "module_id": mid, "operation": OPS[nid],
            "config": {"phase": 2},
            "inputs": dict(EDGES[nid]),
            "outputs": {OUT_PORT[nid]: OUT[nid]},
            "resources": {"class": "cpu_heavy", "parallel": True},
        }
        if nid == "write":
            n["resources"] = {"class": "io", "parallel": False}
        nodes.append(n)
    return {
        "schema": "astrocs.typed-dag/v1",
        "pipeline_id": "phase2.typed",
        "phase": "phase2",
        "version": "1.0.0",
        "nodes": nodes,
        "outputs": {"mosaic": "artifact:mosaic"},
    }


class TestPlanGraph(unittest.TestCase):
    def setUp(self):
        self.compiler = TypedDagCompiler()

    def test_fixture_compiles(self):
        doc = json.loads(FIXTURE.read_text(encoding="utf-8"))
        res = self.compiler.compile(doc)
        self.assertTrue(res.ok, "\n".join(res.error_lines()))
        plan = json.loads(res.plan.to_json())
        self.assertEqual(plan["schema"], PLAN_SCHEMA_CONST)
        self.assertEqual(plan["phase"], "phase2")
        self.assertEqual(len(plan["nodes"]), 7)
        self.assertTrue(plan["acyclic"])

    def test_every_node_single_unique_operation(self):
        doc = ir_doc()
        res = self.compiler.compile(doc)
        self.assertTrue(res.ok, "\n".join(res.error_lines()))
        plan = json.loads(res.plan.to_json())
        module_ids = [n["module_id"] for n in plan["nodes"]]
        self.assertEqual(len(module_ids), len(set(module_ids)),
                         "每 module 只能出现一次(唯一绑定)")
        for n in plan["nodes"]:
            self.assertTrue(n["operation"])
            self.assertTrue(n["entry"])
            self.assertEqual(n["phase"], "phase2")
            self.assertEqual(plan["module_bindings"][n["module_id"]], n["operation"])

    def test_edges_typed(self):
        doc = ir_doc()
        res = self.compiler.compile(doc)
        self.assertTrue(res.ok)
        plan = json.loads(res.plan.to_json())
        self.assertTrue(plan["edges"])
        for e in plan["edges"]:
            for key in ("data_schema_id", "unit", "coordinate", "scalar", "shape_hint"):
                self.assertTrue(e[key], f"edge {e['artifact']} 缺 {key}")

    def test_no_aggregate_session_modules(self):
        reg = Registry.load_default()
        for banned in ("astrocs.phase2.resample", "astrocs.phase3.resample"):
            self.assertIsNone(reg.module(banned),
                              f"聚合 Session module {banned} 不得入绑定表")
        doc = ir_doc()
        doc["nodes"][0]["module_id"] = "astrocs.phase2.resample"
        doc["nodes"][0]["operation"] = "resample"
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok, "聚合 Session module 入图应 FAIL")

    def test_cli_plan_generation(self):
        """CLI 模式生成计划图: exit 0 + stdout 含 plan-graph schema。"""
        cp = subprocess.run(
            [sys.executable, str(COMPILER), str(FIXTURE)],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(cp.returncode, 0, f"stderr: {cp.stderr}")
        plan = json.loads(cp.stdout)
        self.assertEqual(plan["schema"], PLAN_SCHEMA_CONST)
        self.assertEqual(len(plan["nodes"]), 7)

    def test_cli_rejects_implicit_path(self):
        with tempfile.TemporaryDirectory() as td:
            bad = pathlib.Path(td) / "bad.json"
            doc = ir_doc()
            doc["nodes"][0]["inputs"]["calibrated"] = "run/frames.fits"
            bad.write_text(json.dumps(doc), encoding="utf-8")
            cp = subprocess.run(
                [sys.executable, str(COMPILER), str(bad)],
                capture_output=True, text=True, timeout=60)
            self.assertNotEqual(cp.returncode, 0)
            self.assertIn("IMPLICIT_PATH", cp.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
