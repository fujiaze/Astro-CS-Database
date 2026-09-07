# -*- coding: utf-8 -*-
"""V8-CIQA-001 P2-GAP-4 回归：空 outputs 检查的第二重内容级验证。

背景（CIQA-ATT-011）：fast profile 57 项中 56 项 outputs 为空，此前 runner 对
"exit 0 且登记 outputs 为空" 的检查无任何内容级锚——检查脚本自身静默失败
（exit 0 且 stdout/stderr 全空）会被记 PASS。修复：非 waivable 且空 outputs
的检查，exit 0 时要求 stdout/stderr 至少留痕，否则 FAIL(empty_outputs)
（并入 HARD_FAILURE_VERDICTS，known_failures 可基线化）。

豁免白名单语义：登记 waivable=true 即显式声明允许静默（架构上空 outputs 合理
的检查走此通道）；outputs 非空的检查以产物存在性为锚，不受本防线影响。

全离线：tempfile 仓库 + 真实 runner 进程，验证 verdict/exit code/fail_detail。
"""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import _helpers as H


def _run_fast(repo: Path, checks: list[dict], out_name: str):
    out_root = Path(out_name)
    H.write_registry(repo, checks)
    H.write_ci_result_schema(repo)
    proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
    return proc, out_root


class TestEmptyOutputsGuard(unittest.TestCase):
    """ATT-011 fixture 重放：静默失败必须不再 PASS。"""

    SILENT = ["python3", "-c", "import sys; sys.exit(0)"]   # 全空输出
    CHATTY = ["python3", "-c", "print('ok')"]               # stdout 留痕

    def test_silent_exit0_empty_outputs_is_fail(self):
        """空 outputs + exit 0 + stdout/stderr 全空 -> FAIL(empty_outputs)，总 FAIL。"""
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            proc, out_root = _run_fast(
                repo,
                [H.check(id="GAP4-SILENT", command=self.SILENT)],
                Path(td) / "out")
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "GAP4-SILENT")
            self.assertEqual(per["verdict"], "FAIL(empty_outputs)")
            self.assertEqual(per["exit_code"], 0)  # 命令本身 exit 0
            self.assertIn("stdout/stderr", per["reason"] or "")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")   # 不再 PASS（ATT-011 修复）
            self.assertEqual(ci["summary"]["fail_detail"].get("FAIL(empty_outputs)"),
                             1)
            # 并入硬失败值域：known_failures 比较路径可见（HARD_FAILURE_VERDICTS）
            import importlib.util
            spec = importlib.util.spec_from_file_location(
                "astrocs_run_mod", H.RUNNER)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            self.assertIn("FAIL(empty_outputs)", mod.HARD_FAILURE_VERDICTS)

    def test_chatty_exit0_empty_outputs_is_pass(self):
        """空 outputs 但 stdout 留痕 -> PASS（最低限度内容级锚满足）。"""
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            proc, out_root = _run_fast(
                repo,
                [H.check(id="GAP4-CHATTY", command=self.CHATTY)],
                Path(td) / "out")
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "GAP4-CHATTY")
            self.assertEqual(per["verdict"], "PASS")

    def test_stderr_trail_also_counts(self):
        """仅 stderr 留痕（如进度/告警输出）同样满足内容级锚。"""
        cmd = ["python3", "-c",
               "import sys; sys.stderr.write('probe: cleanup done\\n')"]
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            proc, out_root = _run_fast(
                repo,
                [H.check(id="GAP4-STDERR", command=cmd)],
                Path(td) / "out")
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "GAP4-STDERR")
            self.assertEqual(per["verdict"], "PASS")

    def test_waivable_empty_outputs_exempt(self):
        """豁免白名单：waivable=true 显式登记允许静默 -> PASS。"""
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            proc, out_root = _run_fast(
                repo,
                [H.check(id="GAP4-WAIV", command=self.SILENT, waivable=True)],
                Path(td) / "out")
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "GAP4-WAIV")
            self.assertEqual(per["verdict"], "PASS")

    def test_outputs_present_silent_command_still_pass(self):
        """outputs 非空且产物存在：以产物为锚，静默命令不受防线影响。"""
        cmd = ["python3", "-c", "open('probe.txt','w').write('x')"]
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            proc, out_root = _run_fast(
                repo,
                [H.check(id="GAP4-PROD", command=cmd, outputs=["probe.txt"])],
                Path(td) / "out")
            self.assertEqual(proc.returncode, 0, proc.stderr)
            per = H.load_check_result(out_root, "GAP4-PROD")
            self.assertEqual(per["verdict"], "PASS")

    def test_exempt_registry_ids_are_silence_exempt(self):
        """显式豁免白名单：EMPTY_OUTPUT_SILENCE_EXEMPT 内 id 静默成功仍 PASS。

        背景：API-DOCS/UNIT-CLOSURE 为既有诚实静默成功脚本（直跑 rc=0 无输出，
        见 run/local/agent_ciqa_p2/gap4_waiver_decision.md），按任务语义登记豁免。
        本用例用同 id fixture 验证豁免语义而非真实脚本（离线、不依赖 tools/）。
        """
        import importlib.util
        spec = importlib.util.spec_from_file_location("astrocs_run_mod2", H.RUNNER)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        exempt_ids = sorted(mod.EMPTY_OUTPUT_SILENCE_EXEMPT)
        self.assertIn("API-DOCS", exempt_ids)
        self.assertIn("UNIT-CLOSURE", exempt_ids)
        # silent_failure 单元级：豁免 id 全空输出不判静默失败
        for cid in ("API-DOCS", "UNIT-CLOSURE"):
            self.assertFalse(mod.silent_failure(H.check(id=cid), "", ""))
        # 非豁免 id 仍判静默失败（防线对白名单外关闭豁免）
        self.assertTrue(mod.silent_failure(H.check(id="OTHER"), "", ""))


if __name__ == "__main__":
    unittest.main()
