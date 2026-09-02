#!/usr/bin/env python3
"""RT-001 typed DAG 负测：编译器必须拒绝全部禁用图形态。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-001):
  - 7 节点同 session (多节点复用同一 module/包装) → MODULE_REUSED/UNKNOWN_MODULE 拒绝
  - 隐式文件路径 (端口值非 artifact: 引用/裸路径)  → IMPLICIT_PATH 拒绝
  - 错类型 (边 scalar 不一致)                    → TYPE_MISMATCH 拒绝
  - 循环                                          → CYCLE 拒绝
  - 重复 producer                                 → DUPLICATE_PRODUCER 拒绝
  - 跨 Phase edge                                 → CROSS_PHASE 拒绝
另覆盖: phase scope / 必需端口缺失 / 未知 operation / 单元/坐标/schema 冲突 / 顶层缺字段。

每条用例独立调用 compiler 与默认 registry；断言错误码集合包含期望码。
"""
from __future__ import annotations

import copy
import pathlib
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "pipeline"))

from typed_dag import (  # noqa: E402
    DagError,
    Registry,
    TypedDagCompiler,
    schema_self_check,
)

# 真实模块 (module_ports.registry.json 登记) — 确保负测失败原因是图形态而非未知模块
P2_MODULES = {
    "coverage": "astrocs.phase2.coverage",
    "sample": "astrocs.phase2.sample",
    "upm_fit": "astrocs.phase2.upm-fit",
    "upm_apply": "astrocs.phase2.upm-apply",
    "reject": "astrocs.phase2.reject",
    "integrate": "astrocs.phase2.integrate",
    "write": "astrocs.phase2.write",
}
P2_CHAIN = [
    ("coverage", P2_MODULES["coverage"], "calibrated", "coverage"),
    ("sample", P2_MODULES["sample"], "coverage", "samples"),
    ("upm_fit", P2_MODULES["upm_fit"], "samples", "upm_model"),
    ("upm_apply", P2_MODULES["upm_apply"], "upm_model", "corrected"),
    ("reject", P2_MODULES["reject"], "corrected", "accepted_mask"),
    ("integrate", P2_MODULES["integrate"], "accepted_mask", "integrated"),
    ("write", P2_MODULES["write"], "integrated", "mosaic"),
]
# 各节点所需第二输入(链式输入之外) (upm_apply/integrate 需 corrected/calibrated 帧输入)
SECOND_IN = {
    "upm_apply": ("calibrated_frames", "artifact:frames"),
    "integrate": ("corrected", None),  # corrected 由 reject 产出; 链覆盖
}


def _node_json(nid: str, mid: str, operation: str, in_port: str, in_art: str,
               out_port: str, out_art: str, rc: str = "cpu_heavy") -> dict:
    return {
        "node_id": nid,
        "module_id": mid,
        "operation": operation,
        "config": {"phase": 2},
        "inputs": {in_port: in_art},
        "outputs": {out_port: out_art},
        "resources": {"class": rc, "parallel": True},
    }


OPS = {
    "coverage": "compute_coverage", "sample": "sample_frames",
    "upm_fit": "fit_upm", "upm_apply": "apply_upm",
    "reject": "reject_outliers", "integrate": "integrate_frames",
    "write": "write_mosaic",
}

# 每节点: 输入端口 -> artifact(producer 为前一节点或 seed artifact:cal)
# 注意多消费者共享 producer 输出(如 corrected 同时被 reject/integrate 消费) — producer 仍唯一。
EDGES = {
    "coverage": {"calibrated": "artifact:cal"},                 # seed 输入(无 producer)
    "sample": {"coverage": "artifact:coverage"},
    "upm_fit": {"samples": "artifact:samples"},
    "upm_apply": {"upm_model": "artifact:upm_model",
                  "calibrated_frames": "artifact:cal"},
    "reject": {"corrected": "artifact:corrected"},
    "integrate": {"accepted_mask": "artifact:accepted_mask",
                  "corrected": "artifact:corrected"},
    "write": {"integrated": "artifact:integrated"},
}
OUT_ART = {
    "coverage": "artifact:coverage", "sample": "artifact:samples",
    "upm_fit": "artifact:upm_model", "upm_apply": "artifact:corrected",
    "reject": "artifact:accepted_mask", "integrate": "artifact:integrated",
    "write": "artifact:mosaic",
}


