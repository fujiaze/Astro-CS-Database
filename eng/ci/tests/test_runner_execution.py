# -*- coding: utf-8 -*-
"""V8-CI-002 单测：单检查执行语义（PASS / FAIL / TIMEOUT / SIGNAL / dirty / 日志捕获）。

对应场景：4 PASS 路径 + CI_RESULT schema 结构、5 FAIL 路径、6 TIMEOUT、
7 SIGNAL、8 工作区纯净性（dirty 检查与 mutates_workspace 对照）、9 日志捕获。
"""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]
_ENG = Path(__file__).resolve().parents[2]
if str(_ENG) not in sys.path:
    sys.path.insert(0, str(_ENG))

from ci.tests import _helpers as H  # noqa: E402


class TestPassPath(unittest.TestCase):
    """场景 4：命令 exit 0 → PASS；CI_RESULT.json 满足 ci_result.schema.json 结构。"""

    def test_pass_verdict_and_ci_result_structure(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-PASS",
                                            command=["python3", "-c", "print('ok')"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)

            per = H.load_check_result(out_root, "CHK-PASS")
            self.assertEqual(per["verdict"], "PASS")
            self.assertEqual(per["exit_code"], 0)
            self.assertIsNone(per["signal"])
            self.assertFalse(per["timed_out"])
            self.assertIsInstance(per["duration_seconds"], (int, float))
            self.assertTrue(per["started_utc"].endswith("Z"))

            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "PASS")
            self.assertEqual(ci["summary"]["pass"], 1)
            self.assertEqual(ci["summary"]["fail"], 0)
            self.assertEqual(ci["summary"]["known_fail"], 0)
            # 结构断言对齐 eng/ci/ci_result.schema.json（不 import jsonschema）
            self.assertEqual(ci["schema_version"], 1)
            self.assertIn(ci["profile"], ("fast", "linux-main", "linux-deep",
                                          "windows-main", "fatduck"))
            self.assertRegex(ci["source_sha"], r"^[0-9a-f]{40}$")
            self.assertTrue(ci["started_utc"] and ci["finished_utc"])
            self.assertIsInstance(ci["checks"], list)
            for entry in ci["checks"]:
                for key in ("id", "exit_code", "duration_seconds", "verdict"):
                    self.assertIn(key, entry)
            self.assertEqual(ci["checks"][0]["verdict"], "PASS")
            self.assertEqual(ci["checks"][0]["id"], "CHK-PASS")
            self.assertEqual(ci["source_sha"],
                             H.sh(["git", "rev-parse", "HEAD"], cwd=repo).stdout.strip())


class TestFailPath(unittest.TestCase):
    """场景 5：命令 exit 3 → FAIL，CI 退出码 1，summary 计数分离。"""

    def test_nonzero_exit_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-FAIL3",
                                            command=["python3", "-c", "raise SystemExit(3)"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, f"FAIL 必须 exit 1：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-FAIL3")
            self.assertEqual(per["verdict"], "FAIL")
            self.assertEqual(per["exit_code"], 3)
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertGreaterEqual(ci["summary"]["fail"], 1)
            self.assertEqual(ci["summary"]["fail_detail"].get("FAIL"), 1)
            self.assertEqual(ci["summary"]["pass"], 0)


class TestTimeout(unittest.TestCase):
    """场景 6：超过登记 timeout_seconds → TIMEOUT，CI 退出码 1（外层 150s 不触发）。"""

    def test_timeout_kills_process(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-SLOW",
                                            command=["python3", "-c", "import time; time.sleep(30)"],
                                            timeout_seconds=1)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, f"TIMEOUT 必须 exit 1：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-SLOW")
            self.assertEqual(per["verdict"], "TIMEOUT")
            self.assertTrue(per["timed_out"])
            self.assertLess(per["duration_seconds"], 20,
                            "检查超时必须在登记 timeout 量级被终止，而非外层 timeout")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["fail_detail"].get("TIMEOUT"), 1)


