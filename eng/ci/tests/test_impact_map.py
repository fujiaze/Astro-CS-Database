"""V8-CI-004：eng/ci/impact_map.json 一致性校验与 changed-path 行为测试。

覆盖三层：
1. 注册表一致性 —— 映射引用的每个检查 id 都存在于 eng/ci/checks.json（70 项注册表），
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
REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(CI_DIR / "tests"))

import _helpers as H  # noqa: E402

REGISTRY_PATH = CI_DIR / "checks.json"
MAP_PATH = CI_DIR / "impact_map.json"
GATE_PATH = CI_DIR.parent / "tools" / "quality" / "check_impact_map.py"


def _load_gate():
    """加载 CHK-IMPACT-MAP 判定器（单源：本测试不再自带一套 id/路径判据）。"""
    spec = importlib.util.spec_from_file_location("astrocs_impact_gate", GATE_PATH)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

# 规格九类必含（V8-CI-004 spec_ref：tasks/02_CI_TASKS.md）
NINE_REQUIRED_CLASSES = {
    "版本": ["VERSION-CONSISTENCY", "VERSION-NAMESPACES"],
    # W4-A3 口径订正（不改规格类别，只把 id 改绑现行注册表的两层命名）：
    #   "schema" 原举 `TASK-RESULT-SCHEMA`（`CHK-SCHEMA` 的旧 step）已于 2026-09-16 退役
    #   （唯一默认输入 evidence/v6_1_rework/** 随旧世代删除；见 docs/ci/01_CHECKS.md §2.1）。
    #   该类现由聚合项 `CHK-SCHEMA` + 其现行 steps 承载。
    "schema": ["CHK-SCHEMA", "CON-CONFIG-CONTRACTS", "LOG-CONTRACT-SELFCHECK"],
    "合同索引": ["CONTRACT-GRAPH"],
    # TRACEABILITY-CODE 于 2026-09-16 按负责人裁决退役（唯一默认输入 artifacts/evidence/prerelease-v5/
    # tables/TRACEABILITY.csv 随 artifacts/ 删除，b1290525）：见 docs/ci/01_CHECKS.md §2 退役记录。
    # 本类仍由后两项覆盖（两者均未受影响、仍可跑）。
    "SCI→TEST 追踪": ["TRACEABILITY-MATRIX", "CON-TRACEABILITY"],
    "ACR dormant": ["ACR-DORMANT"],
    "生产可达性": ["PROD-REACH-SELFTEST"],
    "serial-heavy": ["NO-SERIAL-HEAVY", "SERIAL-HARDCODE"],
    "陈旧版本注释": ["CON-COMMENTS"],
    "相关单测": ["UT-VERSION", "UT-RUNTIME", "UT-GLOSSARY", "UT-TRACEABILITY",
                 "UT-SCIENCELINT", "UT-CONTRACTS", "UT-MONITORING", "UT-PIPELINE",
                 "UT-ARTIFACT"],
}

# 生产映射承诺的路径域 —— 按**现行树**重锚（ROOT-008/ARCH-001 与 2026-09-21 根目录整合后）。
# 迁移前锚（schemas/ modules/ runtime/ evidence/ graph/ launch/ AstroCS.wiki/）在现行
# 根清单中不存在，作为判据锚即「锚失效」；现行锚的权威声明面 = CHK-IMPACT-MAP 的
# PROBE_DOMAINS，本表只保留与生产映射直接对应的等价断言（单源，避免两套判据）。
#
# 本轮重锚（三处已退役锚，全部按门 PROBE_DOMAINS 同口径订正）：
#   * 根 cli/ 已随 ARCH-001 迁移退役（AGENTS.md §7：CLI 现落 lib/infrastructure/cli/），
#     本表原有的 lib/infrastructure/cli/** 条目即其继承者，故不再单列根 cli/**；
#   * reports/ 已退役 → artifacts/evidence/**（check_impact_map.py PROBE_DOMAINS 注记
#     「reports/ 退役 → artifacts/evidence/**（AGENTS.md §7）」），按继承者重锚；
#   * 工程控制/<控制包>/ 目录按 CONTROL_PACK_SPEC §9「收口即清理」随时移出仓库，
#     锚只取稳定的根条目「工程控制」（与门 PROBE_DOMAINS 一致）。
REQUIRED_DOMAINS = {
    "VERSION": "VERSION",
    "CMakeLists.txt": "CMakeLists.txt",
    "CMakePresets.json": "CMakePresets.json",
    "eng/cmake/**": "eng/cmake/install_layout.cmake",
    "eng/ci/**": "eng/ci/run.py",
    "docs/**": "docs/VERSIONING.md",
    "docs/contracts/**": "docs/contracts/DATA_SEMANTICS.md",
    "docs/science/**": "docs/science/ASTROMETRY.md",
    "lib/**": "lib/algorithms/psf/src/dpsf_psf.cpp",
    "lib/infrastructure/**": "lib/infrastructure/cli/main.cpp",
    "lib/include/**": "lib/include/astrocs/common_abi_v1.h",
    "eng/contracts/**": "eng/contracts/schemas/run_manifest.schema.json",
    "third_party/**": "lib/third_party/nlohmann/json.hpp",
    "eng/tests/**": "eng/tests/testkit/registry.json",
    "testdata/**": "testdata/index.json",
    "artifacts/**": "artifacts/ci",
    "artifacts/evidence/**": "artifacts/evidence/README.md",
    "工程控制/**": "工程控制",
    "eng/tools/**": "eng/tools/quality/check_module_map.py",
    "eng/tools/quality/**": "eng/tools/quality/check_module_map.py",
    "eng/tools/monitoring/**": "eng/tools/monitoring/run_monitored.py",
    "eng/packaging/**": "eng/packaging/astrocs.product.json",
    "AGENTS.md": "AGENTS.md",
    "memory.md": "memory.md",
    "README.md": "README.md",
    ".github/**": ".github/workflows/ci-linux.yml",
}


def _load_run_module():
    """从主仓库 eng/ci/run.py 加载 runner 模块（复用 _match_prefix 等纯函数）。"""
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
        # W4-A3 根因修复：注册表的执行单元是**两层**结构 —— 顶层聚合项（CHK-* 等）
        # 与其 steps[].id（CI-001 ID 收敛时原样保留的旧 ID）。impact_map 引用的是
        # step id（细粒度语义），故 id 面必须是「顶层 ∪ steps」；只索引顶层会把
        # 48/59 条合法映射误判为「注册表不存在」。口径单源 = CHK-IMPACT-MAP 判定器。
        cls.gate = _load_gate()
        cls.index = cls.gate.index_registry(cls.registry)
        cls.all_ids = cls.gate.registry_ids(cls.index)
        cls.fast_ids = cls.index["fast"]
        cls.step_ids = cls.index["steps"]
        cls.top_ids = cls.index["top"]
        cls.mapped_ids = set()
        for rule in cls.impact.get("rules", []):
            cls.mapped_ids.update(rule.get("checks", []))
        cls.mapped_ids |= set(cls.impact.get("fallback", []))
        cls.mapped_ids |= set(cls.impact.get("base_checks", []))

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

    def test_id_face_is_two_layer_registry(self):
        """id 面口径：顶层 ∪ steps（step 未声明 profiles 时继承父项）。

        负例断言：若回退到「只索引顶层」，本测试必须失败（防口径再次漂移）。
        """
        self.assertTrue(self.step_ids, "注册表必须含 steps（CI-001 ID 收敛的两层结构）")
        self.assertTrue(self.step_ids - self.top_ids, "steps 与顶层 id 必须不同集")
        top_only = {c["id"] for c in self.registry["checks"]}
        self.assertTrue(self.mapped_ids - top_only,
                        "映射必须存在只以 step id 形式登记的引用（本口径的判别面）")
        self.assertEqual(sorted(self.mapped_ids - self.all_ids), [],
                         "id 面必须覆盖映射的全部引用")

    def test_nine_required_classes_covered(self):
        for name, ids in NINE_REQUIRED_CLASSES.items():
            missing = [i for i in ids if i not in self.mapped_ids]
            self.assertEqual(missing, [], f"必含类「{name}」未映射：{missing}")
        # 必含类引用的 id 也必须落在统一 id 面内（与 test_all_ids_exist_in_registry 同口径）
        unknown = sorted({i for ids in NINE_REQUIRED_CLASSES.values() for i in ids}
                         - self.all_ids)
        self.assertEqual(unknown, [], f"必含类引用了未登记 id：{unknown}")

    def test_every_rule_carries_base_core(self):
        base = set(self.impact["fallback"]) - {"WORKSPACE-ADOPTION", "RECONCILE-STATE"}
        for idx, rule in enumerate(self.impact["rules"]):
            missing = sorted(base - set(rule["checks"]))
            self.assertEqual(
                missing, [],
                f"rules[{idx}]（{rule['paths'][:2]}…）缺少 BASE 核心类：{missing}")

    def test_required_path_domains_covered(self):
        """路径域覆盖 + 锚存活：探针必须在**现行树**中存在，否则本断言自身失效。"""
        # 单源约束（防两套判据再次漂移）：本表只允许出现 CHK-IMPACT-MAP 的
        # PROBE_DOMAINS 已登记的路径域。门退役某域而本表未跟随时，本条判红——
        # 根 cli/ 与 reports/ 的历史漂移正是这样漏过一轮的。
        gate_domains = {d for d, _anchor, _probe in self.gate.PROBE_DOMAINS}
        stray = sorted(set(REQUIRED_DOMAINS) - gate_domains)
        self.assertEqual(stray, [],
                         f"本表含 CHK-IMPACT-MAP 未登记的路径域（单源漂移）：{stray}")
        runner = _load_run_module()
        for domain, probe in REQUIRED_DOMAINS.items():
            anchor = REPO / probe
            self.assertTrue(anchor.exists(),
                            f"路径域 {domain} 的探针锚在当前树不存在（锚失效）：{probe}")
            hit = any(runner._match_prefix(probe, pat)
                      for rule in self.impact["rules"] for pat in rule["paths"])
            self.assertTrue(hit, f"路径域 {domain}（探针 {probe}）未被任何规则覆盖")

    def test_gate_probe_domains_are_live_and_covered(self):
        """与 CHK-IMPACT-MAP 判定器同口径复核（单源）：探针锚存活 + 规则覆盖。"""
        runner = _load_run_module()
        for domain, anchor, probe in self.gate.PROBE_DOMAINS:
            self.assertTrue((REPO / anchor).exists(),
                            f"[gate] 锚存活失效：{domain} -> {anchor}")
            hit = any(runner._match_prefix(probe, pat)
                      for rule in self.impact["rules"] for pat in rule["paths"])
            self.assertTrue(hit, f"[gate] 路径域 {domain}（探针 {probe}）未被任何规则覆盖")

    def test_no_retired_ids_referenced(self):
        """R5：映射不得引用 docs/ci/01_CHECKS.md §2.1/§2.3 已退役 / RESERVED 的 id。"""
        doc = (REPO / "docs" / "ci" / "01_CHECKS.md").read_text(encoding="utf-8")
        retired = self.gate.parse_retired(doc)
        self.assertTrue(retired, "退役/RESERVED 表解析为空 ⇒ fail-closed（判据面失效）")
        refs = sorted(self.mapped_ids & retired)
        self.assertEqual(refs, [], f"映射引用了已退役/RESERVED id：{refs}")

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
        """小型映射，形态与生产 eng/ci/impact_map.json 对齐（含 BASE 核心）。"""
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
        (repo / "eng" / "ci" / "impact_map.json").write_text(
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
        """--impact-map 注入生效（--impact-map eng/ci/impact_map.json --plan-only 组合语义）。"""
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
