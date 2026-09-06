# -*- coding: utf-8 -*-
"""V8-CI-010 修复轮 3（F-R2-02）：ci/bootstrap.py UTF-8 输出单测。

回归锚点：hosted windows 诊断步在 pwsh 子进程下 cp1252 控制台，
``bootstrap --json`` 报告与 stderr 结构化 FAIL 里的中文 repair 文案
（ensure_ascii=False 非 ASCII）在 strict 编码下 UnicodeEncodeError →
stdout 0B、exit 1、诊断 report/stderr_fail_payload 双 null
（hosted BOOTSTRAP_DIAG.json 实测 + 本地 PYTHONIOENCODING=cp1252 复现）。

修复语义：main() 入口对自身 stdout/stderr reconfigure(encoding="utf-8")，
仅影响 bootstrap 输出流编码，不改探测逻辑与诊断步。

测试策略：
- 子进程级：PYTHONIOENCODING=cp1252 真跑 bootstrap（--json 与 --help 两路），
  断言 stdout/stderr 按 UTF-8 解码即合法 JSON/文本、无 UnicodeEncodeError、
  非 ASCII repair 文案完整保真（修复前此环境 stdout 0B + traceback）；
- in-process 级：以 cp1252 TextIOWrapper 替换 stdio，直接调 main()，
  断言 reconfigure 后输出字节流按 UTF-8 可解码、编码确为 utf-8。

仅 stdlib；子进程带 timeout。
"""
from __future__ import annotations

import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

BOOTSTRAP = _REPO / "ci" / "bootstrap.py"
POLICY = _REPO / "ci" / "toolchain.policy.json"

# 本地非 hosted：linux 节必然受控 FAIL（runner/gcc-14/... 缺失路径）
# → stderr 走中文 repair 文案，正是修复前的崩溃面
_CP1252_ENV = dict(os.environ, PYTHONIOENCODING="cp1252")


class TestBootstrapUtf8UnderCp1252(unittest.TestCase):
    """子进程级：cp1252 stdio 下 stdout/stderr 必须 UTF-8 完整输出。"""

    def test_json_report_and_stderr_payload_survive_cp1252(self):
        with tempfile.TemporaryDirectory() as td:
            out_path = Path(td) / "out.json"
            err_path = Path(td) / "err.json"
            with open(out_path, "wb") as fo, open(err_path, "wb") as fe:
                proc = subprocess.run(
                    [sys.executable, str(BOOTSTRAP), "--policy", str(POLICY),
                     "--platform", "linux", "--json"],
                    timeout=60, env=_CP1252_ENV,
                    stdout=fo, stderr=fe)
            # 受控失败 exit 2（本地预期），绝不允许编码崩溃 exit 1
            self.assertEqual(proc.returncode, 2, err_path.read_bytes()[-500:])
            blob = out_path.read_bytes()
            self.assertTrue(blob, "stdout 0B（F-R2-02 修复前症状）")
            self.assertNotIn(b"UnicodeEncodeError", blob + err_path.read_bytes())
            # stdout 完整 JSON 报告（UTF-8 解码即成）
            report = json.loads(blob.decode("utf-8"))
            self.assertEqual(report["task_id"], "V8-CI-007")
            self.assertEqual(report["platform"], "linux")
            self.assertFalse(report["ok"])
            self.assertEqual(len(report["items"]), 6)
            # stderr 结构化 FAIL payload：UTF-8 解码 + 中文 repair 文案保真
            err_text = err_path.read_bytes().decode("utf-8")
            payload = json.loads(err_text)
            self.assertEqual(payload["verdict"], "FAIL")
            repairs = " ".join(e["repair"] for e in payload["failed_tools"])
            # runner/architecture 为探测宿主自身属性（本地必 FAIL），文案非环境依赖
            self.assertIn("非 GitHub hosted", repairs)
            # 工具链缺失文案是环境依赖面：hosted 预装 gcc-14/clang-18/cmake/ninja，
            # dev 机装齐后 failed_tools 仅剩 runner 项 → 按宿主实况动态选择断言
            tool_failures = [e for e in payload["failed_tools"]
                             if e["tool"] not in ("runner", "architecture")]
            if tool_failures:
                # 宿主存在工具缺失项：探测缺失文案（"…不在 PATH…"族，
                # 覆盖 gcc-14/clang-18「不在 PATH 或版本不可解析」、
                # cmake「不在 PATH 或版本 < x」、ninja「不在 PATH」变体）必须保真
                for e in tool_failures:
                    self.assertIn("不在 PATH", e["repair"], e)
            else:
                # 宿主工具装齐：failed_tools 非空且每项 repair 非空字符串
                # （核心意图不变：CP1252 下中文 repair payload 不失真）
                self.assertTrue(payload["failed_tools"])
                for e in payload["failed_tools"]:
                    self.assertIsInstance(e["repair"], str)
                    self.assertTrue(e["repair"], e)

    def test_help_text_survives_cp1252(self):
        with tempfile.TemporaryDirectory() as td:
            out_path = Path(td) / "out.txt"
            with open(out_path, "wb") as fo:
                proc = subprocess.run(
                    [sys.executable, str(BOOTSTRAP), "--help"],
                    timeout=60, env=_CP1252_ENV,
                    stdout=fo, stderr=subprocess.PIPE)
            self.assertEqual(proc.returncode, 0, proc.stderr[-500:])
            text = out_path.read_bytes().decode("utf-8")
            self.assertIn("hosted runner 工具链 bootstrap 门禁", text)


