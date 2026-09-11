# -*- coding: utf-8 -*-
"""CI-001 单测：收紧 CI 注册表与资源 fail-closed（owner=CI-001，ASTROCS_CONSTITUTION_ALIGNMENT_CONTROL_V1）。

控制包 02_GATES_AND_EXECUTION.md §执行："P0/P1、Windows build/test/package、
manifest/hash、真实数据完整性不可 waiver"；任务 CI-001 目标三句：

1. WIN-BUILD-RELEASE / WIN-TEST-UNIT / WIN-PACKAGE-CANDIDATE 不可 waiver
   （waivable=false：prerequisite 不满足 → FAIL(prerequisite)，不再 SKIPPED）；
2. requires_monitor=true 的检查命令必须请求冻结门禁判定（--gate-required，
   监控必须调用 evaluate），且 ci/run.py 对监控证据缺 frozen_gate 的检查
   判 FAIL(monitor_gate_missing)（编排层 fail-closed）；
3. Fatduck select-candidate：候选缺失（select_candidate.py exit 3）必须使
   select job 失败，不得 notice 后静默 success（G-CI：Windows 候选必存在）。

先红后绿：本文件在实现前落地（RED 留档），实现后全绿（GREEN）。
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from ci.tests import _helpers as H  # noqa: E402

MON_JSON = "run/ci/monitor/CHK-MON.json"

MONITOR_CMD_TEMPLATE = [
    "python3", "ci/resource_monitor.py", "--timeout", "30",
    "--output", MON_JSON,
]
MONITOR_TAIL = ["--", "python3", "-c", "print('ok')"]


def _load_registry() -> list[dict]:
    return json.loads((_REPO / "ci" / "checks.json").read_text(encoding="utf-8"))["checks"]


def _by_id(checks: list[dict]) -> dict:
    return {c["id"]: c for c in checks}


def _runner_mon_check(repo: Path, out_root: Path, check: dict) -> subprocess.CompletedProcess:
    """在 fixture 仓库执行单个 requires_monitor 检查的 runner。"""
    H.write_registry(repo, [check])
    H.write_ci_result_schema(repo)
    ci_dir = repo / "ci"
    ci_dir.mkdir(exist_ok=True)
    # probe_prerequisite 要求统一监控包装器存在（repo/ci/resource_monitor.py）；
    # shim 真跑时经 runpy 透传 repo/tools/monitoring/run_monitored.py，故一并
    # 复制实体与其同目录依赖（resource_probe fallback import）。
    shutil.copyfile(_REPO / "ci" / "resource_monitor.py", ci_dir / "resource_monitor.py")
    mon_dir = repo / "tools" / "monitoring"
    mon_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(_REPO / "tools" / "monitoring" / "run_monitored.py",
                    mon_dir / "run_monitored.py")
    shutil.copyfile(_REPO / "tools" / "monitoring" / "resource_probe.py",
                    mon_dir / "resource_probe.py")
    # 对齐主仓库 .gitignore 行为：fixture 仓库忽略 __pycache__，避免 shim 真
    # 跑 import resource_probe 产生的 .pyc 被判工作区 dirty（主仓库同路径已忽略）。
    (repo / ".gitignore").write_text("__pycache__/\n*.pyc\n", encoding="utf-8")
    return H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)


class TestWinChecksNotWaivable(unittest.TestCase):
    """目标 1：WIN-BUILD/TEST/PACKAGE 三项 waivable=false（不可 waiver）。"""

    def test_win_three_checks_flag_false(self):
        reg = _by_id(_load_registry())
        for cid in ("WIN-BUILD-RELEASE", "WIN-TEST-UNIT", "WIN-PACKAGE-CANDIDATE"):
            self.assertIn(cid, reg, cid)
            self.assertFalse(reg[cid]["waivable"],
                             f"{cid} 必须不可 waiver（CI-001 fail-closed）")

    def test_platform_mismatch_fails_closed_on_linux(self):
        """waivable=false 后：platform 门控不满足 → FAIL(prerequisite)，不再 SKIPPED。"""
        if sys.platform.startswith("win"):
            self.skipTest("仅非 Windows 宿主验证平台门控 fail-closed")
        reg = _by_id(_load_registry())
        check = dict(reg["WIN-BUILD-RELEASE"])
        check["command"] = ["python3", "-c", "print('noop')"]  # 门控命中后不执行
        check["prerequisite_tools"] = []
        check["requires_monitor"] = False  # 本用例聚焦 platform 门控，剥离监控存在性前置
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [check])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "windows-main",
                                 "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "WIN-BUILD-RELEASE")
            self.assertEqual(per["verdict"], "FAIL(prerequisite)")
            self.assertIn("platform_mismatch", per.get("reason") or "")
            self.assertFalse(per["waivable"])
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")


class TestMonitoredChecksRequestGate(unittest.TestCase):
    """目标 2a：所有被监控包装的 requires_monitor=true 检查必须请求 --gate-required。"""

    def test_gate_required_present_before_separator(self):
        reg = _by_id(_load_registry())
        monitored = [c for c in reg.values() if c.get("requires_monitor")]
        self.assertGreaterEqual(len(monitored), 8, "注册表 requires_monitor 检查数")
        for c in monitored:
            cmd = c["command"]
            # requires_monitor 语义 = 检查必须经统一监控包装器执行
            # （ci/run.py probe_prerequisite）；无包装命令的 requires_monitor
            # 登记属矛盾（CI-001 已修正 UT-QUALITY）。
            self.assertIn("ci/resource_monitor.py", cmd,
                          f"{c['id']} requires_monitor 必须携带监控包装器")
            self.assertIn("--gate-required", cmd,
                          f"{c['id']} 监控命令必须请求冻结门禁判定（--gate-required）")
            self.assertLess(cmd.index("--gate-required"), cmd.index("--"),
                            f"{c['id']} --gate-required 必须在监控参数区（`--` 前）")

    def test_win_checks_request_gate(self):
        reg = _by_id(_load_registry())
        for cid in ("WIN-BUILD-RELEASE", "WIN-TEST-UNIT", "WIN-PACKAGE-CANDIDATE"):
            self.assertIn("--gate-required", reg[cid]["command"],
                          f"{cid} 监控必须调用 evaluate（--gate-required）")

    def test_ut_quality_registration_conformance(self):
        """UT-QUALITY 登记矛盾修正：命令无监控包装 → heavy/requires_monitor 均 false。"""
        reg = _by_id(_load_registry())
        c = reg["UT-QUALITY"]
        self.assertNotIn("ci/resource_monitor.py", c["command"],
                         "前提：UT-QUALITY 命令无监控包装器")
        self.assertFalse(c["heavy"], "UT-QUALITY heavy 必须 conform 为 false")
        self.assertFalse(c["requires_monitor"], "UT-QUALITY requires_monitor 必须 conform 为 false")


class TestRunnerGateEvidenceContract(unittest.TestCase):
    """目标 2b：ci/run.py 对 requires_monitor 检查的监控证据合同。

    监控证据（outputs 中含 cpu_samples 的 JSON）必须带 frozen_gate 判定
    （evaluate 已被调用且结论合法）；缺失/非法/矛盾 → FAIL(monitor_gate_missing)。
    正/负用例与监控器解耦（直接落证据 JSON），shim 集成路径单独覆盖。
    """

    @staticmethod
    def _mon_check(command: list[str]) -> dict:
        return H.check(id="CHK-MON", command=command, outputs=[MON_JSON],
                       requires_monitor=True, heavy=True)

    def test_evidence_without_frozen_gate_fails_closed(self):
        """故障注入：监控只采样不判定（证据无 frozen_gate）→ FAIL(monitor_gate_missing)。"""
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'cpu_samples': [{'t': 0.1, 'cpu_percent': 90.0}], 'exit_code': 0}))"
        )
        cmd = ["python3", "-c", writer]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)")
            self.assertIn("frozen_gate", per.get("reason") or "")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")

    def test_evidence_with_gate_verdict_passes(self):
        """正向：证据含合法 frozen_gate.verdict=pass → PASS。"""
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'cpu_samples': [{'t': 0.1, 'cpu_percent': 90.0}], 'exit_code': 0,"
            "'frozen_gate': {'verdict': 'pass', 'violations': [], 'reason': None,"
            "'metrics': {}}}))"
        )
        cmd = ["python3", "-c", writer]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "PASS")

    def test_evidence_with_contradicted_gate_fail_fails_closed(self):
        """矛盾注入：verdict=fail 但 exit_code=0（门禁未传导退出码）→ 必败。"""
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'cpu_samples': [{'t': 0.1, 'cpu_percent': 5.0}], 'exit_code': 0,"
            "'frozen_gate': {'verdict': 'fail',"
            "'violations': ['frozen_avg_utilization_low: x'], 'reason': None,"
            "'metrics': {}}}))"
        )
        cmd = ["python3", "-c", writer]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)")

    def test_evidence_with_illegal_verdict_fails_closed(self):
        """非法 verdict（伪造值域外）→ FAIL(monitor_gate_missing)。"""
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'cpu_samples': [{'t': 0.1, 'cpu_percent': 90.0}], 'exit_code': 0,"
            "'frozen_gate': {'verdict': 'skip', 'violations': []}}))"
        )
        cmd = ["python3", "-c", writer]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)")

    def test_evidence_without_monitor_samples_fails_closed(self):
        """监控证据存在但无 cpu_samples（非监控证据 JSON）→ FAIL(monitor_gate_missing)。"""
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'summary': 'not a monitor evidence json'}))"
        )
        cmd = ["python3", "-c", writer]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)")

    def test_registered_output_missing_is_existing_guard(self):
        """登记 outputs 缺失（含监控 JSON 未产出）→ 既有 FAIL(missing_output) 防线。"""
        cmd = ["python3", "-c", "print('no evidence at all')"]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL(missing_output)")

    def test_shim_gate_required_end_to_end_not_applicable(self):
        """shim 集成：--gate-required 真跑短命令 → not_applicable（显式分类非豁免）→ PASS。"""
        cmd = MONITOR_CMD_TEMPLATE + ["--gate-required"] + MONITOR_TAIL
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "PASS", json.dumps(per, ensure_ascii=False))
            evidence = H.load_json(repo / MON_JSON)
            gate = evidence.get("frozen_gate")
            self.assertIsInstance(gate, dict, "监控证据必须含 frozen_gate（evaluate 已调用）")
            self.assertEqual(gate["verdict"], "not_applicable")

    def test_shim_gate_fail_propagates_exit_10(self):
        """故障注入：--gate-required + 计算区间 15s + 空转子进程 → 门禁 fail → exit 10 → FAIL。"""
        cmd = MONITOR_CMD_TEMPLATE + [
            "--gate-required", "--gate-compute-interval", "15",
            "--", "python3", "-c", "import time; time.sleep(1.2)",
        ]
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL")
            self.assertEqual(per["exit_code"], 10, "门禁 fail 必须以 RESOURCE 退出码 10 传导")


class TestRunMonitoredGateRequiredFlag(unittest.TestCase):
    """目标 2c：run_monitored --gate-required 旗标语义（库级，最小化时长）。"""

    def test_gate_required_writes_frozen_gate_and_exit_10_on_fail(self):
        """--gate-required + 显式 15s 计算区间 + 空转子进程 → frozen_gate fail + rc 10。"""
        from tools.monitoring import run_monitored as RM
        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "ev.json"
            rc = RM.main(["--timeout", "30", "--output", str(out),
                          "--gate-required", "--gate-compute-interval", "15",
                          "--", sys.executable, "-c", "import time; time.sleep(1.0)"])
            self.assertEqual(rc, 10, "门禁 fail 必须 exit 10（RESOURCE）")
            gate = H.load_json(out)["frozen_gate"]
            self.assertEqual(gate["verdict"], "fail")
            self.assertTrue(gate["violations"])

    def test_gate_required_short_interval_not_applicable(self):
        """短区间（<10s）→ not_applicable（显式分类非豁免），退出码透传子进程。"""
        from tools.monitoring import run_monitored as RM
        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "ev.json"
            rc = RM.main(["--timeout", "30", "--output", str(out),
                          "--gate-required",
                          "--", sys.executable, "-c", "print('ok')"])
            self.assertEqual(rc, 0)
            gate = H.load_json(out)["frozen_gate"]
            self.assertEqual(gate["verdict"], "not_applicable")

    def test_gate_required_mutually_exclusive_with_gate_workers(self):
        """--gate-required 与 --gate-workers 互斥：同给 → 用法错误（exit 2）。"""
        from tools.monitoring import run_monitored as RM
        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "ev.json"
            with self.assertRaises(SystemExit) as ctx:
                RM.main(["--timeout", "30", "--output", str(out),
                         "--gate-required", "--gate-workers", "2",
                         "--", sys.executable, "-c", "print('ok')"])
            self.assertEqual(ctx.exception.code, 2)

    def test_gate_required_host_probe_unavailable_fails_closed(self):
        """host_probe 不可得（proc_root 指向空目录）→ 门禁 fail-closed → exit 10。"""
        from tools.monitoring import run_monitored as RM
        with tempfile.TemporaryDirectory() as td:
            empty = Path(td) / "nonproc"
            empty.mkdir()
            rc = RM.main(["--timeout", "30",
                          "--gate-required", "--gate-compute-interval", "15",
                          "--", sys.executable, "-c", "print('ok')"])
            # main() 无 proc_root 注入口 → 经库函数等价路径验证 fail-closed 分母缺失
            result = RM.run_monitored(
                [sys.executable, "-c", "print('ok')"], timeout=30,
                proc_root=str(empty))
            probe_cpus = (result.get("host_probe") or {}).get("effective_cpu_cores")
            gate = RM.evaluate_frozen_gate(result, effective_cpus=probe_cpus,
                                           allocated_workers=probe_cpus,
                                           compute_interval_seconds=15.0)
            self.assertEqual(gate["verdict"], "fail")
            self.assertTrue(any(v.startswith("monitoring_missing")
                                for v in gate["violations"]))
            self.assertEqual(rc, 10)


class TestFatduckNoCandidateFails(unittest.TestCase):
    """目标 3：候选缺失必须失败——select job 在 rc=3 时 exit 3，不再静默 success。"""

    def test_select_step_exits_3_on_no_candidate(self):
        import re
        yml = (_REPO / ".github" / "workflows" / "fatduck.yml").read_text(encoding="utf-8")
        m = re.search(r"id: sel.*?(?=\s+- name:)", yml, re.S)
        self.assertIsNotNone(m, "fatduck.yml 必须有 id=sel 步骤")
        script = m.group(0)
        rc3 = re.search(r"if \[ \"\$rc\" -eq 3 \]; then(.*?)(fi\b)", script, re.S)
        self.assertIsNotNone(rc3, "sel 脚本必须有 rc=3 分支")
        self.assertIn("exit 3", rc3.group(1),
                      "no-candidate 必须使步骤失败（exit 3，候选缺失必须失败）")
        self.assertNotIn("::notice::no-candidate", script,
                         "no-candidate 不得再以 notice 记为设计内正常路径")

    def test_validate_job_gate_condition_unchanged(self):
        """fatduck-validate 仍仅在 has_candidate=true 时跑（job fail → skip 下游）。"""
        yml = (_REPO / ".github" / "workflows" / "fatduck.yml").read_text(encoding="utf-8")
        self.assertIn(
            "if: needs.select-candidate.outputs.has_candidate == 'true'", yml)


if __name__ == "__main__":
    unittest.main()
