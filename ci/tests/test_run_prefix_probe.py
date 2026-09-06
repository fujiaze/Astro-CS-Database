# -*- coding: utf-8 -*-
"""V8-CI-010 F-R3-03 单测：probe_prerequisite 对 run/ 前缀参数的豁免。

背景：ci/checks.json 中 BUILD-GCC-RELEASE / DEEP-* 检查的 command 携带
``--build-dir run/ci/...``、``--output run/ci/...`` 等仓库相对参数；其中
部分（如 BUILD-GCC-RELEASE 的 build 目录）不在 outputs 登记。路径探测
循环把它们当作"command 引用的仓库静态输入"做存在性检查，而 run/ 是
运行时产物目录（AGENTS.md 临时操作目录，首轮运行时才生成）→
BUILD-GCC-RELEASE 与 DEEP-* 持续误 SKIP。

修复语义：仓库相对路径以 ``run/`` 开头的参数一律跳过探测（与 outputs
排除同语义）；其他探测语义不变。

覆盖三层：
1. 主仓库真实登记（6 个 command 含 run/ 前缀参数的检查）probe 全通过；
2. fixture 仓库端到端：command 含 run/ 前缀参数的检查真跑 PASS
   （修复前同类参数会被预检拒为 FAIL(prerequisite)/SKIPPED）；
3. 回归保护：非 run/ 前缀且不存在的仓库路径仍被拒（不放宽过头）。

fixture 全部落 tempfile；只读主仓库资产，不修改任何被检文件。
"""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

CI_DIR = Path(__file__).resolve().parents[1]
REPO = CI_DIR.parent
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

from ci.tests import _helpers as H  # noqa: E402
from ci import run as ci_run  # noqa: E402

REGISTRY = json.loads((CI_DIR / "checks.json").read_text(encoding="utf-8"))
BY_ID = {c["id"]: c for c in REGISTRY["checks"]}

# command 中携带 run/ 前缀仓库相对参数的检查（F-R3-03 受害者全集；
# DEEP-COMPLEXITY 的 run/ 路径已在 outputs，列在此处一并守护）。
RUN_PREFIX_IDS = ("BUILD-GCC-RELEASE", "DEEP-CLANG-BUILD", "DEEP-SAN-ASAN",
                  "DEEP-SAN-TSAN", "DEEP-COV-CPP", "DEEP-COV-PY",
                  "DEEP-COMPLEXITY")


def _command_has_run_prefix_arg(check: dict) -> bool:
    return any(arg == "run" or arg.startswith(("run/", "run\\"))
               for arg in check["command"][1:])


class TestRunPrefixProbe(unittest.TestCase):
    """run/ 前缀参数不再触发"仓库路径不存在"预检拒绝。"""

    def test_registry_targets_carry_run_prefix_args(self):
        # 前置自洽：受试 id 确实携带 run/ 前缀参数（注册表漂移时让本组
        # 测试先失语，避免假绿）。
        for cid in RUN_PREFIX_IDS:
            self.assertIn(cid, BY_ID, cid)
            self.assertTrue(_command_has_run_prefix_arg(BY_ID[cid]), cid)

    def test_probe_passes_for_real_registry_checks(self):
        # 主仓库真实登记：mock which 放行 prerequisite_tools 探测后，
        # run/ 前缀参数不应再产生任何 probe 拒绝理由。
        for cid in RUN_PREFIX_IDS:
            check = BY_ID[cid]
            with mock.patch.object(ci_run.shutil, "which",
                                   return_value="/usr/bin/fake"):
                ok, reason = ci_run.probe_prerequisite(check, REPO,
                                                       check["platform"])
            self.assertTrue(ok, f"{cid}: {reason}")
            self.assertIsNone(reason, cid)

    def test_run_prefixed_build_dir_check_executes_end_to_end(self):
        # fixture 端到端：command 引用一个不存在且未登记 outputs 的
        # run/ 前缀路径 → 修复前 FAIL(prerequisite)（仓库路径不存在），
        # 修复后探测跳过、检查真跑 PASS。waivable=False 使误判表现为
        # 硬失败，PASS 判定更锋利。
        check = H.check(
            id="RUN-PREFIX-T",
            command=["python3", "-c", "print('ok')",
                     "--build-dir", "run/ci/ut-run-prefix-build"],
            outputs=[],
            waivable=False,
        )
        self.assertTrue(_command_has_run_prefix_arg(check))
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            H.write_registry(repo, [check])
            H.write_ci_result_schema(repo)
            out = Path(td) / "out"
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out)],
                                repo, timeout=150)
            self.assertEqual(proc.returncode, 0, proc.stderr[-400:])
            entry = H.load_check_result(out, "RUN-PREFIX-T")
            self.assertEqual(entry["verdict"], "PASS", entry.get("reason"))
            self.assertNotIn("prerequisite", entry.get("reason", "") or "")

    def test_non_run_missing_path_still_rejected(self):
        # 回归保护：run/ 之外的仓库相对路径探测语义不变——不存在的
        # 静态输入仍被拒（不可 waivable → FAIL(prerequisite)）。
        check = H.check(
            id="STATIC-MISSING-T",
            command=["python3", "config/definitely-missing-input.yaml",
                     "-c", "print('never')"],
            waivable=False,
        )
        with tempfile.TemporaryDirectory() as td:
            repo = H.make_repo(Path(td) / "repo")
            H.write_registry(repo, [check])
            H.write_ci_result_schema(repo)
            out = Path(td) / "out"
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out)],
                                repo, timeout=150)
            self.assertNotEqual(proc.returncode, 0)
            entry = H.load_check_result(out, "STATIC-MISSING-T")
            self.assertEqual(entry["verdict"], "FAIL(prerequisite)")
            self.assertIn("仓库路径不存在", entry.get("reason", ""))


if __name__ == "__main__":
    unittest.main()
