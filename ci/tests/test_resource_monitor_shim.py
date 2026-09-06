# -*- coding: utf-8 -*-
"""V8-CI-010 修复轮 3（F-R2-01）：resource_monitor shim runpy 桥接链单测。

回归锚点：hosted WIN-BUILD-RELEASE / WIN-TEST-UNIT 经
``ci/resource_monitor.py``（runpy.run_path 桥接 tools/monitoring/run_monitored.py）
执行时 sys.path 断裂 —— shim 自身 sys.path[0]=ci/，runpy 不做"目标脚本目录
入 sys.path"注入，run_monitored 的同目录导入（fallback ``import
resource_probe``）与 repo 根包导入双双 ModuleNotFoundError → 凡经 shim 包装
的检查必败（linux 控制节点同命令可复现）。

本组测试锁定修复语义：
1. 包装执行与直接执行 ``python3 tools/monitoring/run_monitored.py`` 的
   sys.path[0] / argv[0] 逐项一致（等价性，用真子进程取基线）；
2. run_monitored 同目录模块导入在包装链内可解析；
3. 退出码透传、无参能力 JSON、target 存在性；
4. in-process 调用后 sys.path 无残留（不污染调用方）。

仅 stdlib；全部外部命令经被测 shim 自带的 timeout 保护。
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))


def _load_shim():
    spec = importlib.util.spec_from_file_location(
        "resource_monitor_shim", _REPO / "ci" / "resource_monitor.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


SHIM = _load_shim()
MONITOR_DIR = _REPO / "tools" / "monitoring"

# 包装执行内的探测脚本：打印 sys.path[0] 与 argv[0]（与"直接执行"基线可比）
PATH_PROBE_SRC = textwrap.dedent("""
    import json, sys
    print(json.dumps({"path0": sys.path[0], "argv0": sys.argv[0]}))
""")

# 同目录导入探测脚本：复现 run_monitored.py 顶部 try/except 导入形态
PAIR_PROBE_SRC = textwrap.dedent("""
    import json, sys
    result = {"relative": False, "direct": False}
    try:
        from pair_mod import PAIR_OK  # noqa: F401
        result["relative"] = True
    except ImportError:
        pass
    try:
        import pair_mod  # noqa: F401
        result["direct"] = True
    except ImportError:
        pass
    print(json.dumps(result))
""")


def _run_shim_capture(args: list[str]):
    """in-process 调 shim main() 并捕获其 stdout（monitor JSON 打印到 stdout）。"""
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = SHIM.main(list(args))
    return code, buf.getvalue()


def _monitor_result_of(stdout_text: str) -> dict:
    """从 shim stdout 解析被包装命令的结果（run_monitored 最后整行 JSON）。"""
    for line in reversed(stdout_text.splitlines()):
        line = line.strip()
        if line.startswith("{"):
            obj = json.loads(line)
            if "command" in obj and "exit_code" in obj:
                return obj
    raise AssertionError(f"monitor 结果 JSON 未找到：{stdout_text[:200]!r}")


class TestShimSysPathSemantics(unittest.TestCase):
    """核心回归：包装执行与直接执行 sys.path/argv 语义一致（F-R2-01）。"""

    def test_wrapped_path0_and_argv0_match_direct_execution(self):
        import subprocess
        with tempfile.TemporaryDirectory() as td:
            wrapper = Path(td) / "path_probe.py"
            wrapper.write_text(PATH_PROBE_SRC, encoding="utf-8")
            # 基线：直接执行 python3 <script>（解释器脚本模式）
            direct = subprocess.run(
                [sys.executable, str(wrapper)],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(direct.returncode, 0, direct.stderr[-400:])
            baseline = json.loads(direct.stdout.strip().splitlines()[-1])
            # 包装执行：经 shim runpy 桥接运行同一脚本
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                code = SHIM.main(
                    ["--timeout", "60", "--", sys.executable, str(wrapper)])
            self.assertEqual(code, 0, buf.getvalue()[-400:])
            wrapped = json.loads(_monitor_result_of(buf.getvalue())
                                 ["stdout_tail"][-1])
            # 逐项一致：sys.path[0] = 目标脚本所在目录（最小修目标语义）、
            # argv[0] = 目标脚本路径
            self.assertEqual(baseline["path0"], str(wrapper.parent))
            self.assertEqual(wrapped["path0"], baseline["path0"])
            self.assertEqual(wrapped["argv0"].replace("\\", "/"),
                             baseline["argv0"].replace("\\", "/"))

    def test_same_dir_import_resolves_inside_wrapped_chain(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td) / "samedir_probe"
            base.mkdir()
            (base / "pair_mod.py").write_text("PAIR_OK = 1\n", encoding="utf-8")
            (base / "probe.py").write_text(PAIR_PROBE_SRC, encoding="utf-8")
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                code = SHIM.main(["--timeout", "60", "--", sys.executable,
                                  str(base / "probe.py")])
            self.assertEqual(code, 0, buf.getvalue()[-400:])
            tail = json.loads(_monitor_result_of(buf.getvalue())["stdout_tail"][-1])
            self.assertEqual(tail, {"relative": True, "direct": True})

    def test_sys_path_restored_after_in_process_call(self):
        before = list(sys.path)
        code, _ = _run_shim_capture(["--", sys.executable, "-c", "pass"])
        self.assertEqual(code, 0)
        self.assertEqual(sys.path, before, "shim 不得向调用方泄漏 sys.path 条目")

    def test_no_args_capability_does_not_pollute_sys_path(self):
        before = list(sys.path)
        code, out = _run_shim_capture([])
        self.assertEqual(code, 0)
        cap = json.loads(out)
        self.assertEqual(cap["script"], "ci/resource_monitor.py")
        self.assertEqual(cap["wrapped_target"],
                         "tools/monitoring/run_monitored.py")
        self.assertTrue(cap["target_exists"])
        self.assertEqual(sys.path, before)


class TestShimBridgeBasics(unittest.TestCase):
    """桥接基本语义：target 存在、退出码透传（能力 JSON 已在上组覆盖）。"""

    def test_monitored_target_exists(self):
        self.assertTrue(MONITOR_DIR.is_dir())
        self.assertTrue((MONITOR_DIR / "run_monitored.py").is_file())

    def test_exit_code_passthrough(self):
        code, _ = _run_shim_capture(
            ["--timeout", "60", "--", sys.executable, "-c",
             "raise SystemExit(3)"])
        self.assertEqual(code, 3)

    def test_wrapped_import_failure_now_resolves(self):
        # 回归核心：修复前 fallback import resource_probe 双败 exit 1；
        # 修复后 run_monitored.py 自身可被包装执行（--help 走 argparse exit 0）
        code, out = _run_shim_capture(
            ["--timeout", "60", "--", sys.executable,
             str(MONITOR_DIR / "run_monitored.py"), "--help"])
        self.assertEqual(code, 0, out[-400:])
        result = _monitor_result_of(out)
        self.assertEqual(result["exit_code"], 0)
        self.assertTrue(any("usage:" in ln for ln in result["stdout_tail"]),
                        result["stdout_tail"][:3])


if __name__ == "__main__":
    unittest.main()