class TestSignal(unittest.TestCase):
    """场景 7：进程被信号终止（SIGTERM）→ verdict SIGNAL。

    run.py 实现：Popen 未 setpgid，子进程自杀 SIGTERM 后 returncode=-15；
    -15 仅在非 timeout 路径判为 SIGNAL（源码 execute_check verdict 顺序）。

    平台边界（GATE-TRIAGE-01）：本场景的判据是 **POSIX 信号语义**——Windows 上
    os.kill(pid, SIGTERM) 走 TerminateProcess，returncode 是正的退出码而**不是**
    -15，"被信号终止"这一事实在 Windows 进程 API 上不可表达。原实现无条件断言
    returncode == -15 ⇒ CHK-CI-CONTRACT-SELFTESTS 在 Windows 上必红且是假红
    （被测对象没有缺陷，是断言绑了平台）。故 Windows 上显式跳过并给出理由；
    verdict 顺序逻辑本身由 Linux 档（linux-main / fast）持续覆盖。
    """

    @unittest.skipIf(sys.platform.startswith("win"),
                     "POSIX 信号语义：Windows 上进程无负 returncode，SIGNAL 不可表达")
    def test_sigterm_yields_signal_verdict(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            code = "import os, signal; os.kill(os.getpid(), signal.SIGTERM)"
            H.write_registry(repo, [H.check(id="CHK-SIG",
                                            command=["python3", "-c", code],
                                            timeout_seconds=30)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, f"SIGNAL 必须 exit 1：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-SIG")
            self.assertEqual(per["verdict"], "SIGNAL")
            self.assertEqual(per["signal"], 15)
            self.assertFalse(per["timed_out"])
            self.assertEqual(per["exit_code"], -15)
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["fail_detail"].get("SIGNAL"), 1)


class TestWorkspacePurity(unittest.TestCase):
    """场景 8：mutates_workspace=false 的检查产生未跟踪文件 → FAIL(dirty)；
    mutates_workspace=true 同类命令不触发 dirty；纯净组保持 PASS。"""

    # V8-CIQA-001 P2-GAP-4：检查命令须留 stdout 痕迹（空 outputs + 全空输出
    # 会被 runner 判 FAIL(empty_outputs)），dirty 豁免语义不受影响。
    CREATE = ["python3", "-c", "open('X.txt','w').write('x');print('X.txt written')"]

    def test_dirty_violation_for_non_mutating_check(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-DIRTY", command=self.CREATE,
                                            mutates_workspace=False)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, f"dirty 必须 exit 1：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-DIRTY")
            self.assertEqual(per["verdict"], "FAIL(dirty)")
            self.assertEqual(per["exit_code"], 0)  # 命令本身成功，违规在纯净性
            self.assertTrue(per["dirty"]["checked"])
            self.assertIn("X.txt", per["dirty"]["violations"])
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["fail_detail"].get("FAIL(dirty)"), 1)

    def test_mutating_check_dirty_scope_is_declared_surface(self):
        """§9.3：mutates_workspace=true 的可写面 = 登记 outputs ∪ dirty_ignore_*，
        **不再**自我豁免——越界写仍 FAIL(dirty)；写在登记 outputs 内才 PASS。

        权威依据 docs/ci/CI_SPEC.md §9.3（「mutates_workspace（真强制，不改名）」）：
        「mutates_workspace: true 的执行单元可写面 = 登记 outputs ∪ dirty_ignore_exact/
        dirty_ignore_prefixes；**不再**无条件跳过执行前后的工作区对比（旧行为是
        自我豁免）。写出可写面之外的任何路径 ⇒ FAIL(dirty)（与 mutates_workspace:
        false 同判据）」。旧断言（dirty.checked=False + PASS）正是被废止的自我豁免
        语义，会把真违规判绿。同口径注册门 = CHK-GATE-FAILCLOSED-SELFTEST 的
        test_F（越界写判红）/ test_G（只写登记 outputs 判绿）。
        """
        # (1) 可写面为空（outputs=[]）→ 写 X.txt 越界 → FAIL(dirty)
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-MUT", command=self.CREATE,
                                            mutates_workspace=True)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, f"越界写必须 exit 1：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-MUT")
            self.assertEqual(per["verdict"], "FAIL(dirty)")
            self.assertTrue(per["dirty"]["checked"],
                            "mutates_workspace=true 同样做工作区对比（无自我豁免）")
            self.assertIn("X.txt", per["dirty"]["violations"])
            self.assertEqual(per["exit_code"], 0, "命令本身成功，违规在纯净性")
            self.assertTrue((repo / "X.txt").is_file())
        # (2) 同一命令把 X.txt 登记进可写面 → PASS（豁免来自登记面，不是自我豁免）
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-MUT2", command=self.CREATE,
                                            mutates_workspace=True, outputs=["X.txt"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MUT2")
            self.assertEqual(per["verdict"], "PASS")
            self.assertTrue(per["dirty"]["checked"])
            self.assertEqual(per["dirty"]["violations"], [])

    def test_clean_check_passes_untouched_workspace(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-CLEAN",
                                            command=["python3", "-c", "print('clean')"],
                                            mutates_workspace=False)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-CLEAN")
            self.assertEqual(per["verdict"], "PASS")
            self.assertTrue(per["dirty"]["checked"])
            self.assertEqual(per["dirty"]["violations"], [])


class TestLogCapture(unittest.TestCase):
    """场景 9：命令输出（含 stderr）捕获到 out_root/logs/<check-id>.log。

    run.py 实现写 logs/<check-id>.log（zstd 附加 .log.zst 且保留原始 .log，
    本机 zstd 探测已确认源文件保留）。
    """

    def test_stderr_captured_into_log_file(self):
        marker = "boom-to-stderr-V8CI002"
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(
                id="CHK-LOG",
                command=["python3", "-c",
                         f"import sys; print('out-line'); print({marker!r}, file=sys.stderr)"],
            )])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-LOG")
            self.assertEqual(per["verdict"], "PASS")
            log_rel = per.get("log")
            self.assertTrue(log_rel, "per-check 结果必须记录 log 路径")
            log_abs = out_root / log_rel if not Path(log_rel).is_absolute() else Path(log_rel)
            self.assertTrue(log_abs.is_file(), f"日志文件缺失：{log_abs}")
            self.assertEqual(Path(log_rel).as_posix(), "logs/CHK-LOG.log")
            text = log_abs.read_text(encoding="utf-8")
            self.assertIn(marker, text)
            self.assertIn("out-line", text)
            self.assertIn("===== STDERR =====", text)
            self.assertIn(marker, per["stderr_tail"])


