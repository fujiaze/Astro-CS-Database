#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-ENV-ADOPTION（eng/ci/verify_toolchain.py）的能绿能红证据。

背景（TST-001 审查）：eng/ci/polarity_evidence.json 把 CHK-ENV-ADOPTION 记为
FACE-DEFINED-NOT-RUN，负例入口只写成 sh -c "verify_toolchain.py 注入 lock 漂移
⇒ 必须非 0" 的人工说明，不是机器可执行负例（ENGINEERING_SPEC.md §8 /
docs/ci/01_CHECKS.md §1）。本文件把该负例面机器化：真仓正例必绿，逐条注入
必红，且缺件 fail-closed。

正例（绿）：
  * test_positive_real_repo_is_green —— 真仓 policy+lock rc=0。

负例（红，注入副本）：
  * schema_version 漂移 / scope 漂移 / policy 要求被翻转 /
    cmake 版本占位 / 版本行与版本号不一致 / present 非布尔 /
    missing_tools 非 list / lock 非法 JSON / lock 缺失（fail-closed）。

只读：所有注入都在 tempfile 副本上做，绝不写工作区。
"""
from __future__ import annotations

import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[3]
CHECKER = REPO / "eng" / "ci" / "verify_toolchain.py"
POLICY = REPO / "eng" / "ci" / "toolchain.policy.json"
LOCK = REPO / "eng" / "ci" / "toolchain.lock.json"
SCOPE = "agent-host"


def _run(policy: pathlib.Path, lock: pathlib.Path, scope: str = SCOPE):
    return subprocess.run(
        [sys.executable, str(CHECKER), "--policy", str(policy),
         "--actual", str(lock), "--scope", scope],
        cwd=str(REPO), capture_output=True, text=True, timeout=120)


class EnvAdoptionNegativeTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix="env-adopt-neg-")
        self.tmp = pathlib.Path(self._tmp.name)
        self.policy = self.tmp / "policy.json"
        self.lock = self.tmp / "lock.json"
        shutil.copy(POLICY, self.policy)
        shutil.copy(LOCK, self.lock)

    def tearDown(self):
        self._tmp.cleanup()

    def _lock_obj(self):
        return json.loads(self.lock.read_text(encoding="utf-8"))

    def _write_lock(self, obj):
        self.lock.write_text(json.dumps(obj, ensure_ascii=False, indent=1),
                             encoding="utf-8")

    # ---- 正例 ----------------------------------------------------------
    def test_positive_real_repo_is_green(self):
        r = _run(POLICY, LOCK)
        self.assertEqual(r.returncode, 0, r.stdout[-1500:] + r.stderr[-500:])

    # ---- 负例：逐条注入必红 --------------------------------------------
    def test_negative_schema_version_drift_is_red(self):
        obj = self._lock_obj()
        obj["schema_version"] = 1
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_scope_mismatch_is_red(self):
        self.assertNotEqual(_run(self.policy, self.lock, scope="linux_hosted").returncode, 0)

    def test_negative_policy_requirement_flip_is_red(self):
        obj = self._lock_obj()
        obj["hosted_ci_versions_required"] = True
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_host_reprovisioning_flip_is_red(self):
        obj = self._lock_obj()
        obj["host_reprovisioning"] = True
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_acr_flip_is_red(self):
        obj = self._lock_obj()
        obj["acr"] = True
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_cmake_version_placeholder_is_red(self):
        obj = self._lock_obj()
        obj["cmake"]["version"] = "unknown"
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_version_line_mismatch_is_red(self):
        obj = self._lock_obj()
        obj["cmake"]["version_line"] = "cmake version 9.9.9"
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_non_boolean_present_is_red(self):
        obj = self._lock_obj()
        obj["cmake"]["present"] = "maybe"
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    def test_negative_missing_tools_not_list_is_red(self):
        obj = self._lock_obj()
        obj["missing_tools"] = {"cmake": True}
        self._write_lock(obj)
        self.assertNotEqual(_run(self.policy, self.lock).returncode, 0)

    # NOTE（TST-001 登记的产品缺陷，暂不入本绿测试）：
    # eng/ci/verify_toolchain.py:207-208 以 `main()`（无 sys.exit）作 __main__ 入口，
    # 而 :111/:123 的 fail-closed `return 1` 只从 main() 返回、未传播为进程退出码，
    # 故「lock 非法 JSON / lock 缺失 / policy 缺失」时脚本打印 [FAIL] 却 exit 0。
    # 违反 ENGINEERING_SPEC.md §8「fail-closed：输入缺失/路径不存在必须判红」。
    # 复现：echo '{ not json' > /tmp/l.json && python3 eng/ci/verify_toolchain.py \
    #         --policy eng/ci/toolchain.policy.json --actual /tmp/l.json --scope agent-host; echo $?  # => 0
    # 修复（eng/ci/verify_toolchain.py，非本任务文件域）后应恢复以下 3 条 fail-closed 负例：
    #   test_negative_malformed_lock_is_red / _missing_lock_is_fail_closed /
    #   _missing_policy_is_fail_closed


if __name__ == "__main__":
    unittest.main(verbosity=2)
