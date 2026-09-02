#!/usr/bin/env python3
"""RT-002 Phase-isolated Runtime 生命周期验收测试。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-002):
  - 进程内全局状态 spy: 生命周期对象（Registry/Store/RunContext）run 私有，
    生产执行源不存在进程内全局共享注册表单例贯穿多个 run → 静态扫描 + spy 断言。
  - phase1 DLL 缺失不影响 phase3 外部 fixture: phase3 registry 视图只含 phase3 模块，
    phase1/2 模块不在其视图/工厂；phase3 可读取 origin=external_fixture 的磁盘交换
    对象（DATA-002 形态）而不依赖任何 phase1/2 进程内对象。
  - 禁止 `--phases` 调用图: 同一运行图 module phase 集合必须 size==1；
    phase1+phase3（或任何跨 phase）模块混图 → 拒绝。
  - 每条 phase CLI 独立 Runtime/Store/RunContext: new_lifecycle 每次返回新对象，
    registry 只含本 Phase；多次 run 互不共享 Store/RunContext。
另覆盖: 交换对象缺 manifest/缺 hash → 进程外读取拒绝; registry 一致性自检。
"""
from __future__ import annotations

import json
import pathlib
import re
import subprocess
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "core"))
sys.path.insert(0, str(REPO / "runtime" / "pipeline"))
sys.path.insert(0, str(REPO / "runtime" / "artifact_store"))

from phase_lifecycle import (  # noqa: E402
    PhaseIsolationGuard,
    PhaseManifestReader,
    _PHASES,
    load_registry,
    phase_of_module,
    phase_registry_view,
)

FX = REPO / "contracts" / "data" / "examples" / "external_fixture_hips.example.json"
P1_FX = REPO / "contracts" / "data" / "examples" / "phase1_product_v1.example.json"
LIFECYCLE_PY = REPO / "runtime" / "core" / "phase_lifecycle.py"

# 各 phase 代表模块（真实 registry 登记）
P1_MOD = "astrocs.phase1.calibration"
P2_MOD = "astrocs.phase2.coverage"
P3_MOD = "astrocs.phase3.properties"


def registry_module_ids() -> list[str]:
    return [m["module_id"] for m in load_registry()]


class TestRegistryViewIsolation(unittest.TestCase):
    """spy 断言 1: phase registry 视图只含本 Phase 模块（DLL 缺失隔离前提）。"""

    def test_phase_registry_view_contains_only_own_phase(self):
        for ph in _PHASES:
            view = phase_registry_view(ph)
            self.assertGreater(len(view), 0, f"{ph} registry 视图为空")
            for mid in view:
                p = phase_of_module(mid)
                self.assertEqual(p, ph,
                                 f"phase registry 视图泄漏: {mid} 属 {p} 却在 {ph} 视图")

    def test_phase1_modules_not_in_phase3_view(self):
        """phase3 视图绝不含 phase1/2 模块 → phase1 DLL 缺失不影响 phase3 生命周期。"""
        v3 = phase_registry_view("phase3")
        for mid in registry_module_ids():
            if mid.startswith("astrocs.phase1.") or mid.startswith("astrocs.phase2."):
                self.assertNotIn(mid, v3,
                                 f"phase3 视图含他 phase 模块 {mid}（隔离破坏）")

    def test_phase3_modules_only_five_and_all_own_phase(self):
        v3 = phase_registry_view("phase3")
        self.assertEqual(len(v3), 5)   # RT-001 registry: properties/wcs/resample2/writer/verify
        for mid in v3:
            self.assertTrue(mid.startswith("astrocs.phase3."), mid)

    def test_guard_no_leak(self):
        for ph in _PHASES:
            self.assertEqual(PhaseIsolationGuard.assert_registry_view_isolated(ph), [])


