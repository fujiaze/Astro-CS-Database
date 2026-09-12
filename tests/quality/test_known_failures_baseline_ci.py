# -*- coding: utf-8 -*-
"""CI-BASELINE-001 单测：known-failures 基线机器化门（05_FINDINGS_REGISTER §STD-F9）。

被测面：tools/quality/known_failures_baseline.py（--mode verify / --mode check /
--selftest）与版本化基线 ci/known_failures.json。

契约（07_CI_MACHINE_CONTRACT §已有失败基线）：
  新失败（不在基线）→ FAIL；基线项失败 → 全绿（KNOWN）；
  基线项 expected=fail 却已通过 → FAIL（修复后必须删除基线项）；
  条目过期 → FAIL；结构非法 / 永不允许豁免类别 → FAIL。

用例（先红后绿：每条负例都断言 exit 1 且给出定位串）：
  T1  真实仓库版本化基线 verify → PASS（现场漂移即红）
  T2  verify 负例：缺 first_seen_commit / 缺 owner / 未知 unit / 重复登记
  T3  verify 负例：07 合同「永不允许豁免」类别（数据损坏/ABI/追踪断裂）入库
  T4  verify 负例：条目过期
  T5  verify 负例：expected=conditional 但无 activation
  T6  check 绿：失败集 ⊆ 基线（基线项失败全绿）
  T7  check 红：构造一个不在基线的假失败 → FAIL（负向注入必败）
  T8  check 红：基线项 expected=fail 但本次未失败 → FAIL
  T9  check 红：结果来源缺失（fail-closed，不得静默跳过）
  T10 JUnit 解析语义：failure / skipped / pass
  T11 legacy 模式零回归：40 项 findings 报告结构不变、rc=0
  T12 CI 面 per-check 结果目录来源（含 ASTROCS_CI_OUT_ROOT 缺省形态）
  T13 注册表漂移锚：两门登记形态、末位次序、JUnit 路径一致性、基线 unit 可追溯
  T14 环境隔离守卫（FD-R1-018）：父进程带 ASTROCS_CI_OUT_ROOT 时断言不受污染

环境无关性（FD-R1-018，前台轮末 CI 取证裁定）：ci/run.py 会给**每个**检查注入
ASTROCS_CI_OUT_ROOT（本次改动引入），本文件在 CI 内运行时该变量必然存在；若用例
让被测命令走「无显式来源 → 读 env 指向的真实 per-check 结果」回退路径，断言会被
真实红灯（AGENTS-GOV/CON-COMMENTS…）污染而崩。故 run_tool 一律在子进程 env 中
剔除 ASTROCS_CI_OUT_ROOT，check 用例显式传 --ci-checks-dir/--ci-result；
仅 T12c 以显式 env 覆盖形态验证回退路径本身，T14 固化该隔离性质。
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools" / "quality" / "known_failures_baseline.py"
BASELINE = REPO / "ci" / "known_failures.json"
SCHEMA_ID = "astrocs.known-failures-baseline/v2"
SHA40 = "0" * 40

sys.path.insert(0, str(REPO / "tools" / "quality"))
import known_failures_baseline as K  # noqa: E402


def entry(**overrides) -> dict:
    """合法基线条目（fixture 默认值；unit 取真实已登记 CTest 目标）。"""
    base = {
        "check_id": "p1_noise",
        "unit": "p1_noise",
        "kind": "ctest",
        "category": "TOOLING_DRIFT",
        "owner": "SA-CI",
        "reason": "fixture：既有失败项",
        "first_seen_commit": "f8778bbbf010e92f49670dd9d2fbef7fe98d6731",
        "source_sha": "f8778bbbf010e92f49670dd9d2fbef7fe98d6731",
        "reproducer": "python3 tools/quality/known_failures_baseline.py --selftest",
        "expiry": "2999-01-01T00:00:00Z",
        "expected": "fail",
        "removal_condition": "fixture 永不移除",
        "registered_by": "tests/quality/test_known_failures_baseline_ci.py",
    }
    base.update(overrides)
    return base


def write_baseline(path: Path, entries: list) -> Path:
    path.write_text(json.dumps({"schema": SCHEMA_ID, "failures": entries},
                               ensure_ascii=False, indent=1), encoding="utf-8")
    return path


def write_junit(path: Path, cases: list) -> Path:
    """cases: [(name, 'pass'|'fail'|'skip')]"""
    body = []
    for name, status in cases:
        if status == "fail":
            body.append('  <testcase name="%s" classname="c" status="run">'
                        '<failure message="x"/></testcase>' % name)
        elif status == "skip":
            body.append('  <testcase name="%s" classname="c" status="run">'
                        '<skipped/></testcase>' % name)
        else:
            body.append('  <testcase name="%s" classname="c" status="run"/>' % name)
    xml = ('<?xml version="1.0" encoding="UTF-8"?>\n<testsuite name="CTest" tests="%d">\n'
           % len(cases)) + "\n".join(body) + "\n</testsuite>\n"
    path.write_text(xml, encoding="utf-8")
    return path


def clean_env(**overrides) -> dict:
    """CI 环境无关的子进程 env（FD-R1-018）。

    ci/run.py 对每个检查注入 ASTROCS_CI_OUT_ROOT（= 本次 run 证据根），CI 内
    该变量必然存在；被测工具的 --mode check 在该变量存在且未显式指定来源时会
    回退读取 <out_root>/checks 的**真实** per-check 结果。用例必须隔离该回退，
    否则断言结果取决于 CI 现场的成败集合（本地绿、CI 红）。
    """
    env = {k: v for k, v in os.environ.items() if k != "ASTROCS_CI_OUT_ROOT"}
    env.update(overrides)
    return env


def run_tool(args: list, timeout: int = 300,
             env_overrides: dict | None = None) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(TOOL), *args], cwd=str(REPO),
                          capture_output=True, text=True, timeout=timeout,
                          env=clean_env(**(env_overrides or {})))


class TestBaselineVerify(unittest.TestCase):
    """T1–T5：版本化基线静态校验（结构 / 类别 / 过期 / 条件登记）。"""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="kfbase_")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)

    def test_t1_real_repo_baseline_verifies(self):
        """T1：真实版本化基线 verify → rc=0（现场漂移即红）。"""
        proc = run_tool(["--mode", "verify", "--repo", str(REPO)])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        report = json.loads(proc.stdout)
        self.assertEqual(report["verdict"], "PASS")
        self.assertGreaterEqual(report["entries"], 2)
        units = {d["unit"] for d in report["entry_detail"]}
        self.assertIn("p1_noise_adapter", units)   # F-AIO-001 显式登记
        self.assertIn("UT-CLI", units)             # UT-CLI 修复前遗留显式登记
        for d in report["entry_detail"]:
            self.assertEqual(d["first_seen_reachability"], "reachable",
                             "首次登记 commit 必须可达：%s" % d)

    def test_t2_structure_negative(self):
        """T2：缺字段 / 未知 unit / 重复登记 → rc=1。"""
        cases = {
            "missing_first_seen_commit": [dict(entry(), **{})],
            "missing_owner": [entry()],
            "unknown_unit": [entry(unit="ghost_target", check_id="ghost_target")],
            "duplicate": [entry(), entry()],
        }
        cases["missing_first_seen_commit"][0].pop("first_seen_commit")
        cases["missing_owner"][0].pop("owner")
        for name, entries in cases.items():
            with self.subTest(case=name):
                path = write_baseline(self.tmp / (name + ".json"), entries)
                proc = run_tool(["--mode", "verify", "--repo", str(REPO),
                                 "--baseline", str(path)])
                self.assertEqual(proc.returncode, 1, name + " 必须 FAIL")
                report = json.loads(proc.stdout)
                self.assertEqual(report["verdict"], "FAIL")

    def test_t3_never_waivable_category_rejected(self):
        """T3：07 合同「永不允许豁免」类别入库 → rc=1（逐类别）。"""
        for category in ("DATA_CORRUPTION", "ABI", "TRACEABILITY_BREAK",
                         "HEAVY_UTILIZATION", "LEAK", "CRASH"):
            with self.subTest(category=category):
                path = write_baseline(self.tmp / (category + ".json"),
                                      [entry(category=category)])
                proc = run_tool(["--mode", "verify", "--repo", str(REPO),
                                 "--baseline", str(path)])
                self.assertEqual(proc.returncode, 1, category + " 不得入库")
                self.assertIn("永不允许豁免", proc.stdout)

    def test_t3b_unknown_category_rejected(self):
        """T3b：词表外类别（白名单封闭）→ rc=1。"""
        path = write_baseline(self.tmp / "unknown.json", [entry(category="WHATEVER")])
        proc = run_tool(["--mode", "verify", "--repo", str(REPO), "--baseline", str(path)])
        self.assertEqual(proc.returncode, 1)
        self.assertIn("category 越界", proc.stdout)

    def test_t4_expired_entry_fails(self):
        """T4：条目过期 → rc=1（到期必须重登记或删除）。"""
        path = write_baseline(self.tmp / "expired.json",
                              [entry(expiry="2000-01-01T00:00:00Z")])
        proc = run_tool(["--mode", "verify", "--repo", str(REPO), "--baseline", str(path)])
        self.assertEqual(proc.returncode, 1)
        self.assertIn("已过期", proc.stdout)

    def test_t5_conditional_requires_activation(self):
        """T5：expected=conditional 无 activation → rc=1（防无界豁免）。"""
        path = write_baseline(self.tmp / "cond.json", [entry(expected="conditional")])
        proc = run_tool(["--mode", "verify", "--repo", str(REPO), "--baseline", str(path)])
        self.assertEqual(proc.returncode, 1)
        self.assertIn("activation", proc.stdout)


class TestBaselineCheck(unittest.TestCase):
    """T6–T9：失败集 ⊆ 基线 动态判定（含负向注入必败）。"""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="kfcheck_")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)

    def test_t6_baseline_failure_is_green(self):
        """T6：基线项失败（精确匹配、未过期）→ 全绿，且不得误判 stale。"""
        base = write_baseline(self.tmp / "b.json", [entry(unit="p1_noise",
                                                          check_id="p1_noise")])
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "fail"),
                                                 ("p1wcs_units", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO),
                         "--baseline", str(base), "--ctest-junit", str(junit)])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        report = json.loads(proc.stdout)
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["known"], ["ctest:p1_noise"])
        self.assertEqual(report["new_failures"], [])
        self.assertEqual(report["stale"], [])

    def test_t7_fake_failure_not_in_baseline_fails(self):
        """T7：构造一个不在基线的假失败 → rc=1（负向注入必败）。"""
        base = write_baseline(self.tmp / "b.json", [entry(unit="p1_noise",
                                                          check_id="p1_noise")])
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "fail"),
                                                 ("fake_regression_target", "fail")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO),
                         "--baseline", str(base), "--ctest-junit", str(junit)])
        self.assertEqual(proc.returncode, 1, "不在基线的失败必须 FAIL")
        report = json.loads(proc.stdout)
        self.assertEqual(report["verdict"], "FAIL")
        self.assertEqual(report["new_failures"], ["ctest:fake_regression_target"])
        self.assertEqual(report["known"], ["ctest:p1_noise"])

    def test_t7b_check_kind_fake_failure_fails(self):
        """T7b：检查面（check kind）假失败同样必败（CI_RESULT 来源）。"""
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        result = self.tmp / "CI_RESULT.json"
        result.write_text(json.dumps({"checks": [
            {"id": "UT-CLI", "verdict": "FAIL(dirty)"},
            {"id": "NEW-REGRESSION", "verdict": "FAIL"},
            {"id": "VERSION-CONSISTENCY", "verdict": "PASS"},
        ]}), encoding="utf-8")
        proc = run_tool(["--mode", "check", "--repo", str(REPO),
                         "--baseline", str(base), "--ci-result", str(result)])
        self.assertEqual(proc.returncode, 1)
        report = json.loads(proc.stdout)
        self.assertEqual(report["known"], ["check:UT-CLI"])
        self.assertEqual(report["new_failures"], ["check:NEW-REGRESSION"])

    def test_t8_stale_entry_fails(self):
        """T8：基线项 expected=fail 但本次未失败 → rc=1（修复后必须删除）。"""
        base = write_baseline(self.tmp / "b.json", [entry(unit="p1_noise",
                                                          check_id="p1_noise")])
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO),
                         "--baseline", str(base), "--ctest-junit", str(junit)])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(json.loads(proc.stdout)["stale"], ["ctest:p1_noise"])

    def test_t8b_conditional_absent_is_green(self):
        """T8b：expected=conditional 条目未出现 → 全绿（F-AIO-001 门卫关闭态）。"""
        base = write_baseline(self.tmp / "b.json", [entry(
            unit="p1_noise_adapter", check_id="p1_noise_adapter",
            expected="conditional", activation="门卫 target 激活时才注册该目标")])
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO),
                         "--baseline", str(base), "--ctest-junit", str(junit)])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def test_t9_missing_source_fails_closed(self):
        """T9：基线含 check 条目但未提供 --ci-result → rc=1（fail-closed）。"""
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO),
                         "--baseline", str(base), "--ctest-junit", str(junit)])
        self.assertEqual(proc.returncode, 1, "结果来源缺失必须 FAIL 而非静默跳过")
        self.assertEqual(json.loads(proc.stdout)["unevaluated"], ["check:UT-CLI"])

    def test_t9b_missing_junit_fails_closed(self):
        """T9b：结果文件缺失/损坏 → rc=1。"""
        base = write_baseline(self.tmp / "b.json", [entry()])
        proc = run_tool(["--mode", "check", "--repo", str(REPO), "--baseline", str(base),
                         "--ctest-junit", str(self.tmp / "nope.xml")])
        self.assertEqual(proc.returncode, 1)
        self.assertIn("不存在", proc.stdout)


class TestChecksDirSource(unittest.TestCase):
    """T12：CI 面 per-check 结果目录来源（run 内增量落盘）+ ASTROCS_CI_OUT_ROOT 缺省。"""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="kfdir_")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)

    def _write_check(self, checks_dir: Path, cid: str, verdict: str) -> None:
        checks_dir.mkdir(parents=True, exist_ok=True)
        (checks_dir / (cid + ".json")).write_text(
            json.dumps({"id": cid, "verdict": verdict, "exit_code": 0}),
            encoding="utf-8")

    def test_t12a_checks_dir_source(self):
        """T12a：--ci-checks-dir 提供检查面失败集；基线项失败全绿。"""
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        checks_dir = self.tmp / "checks"
        self._write_check(checks_dir, "UT-CLI", "FAIL(dirty)")
        self._write_check(checks_dir, "VERSION-CONSISTENCY", "PASS")
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO), "--baseline", str(base),
                         "--ctest-junit", str(junit), "--ci-checks-dir", str(checks_dir)])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        report = json.loads(proc.stdout)
        self.assertEqual(report["known"], ["check:UT-CLI"])
        self.assertEqual(report["new_failures"], [])

    def test_t12b_checks_dir_new_failure_fails(self):
        """T12b：目录中出现不在基线的失败 → rc=1。"""
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        checks_dir = self.tmp / "checks"
        self._write_check(checks_dir, "UT-CLI", "FAIL(dirty)")
        self._write_check(checks_dir, "AGENTS-GOV", "FAIL")
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO), "--baseline", str(base),
                         "--ctest-junit", str(junit), "--ci-checks-dir", str(checks_dir)])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(json.loads(proc.stdout)["new_failures"], ["check:AGENTS-GOV"])

    def test_t12c_env_out_root_default(self):
        """T12c：未给来源旗标时取 ASTROCS_CI_OUT_ROOT/checks（ci/run.py 注入形态）。"""
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        out_root = self.tmp / "run-out"
        self._write_check(out_root / "checks", "UT-CLI", "FAIL(dirty)")
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        # FD-R1-018：显式覆盖形态（非 dict(**os.environ, KEY=...)——CI 内
        # ASTROCS_CI_OUT_ROOT 已由 ci/run.py 注入，关键字重复即 TypeError）。
        env = {**os.environ, "ASTROCS_CI_OUT_ROOT": str(out_root)}
        proc = subprocess.run(
            [sys.executable, str(TOOL), "--mode", "check", "--repo", str(REPO),
             "--baseline", str(base), "--ctest-junit", str(junit)],
            cwd=str(REPO), capture_output=True, text=True, timeout=300, env=env)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        report = json.loads(proc.stdout)
        self.assertEqual(report["sources"]["ci_checks_dir"], str(out_root / "checks"))

    def test_t12e_known_fail_verdict_still_counts_as_failure(self):
        """T12e：读已结束 run 的证据（verdict=KNOWN_FAIL）与读 run 内增量结果等价。

        ci/run.py 对已登记基线项失败记 KNOWN_FAIL（计数分离，仍属失败面）；
        若把 KNOWN_FAIL 排除出失败集，重跑门会把基线项误判为 stale。
        """
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        checks_dir = self.tmp / "checks"
        self._write_check(checks_dir, "UT-CLI", "KNOWN_FAIL")
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO), "--baseline", str(base),
                         "--ctest-junit", str(junit), "--ci-checks-dir", str(checks_dir)])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        report = json.loads(proc.stdout)
        self.assertEqual(report["known"], ["check:UT-CLI"])
        self.assertEqual(report["stale"], [])

    def test_t12d_empty_dir_fails_closed(self):
        """T12d：目录存在但零有效结果 → rc=1（fail-closed）。"""
        base = write_baseline(self.tmp / "b.json",
                              [entry(kind="check", unit="UT-CLI", check_id="UT-CLI")])
        empty = self.tmp / "checks"
        empty.mkdir()
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "pass")])
        proc = run_tool(["--mode", "check", "--repo", str(REPO), "--baseline", str(base),
                         "--ctest-junit", str(junit), "--ci-checks-dir", str(empty)])
        self.assertEqual(proc.returncode, 1)

    def test_t14_ci_env_var_does_not_leak_into_isolated_cases(self):
        """T14（FD-R1-018 守卫）：父进程带 ASTROCS_CI_OUT_ROOT 时断言不被污染。

        复现 CI 现场条件：env 指向含真实红灯（AGENTS-GOV FAIL）的 per-check
        目录。若被测命令未显式指定来源，它会回退读该目录 → 与 T7/T9 期望
        不符（这正是 778fe98e 上 UT-QUALITY 4F+1E 的根因）。本用例断言：
        显式 --ci-checks-dir 的隔离来源优先，且 run_tool 已剔除该 env。
        """
        polluted = self.tmp / "ci-out-root"
        self._write_check(polluted / "checks", "AGENTS-GOV", "FAIL")
        self._write_check(polluted / "checks", "CON-COMMENTS", "FAIL")
        isolated = self.tmp / "isolated-checks"
        self._write_check(isolated, "VERSION-CONSISTENCY", "PASS")
        base = write_baseline(self.tmp / "b.json", [entry(unit="p1_noise",
                                                          check_id="p1_noise")])
        junit = write_junit(self.tmp / "r.xml", [("p1_noise", "fail"),
                                                 ("fake_regression_target", "fail")])
        # 父进程 env 带污染变量（模拟 CI）；显式隔离来源必须胜出
        polluted_env = {**os.environ, "ASTROCS_CI_OUT_ROOT": str(polluted)}
        proc = subprocess.run(
            [sys.executable, str(TOOL), "--mode", "check", "--repo", str(REPO),
             "--baseline", str(base), "--ctest-junit", str(junit),
             "--ci-checks-dir", str(isolated)],
            cwd=str(REPO), capture_output=True, text=True, timeout=300, env=polluted_env)
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        report = json.loads(proc.stdout)
        self.assertEqual(report["new_failures"], ["ctest:fake_regression_target"],
                         "隔离来源必须胜出：不得读入 env 指向的真实红灯")
        self.assertEqual(report["sources"]["ci_checks_dir"], str(isolated))
        # run_tool 的默认隔离：无来源旗标 + 父 env 带污染变量 → 不走 env 回退
        proc = run_tool(["--mode", "check", "--repo", str(REPO), "--baseline", str(base),
                         "--ctest-junit", str(junit)])
        self.assertEqual(proc.returncode, 1)
        report = json.loads(proc.stdout)
        self.assertIsNone(report["sources"]["ci_checks_dir"],
                          "run_tool 必须剔除 ASTROCS_CI_OUT_ROOT（环境无关）")
        self.assertEqual(report["new_failures"], ["ctest:fake_regression_target"])


class TestJunitParsing(unittest.TestCase):
    """T10：JUnit 解析语义（failure / skipped / pass）。"""

    def test_t10_status_mapping(self):
        with tempfile.TemporaryDirectory(prefix="kfjunit_") as td:
            path = write_junit(Path(td) / "r.xml", [("a", "fail"), ("b", "skip"),
                                                    ("c", "pass")])
            statuses, errors = K.parse_ctest_junit(path)
            self.assertEqual(errors, [])
            self.assertEqual(statuses, {"a": "fail", "b": "skip", "c": "pass"})

    def test_t10b_empty_or_broken_is_error(self):
        with tempfile.TemporaryDirectory(prefix="kfjunit_") as td:
            empty = Path(td) / "empty.xml"
            empty.write_text('<?xml version="1.0"?><testsuite name="CTest"/>',
                             encoding="utf-8")
            statuses, errors = K.parse_ctest_junit(empty)
            self.assertEqual(statuses, {})
            self.assertTrue(errors, "零 testcase 必须报错（fail-closed）")


class TestRegistryWiring(unittest.TestCase):
    """T13：CI 注册表漂移锚——两门登记形态、次序与 ctest 结果路径一致性。"""

    @classmethod
    def setUpClass(cls):
        cls.registry = json.loads((REPO / "ci" / "checks.json").read_text(encoding="utf-8"))
        cls.by_id = {c["id"]: c for c in cls.registry["checks"]}

    def test_t13a_verify_registered_non_waivable(self):
        """T13a：KNOWN-FAILURES-BASELINE-VERIFY 三 profile、不可豁免、输出落 run/。"""
        check = self.by_id.get("KNOWN-FAILURES-BASELINE-VERIFY")
        self.assertIsNotNone(check, "verify 门必须登记")
        self.assertEqual(sorted(check["profiles"]), ["fast", "linux-main", "windows-main"])
        self.assertFalse(check["waivable"])
        self.assertFalse(check["mutates_workspace"])
        self.assertIn("--mode", check["command"])
        self.assertEqual(check["command"][check["command"].index("--mode") + 1], "verify")
        for out in check["outputs"]:
            self.assertTrue(out.startswith("run/"), "运行产物必须落 run/（AGENTS.md 目录规范）")

    def test_t13b_check_gate_is_last_linux_main_entry(self):
        """T13b：CHECK 门必须排在 linux-main 选中序末位（它读同 run 上游结果）。"""
        check = self.by_id.get("KNOWN-FAILURES-BASELINE-CHECK")
        self.assertIsNotNone(check, "check 门必须登记")
        self.assertEqual(check["profiles"], ["linux-main"])
        self.assertFalse(check["waivable"])
        linux_main = [c["id"] for c in self.registry["checks"]
                      if "linux-main" in c["profiles"]]
        self.assertEqual(linux_main[-1], "KNOWN-FAILURES-BASELINE-CHECK",
                         "聚合型检查必须末位：否则读不到上游 per-check 结果")

    def test_t13c_ctest_junit_path_matches_full_run(self):
        """T13c：CHECK 门读的 JUnit 路径 == CTEST-LINUX-FULL 的 --junit 落点。"""
        full = self.by_id["CTEST-LINUX-FULL"]
        build_dir = full["command"][full["command"].index("--build-dir") + 1]
        junit = full["command"][full["command"].index("--junit") + 1]
        expected = "%s/%s" % (build_dir, junit)
        gate = self.by_id["KNOWN-FAILURES-BASELINE-CHECK"]["command"]
        actual = gate[gate.index("--ctest-junit") + 1]
        self.assertEqual(actual, expected,
                         "基线门与全量 ctest 门的 JUnit 路径必须一致（漂移即红）")
        self.assertIn(expected, full["outputs"], "JUnit 必须登记为 CTEST-LINUX-FULL 的 outputs")

    def test_t13d_baseline_units_map_to_registered_targets(self):
        """T13d：版本化基线的 unit 必须可追溯到注册表/CTest 冻结清单。"""
        data = json.loads(BASELINE.read_text(encoding="utf-8"))
        ids = set(self.by_id)
        ctest_targets = set(json.loads(
            (REPO / "ci" / "ctest_baseline.json").read_text(encoding="utf-8"))["targets"])
        for c in self.by_id.values():
            ctest_targets |= set(c.get("ctest_targets", []))
        for item in data["failures"]:
            self.assertIn(item["kind"], ("check", "ctest"))
            pool = ids if item["kind"] == "check" else ctest_targets
            self.assertIn(item["unit"], pool, item["unit"])
            self.assertEqual(item["check_id"], item["unit"])


class TestSelftestAndLegacy(unittest.TestCase):
    """T11：--selftest 全过；legacy 模式行为零回归。"""

    def test_t11a_selftest_passes(self):
        proc = run_tool(["--selftest"])
        self.assertEqual(proc.returncode, 0, proc.stdout[-2000:])
        report = json.loads(proc.stdout)
        self.assertEqual(report["failed"], 0)
        self.assertGreaterEqual(len(report["cases"]), 14)

    def test_t11b_legacy_mode_unchanged(self):
        """T11b：legacy 默认模式仍产出 40 项 findings 报告（R0-004 行为不变）。"""
        with tempfile.TemporaryDirectory(prefix="kflegacy_") as td:
            out = Path(td) / "KNOWN_FAILURES_BASELINE.json"
            proc = run_tool(["--repo", str(REPO), "--output", str(out)])
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertIn("KNOWN_FAILURES_BASELINE findings=40", proc.stdout)
            report = json.loads(out.read_text(encoding="utf-8"))
            self.assertEqual(report["schema"], "astrocs.known-failures-baseline/v1")
            self.assertEqual(report["finding_count"], 40)
            self.assertEqual(len(report["findings"]), 40)
            for finding in report["findings"]:
                for field in ("finding_id", "severity", "status",
                              "minimal_command", "evidence", "note"):
                    self.assertIn(field, finding)


if __name__ == "__main__":
    unittest.main()
