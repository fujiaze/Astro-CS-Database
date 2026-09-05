"""V8-CI-004：ci/impact_map.json 一致性校验与 changed-path 行为测试。

覆盖三层：
1. 注册表一致性 —— 映射引用的每个检查 id 都存在于 ci/checks.json（70 项注册表），
   且为 fast profile 候选（规格要求 prefer fast candidates）。
2. 规格必含覆盖 —— 版本、schema、合同索引、SCI→TEST 追踪、ACR dormant、
   生产可达性、serial-heavy、陈旧版本注释、相关单测九类在映射中显式落位；
   每条规则携带统一 BASE 核心集；fallback 为最小 always-recheck 核心。
3. 行为（fixture 仓库）—— 改 VERSION 文件只选中版本类检查；改 lib 文件命中
   lib 规则；未覆盖路径触发 fallback；--impact-map 注入覆盖生效。

只读主仓库资产，不修改任何被检文件。
"""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

CI_DIR = Path(__file__).resolve().parents[1]
REPO = CI_DIR.parent
sys.path.insert(0, str(CI_DIR / "tests"))

import _helpers as H  # noqa: E402

REGISTRY_PATH = CI_DIR / "checks.json"
MAP_PATH = CI_DIR / "impact_map.json"

# 规格九类必含（V8-CI-004 spec_ref：tasks/02_CI_TASKS.md）
NINE_REQUIRED_CLASSES = {
    "版本": ["VERSION-CONSISTENCY", "VERSION-NAMESPACES"],
    "schema": ["TASK-RESULT-SCHEMA"],
    "合同索引": ["CONTRACT-GRAPH"],
    "SCI→TEST 追踪": ["TRACEABILITY-CODE", "TRACEABILITY-MATRIX", "CON-TRACEABILITY"],
    "ACR dormant": ["ACR-DORMANT"],
    "生产可达性": ["PROD-REACH-SELFTEST"],
    "serial-heavy": ["NO-SERIAL-HEAVY", "SERIAL-HARDCODE"],
    "陈旧版本注释": ["CON-COMMENTS"],
    "相关单测": ["UT-VERSION", "UT-RUNTIME", "UT-GLOSSARY", "UT-TRACEABILITY",
                 "UT-SCIENCELINT", "UT-CONTRACTS", "UT-MONITORING", "UT-PIPELINE",
                 "UT-ARTIFACT"],
}

# 生产映射承诺的路径域（domain → 探针路径，用 runner 的 _match_prefix 语义验证）
REQUIRED_DOMAINS = {
    "VERSION": "VERSION",
    "CMakeLists.txt": "CMakeLists.txt",
    "CMakePresets.json": "CMakePresets.json",
    "cmake/**": "cmake/toolchain.cmake",
    "ci/**": "ci/run.py",
    "docs/**": "docs/index.md",
    "schemas/**": "schemas/ci_result.schema.json",
    "lib/**": "lib/core/a.cpp",
    "include/**": "include/astrocs/a.h",
    "cli/**": "cli/a.py",
    "modules/**": "modules/services/a.cpp",
    "runtime/**": "runtime/a.cpp",
    "third_party/**": "third_party/fmt/a.cpp",
    "tests/**": "tests/testkit/x.py",
    "contracts/**": "contracts/a.yaml",
    "testdata/**": "testdata/a.fits",
    "evidence/**": "evidence/v8_1_ci_control/a.json",
    "engineering/control/**": "engineering/control/active/x.md",
    "tools/**": "tools/a.py",
    "tools/quality/**": "tools/quality/a.py",
    "tools/monitoring/**": "tools/monitoring/a.py",
    "graph/**": "graph/a.json",
    "launch/**": "launch/a",
    "packaging/**": "packaging/a",
    "scripts/**": "scripts/a.sh",
    "AGENTS.md": "AGENTS.md",
    "memory.md": "memory.md",
    "AstroCS_ENGINEERING_CONSTRAINTS.md": "AstroCS_ENGINEERING_CONSTRAINTS.md",
    "README.md": "README.md",
    ".github/**": ".github/workflows/ci.yaml",
}


def _load_run_module():
    """从主仓库 ci/run.py 加载 runner 模块（复用 _match_prefix 等纯函数）。"""
    spec = importlib.util.spec_from_file_location("astrocs_ci_run_v8ci004", CI_DIR / "run.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _load_registry() -> dict:
    return json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))


def _load_map() -> dict:
    return json.loads(MAP_PATH.read_text(encoding="utf-8"))