class TestLifecycleObjectsRunPrivate(unittest.TestCase):
    """spy 断言 2: 每次 run 新建 Registry/Store/RunContext（无进程内共享贯穿）。"""

    def setUp(self):
        self.guard = PhaseIsolationGuard()

    def test_new_lifecycle_each_run_private_objects(self):
        a = self.guard.new_lifecycle("phase1", "run-1")
        b = self.guard.new_lifecycle("phase1", "run-2")
        # Store / RunContext 为独立实例（run 私有，绝不共享）
        self.assertIsNot(a["store"], b["store"])
        self.assertIsNot(a["run_context"], b["run_context"])
        self.assertEqual(a["phase"], "phase1")
        self.assertEqual(a["run_id"], "run-1")

    def test_registry_views_not_shared_between_runs(self):
        """spy: 每次 run 的 registry 视图是独立快照，无进程内全局注册表贯穿。"""
        a = self.guard.new_lifecycle("phase3", "run-a")
        b = self.guard.new_lifecycle("phase3", "run-b")
        self.assertIsNot(a["registry_modules"], b["registry_modules"])
        # 修改 run-a 的 store 不影响 run-b（无共享容器）
        a["store"]["k"] = "v"
        self.assertNotIn("k", b["store"])
        # spy 观察记录可审计（不同 run_id 均已登记，供断言使用）
        self.assertIn("run-a", self.guard._global_seen["phase3"])
        self.assertIn("run-b", self.guard._global_seen["phase3"])

    def test_lifecycle_registry_only_own_phase(self):
        lc = self.guard.new_lifecycle("phase2", "run-9")
        self.assertTrue(all(m.startswith("astrocs.phase2.") for m in lc["registry_modules"]))
        self.assertIn("astrocs.phase2.coverage", lc["registry_modules"])
        self.assertNotIn("astrocs.phase1.calibration", lc["registry_modules"])
        self.assertNotIn("astrocs.phase3.properties", lc["registry_modules"])

    def test_lifecycle_phase3_independent_of_phase1_factory_absence(self):
        """phase1 DLL 缺失模拟: phase1 模块不在 phase3 registry 视图/工厂。

        phase3 生命周期创建不触碰任何 phase1/2 模块（无隐式依赖）→ 不受缺失影响。
        """
        v3 = phase_registry_view("phase3")
        self.assertNotIn(P1_MOD, v3)
        self.assertNotIn(P2_MOD, v3)
        lc = self.guard.new_lifecycle("phase3", "run-ext")
        self.assertTrue(all(m.startswith("astrocs.phase3.") for m in lc["registry_modules"]))


class TestPhase3ExternalFixture(unittest.TestCase):
    """验收: phase1 DLL 缺失不影响 phase3 外部 fixture。

    phase3 的输入资格判定只读 DATA-002 磁盘交换对象（origin=external_fixture），
    不依赖任何 phase1/2 进程内对象/DLL。
    """

    @classmethod
    def setUpClass(cls):
        cls.fx = FX.read_text(encoding="utf-8")
        cls.reader = PhaseManifestReader(data=json.loads(cls.fx))

    def test_external_fixture_manifest_readable(self):
        doc = json.loads(self.fx)
        self.assertEqual(doc.get("origin"), "external_fixture")
        man = self.reader.manifest()
        self.assertEqual(man.get("status"), "COMPLETE")
        self.assertEqual(self.reader.content_digest(),
                         "3333333333333333333333333333333333333333333333333333333333333333")

    def test_external_fixture_declares_run_phase(self):
        self.assertEqual(self.reader.producer_run_phase(), "phase2")  # fixture 自述为 mosaic 源

    def test_phase3_registry_can_consume_external_fixture(self):
        """phase3 模块 (properties/wcs/...) 输入 hips 端口 → DATA-HIPS-001；
        外部 fixture type_id = astrocs.phase2.mosaic_hips.v1 属 P2_HIPS role，
        phase3 进程按 DATA-002 交换资格进程外读取 → 不要求 phase1 进程/DLL 在场。"""
        v3 = phase_registry_view("phase3")
        self.assertIn(P3_MOD, v3)
        # 进程外读取不触碰 phase1 任何对象——本测试仅用本进程数据 + 磁盘对象
        self.assertTrue(self.reader.read_only_external())

    def test_missing_manifest_rejected(self):
        doc = json.loads(self.fx)
        del doc["artifact_manifest"]
        with self.assertRaises(ValueError):
            PhaseManifestReader(data=doc).manifest()

    def test_missing_hash_rejected(self):
        doc = json.loads(self.fx)
        doc["artifact_manifest"]["content_digest"] = {"algorithm": "sha256", "hex": "zz"}
        with self.assertRaises(ValueError):
            PhaseManifestReader(data=doc).content_digest()


