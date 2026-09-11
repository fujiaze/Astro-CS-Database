# -*- coding: utf-8 -*-
"""CI-001 单测：收紧 CI 注册表与资源 fail-closed（owner=CI-001，ASTROCS_CONSTITUTION_ALIGNMENT_CONTROL_V1）。

控制包 02_GATES_AND_EXECUTION.md §执行："P0/P1、Windows build/test/package、
manifest/hash、真实数据完整性不可 waiver"；任务 CI-001 目标三句：

1. WIN-BUILD-RELEASE / WIN-TEST-UNIT / WIN-PACKAGE-CANDIDATE 不可 waiver
   （waivable=false：prerequisite 不满足 → FAIL(prerequisite)，不再 SKIPPED）；
2. 监控必须调用 evaluate 的 CI 落地面（F-CI-002-04/06 owner 裁决原则
   一致化应用, 2026-09-11）：资源门冻结语义（§10.5/§18.2）针对重计算区间，
   构建/打包/单测为非重计算面——CI 注册表当前零 --gate-required（WIN-* 另
   requires_monitor=false，Linux BUILD-GCC-RELEASE+DEEP-* 仅采样留证）；
   ci/run.py 合同收窄为"命令请求了判定就必须兑现判定证据"
   （FAIL(monitor_gate_missing)），未请求判定的监控检查不强制 frozen_gate；
   资源门真正应用面 = REAL-001 真实数据终验重计算与本地 heavy 重计算
   （run_monitored evaluate_frozen_gate 能力不动）；
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

# owner 裁决（CI-001 复核, 2026-09-11）排除面：构建/打包/单测非重计算面，
# 不请求资源门判定（--gate-required）；waivable=false 收紧维持。
_WIN_IDS = ("WIN-BUILD-RELEASE", "WIN-TEST-UNIT", "WIN-PACKAGE-CANDIDATE")

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
    """目标 2a：注册表资源门口径（owner 裁决原则一致化应用, 2026-09-11）。

    §10.5/§18.2 资源门冻结语义针对重计算区间；构建/打包/单测为非重计算面
    （F-CI-002-04/06）：CI 注册表当前零 --gate-required——Linux 重计算检查
    （BUILD-GCC-RELEASE + DEEP-*）仅采样留证（requires_monitor=true），
    WIN-* 另按裁决 requires_monitor=false。资源门真正应用面 = REAL-001
    真实数据终验重计算与本地 heavy 重计算（run_monitored evaluate 能力不动）。
    """

    def test_registry_gate_flags_removed_by_owner_ruling(self):
        """裁决一致化：全部 requires_monitor 检查命令均不含 --gate-required。"""
        reg = _by_id(_load_registry())
        monitored = [c for c in reg.values() if c.get("requires_monitor")]
        self.assertGreaterEqual(len(monitored), 6, "注册表 requires_monitor 检查数")
        for c in monitored:
            cmd = c["command"]
            self.assertIn("ci/resource_monitor.py", cmd,
                          f"{c['id']} requires_monitor 必须携带监控包装器")
            head = cmd[:cmd.index("--")] if "--" in cmd else cmd
            self.assertNotIn("--gate-required", head,
                             f"{c['id']} 非重计算面不得请求资源门判定（owner 裁决一致化 2026-09-11）")
            self.assertNotIn("--gate-workers", head,
                             f"{c['id']} 不得经 --gate-workers 请求判定（owner 裁决一致化）")

    def test_win_checks_excluded_from_gate_by_owner_ruling(self):
        """owner 裁决（2026-09-11）：WIN-* 为非重计算面，requires_monitor=false。

        依据：F-CI-002-04——包装器仅采样留证无 evaluate，requires_monitor=true
        会触发 run.py monitor_gate_missing 硬失败（Windows 证据无 frozen_gate）；
        R7（heavy→monitor）conform 同步 heavy=false。waivable=false 收紧维持。
        """
        reg = _by_id(_load_registry())
        for cid in _WIN_IDS:
            c = reg[cid]
            self.assertNotIn("--gate-required", c["command"],
                             f"{cid} 非重计算面，不得请求资源门判定（owner 裁决）")
            self.assertFalse(c["waivable"],
                             f"{cid} 不可 waiver 收紧维持不变（owner 裁决）")
            self.assertFalse(c["requires_monitor"],
                             f"{cid} requires_monitor 必须 false（owner 裁决 F-CI-002-04）")
            self.assertFalse(c["heavy"],
                             f"{cid} heavy 必须 false（R7 conform：heavy→monitor）")
            self.assertIn("ci/resource_monitor.py", c["command"],
                          f"{cid} 仍保留监控包装（采样留证）")

    def test_linux_heavy_checks_keep_monitor_sampling(self):
        """BUILD-GCC-RELEASE + DEEP-* 保留监控包装与 requires_monitor=true（采样留证）。"""
        reg = _by_id(_load_registry())
        for cid in ("BUILD-GCC-RELEASE", "DEEP-CLANG-BUILD", "DEEP-SAN-ASAN",
                    "DEEP-SAN-TSAN", "DEEP-COV-CPP", "DEEP-COV-PY"):
            c = reg[cid]
            self.assertTrue(c["requires_monitor"], f"{cid} 采样留证保留")
            self.assertTrue(c["heavy"], f"{cid} heavy 登记不变")
            self.assertIn("ci/resource_monitor.py", c["command"], cid)

    def test_ut_quality_registration_conformance(self):
        """UT-QUALITY 登记矛盾修正：命令无监控包装 → heavy/requires_monitor 均 false。"""
        reg = _by_id(_load_registry())
        c = reg["UT-QUALITY"]
        self.assertNotIn("ci/resource_monitor.py", c["command"],
                         "前提：UT-QUALITY 命令无监控包装器")
        self.assertFalse(c["heavy"], "UT-QUALITY heavy 必须 conform 为 false")
        self.assertFalse(c["requires_monitor"], "UT-QUALITY requires_monitor 必须 conform 为 false")


class TestRunnerGateEvidenceContract(unittest.TestCase):
    """目标 2b：ci/run.py 监控证据合同（请求判定才校验 frozen_gate）。

    F-CI-002-04/06 收窄（owner 裁决原则一致化应用, 2026-09-11）：合同从
    "requires_monitor 必须判定"收窄为"命令请求了判定（--gate-required/
    --gate-workers）就必须兑现判定证据"；未请求判定的监控检查（纯采样留证）
    不强制 frozen_gate。注入形态：真 shim --gate-required 包装，真证据落
    outputs 之外的 RAW 路径，被监控子命令写 outputs 登记的 MON_JSON（伪造面）
    ——校验按 outputs 定位到伪造证据。
    """

    RAW_MON_JSON = "run/ci/monitor/CHK-MON-raw.json"

    @classmethod
    def _mon_check(cls, command: list[str], outputs=None) -> dict:
        # 命令含 RAW --output（_gate_cmd 形态）时两份都登记：RAW 供校验与
        # dirty 豁免（run 产物），MON_JSON 是注入面；否则只登记 MON_JSON。
        if outputs is None:
            outputs = ([MON_JSON, cls.RAW_MON_JSON]
                       if cls.RAW_MON_JSON in command else [MON_JSON])
        return H.check(id="CHK-MON", command=command, outputs=outputs,
                       requires_monitor=True, heavy=True)

    @classmethod
    def _gate_cmd(cls, writer: str) -> list[str]:
        """请求判定形态：真 shim --gate-required；真证据落 RAW（outputs 外）。"""
        return (["python3", "ci/resource_monitor.py", "--timeout", "30",
                 "--output", cls.RAW_MON_JSON, "--gate-required", "--",
                 "python3", "-c", writer])

    def test_evidence_without_frozen_gate_fails_closed(self):
        """故障注入：请求判定但监控证据无 frozen_gate → FAIL(monitor_gate_missing)。"""
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'cpu_samples': [{'t': 0.1, 'cpu_percent': 90.0}], 'exit_code': 0}))"
        )
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(self._gate_cmd(writer)))
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
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(self._gate_cmd(writer)))
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
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(self._gate_cmd(writer)))
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
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(self._gate_cmd(writer)))
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "FAIL(monitor_gate_missing)")

    def test_evidence_without_monitor_samples_fails_closed(self):
        """outputs 定位面无监控证据 JSON（无 cpu_samples）→ FAIL(monitor_gate_missing)。

        RAW 真证据不登记 outputs 且以 dirty_ignore_exact 显式豁免（run 产物
        非工作区漂移）：校验定位面只剩伪造 JSON → 监控证据缺失 fail-closed。
        """
        writer = (
            "import json,pathlib;"
            f"pathlib.Path({MON_JSON!r}).parent.mkdir(parents=True, exist_ok=True);"
            f"pathlib.Path({MON_JSON!r}).write_text(json.dumps("
            "{'summary': 'not a monitor evidence json'}))"
        )
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            check = self._mon_check(self._gate_cmd(writer), outputs=[MON_JSON])
            check["dirty_ignore_exact"] = [self.RAW_MON_JSON]
            proc = _runner_mon_check(repo, out_root, check)
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

    def test_monitored_without_gate_request_skips_evidence_contract(self):
        """F-CI-002-06 收窄正向：未请求判定的监控检查（纯采样留证）不强制 frozen_gate。

        命令无 --gate-required/--gate-workers（裁决一致化后的 CI 注册表面），
        监控证据无 frozen_gate → PASS（不触发 FAIL(monitor_gate_missing)）。
        """
        cmd = MONITOR_CMD_TEMPLATE + MONITOR_TAIL
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            proc = _runner_mon_check(repo, out_root, self._mon_check(cmd))
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MON")
            self.assertEqual(per["verdict"], "PASS")
            evidence = H.load_json(repo / MON_JSON)
            self.assertNotIn("frozen_gate", evidence,
                             "未请求判定时监控器不产出 frozen_gate（能力保留不触发）")

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