class TestImpactMapConsistency(unittest.TestCase):
    """映射与注册表/规格的结构一致性（只读校验）。"""

    @classmethod
    def setUpClass(cls):
        cls.registry = _load_registry()
        cls.impact = _load_map()
        cls.all_ids = {c["id"] for c in cls.registry["checks"]}
        cls.fast_ids = {c["id"] for c in cls.registry["checks"] if "fast" in c["profiles"]}
        cls.mapped_ids = set()
        for rule in cls.impact.get("rules", []):
            cls.mapped_ids.update(rule.get("checks", []))
        cls.mapped_ids |= set(cls.impact.get("fallback", []))

    def test_map_shape(self):
        self.assertIsInstance(self.impact.get("rules"), list, "rules 必须是数组")
        self.assertIsInstance(self.impact.get("fallback"), list, "fallback 必须是数组")
        self.assertTrue(self.impact["rules"], "rules 不得为空")
        for idx, rule in enumerate(self.impact["rules"]):
            self.assertTrue(rule.get("paths"), f"rules[{idx}].paths 不得为空")
            self.assertTrue(rule.get("checks"), f"rules[{idx}].checks 不得为空")
            for pat in rule["paths"]:
                self.assertIsInstance(pat, str)
                self.assertTrue(pat.strip(), f"rules[{idx}] 含空白路径模式")
        # 无完全重复的规则（同一路径模式集合出现两次）
        seen = {tuple(sorted(r["paths"])) for r in self.impact["rules"]}
        self.assertEqual(len(seen), len(self.impact["rules"]), "存在重复路径集合的规则")

    def test_all_ids_exist_in_registry(self):
        unknown = sorted(self.mapped_ids - self.all_ids)
        self.assertEqual(unknown, [], f"映射引用了注册表不存在的检查 id：{unknown}")

    def test_all_ids_are_fast_candidates(self):
        non_fast = sorted(self.mapped_ids - self.fast_ids)
        self.assertEqual(non_fast, [], f"映射引用了非 fast 候选 id（违反 prefer fast）：{non_fast}")

    def test_nine_required_classes_covered(self):
        for name, ids in NINE_REQUIRED_CLASSES.items():
            missing = [i for i in ids if i not in self.mapped_ids]
            self.assertEqual(missing, [], f"必含类「{name}」未映射：{missing}")

    def test_every_rule_carries_base_core(self):
        base = set(self.impact["fallback"]) - {"WORKSPACE-ADOPTION", "RECONCILE-STATE"}
        for idx, rule in enumerate(self.impact["rules"]):
            missing = sorted(base - set(rule["checks"]))
            self.assertEqual(
                missing, [],
                f"rules[{idx}]（{rule['paths'][:2]}…）缺少 BASE 核心类：{missing}")

    def test_required_path_domains_covered(self):
        runner = _load_run_module()
        for domain, probe in REQUIRED_DOMAINS.items():
            hit = any(runner._match_prefix(probe, pat)
                      for rule in self.impact["rules"] for pat in rule["paths"])
            self.assertTrue(hit, f"路径域 {domain}（探针 {probe}）未被任何规则覆盖")

    def test_fallback_minimal_core(self):
        fb = set(self.impact["fallback"])
        self.assertTrue(fb, "fallback 不得为空（未覆盖路径需要兜底重检核心）")
        unknown = sorted(fb - self.all_ids)
        self.assertEqual(unknown, [], f"fallback 引用未登记 id：{unknown}")
        non_fast = sorted(fb - self.fast_ids)
        self.assertEqual(non_fast, [], f"fallback 含非 fast 候选：{non_fast}")
        # fallback 必须含九类中的领域无关核心（相关单测按领域映射，不强制入 fallback）
        core_nine = {i for name, ids in NINE_REQUIRED_CLASSES.items()
                     if name != "相关单测" for i in ids}
        missing = sorted(core_nine - fb)
        self.assertEqual(missing, [], f"fallback 缺少领域无关核心：{missing}")
        # minimal：不超过 fast 全集的 1/3
        self.assertLessEqual(len(fb), len(self.fast_ids) // 3,
                             "fallback 过大，失去 changed-path 收敛意义")

    def test_rule_overlap_unions_not_conflict(self):
        """重叠规则（docs/** 与 docs/contracts/**）按并集收敛，不互斥不报错。"""
        runner = _load_run_module()
        probe = "docs/contracts/INDEX.yaml"
        hit_rules = [rule for rule in self.impact["rules"]
                     if any(runner._match_prefix(probe, pat) for pat in rule["paths"])]
        self.assertGreaterEqual(len(hit_rules), 2, f"{probe} 应同时命中 docs/** 与 docs/contracts/**")
        union = {cid for rule in hit_rules for cid in rule["checks"]}
        self.assertTrue(union, "重叠规则并集不得为空")


