#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MOD-001 / CHK-MODULE-MANIFEST 质量测试（映射门正例 + 五类负例 + 诚实性红线）。

覆盖：
  T1  映射表 23/23 唯一行，且 ID 集合 == docs/plugins/00_INDEX.md §2（机器解析）
  T2  每行八元组字段齐备（目标目录/README/module.yaml/公开头/target/产品清单/共址测试）
  T3  正例 fixture（完整合法映射）→ 检查器 rc=0
  T4  五类负例（缺 manifest / 重复 entrypoint / 无 target / 无测试 / 悬空合同引用）
      各自 rc!=0 且报出对应 finding code
  T5  facade / no-op（导出符号存在但无可执行路径、entrypoint 转发整阶段 Session）
      必须判 NOT_IMPLEMENTED，绝不判 IMPLEMENTED
  T6  真实仓库诚实性：实现只认 target_dir；旧命名目录（lib/star_detector 等）不参与
      实现判定；lib/algorithms|infrastructure 未建立时不得出现 IMPLEMENTED/INSTALLED
  T7  状态词只取 ASTROCS_DESIGN §11.3 词表
  T8  检查器 --selftest 全绿（能绿能红自证）
"""
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

import yaml

REPO = pathlib.Path(__file__).resolve().parents[3]
TOOL = REPO / "eng" / "tools" / "quality" / "check_module_map.py"
MAP = REPO / "docs" / "modules" / "MODULE_MAP.yaml"
INDEX = REPO / "docs" / "plugins" / "00_INDEX.md"
sys.path.insert(0, str(REPO / "eng" / "tools" / "quality" / "fixtures"))
import module_map_fixture as fx  # noqa: E402

VOCABULARY = {
    "CONTRACT_READY", "IMPLEMENTED", "INSTALLED", "VERIFIED",
    "NOT_IMPLEMENTED", "NOT_VERIFIED", "DEFERRED", "DORMANT", "FAIL",
}
POSITIVE_RUNGS = {"CONTRACT_READY", "IMPLEMENTED", "INSTALLED", "VERIFIED"}
REQUIRED_ROW_FIELDS = (
    "id", "group", "plugin_doc", "target_dir", "readme", "module_yaml",
    "target", "target_file", "co_located_tests", "module_id", "module_version",
    "abi_version", "entrypoint", "data_contracts", "schema_links", "product_manifest",
)
# 旧命名实现面 → 目标模块（ARCH-001 迁移前不得据此判实现）
LEGACY_PAIRS = (
    ("calibration", "lib/calibration"),
    ("cosmetic", "lib/cosmetic"),
    ("star_detection", "lib/star_detector"),
    ("psf", "lib/dynamic_psf"),
    ("platesolve", "lib/plate_solve"),
    ("photometry", "lib/photometric_calib"),
    ("noise_snr", "lib/snr_estimator"),
    ("drizzle", "lib/healpix_db/healpix_drizzle"),
    ("sampling", "lib/phase2_samp"),
    ("upm", "lib/phase2_upm"),
    ("rejection", "lib/phase2_rej"),
    ("integration", "lib/phase2_int"),
    ("projection", "lib/phase3_proj"),
    ("resample", "lib/phase3_rsmp"),
    ("fits_output", "lib/phase3_fits"),
    ("gaia_xpsd_client", "lib/gaia_xpsd_client"),
)


def parse_index_ids() -> set:
    """docs/plugins/00_INDEX.md §2 模块总表第 2 列（与检查器同口径的最小解析）。"""
    lines = INDEX.read_text(encoding="utf-8").splitlines()
    start = next(i for i, ln in enumerate(lines) if ln.startswith("## 2."))
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## 3.")), len(lines))
    ids = set()
    for ln in lines[start:end]:
        if not ln.startswith("| "):
            continue
        cells = [c.strip() for c in ln.strip("|").split("|")]
        if len(cells) >= 2 and cells[0].startswith("\u0060") and cells[0].endswith(".md\u0060"):
            ids.add(cells[1])
    return ids


def run_tool(repo_root=None, map_path=None, extra=()):
    cmd = [sys.executable, str(TOOL)]
    if repo_root is not None:
        cmd += ["--repo-root", str(repo_root),
                "--map", str(map_path or (pathlib.Path(repo_root) / "docs/modules/MODULE_MAP.yaml"))]
    cmd += list(extra)
    return subprocess.run(cmd, capture_output=True, text=True, timeout=600, cwd=str(REPO))


def run_real_repo_json(tmpdir, name="real.json"):
    out = pathlib.Path(tmpdir) / name
    proc = run_tool(extra=("--json-out", str(out), "--quiet"))
    data = json.loads(out.read_text(encoding="utf-8"))
    return proc, data


class TestModuleMapTable(unittest.TestCase):
    """T1/T2/T7：映射表结构与权威一致。"""

    @classmethod
    def setUpClass(cls):
        cls.doc = yaml.safe_load(MAP.read_text(encoding="utf-8"))
        cls.mods = cls.doc["modules"]

    def test_t01_map_has_23_unique_rows(self):
        self.assertEqual(len(self.mods), 23, "映射表必须恰好 23 行")
        ids = [m["id"] for m in self.mods]
        self.assertEqual(len(set(ids)), 23, "23 个模块 ID 必须唯一")
        self.assertEqual(len(ids), len(set(ids)))

    def test_t02_map_ids_equal_authority_index(self):
        index_ids = parse_index_ids()
        self.assertEqual(len(index_ids), 23, "00_INDEX §2 应机器解析出 23 个模块 ID")
        self.assertEqual({m["id"] for m in self.mods}, index_ids)

    def test_t03_rows_carry_eight_tuple_fields(self):
        for m in self.mods:
            for key in REQUIRED_ROW_FIELDS:
                self.assertIn(key, m, "%s 缺字段 %s" % (m.get("id"), key))
            self.assertTrue(str(m["target_dir"]).startswith(("lib/algorithms/", "lib/infrastructure/")),
                            "%s 目标目录必须在 lib/algorithms|infrastructure 下" % m["id"])
            self.assertTrue(str(m["target_dir"]).endswith("/" + m["id"]))
            self.assertTrue((REPO / m["plugin_doc"]).is_file(), "%s 插件文档不存在" % m["id"])
            self.assertIn("product_manifest", m)

    def test_t04_no_status_field_in_map(self):
        """防「表内自证绿」：映射表不得携带模块状态声明。"""
        for m in self.mods:
            self.assertNotIn("status", m, "%s 不得在映射表内声明 status" % m["id"])
        self.assertNotIn("status", self.doc)


class TestCheckerFixtureGates(unittest.TestCase):
    """T3/T4/T5：正例 rc=0；五类负例各自 rc!=0；facade/no-op 判 NOT_IMPLEMENTED。"""

    def setUp(self):
        self._td = tempfile.TemporaryDirectory(prefix="mod001-test-")
        self.tmp = pathlib.Path(self._td.name)

    def tearDown(self):
        self._td.cleanup()

    def _run(self, mutation):
        root = fx.build_repo(self.tmp / (mutation or "positive"), mutation=mutation)
        out = self.tmp / ((mutation or "positive") + ".json")
        proc = run_tool(repo_root=root, extra=("--json-out", str(out), "--quiet"))
        data = json.loads(out.read_text(encoding="utf-8"))
        return proc, data, root

    def test_t10_positive_fixture_rc0(self):
        proc, data, _ = self._run(None)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertEqual(data["summary"]["modules_total"], 23)
        self.assertEqual(data["summary"]["fail_findings"], 0)
        self.assertEqual(data["summary"]["verdict"], "PASS")

    def test_t11_negative_missing_manifest(self):
        proc, data, _ = self._run("missing_manifest")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("missing_module_yaml", {f["code"] for f in data["findings"]})

    def test_t12_negative_duplicate_entrypoint(self):
        proc, data, _ = self._run("duplicate_entrypoint")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("duplicate_entrypoint", {f["code"] for f in data["findings"]})
        dup = [m for m in data["modules"] if m["id"] == "cosmetic"][0]
        self.assertEqual(dup["status"], "FAIL", "抢同一注册键的模块主状态必须是 FAIL")
        self.assertNotIn(dup["status"], {"IMPLEMENTED", "INSTALLED", "VERIFIED"})

    def test_t13_negative_missing_target(self):
        proc, data, _ = self._run("missing_target")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("missing_cmake_target", {f["code"] for f in data["findings"]})
        self.assertFalse([m for m in data["modules"] if m["id"] == "star_detection"][0]["items"]["cmake_target"])

    def test_t14_negative_missing_tests(self):
        proc, data, _ = self._run("missing_tests")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("missing_co_located_tests", {f["code"] for f in data["findings"]})
        self.assertFalse([m for m in data["modules"] if m["id"] == "psf"][0]["items"]["co_located_tests"])

    def test_t15_negative_dangling_contract_ref(self):
        proc, data, _ = self._run("dangling_contract_ref")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("dangling_data_contract", {f["code"] for f in data["findings"]})

    def test_t16_facade_session_forward_is_not_implemented(self):
        proc, data, _ = self._run("facade_session")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("facade_session_forward", {f["code"] for f in data["findings"]})
        mod = [m for m in data["modules"] if m["id"] == "photometry"][0]
        self.assertEqual(mod["status"], "NOT_IMPLEMENTED",
                         "entrypoint 转发整阶段 Session 必须判 NOT_IMPLEMENTED")
        self.assertFalse(mod["implemented"])
        self.assertNotIn(mod["status"], {"IMPLEMENTED", "INSTALLED", "VERIFIED"})

    def test_t17_noop_entrypoint_is_not_implemented(self):
        proc, data, _ = self._run("noop_entrypoint")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("noop_entrypoint", {f["code"] for f in data["findings"]})
        mod = [m for m in data["modules"] if m["id"] == "noise_snr"][0]
        self.assertEqual(mod["status"], "NOT_IMPLEMENTED", "无可执行路径必须判 NOT_IMPLEMENTED")
        self.assertFalse(mod["implemented"])

    def test_t18_selftest_battery_passes(self):
        proc = run_tool(extra=("--selftest",))
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("SELFTEST PASS", proc.stdout)


class TestRealRepoHonesty(unittest.TestCase):
    """T6/T7：真实仓库诚实判定（实现只认 target_dir；状态词只取 §11.3）。"""

    @classmethod
    def setUpClass(cls):
        cls._td = tempfile.TemporaryDirectory(prefix="mod001-real-")
        cls.proc, cls.data = run_real_repo_json(cls._td.name)

    @classmethod
    def tearDownClass(cls):
        cls._td.cleanup()

    def test_t20_json_reports_all_23_modules(self):
        self.assertEqual(self.data["summary"]["modules_total"], 23)
        self.assertEqual(self.data["summary"]["index_ids_total"], 23)
        self.assertEqual(self.data["summary"]["ids_unique"], 23)
        self.assertEqual(len(self.data["modules"]), 23)

    def test_t21_status_words_are_11_3_only(self):
        self.assertEqual(set(self.data["status_vocabulary"]), VOCABULARY)
        for m in self.data["modules"]:
            self.assertIn(m["status"], VOCABULARY, m["id"])
            self.assertIn(m["verification_status"], VOCABULARY, m["id"])

    def test_t22_implementation_only_from_target_dir(self):
        """红线：没有目标目录 ⇒ 绝不可能是 IMPLEMENTED/INSTALLED/VERIFIED。"""
        for m in self.data["modules"]:
            target = REPO / m["target_dir"]
            if not target.is_dir():
                self.assertNotIn(m["status"], {"IMPLEMENTED", "INSTALLED", "VERIFIED"},
                                 "%s 目标目录不存在却被判 %s" % (m["id"], m["status"]))
                self.assertFalse(m["implemented"], m["id"])
            if m["implemented"]:
                self.assertTrue(target.is_dir(), "%s 实现必须来自 target_dir" % m["id"])

    def test_t23_legacy_dirs_never_count_as_implemented(self):
        by_id = {m["id"]: m for m in self.data["modules"]}
        for mid, legacy in LEGACY_PAIRS:
            if (REPO / legacy).exists() and not (REPO / by_id[mid]["target_dir"]).is_dir():
                self.assertNotIn(by_id[mid]["status"], {"IMPLEMENTED", "INSTALLED", "VERIFIED"},
                                 "%s：旧命名目录 %s 存在不得算实现" % (mid, legacy))

    def test_t24_real_repo_verdict_is_honest_red_until_arch001(self):
        """lib/algorithms|infrastructure 未建立时，门必须红灯（如实），不得为绿放宽。"""
        legacy_present = (REPO / "lib" / "star_detector").exists()
        targets_present = (REPO / "lib" / "algorithms").is_dir() or (REPO / "lib" / "infrastructure").is_dir()
        if legacy_present and not targets_present:
            self.assertNotEqual(self.proc.returncode, 0,
                                "ARCH-001 未落地时本门应红灯；变绿说明判据被放宽")
            self.assertEqual(self.data["summary"]["verdict"], "FAIL")
            for m in self.data["modules"]:
                self.assertIn(m["status"], {"NOT_IMPLEMENTED", "CONTRACT_READY", "FAIL"}, m["id"])

    def test_t25_no_facade_or_noop_judged_implemented(self):
        for m in self.data["modules"]:
            codes = {f["code"] for f in m["findings"]}
            if codes & {"facade_session_forward", "noop_entrypoint"}:
                self.assertNotIn(m["status"], {"IMPLEMENTED", "INSTALLED", "VERIFIED"}, m["id"])
                self.assertFalse(m["implemented"], m["id"])


if __name__ == "__main__":
    unittest.main()