class TestBootstrapInProcessReconfigure(unittest.TestCase):
    """in-process：cp1252 流被 main() 入口 reconfigure 为 utf-8。"""

    def _run_with_cp1252_stdio(self, argv):
        out_buf = io.TextIOWrapper(io.BytesIO(), encoding="cp1252")
        err_buf = io.TextIOWrapper(io.BytesIO(), encoding="cp1252")
        real_out, real_err = sys.stdout, sys.stderr
        sys.stdout, sys.stderr = out_buf, err_buf
        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location(
                "bootstrap_under_test", BOOTSTRAP)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            code = mod.main(list(argv))
        finally:
            sys.stdout, sys.stderr = real_out, real_err
        return code, out_buf, err_buf

    def test_main_reconfigures_cp1252_streams_to_utf8(self):
        code, out_buf, err_buf = self._run_with_cp1252_stdio(
            ["--policy", str(POLICY), "--platform", "linux", "--json"])
        self.assertEqual(code, 2)
        out_buf.flush()   # TextIOWrapper 缓冲落盘后再读底层字节流
        err_buf.flush()
        # 编码已被 reconfigure 为 utf-8，中文 repair 文案不再崩
        self.assertEqual(out_buf.encoding, "utf-8")
        self.assertEqual(err_buf.encoding, "utf-8")
        report = json.loads(out_buf.buffer.getvalue().decode("utf-8"))
        self.assertEqual(report["task_id"], "V8-CI-007")
        payload = json.loads(err_buf.buffer.getvalue().decode("utf-8"))
        self.assertEqual(payload["verdict"], "FAIL")

    def test_reconfigure_failure_keeps_probe_flow(self):
        """流无 reconfigure（AttributeError 面）时探测流程不受影响。"""
        class NoReconfigure:
            encoding = "cp1252"
            buffer = io.BytesIO()

            def write(self, s):
                self._acc = getattr(self, "_acc", "") + s

            def flush(self):
                pass

            def getvalue(self):
                return getattr(self, "_acc", "")
        fake_out, fake_err = NoReconfigure(), NoReconfigure()
        real_out, real_err = sys.stdout, sys.stderr
        sys.stdout, sys.stderr = fake_out, fake_err
        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location(
                "bootstrap_under_test2", BOOTSTRAP)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            code = mod.main(["--policy", str(POLICY),
                             "--platform", "linux", "--json"])
        finally:
            sys.stdout, sys.stderr = real_out, real_err
        self.assertEqual(code, 2)
        self.assertIn("V8-CI-007", fake_out.getvalue())


if __name__ == "__main__":
    unittest.main()