class TestCrashTristate(unittest.TestCase):
    """GATE-TRIAGE-01：三态（PASS/FAIL/CRASH）+ 崩溃独立退出码（run.py 面）。

    判据：检查器抛未捕获异常 ⇒ 它**没有给 verdict**（门崩），必须记 CRASH 且
    runner 退出码 = 3；检查器给出结构化 FAIL 判词（即使输出里有 Traceback，
    例如 unittest 失败报告）⇒ 仍记 FAIL、退出码 1（不得把真判红误报成门崩）。
    """

    def _run(self, code: str):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        root = Path(td.name)
        repo = H.make_repo(root / "repo")
        out_root = root / "out"
        H.write_registry(repo, [H.check(id="CHK-TRISTATE",
                                        command=[sys.executable, "-c", code],
                                        timeout_seconds=30)])
        H.write_ci_result_schema(repo)
        proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
        return proc, H.load_check_result(out_root, "CHK-TRISTATE")

    def test_uncaught_exception_is_crash_with_exit_code_3(self):
        code = ("def boom():\n"
                "    raise ValueError('selftest injected crash')\n"
                "boom()\n")
        proc, per = self._run(code)
        self.assertEqual(per["verdict"], "CRASH",
                         "门崩必须记 CRASH（无 verdict），不得与判红同码")
        self.assertEqual(proc.returncode, 3, f"崩溃必须换独立退出码 3：{proc.stderr}")
        # 判词必须带「文件:行」且崩溃证据不截断（栈帧是第一现场）
        self.assertIn("line", per["reason"], per["reason"])
        self.assertIn("selftest injected crash", per["stderr_tail"])

    def test_fail_with_traceback_stays_fail(self):
        code = ("import sys, traceback\n"
                "try:\n"
                "    raise AssertionError('assertion')\n"
                "except AssertionError:\n"
                "    traceback.print_exc()\n"
                "print('SELFTEST_TOOL_FAIL: verdict=FAIL')\n"
                "sys.exit(1)\n")
        proc, per = self._run(code)
        self.assertEqual(per["verdict"], "FAIL",
                         "有结构化 verdict 的判红不得被误报成门崩")
        self.assertEqual(proc.returncode, 1)

    def test_green_stays_pass(self):
        proc, per = self._run("print('SELFTEST_TOOL_PASS: verdict=PASS')\n")
        self.assertEqual(per["verdict"], "PASS")
        self.assertEqual(proc.returncode, 0)


if __name__ == "__main__":
    unittest.main()