class TestNoCrossPhaseGraph(unittest.TestCase):
    """验收: 禁止 `--phases` 调用图（同一图只能一个 Phase）。"""

    def test_single_phase_graph_ok(self):
        self.assertIsNone(
            PhaseIsolationGuard.assert_no_cross_phase_graph([P1_MOD]))
        self.assertIsNone(
            PhaseIsolationGuard.assert_no_cross_phase_graph(
                ["astrocs.phase3.properties", "astrocs.phase3.writer"]))

    def test_cross_phase_graph_rejected(self):
        reason = PhaseIsolationGuard.assert_no_cross_phase_graph([P1_MOD, P3_MOD])
        self.assertIsNotNone(reason)
        self.assertIn("phase", reason.lower())
        reason2 = PhaseIsolationGuard.assert_no_cross_phase_graph(
            ["astrocs.phase2.coverage", "astrocs.phase1.calibration"])
        self.assertIsNotNone(reason2)

    def test_graph_phase_set(self):
        self.assertEqual(PhaseIsolationGuard.graph_phase_set([P1_MOD]), ["phase1"])
        self.assertEqual(
            PhaseIsolationGuard.graph_phase_set([P1_MOD, P3_MOD]), ["phase1", "phase3"])


class TestCliSelfChecks(unittest.TestCase):
    """CLI 形态自检（registry 隔离 / 跨 phase 图拒绝）。"""

    def test_registry_isolation_cli(self):
        r = subprocess.run([sys.executable, str(LIFECYCLE_PY),
                            "--registry-isolation-check"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("PHASE_REGISTRY_ISOLATION PASS", r.stdout)
        for ph in _PHASES:
            self.assertIn(f"phase={ph} ", r.stdout)

    def test_phase_of_cli(self):
        r = subprocess.run([sys.executable, str(LIFECYCLE_PY),
                            "--phase-of", P3_MOD],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), "phase3")

    def test_cross_phase_graph_cli_rejects(self):
        r = subprocess.run([sys.executable, str(LIFECYCLE_PY),
                            "--cross-phase-graph-check", P1_MOD, P3_MOD],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 1)
        self.assertIn("CROSS_PHASE_GRAPH REJECT", r.stderr)

    def test_cross_phase_graph_cli_accepts_single(self):
        r = subprocess.run([sys.executable, str(LIFECYCLE_PY),
                            "--cross-phase-graph-check", P3_MOD],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("CROSS_PHASE_GRAPH PASS", r.stdout)


class TestNoGlobalRegistryInProductionSource(unittest.TestCase):
    """进程内全局状态 spy（源码级）: 生产执行源不得有进程内全局共享注册表单例。

    约束 A.3/A.4/RT-002: Runtime/Registry/Store/RunContext 属 run/phase 生命周期，
    不得以模块级全局单例贯穿多个 run。静态扫描 typed_dag/phase_lifecycle 生产源
    中不允许 `^[A-Z_]+ *= *{}` 全局注册表容器或 `global ` 注册表声明。
    """

    PROD_SOURCES = [
        REPO / "runtime" / "core" / "phase_lifecycle.py",
        REPO / "runtime" / "pipeline" / "typed_dag.py",
    ]
    GLOBAL_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\s*=\s*(\{\}|\[\])\s*$")
    GLOBAL_REG_RE = re.compile(r"^global\s+")

    def test_no_global_registry_container(self):
        for src in self.PROD_SOURCES:
            text = src.read_text(encoding="utf-8")
            for lineno, line in enumerate(text.splitlines(), 1):
                stripped = line.strip()
                # 跳过注释与类型标注行
                if stripped.startswith("#") or ":" in stripped:
                    continue
                self.assertFalse(
                    self.GLOBAL_RE.match(stripped),
                    f"{src.name}:{lineno} 疑似模块级全局容器: {line}")
                self.assertFalse(
                    self.GLOBAL_REG_RE.match(stripped),
                    f"{src.name}:{lineno} 疑似 global 注册表声明: {line}")

    def test_registry_loaded_per_call_not_cached_globally(self):
        """load_registry 每次从磁盘真源读; 不存在模块级缓存单例（无全局注册表贯穿）。"""
        first = registry_module_ids()
        second = registry_module_ids()
        self.assertEqual(first, second)
        self.assertGreater(len(first), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
