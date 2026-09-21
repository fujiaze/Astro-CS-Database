# -*- coding: utf-8 -*-
"""eng/ci/tests/test_gate_failclosed_selftest.py — 门禁 fail-closed 契约的红绿双向自测。

覆盖（GATE-501：D-10 / D-12 落地后的 runner 语义，权威 docs/ci/CI_SPEC.md §9）
  A. requires_monitor=true ⇒ 必须产出监控证据；缺失 ⇒ FAIL(monitor_gate_missing)（红）
  B. 证据含 frozen_gate 且四条 L2 冻结判据违规 ⇒ 判红（D-10 恒真门堵口）（红）
  C. 证据合规 ⇒ PASS（绿）
  D. 纯采样留证（无 frozen_gate、命令未请求判定）⇒ PASS（绿，契约不变）
  E. 命令请求判定（--gate-required）但证据无 frozen_gate ⇒ 判红（红）
  F. mutates_workspace=true 写出登记面之外 ⇒ FAIL(dirty)（红，取消自我豁免）
  G. mutates_workspace=true 只写登记 outputs ⇒ PASS（绿）
  H. 登记输出缺失 ⇒ FAIL(missing_output)（红）

本模块**自包含**（不 import ci.tests 包）：可直接被
  python3 -B -m unittest discover -s eng/ci/tests -t eng/ci/tests -p test_gate_failclosed_selftest.py
执行，并作为注册检查 CHK-GATE-FAILCLOSED-SELFTEST 在 fast 档常跑。
所有 fixture 建在临时目录，绝不写主工作区；所有外部命令带 timeout。
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
RUNNER = REPO / "eng" / "ci" / "run.py"
CONTRACT = REPO / "eng" / "contracts" / "resource_gate_v1.json"

GOOD_METRICS = {
    "effective_cpus": 16, "allocated": 16, "interval_seconds": 60.0,
    "avg_utilization": 0.95, "p50_utilization": 0.97,
    "sample_pass_fraction": 1.0, "max_low_window_seconds": 0.0,
    "utilization_evaluated": True,
}
BAD_METRICS = {
    "effective_cpus": 16, "allocated": 16, "interval_seconds": 389.278,
    "avg_utilization": 0.245275, "p50_utilization": 0.038825,
    "sample_pass_fraction": 0.069272, "max_low_window_seconds": 142.049,
    "utilization_evaluated": True,
}
MON = "run/ci/monitor/T-GATE.json"


def _sh(argv, cwd, timeout=120):
    return subprocess.run(argv, cwd=str(cwd), capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout)


def _write_script(path_rel: str, payload: dict | None, extra_write: str | None = None) -> str:
    """生成一段 python -c 脚本：按需写监控证据 / 额外文件。"""
    lines = ["import json,pathlib"]
    if payload is not None:
        lines.append(f"p=pathlib.Path({path_rel!r});p.parent.mkdir(parents=True,exist_ok=True)")
        lines.append(f"p.write_text(json.dumps({payload!r}),encoding='utf-8')")
    if extra_write:
        lines.append(f"q=pathlib.Path({extra_write!r});q.parent.mkdir(parents=True,exist_ok=True)")
        lines.append("q.write_text('x',encoding='utf-8')")
    lines.append("print('ok')")
    return ";".join(lines)


def _evidence(metrics: dict, verdict: str = "pass") -> dict:
    return {"duration_seconds": metrics["interval_seconds"], "poll_interval": 0.2,
            "cpu_samples": [{"t": 0.0, "cpu_percent": 100.0}],
            "frozen_gate": {"verdict": verdict, "violations": [], "recorded": [],
                            "metrics": metrics}}


class GateFailClosedBase:
    """fixture 混入（不直接继承 TestCase：注册表校验器 R11 按 AST 统计
    直接继承 unittest.TestCase 的 test_* 方法数，混入类必须显式并列继承）。"""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.repo = Path(self._tmp.name)
        _sh(["git", "init", "-q", "-b", "main"], self.repo)
        _sh(["git", "config", "user.email", "t@astrocs.invalid"], self.repo)
        _sh(["git", "config", "user.name", "CI Test"], self.repo)
        (self.repo / "A.txt").write_text("A\n", encoding="utf-8")
        _sh(["git", "add", "-A"], self.repo)
        _sh(["git", "commit", "-q", "-m", "init"], self.repo)
        reg_dir = self.repo / "eng" / "ci"
        reg_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(REPO / "eng" / "ci" / "ci_result.schema.json",
                     reg_dir / "ci_result.schema.json")
        (self.repo / "eng" / "contracts").mkdir(parents=True, exist_ok=True)
        shutil.copy2(CONTRACT, self.repo / "eng" / "contracts" / "resource_gate_v1.json")
        # requires_monitor 的前置锚：统一监控包装器必须存在（V8-CI-003）。
        # fixture 只验证**判定语义**，不执行监控本身，故落最小存在性桩。
        (reg_dir / "resource_monitor.py").write_text(
            "#!/usr/bin/env python3\nprint('fixture monitor stub')\n", encoding="utf-8")
        self.reg_dir = reg_dir

    def tearDown(self):
        self._tmp.cleanup()

    def run_check(self, check: dict) -> tuple[dict, subprocess.CompletedProcess]:
        self.reg_dir.joinpath("checks.json").write_text(
            json.dumps({"schema_version": 1, "checks": [check]}, ensure_ascii=False),
            encoding="utf-8")
        out_root = self.repo / "out"
        proc = _sh([sys.executable, str(RUNNER), "--repo-root", str(self.repo),
                    "--check", check["id"], "--output-root", str(out_root)], self.repo)
        per = json.loads((out_root / "checks" / f"{check['id']}.json").read_text(
            encoding="utf-8"))
        return per, proc

    @staticmethod
    def check(**overrides) -> dict:
        base = {
            "id": "T-GATE", "profiles": ["fast"], "platform": "any",
            "command": ["python3", "-c", "print('ok')"], "timeout_seconds": 60,
            "heavy": False, "mutates_workspace": False, "outputs": [],
            "waivable": False, "changed_paths": [], "requires_monitor": False,
        }
        base.update(overrides)
        return base


class TestMonitorEvidenceFailClosed(GateFailClosedBase, unittest.TestCase):
    def test_A_requires_monitor_without_evidence_is_red(self):
        per, _ = self.run_check(self.check(requires_monitor=True))
        self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)", per.get("reason"))

    def test_B_frozen_gate_violation_is_red(self):
        per, _ = self.run_check(self.check(
            requires_monitor=True, outputs=[MON],
            command=["python3", "-c", _write_script(MON, _evidence(BAD_METRICS))]))
        self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)", per.get("reason"))
        self.assertIn("L2 冻结判据", per.get("reason") or "")

    def test_C_compliant_evidence_is_green(self):
        per, _ = self.run_check(self.check(
            requires_monitor=True, outputs=[MON],
            command=["python3", "-c", _write_script(MON, _evidence(GOOD_METRICS))]))
        self.assertEqual(per["verdict"], "PASS", per.get("reason"))

    def test_D_sampling_only_without_gate_request_is_green(self):
        ev = _evidence(GOOD_METRICS)
        ev.pop("frozen_gate")
        per, _ = self.run_check(self.check(
            requires_monitor=True, outputs=[MON],
            command=["python3", "-c", _write_script(MON, ev)]))
        self.assertEqual(per["verdict"], "PASS", per.get("reason"))

    def test_E_gate_requested_without_frozen_gate_is_red(self):
        ev = _evidence(GOOD_METRICS)
        ev.pop("frozen_gate")
        per, _ = self.run_check(self.check(
            requires_monitor=True, outputs=[MON],
            command=["python3", "-c", _write_script(MON, ev), "--gate-required", "--"]))
        self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)", per.get("reason"))


class TestMutatesWorkspaceFailClosed(GateFailClosedBase, unittest.TestCase):
    def test_F_write_outside_declared_surface_is_red(self):
        per, _ = self.run_check(self.check(
            mutates_workspace=True, outputs=[MON],
            command=["python3", "-c",
                     _write_script(MON, _evidence(GOOD_METRICS), extra_write="leak.txt")]))
        self.assertEqual(per["verdict"], "FAIL(dirty)", per.get("reason"))
        self.assertIn("leak.txt", " ".join(per["dirty"]["violations"]))

    def test_G_write_inside_declared_output_is_green(self):
        per, _ = self.run_check(self.check(
            mutates_workspace=True, outputs=[MON],
            command=["python3", "-c", _write_script(MON, _evidence(GOOD_METRICS))]))
        self.assertEqual(per["verdict"], "PASS", per.get("reason"))

    def test_H_missing_declared_output_is_red(self):
        per, _ = self.run_check(self.check(mutates_workspace=True, outputs=[MON]))
        self.assertEqual(per["verdict"], "FAIL(missing_output)", per.get("reason"))


if __name__ == "__main__":
    unittest.main()
