# -*- coding: utf-8 -*-
"""V8-CI-002 单测：known_failures 基线合同（07 合同）。

对应场景 13：精确匹配 → KNOWN_FAIL（总 verdict 仍 FAIL、计数分离）；
过期条目 → FAIL；结构非法 → FAIL（内部校验），JSON 语法坏 → RunnerError exit 2。
匹配键以源码为准：check_id 精确相等 + expiry 未过期（signature 缺省不参与匹配）。
"""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from ci.tests import _helpers as H  # noqa: E402

SHA40 = "a" * 40
FAIL_CMD = ["python3", "-c", "raise SystemExit(7)"]
# 未来时间戳：绕开时钟偏移；过期测试用明确的过去时间戳
FAR_FUTURE = "2999-01-01T00:00:00Z"
PAST = "2000-01-01T00:00:00Z"


def kf_entry(**overrides) -> dict:
    base = {
        "check_id": "CHK-KF",
        "owner": "SA-CI-32",
        "reproducer": "ci/tests/test_runner_contract.py",
        "source_sha": SHA40,
        "expiry": FAR_FUTURE,
        "reason": "单元测试已知失败（fixture）",
    }
    base.update(overrides)
    return base


def run_with_baseline(repo: Path, out_root: Path, baseline: dict | list | str,
                      arg_name: str = "--known-failures"):
    """写基线文件并执行 runner（exit 码透传）。"""
    H.write_ci_result_schema(repo)
    kf = repo / "ci" / "kf_fixture.json"
    if isinstance(baseline, str):
        kf.write_text(baseline, encoding="utf-8")  # 语法非法
    else:
        kf.write_text(json.dumps(baseline, ensure_ascii=False, indent=1),
                      encoding="utf-8")
    return H.run_runner(["--profile", "fast", arg_name, str(kf),
                         "--output-root", str(out_root)], repo)


class TestKnownFailuresBaseline(unittest.TestCase):
    """known_failures 精确匹配 / 过期 / 结构非法 / JSON 语法坏。"""

    def test_exact_match_is_known_fail_overall_fail(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-KF", command=FAIL_CMD)])
            proc = run_with_baseline(repo, out_root, [kf_entry()])
            self.assertEqual(proc.returncode, 1, f"KNOWN_FAIL 总 verdict 仍 FAIL：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-KF")
            self.assertEqual(per["verdict"], "KNOWN_FAIL")
            self.assertEqual(per["exit_code"], 7)
            self.assertEqual(per["known_failure"]["owner"], "SA-CI-32")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")          # 总 verdict 不放行
            self.assertEqual(ci["summary"]["known_fail"], 1)  # 计数分离
            self.assertEqual(ci["summary"]["fail"], 0)
            self.assertNotIn("FAIL", ci["summary"]["fail_detail"])
            self.assertEqual(ci["known_failures"]["matched"], ["CHK-KF"])
            self.assertEqual(ci["known_failures"]["new_failures"], [])

    def test_unmatched_failure_counts_as_new(self):
        """失败检查未登记基线 → new_failures、verdict FAIL（fail>=1）。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-NEW", command=FAIL_CMD)])
            proc = run_with_baseline(repo, out_root, [kf_entry(check_id="CHK-OTHER")])
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-NEW")
            self.assertEqual(per["verdict"], "FAIL")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["fail"], 1)
            self.assertEqual(ci["known_failures"]["new_failures"], ["CHK-NEW"])
            self.assertEqual(ci["known_failures"]["matched"], [])

    def test_expired_entry_keeps_fail(self):
        """基线条目已过期 → expired 记录、检查维持 FAIL。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-KF", command=FAIL_CMD)])
            proc = run_with_baseline(repo, out_root, [kf_entry(expiry=PAST)])
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-KF")
            self.assertEqual(per["verdict"], "FAIL")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["fail"], 1)
            self.assertEqual(ci["summary"]["known_fail"], 0)
            self.assertEqual(ci["known_failures"]["expired"][0]["check_id"], "CHK-KF")

    def test_registered_but_not_occurring_fails(self):
        """基线登记一条未发生的失败（检查实际 PASS）→ expired 非空 → verdict FAIL。

        源码实现：expired 条目无论检查结果如何都使总 verdict=FAIL（build_ci_result
        对 known_summary.expired 非空即 FAIL）。
        """
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-OK", command=["python3", "-c", "print('ok')"])])
            proc = run_with_baseline(repo, out_root, [kf_entry(check_id="CHK-GHOST", expiry=PAST)])
            self.assertEqual(proc.returncode, 1, f"过期基线必须 FAIL：{proc.stderr}")
            per = H.load_check_result(out_root, "CHK-OK")
            self.assertEqual(per["verdict"], "PASS")  # 检查本身 PASS
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")   # 基线过期拖垮整体
            self.assertEqual(ci["known_failures"]["expired"][0]["check_id"], "CHK-GHOST")

    def test_structure_invalid_baseline_fails(self):
        """基线结构非法（缺必填字段 / source_sha 非法）→ known_failures.invalid → FAIL。

        源码实现：结构错误条目被丢弃，但 invalid 非空使总 verdict=FAIL。
        """
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-OK", command=["python3", "-c", "print('ok')"])])
            bad = kf_entry()  # 缺 reason
            bad.pop("reason")
            proc = run_with_baseline(repo, out_root, [bad])
            self.assertEqual(proc.returncode, 1, f"结构非法基线必须 FAIL：{proc.stderr}")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["pass"], 1)
            self.assertTrue(ci["known_failures"]["invalid"], "invalid 列表应记录结构错误")
            # 条目被丢弃 → 检查自身 PASS；仅 invalid 拖垮整体
            self.assertEqual(H.load_check_result(out_root, "CHK-OK")["verdict"], "PASS")

    def test_malformed_json_baseline_is_runner_error(self):
        """基线文件 JSON 语法坏 → RunnerError → exit 2（runner 配置错误）。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-OK", command=["python3", "-c", "print('ok')"])])
            proc = run_with_baseline(repo, out_root, "{not-valid-json")
            self.assertEqual(proc.returncode, 2, f"JSON 语法坏应为 exit 2：{proc.stderr}")
            self.assertTrue(proc.stderr.strip())


class TestKnownFailuresSignature(unittest.TestCase):
    """签名变化：基线登记 signature 与实际日志不一致 → 判回 FAIL（signature_changed）。"""

    def test_signature_mismatch_downgrades_to_fail(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-SIG", command=FAIL_CMD)])
            proc = run_with_baseline(repo, out_root, [kf_entry(check_id="CHK-SIG",
                                                       signature="deadbeefdeadbeef")])
            self.assertEqual(proc.returncode, 1, proc.stderr)
            per = H.load_check_result(out_root, "CHK-SIG")
            self.assertEqual(per["verdict"], "FAIL")  # KNOWN_FAIL 被降级
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertIn("CHK-SIG", ci["known_failures"]["signature_changed"])
            self.assertEqual(ci["summary"]["fail"], 1)
            self.assertEqual(ci["summary"]["known_fail"], 0)


class TestNoBaselineFile(unittest.TestCase):
    """无基线文件：known_failures 逻辑整体跳过（enabled=false），不拖累 verdict。"""

    def test_missing_baseline_file_skips_baseline_logic(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="FAIL-NAKED", command=FAIL_CMD)])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, proc.stderr)
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertFalse(ci["known_failures"]["enabled"])
            self.assertEqual(ci["known_failures"]["baseline_path"], None)


if __name__ == "__main__":
    unittest.main()
