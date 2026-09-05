# -*- coding: utf-8 -*-
"""V8-CI-002 单测：检查选择（profile / --check / --focus / --changed-from / plan-only / registry 校验）。

对应场景：1 profile 选择与 plan-only、2 --check 显式指定、3 registry 非法 → exit 2、
10 空集不 PASS（含 fatduck 例外）、11 --changed-from + impact_map、12 --focus。
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


class TestProfileSelection(unittest.TestCase):
    """场景 1：--profile 只选 profiles 含该 profile 的检查；--plan-only 只出清单不执行。"""

    def test_plan_only_selects_profile_subset(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [
                H.check(id="CHK-F", profiles=["fast"]),
                H.check(id="CHK-L", profiles=["linux-deep"]),
            ])
            # fast → 只含 CHK-F
            proc = H.run_runner(["--profile", "fast", "--plan-only",
                                 "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            plan = json.loads(proc.stdout)  # stdout 必须是可解析 JSON
            self.assertEqual(plan["selected_count"], 1)
            self.assertEqual([c["id"] for c in plan["checks"]], ["CHK-F"])
            self.assertEqual(plan["profile"], "fast")
            self.assertIn("CHK-F", proc.stdout)
            # linux-deep → 只含 CHK-L
            proc2 = H.run_runner(["--profile", "linux-deep", "--plan-only",
                                  "--output-root", str(out_root)], repo)
            self.assertEqual(proc2.returncode, 0, proc2.stderr)
            plan2 = json.loads(proc2.stdout)
            self.assertEqual([c["id"] for c in plan2["checks"]], ["CHK-L"])

    def test_plan_only_does_not_execute(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-P")])
            proc = H.run_runner(["--profile", "fast", "--plan-only",
                                 "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertFalse(out_root.exists(), "plan-only 不得创建 out_root")

    def test_profile_execution_runs_only_selected(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [
                H.check(id="CHK-F", profiles=["fast"]),
                H.check(id="CHK-L", profiles=["linux-deep"]),
            ])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "PASS")
            self.assertEqual([c["id"] for c in ci["checks"]], ["CHK-F"])
            self.assertEqual(ci["summary"]["total"], 1)


class TestExplicitCheck(unittest.TestCase):
    """场景 2：--check 优先于 profile；未知 id → exit 2。"""

    def test_check_flag_overrides_profile(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [
                H.check(id="CHK-A"),
                H.check(id="CHK-B"),
            ])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--check", "CHK-B",
                                 "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            ci = H.load_ci_result(out_root)
            self.assertEqual([c["id"] for c in ci["checks"]], ["CHK-B"])
            self.assertEqual(ci["summary"]["total"], 1)
            self.assertEqual(H.load_check_result(out_root, "CHK-B")["verdict"], "PASS")

    def test_check_unknown_id_returns_runner_error(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-A")])
            proc = H.run_runner(["--check", "NO-SUCH-CHECK",
                                 "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 2, f"未知 --check 应为 runner 错误 exit 2：{proc.stderr}")
            self.assertTrue(proc.stderr.strip(), "RunnerError 信息应写入 stderr")


class TestRegistryValidation(unittest.TestCase):
    """场景 3：registry 非法（缺必填字段 / 空 checks）→ RunnerError → exit 2，而非 FAIL。"""

    def test_registry_missing_required_field(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            bad = H.check(id="CHK-BAD")
            bad.pop("timeout_seconds")  # 必填字段缺失
            H.write_registry(repo, [bad])
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 2, f"缺字段应为 exit 2：{proc.stderr}")
            self.assertTrue(proc.stderr.strip())
            self.assertFalse((out_root / "CI_RESULT.json").exists(),
                             "runner 错误路径不得产出 CI_RESULT.json")

    def test_registry_empty_checks_list(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [])
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 2, f"空 checks 应为 exit 2：{proc.stderr}")
            self.assertTrue(proc.stderr.strip())


class TestEmptySelection(unittest.TestCase):
    """场景 10：空集不 PASS；fatduck profile 空集例外 → FATDUCK_PENDING。"""

    def test_empty_fast_selection_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-D", profiles=["linux-deep"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fast", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 1, f"空集必须 FAIL exit 1：{proc.stderr}")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertIn("no_checks_selected", ci.get("verdict_reason", ""))
            self.assertEqual(ci["summary"]["total"], 0)
            self.assertEqual(ci["checks"], [])

    def test_fatduck_empty_profile_pending(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-F", profiles=["fast"])])
            H.write_ci_result_schema(repo)
            proc = H.run_runner(["--profile", "fatduck", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, f"fatduck 空集应 FATDUCK_PENDING exit 0：{proc.stderr}")
            ci = H.load_ci_result(out_root)
            self.assertEqual(ci["verdict"], "FATDUCK_PENDING")


class TestChangedFromImpactMap(unittest.TestCase):
    """场景 11：--changed-from 经 ci/impact_map.json 过滤；未命中路径 → 空集 FAIL。"""

    def _write_impact_map(self, repo: Path) -> None:
        (repo / "ci" / "impact_map.json").write_text(json.dumps({
            "rules": [
                {"paths": ["A.txt"], "checks": ["CHK-A"]},
                {"paths": ["B.txt"], "checks": ["CHK-B"]},
            ],
            "fallback": [],
        }, ensure_ascii=False, indent=1), encoding="utf-8")

    def test_changed_from_maps_modified_path(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-A"), H.check(id="CHK-B")])
            self._write_impact_map(repo)
            (repo / "A.txt").write_text("A-changed\n", encoding="utf-8")  # 工作区未提交修改
            proc = H.run_runner(["--profile", "fast", "--changed-from", "HEAD",
                                 "--plan-only", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            plan = json.loads(proc.stdout)
            self.assertEqual([c["id"] for c in plan["checks"]], ["CHK-A"])

    def test_changed_from_unmapped_path_empty_fails(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, [H.check(id="CHK-A"), H.check(id="CHK-B")])
            self._write_impact_map(repo)
            (repo / "C.txt").write_text("unmapped\n", encoding="utf-8")  # 未命中任何 rule
            # plan-only 侧：选中 0 项
            proc = H.run_runner(["--profile", "fast", "--changed-from", "HEAD",
                                 "--plan-only", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(json.loads(proc.stdout)["selected_count"], 0)
            # 执行侧：空集 → FAIL exit 1
            out2 = root / "out2"
            H.write_ci_result_schema(repo)
            proc2 = H.run_runner(["--profile", "fast", "--changed-from", "HEAD",
                                  "--output-root", str(out2)], repo)
            self.assertEqual(proc2.returncode, 1, f"未命中路径空集应 FAIL exit 1：{proc2.stderr}")
            ci = H.load_ci_result(out2)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertIn("no_checks_selected", ci.get("verdict_reason", ""))


class TestFocusFilter(unittest.TestCase):
    """场景 12：--focus 按检查 id 或路径前缀过滤。"""

    REGISTRY = [
        H.check(id="CHK-CLI", changed_paths=["src/cli/**"]),
        H.check(id="CHK-DOC", changed_paths=["docs/**"]),
    ]

    def test_focus_by_check_id(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, self.REGISTRY)
            proc = H.run_runner(["--profile", "fast", "--focus", "CHK-DOC",
                                 "--plan-only", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            plan = json.loads(proc.stdout)
            self.assertEqual([c["id"] for c in plan["checks"]], ["CHK-DOC"])

    def test_focus_by_path_prefix(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            H.write_registry(repo, self.REGISTRY)
            proc = H.run_runner(["--profile", "fast", "--focus", "src/cli/foo.py",
                                 "--plan-only", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            plan = json.loads(proc.stdout)
            self.assertEqual([c["id"] for c in plan["checks"]], ["CHK-CLI"])


if __name__ == "__main__":
    unittest.main()
