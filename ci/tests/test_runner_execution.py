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

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

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
            # 结构断言对齐 ci/ci_result.schema.json（不 import jsonschema）
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
    """

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

    CREATE = ["python3", "-c", "open('X.txt','w').write('x')"]

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

    def test_mutating_check_exempt_from_dirty(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            # 单独一组：mutates_workspace=true 的同类命令不触发 dirty，verdict PASS
            H.write_registry(repo, [H.check(id="CHK-MUT", command=self.CREATE,
                                            mutates_workspace=True)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "CHK-MUT")
            self.assertEqual(per["verdict"], "PASS")
            self.assertFalse(per["dirty"]["checked"])
            self.assertEqual(per["dirty"]["violations"], [])
            self.assertTrue((repo / "X.txt").is_file())

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


if __name__ == "__main__":
    unittest.main()