class TestImpactMapBehavior(unittest.TestCase):
    """fixture 仓库中的 changed-path → checks 行为验证。"""

    def _fixture_map(self) -> dict:
        """小型映射，形态与生产 ci/impact_map.json 对齐（含 BASE 核心）。"""
        base = ["BASE-VER", "BASE-SCHEMA", "BASE-TRACE", "BASE-ACR", "BASE-SERIAL",
                "BASE-COMMENTS", "BASE-REACH"]
        return {
            "rules": [
                {"paths": ["VERSION"], "checks": base + ["V-NS"]},
                {"paths": ["lib/**"], "checks": base + ["LIB-DUP", "LIB-ABI"]},
            ],
            "fallback": base + ["FB-ADOPTION"],
        }

    def _registry_for(self) -> list[dict]:
        ids = ["V-NS", "LIB-DUP", "LIB-ABI", "FB-ADOPTION",
               "BASE-VER", "BASE-SCHEMA", "BASE-TRACE", "BASE-ACR",
               "BASE-SERIAL", "BASE-COMMENTS", "BASE-REACH"]
        return [H.check(id=cid) for cid in ids]

    def _plan(self, repo: Path, out_root: Path, extra_args: list[str]) -> dict:
        proc = H.run_runner(["--profile", "fast", "--changed-from", "HEAD",
                             "--plan-only", "--output-root", str(out_root), *extra_args], repo)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return json.loads(proc.stdout)

    def _seed_fixture(self, repo: Path) -> None:
        """写入 registry + 映射并提交，使后续变更仅剩目标路径（fixture 资产不得成为未覆盖路径）。"""
        H.write_registry(repo, self._registry_for())
        (repo / "ci" / "impact_map.json").write_text(
            json.dumps(self._fixture_map(), ensure_ascii=False), encoding="utf-8")
        H.sh(["git", "add", "-A"], cwd=repo)
        H.sh(["git", "commit", "-m", "fixture: registry+map"], cwd=repo)

    def test_version_change_selects_only_version_class(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            self._seed_fixture(repo)
            (repo / "VERSION").write_text("0.11.0-alpha.2\n", encoding="utf-8")  # 未提交新增
            plan = self._plan(repo, out_root, [])
            selected = {c["id"] for c in plan["checks"]}
            self.assertEqual(selected, {"V-NS", "BASE-VER", "BASE-SCHEMA", "BASE-TRACE",
                                        "BASE-ACR", "BASE-SERIAL", "BASE-COMMENTS",
                                        "BASE-REACH"},
                             "改 VERSION 只应选中版本规则（BASE 核心 + V-NS）")
            self.assertNotIn("LIB-DUP", selected, "lib 专属检查不得被 VERSION 变更选中")
            self.assertNotIn("FB-ADOPTION", selected, "无未覆盖路径时不得触发 fallback")

    def test_version_change_via_committed_diff(self):
        """已提交路径同样命中：make_repo 后提交 VERSION 变更，HEAD..HEAD 相同，
        改用 HEAD 检出差异由 uncommitted 覆盖；此处验证 committed diff 路径。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            self._seed_fixture(repo)
            (repo / "VERSION").write_text("0.11.0-alpha.3\n", encoding="utf-8")
            H.sh(["git", "add", "VERSION"], cwd=repo)
            H.sh(["git", "commit", "-m", "bump"], cwd=repo)
            proc = H.run_runner(["--profile", "fast", "--changed-from", "HEAD~1",
                                 "--plan-only", "--output-root", str(out_root)], repo)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            selected = {c["id"] for c in json.loads(proc.stdout)["checks"]}
            self.assertIn("V-NS", selected, "已提交 VERSION 变更应命中版本规则")
            self.assertNotIn("LIB-DUP", selected)

    def test_lib_change_hits_lib_rule(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            self._seed_fixture(repo)
            (repo / "lib").mkdir()
            (repo / "lib" / "core.cpp").write_text("// changed\n", encoding="utf-8")
            plan = self._plan(repo, out_root, [])
            selected = {c["id"] for c in plan["checks"]}
            self.assertIn("LIB-DUP", selected)
            self.assertIn("LIB-ABI", selected)
            self.assertNotIn("V-NS", selected, "版本类检查不得被 lib 变更选中")

    def test_unmatched_path_triggers_fallback(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            self._seed_fixture(repo)
            (repo / "zzz").mkdir()
            (repo / "zzz" / "unknown.bin").write_text("?", encoding="utf-8")
            plan = self._plan(repo, out_root, [])
            selected = {c["id"] for c in plan["checks"]}
            self.assertIn("FB-ADOPTION", selected, "未覆盖路径必须并集 fallback 核心")
            self.assertNotIn("LIB-DUP", selected, "fallback 不应扩大到领域专属检查")

    def test_impact_map_override_flag(self):
        """--impact-map 注入生效（--impact-map ci/impact_map.json --plan-only 组合语义）。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            out_root = root / "out"
            self._seed_fixture(repo)
            alt = root / "alt_map.json"
            alt.write_text(json.dumps({
                "rules": [{"paths": ["VERSION"], "checks": ["V-NS"]}],
                "fallback": [],
            }, ensure_ascii=False), encoding="utf-8")
            (repo / "VERSION").write_text("0.11.0-alpha.2\n", encoding="utf-8")
            plan = self._plan(repo, out_root, ["--impact-map", str(alt)])
            selected = {c["id"] for c in plan["checks"]}
            self.assertEqual(selected, {"V-NS"}, "注入映射应完全替换默认映射")


if __name__ == "__main__":
    unittest.main()