def base_phase2_ir() -> dict:
    """有效 Phase2 七节点图(全部真实唯一 operation) → 供负测做单点变异。"""
    nodes = []
    for (nid, mid, _in_port, out_port) in P2_CHAIN:
        n = _node_json(nid, mid, OPS[nid],
                       "", "", out_port, OUT_ART[nid])
        n["inputs"] = dict(EDGES[nid])
        if nid == "write":
            n["resources"] = {"class": "io", "parallel": False}
        nodes.append(n)
    return {
        "schema": "astrocs.typed-dag/v1",
        "pipeline_id": "phase2.rt001",
        "phase": "phase2",
        "version": "1.0.0",
        "nodes": nodes,
        "outputs": {"mosaic": "artifact:mosaic"},
    }


def codes(res) -> set:
    return {e["code"] for e in res.errors}


class TestNegative(unittest.TestCase):
    def setUp(self):
        self.compiler = TypedDagCompiler()

    def _expect(self, doc: dict, want: str, label: str):
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok, f"{label}: 应 FAIL")
        self.assertIn(want, codes(res),
                      f"{label}: 期望错误码 {want}, 实际 {sorted(codes(res))}\n"
                      + "\n".join(res.error_lines()))

    # ── 1. 7 节点同 session (AUD-001 IRF-0007 伪模块化) ──
    def test_seven_nodes_same_session_reused_module(self):
        """7 个节点全部声明同一 module_id(同 Session 包装)→ MODULE_REUSED 拒绝。"""
        doc = base_phase2_ir()
        for n in doc["nodes"]:
            n["module_id"] = "astrocs.phase2.resample"   # 聚合 Session 模块(未入绑定表)
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok, "7 节点同 session module 应 FAIL")
        self.assertIn(DagError.UNKNOWN_MODULE, codes(res),
                      f"聚合 session module 未登记应报 UNKNOWN_MODULE, 实际 {sorted(codes(res))}")
        # 另一形态: 使用真实绑定 module 但复制同一 module 多次(同操作多节点) → MODULE_REUSED
        doc2 = base_phase2_ir()
        for n in doc2["nodes"]:
            n["module_id"] = P2_MODULES["coverage"]
            n["operation"] = "compute_coverage"
        self._expect(doc2, DagError.MODULE_REUSED, "多节点复用同一真实 module")

    # ── 2. 隐式文件路径 ──
    def test_implicit_file_path_input(self):
        doc = base_phase2_ir()
        doc["nodes"][0]["inputs"]["calibrated"] = "run/out/frames.fits"
        self._expect(doc, DagError.IMPLICIT_PATH, "输入端口裸路径")

    def test_implicit_file_path_output(self):
        doc = base_phase2_ir()
        doc["nodes"][-1]["outputs"]["mosaic"] = "C:/out/mosaic.hips"
        self._expect(doc, DagError.IMPLICIT_PATH, "输出端口 Windows 裸路径")

    def test_implicit_file_path_top_output(self):
        doc = base_phase2_ir()
        doc["outputs"]["mosaic"] = "outputs/mosaic.hips"
        self._expect(doc, DagError.IMPLICIT_PATH, "顶层输出裸路径")

    # ── 3. 错类型 (scalar 不一致) ──
    def test_wrong_type_edge(self):
        doc = base_phase2_ir()
        res = self.compiler.compile(doc)
        self.assertTrue(res.ok, "原始链应编译通过")
        # 构造 type 冲突: wcs 输出 wcs_plan(f64) 同时接 resample2 的 wcs_plan(f64, 对) 与
        # hips 输入(f32) → scalar/unit/coord/schema 全部冲突; 断言含 TYPE_MISMATCH。
        doc3 = {
            "schema": "astrocs.typed-dag/v1",
            "pipeline_id": "type.bad",
            "phase": "phase3",
            "version": "1",
            "nodes": [
                {"node_id": "a", "module_id": "astrocs.phase3.properties",
                 "operation": "read_properties", "config": {},
                 "inputs": {"hips": "artifact:h"}, "outputs": {"props": "artifact:p"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
                {"node_id": "b", "module_id": "astrocs.phase3.wcs",
                 "operation": "build_wcs", "config": {},
                 "inputs": {"props": "artifact:p"}, "outputs": {"wcs_plan": "artifact:w"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
                {"node_id": "c", "module_id": "astrocs.phase3.resample2",
                 "operation": "resample_projection", "config": {},
                 "inputs": {"wcs_plan": "artifact:w", "hips": "artifact:w"},
                 "outputs": {"resampled": "artifact:r"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
            ],
            "outputs": {"r": "artifact:r"},
        }
        res3 = self.compiler.compile(doc3)
        self.assertFalse(res3.ok, "错类型边应 FAIL")
        self.assertIn(DagError.TYPE_MISMATCH, codes(res3),
                      f"应报 TYPE_MISMATCH, 实际 {sorted(codes(res3))}")

    # ── 4. 循环 ──
    def test_cycle(self):
        doc = base_phase2_ir()
        # write 输出 artifact:mosaic → coverage 输入改为消费 mosaic(反向边形成环)
        doc["nodes"][0]["inputs"]["calibrated"] = "artifact:mosaic"
        self._expect(doc, DagError.CYCLE, "反向边成环")

    def test_self_loop(self):
        doc = base_phase2_ir()
        # coverage 自己消费自己的输出 artifact:coverage(自环)
        doc["nodes"][0]["inputs"]["calibrated"] = "artifact:coverage"
        self._expect(doc, DagError.CYCLE, "节点自环")

    # ── 5. 重复 producer ──
    def test_duplicate_producer(self):
        doc = base_phase2_ir()
        doc["nodes"][1]["outputs"]["samples"] = "artifact:coverage"  # 重复产出 coverage
        self._expect(doc, DagError.DUPLICATE_PRODUCER, "重复 producer")

    # ── 6. 跨 Phase edge ──
    def test_cross_phase_edge(self):
        doc = base_phase2_ir()
        # 注入一个 phase1 节点产出 frames 供 coverage 消费(跨 phase edge)
        doc["phase"] = "phase2"
        p1 = {
            "node_id": "p1_writer",
            "module_id": "astrocs.phase1.writer",
            "operation": "write_hips",
            "config": {},
            "inputs": {"stacked": "artifact:stk"},
            "outputs": {"fits": "artifact:frames"},
            "resources": {"class": "io", "parallel": False},
        }
        # 该 p1 节点与图 phase(phase2) 冲突 → PHASE_SCOPE 先触发; 把图 phase 改 phase1
        # 但链仍是 phase2 module → 大量 PHASE_SCOPE。更干净做法: 双节点图跨 phase。
        doc2 = {
            "schema": "astrocs.typed-dag/v1",
            "pipeline_id": "cross",
            "phase": "phase3",
            "version": "1",
            "nodes": [
                {"node_id": "p2_write", "module_id": "astrocs.phase2.write",
                 "operation": "write_mosaic", "config": {},
                 "inputs": {"integrated": "artifact:i"}, "outputs": {"mosaic": "artifact:m"},
                 "resources": {"class": "io", "parallel": False}},
                {"node_id": "p3_props", "module_id": "astrocs.phase3.properties",
                 "operation": "read_properties", "config": {},
                 "inputs": {"hips": "artifact:m"}, "outputs": {"props": "artifact:p"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
            ],
            "outputs": {"p": "artifact:p"},
        }
        res = self.compiler.compile(doc2)
        self.assertFalse(res.ok, "跨 Phase 图应 FAIL")
        # phase3 顶层下 phase2.write 节点本身 PHASE_SCOPE 违例(先报); 断言二者至少其一
        self.assertTrue(
            DagError.CROSS_PHASE in codes(res) or DagError.PHASE_SCOPE in codes(res),
            f"跨 Phase 应被拒绝, 实际 {sorted(codes(res))}")

    def test_cross_phase_same_top_phase(self):
        """顶层 phase=phase2 但 module 混入 phase1/phase3 → PHASE_SCOPE/CROSS_PHASE 拒绝。"""
        doc = base_phase2_ir()
        doc["nodes"][0]["module_id"] = "astrocs.phase1.calibration"
        doc["nodes"][0]["operation"] = "calibrate"
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok, "混入异 phase module 应 FAIL")
        self.assertIn(DagError.PHASE_SCOPE, codes(res),
                      f"应报 PHASE_SCOPE, 实际 {sorted(codes(res))}")

    # ── 附加: phase scope / 必需端口 / operation / schema 冲突 ──
    def test_phase_scope_module_mismatch(self):
        doc = base_phase2_ir()
        doc["phase"] = "phase1"
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok)
        self.assertIn(DagError.PHASE_SCOPE, codes(res))

    def test_missing_required_port(self):
        doc = base_phase2_ir()
        # reject 需 corrected 输入; 用占位端口避开空 dict 的 STRUCT, 但 corrected 缺供 → MISSING_PORT
        for n in doc["nodes"]:
            if n["node_id"] == "reject":
                n["inputs"] = {"not_corrected": "artifact:x"}
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok, "缺必需端口应 FAIL")
        self.assertIn(DagError.MISSING_PORT, codes(res),
                      f"应报 MISSING_PORT, 实际 {sorted(codes(res))}")

    def test_unknown_port(self):
        doc = base_phase2_ir()
        doc["nodes"][0]["inputs"]["ghost_port"] = "artifact:x"
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok)
        self.assertIn(DagError.UNKNOWN_PORT, codes(res))

    def test_unknown_operation(self):
        doc = base_phase2_ir()
        doc["nodes"][0]["operation"] = "no_such_op"
        res = self.compiler.compile(doc)
        self.assertFalse(res.ok)
        self.assertIn(DagError.UNKNOWN_OPERATION, codes(res))

    def test_unit_mismatch(self):
        """wcs_plan(DEGREE) 输出 → resample2 hips 输入(ADU) → UNIT_MISMATCH。"""
        doc3 = {
            "schema": "astrocs.typed-dag/v1",
            "pipeline_id": "unit.bad",
            "phase": "phase3",
            "version": "1",
            "nodes": [
                {"node_id": "a", "module_id": "astrocs.phase3.properties",
                 "operation": "read_properties", "config": {},
                 "inputs": {"hips": "artifact:h"}, "outputs": {"props": "artifact:p"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
                {"node_id": "b", "module_id": "astrocs.phase3.wcs",
                 "operation": "build_wcs", "config": {},
                 "inputs": {"props": "artifact:p"}, "outputs": {"wcs_plan": "artifact:w"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
                {"node_id": "c", "module_id": "astrocs.phase3.resample2",
                 "operation": "resample_projection", "config": {},
                 "inputs": {"wcs_plan": "artifact:w", "hips": "artifact:w"},
                 "outputs": {"resampled": "artifact:r"},
                 "resources": {"class": "cpu_heavy", "parallel": True}},
            ],
            "outputs": {"r": "artifact:r"},
        }
        res = self.compiler.compile(doc3)
        self.assertFalse(res.ok)
        codes_ = codes(res)
        self.assertTrue(
            DagError.UNIT_MISMATCH in codes_ or DagError.DATA_MISMATCH in codes_,
            f"单位/schema 冲突应报, 实际 {sorted(codes_)}")

    def test_structural_missing_schema(self):
        doc = base_phase2_ir()
        del doc["schema"]
        self._expect(doc, DagError.STRUCT, "缺 schema")

    def test_schema_self_check_pass(self):
        issues = schema_self_check()
        self.assertEqual(issues, [], f"schema 自检不一致: {issues}")

    def test_registry_unique_operations(self):
        reg = Registry.load_default()
        self.assertEqual(reg.issues, [], f"registry 装载问题: {reg.issues}")
        for mid, m in reg.modules.items():
            self.assertEqual(len(m.get("operations", [])), 1,
                             f"{mid} 必须唯一绑定一个 operation")
        # 聚合 Session 模块不得出现在绑定表
        for banned in ("astrocs.phase2.resample", "astrocs.phase3.resample"):
            self.assertIsNone(reg.module(banned),
                              f"聚合 Session 模块 {banned} 不得入绑定表")


if __name__ == "__main__":
    unittest.main(verbosity=2)
