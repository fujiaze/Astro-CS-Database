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
      实现判定，且 ARCH-001 完成后旧命名面必须清零（回归即红）；把模块还原成
      「迁移前形态」（旧命名面在 + target_dir 不在）时，门不得判实现、缺实现必须如实红灯
      （夹具反事实，见 T24 的沿革注释）
  T7  状态词只取 ASTROCS_DESIGN §12.5 词表
  T8  检查器 --selftest 全绿（能绿能红自证）
"""
from __future__ import annotations

import json
import pathlib
import re
import shutil
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

    @staticmethod
    def _module(data, mid):
        return [m for m in data["modules"] if m["id"] == mid][0]

    @staticmethod
    def _details(mod, code):
        return [str(f.get("detail", "")) for f in mod["findings"] if f["code"] == code]

    # FIX-404 起，缺失声明不再报 missing_* 专用码，而是统一报 fake_path/fake_target
    # （未登记缺口 = 假路径；不得删声明，须登记 owner）。GATE-502 按现行检查器语义
    # 收紧断言：既锁 finding 码，也锁**具体模块 + 具体字段 + 具体路径**，判据未放宽。
    def test_t11_negative_missing_manifest(self):
        proc, data, _ = self._run("missing_manifest")
        self.assertNotEqual(proc.returncode, 0)
        mod = self._module(data, "calibration")
        self.assertFalse(mod["items"]["module_yaml"], "缺 module.yaml 必须判该模块未具备")
        hits = [d for d in self._details(mod, "fake_path") if "module_yaml 声明指向不存在的" in d]
        self.assertTrue(hits, "缺 module.yaml 必须报 fake_path(module_yaml) 且指明路径: %s"
                        % mod["findings"])

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
        mod = self._module(data, "star_detection")
        self.assertFalse(mod["items"]["cmake_target"], "缺 add_library 必须判该模块无 target")
        self.assertTrue(self._details(mod, "fake_target"),
                        "无 target 必须报 fake_target（声明指向不存在的 target）: %s" % mod["findings"])
        self.assertTrue([d for d in self._details(mod, "fake_path") if "target_file" in d],
                        "无 target 必须同时报 fake_path(target_file): %s" % mod["findings"])

    def test_t14_negative_missing_tests(self):
        proc, data, _ = self._run("missing_tests")
        self.assertNotEqual(proc.returncode, 0)
        mod = self._module(data, "psf")
        self.assertFalse(mod["items"]["co_located_tests"], "缺共址测试目录必须判该模块未具备")
        hits = [d for d in self._details(mod, "fake_path") if "co_located_tests 声明指向不存在的" in d]
        self.assertTrue(hits, "缺共址测试必须报 fake_path(co_located_tests): %s" % mod["findings"])

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
    """T6/T7：真实仓库诚实判定（实现只认 target_dir；状态词只取 §12.5）。"""

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

    def test_t21_status_words_are_12_5_only(self):
        self.assertEqual(set(self.data["status_vocabulary"]), VOCABULARY)
        for m in self.data["modules"]:
            self.assertIn(m["status"], VOCABULARY, m["id"])
            self.assertIn(m["verification_status"], VOCABULARY, m["id"])

    def test_t22_implementation_only_from_target_dir(self):
        """红线：没有目标目录 ⇒ 绝不可能是 IMPLEMENTED/INSTALLED/VERIFIED。

        原写法是 `if not target.is_dir(): assertNotIn(status, 正阶梯)` —— ARCH-001 落地后
        23/23 target_dir 全在位 ⇒ 该分支恒不执行（空断言）。这里改成**逻辑等价的逆否形式**
        （正阶梯状态 ⇒ target_dir 必须是实物），断言因此在当前树上每次都真的跑：
        当前 6 个模块 implemented=True，逐条受检。反事实（target_dir 缺失）由 T24 在夹具上守。
        """
        checked = 0
        for m in self.data["modules"]:
            target = REPO / m["target_dir"]
            if m["status"] in POSITIVE_RUNGS or m["implemented"]:
                checked += 1
                self.assertTrue(target.is_dir(),
                                "%s 被判 %s（implemented=%s），但 target_dir 不存在：%s"
                                % (m["id"], m["status"], m["implemented"], m["target_dir"]))
        self.assertGreater(checked, 0, "无任何正阶梯模块 ⇒ 本用例退化为空断言")

    def test_t23_legacy_dirs_never_count_as_implemented(self):
        """ARCH-001 完成态棘轮：旧命名面必须清零，且门不得据旧目录判任何实现。

        原写法 `if (REPO/legacy).exists() and not target.is_dir(): assertNotIn(status, 正阶梯)`
        在本树恒不执行（16 组旧命名目录全不存在、23/23 target_dir 全在位）——空断言。
        MODULE_MAP.yaml 头注第 18-20 行：「迁移完成后这些路径在树中一律不存在」⇒ 旧命名面
        回归是必须判红的缺陷，据此重钉两条当前真实成立、有判别力的断言：
          ① 16 组旧命名目录在树中必须全部不存在（mkdir 任一个即红）；
          ② 门必须报零条 legacy_paths_present（旧目录回来而未同步删 legacy_paths 登记 ⇒
             门出 NOTE，此处即刻变红）。
        「旧目录存在也不得被算成实现」的**门侧反事实**由
        test_t24_pre_migration_shape_is_never_judged_implemented 在夹具上守；声明面口径
        （MODULE_MAP 的 23 条 legacy_paths）另由
        eng/tests/quality/test_mod002_migration_refs.py::test_t01b_legacy_paths_are_pre_migration_paths
        （第 146-155 行）守 —— 本用例是硬编码配对表的回归棘轮，两张表任一被改都拦得住。
        """
        present = sorted(p for _mid, p in LEGACY_PAIRS if (REPO / p).exists())
        self.assertEqual(present, [],
                         "ARCH-001 迁移清单内的旧命名面必须已不在树中（迁移不得留双份）：%s" % present)
        notes = [(x["module"], x["detail"]) for x in self.data["findings"]
                 if x["code"] == "legacy_paths_present"]
        self.assertEqual(notes, [], "门观测到旧命名面仍在树中（旧命名面回归）：%s" % notes)

    def test_t24_pre_migration_shape_is_never_judged_implemented(self):
        """反事实注入：把模块还原成 ARCH-001 前形态 ⇒ 绝不判实现，缺实现必须如实红灯。

        沿革（为什么重钉）：本用例原断言「lib/star_detector 在 且 lib/algorithms|infrastructure
        不在 ⇒ 本门必红、23 模块状态全非正阶梯」。ARCH-001 落地后该前提恒假 ⇒ 断言块永不执行
        （空断言，AGENTS.md §5）。迁移前形态作为**状态**已不存在，但它守的判据是**永久**的
        （check_module_map.py 头注「诚实性红线」第 68-74 行；MODULE_MAP.yaml 头注第 18-22 行）：
        实现只认 target_dir，旧命名面存在不得成为实现证据，缺实现的门不得为绿放宽。
        故把「状态快照」改成夹具上的**反事实注入**，三段断言当前都真的跑：
          ① 正例基线：该模块判 INSTALLED 且 implemented —— 判据可达，不是靠放宽换绿；
          ② 还原成迁移前形态（旧命名目录在 + target_dir 不在）且缺口**未登记** ⇒
             rc!=0、verdict=FAIL、该模块 NOT_IMPLEMENTED 且 implemented=False
             （= 原用例「迁移未落地时本门必红」的等价物，fail-closed）；
          ③ 同一形态但缺口**已登记**（owner=BLD-401）⇒ 判定不变（仍 NOT_IMPLEMENTED，
             绝不 IMPLEMENTED/INSTALLED/VERIFIED），只是严重级降为可见 GAP、rc 回到 0；
             旧命名面被观测为 NOTE(legacy_paths_present)，但不得抬升任何模块的状态。
        树侧（旧路径必须已不在树中）由本文件 test_t23 与
        test_mod002_migration_refs.py::test_t01b_legacy_paths_are_pre_migration_paths:146 守。
        """
        with tempfile.TemporaryDirectory(prefix="mod001-premigration-") as td:
            tmp = pathlib.Path(td)
            row = yaml.safe_load(MAP.read_text(encoding="utf-8"))["modules"][0]
            mid = str(row["id"])
            tdir = str(row["target_dir"])
            legacy = str((row.get("legacy_paths") or [""])[0])
            self.assertTrue(legacy, "注入锚失效：映射表首行无 legacy_paths")

            def mod_of(data):
                return [x for x in data["modules"] if x["id"] == mid][0]

            def inject(root):
                """还原 ARCH-001 前形态：旧命名面在（带一份像样的生产源）、target_dir 不在。"""
                shutil.rmtree(root / tdir)
                (root / legacy / "src").mkdir(parents=True, exist_ok=True)
                (root / legacy / "src" / (mid + ".cpp")).write_text(
                    'extern "C" int ' + str(row["entrypoint"])
                    + '(const void *h, const char *r, void *o) '
                      "{ (void)h; (void)r; (void)o; return 0; }\n",
                    encoding="utf-8")

            def check(root, name):
                out = tmp / (name + ".json")
                proc = run_tool(repo_root=root, extra=("--json-out", str(out), "--quiet"))
                return proc, json.loads(out.read_text(encoding="utf-8"))

            # ① 正例基线：判据可达
            proc, data = check(fx.build_repo(tmp / "base"), "base")
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertIn(mod_of(data)["status"], POSITIVE_RUNGS, "夹具正例基线必须判正阶梯")
            self.assertTrue(mod_of(data)["implemented"])

            # ② 迁移前形态 + 缺口未登记 ⇒ 如实红灯
            root = fx.build_repo(tmp / "unregistered")
            inject(root)
            proc, data = check(root, "unregistered")
            self.assertNotEqual(proc.returncode, 0, "缺实现且未登记缺口必须红灯（fail-closed）")
            self.assertEqual(data["summary"]["verdict"], "FAIL")
            mod = mod_of(data)
            self.assertEqual(mod["status"], "NOT_IMPLEMENTED", "旧命名面在不得被判实现")
            self.assertFalse(mod["implemented"])
            self.assertIn("fake_path",
                          {x["code"] for x in mod["findings"] if x["severity"] == "FAIL"})

            # ③ 同一形态 + 缺口已登记 ⇒ 判定不变，只降严重级
            root = fx.build_repo(tmp / "registered")
            inject(root)
            mp = root / "docs/modules/MODULE_MAP.yaml"
            doc = yaml.safe_load(mp.read_text(encoding="utf-8"))
            doc["declared_absent_paths"] = {
                "checked_at": "fixture", "path_gap_count": 1, "target_gap_count": 0,
                "items": [{"module": mid, "key": "target_dir", "path": tdir,
                           "owner": "BLD-401", "reason": "夹具反事实：还原 ARCH-001 前形态"}]}
            mp.write_text(yaml.safe_dump(doc, allow_unicode=True, sort_keys=False),
                          encoding="utf-8")
            proc, data = check(root, "registered")
            mod = mod_of(data)
            # 判定先于 rc：注入「旧命名面算实现」时，下面这条必须第一个炸（诊断指向判定本身）
            self.assertEqual(mod["status"], "NOT_IMPLEMENTED",
                             "缺口登记只降严重级，不得把「缺实现」改判成实现")
            self.assertFalse(mod["implemented"])
            self.assertNotIn(mod["status"], POSITIVE_RUNGS)
            self.assertEqual(proc.returncode, 0, "已登记缺口 ⇒ 严重级降为 GAP，rc 不为红")
            self.assertIn("declared_absent_registered", {x["code"] for x in mod["findings"]})
            self.assertEqual([(x["module"], x["severity"]) for x in data["findings"]
                              if x["code"] == "legacy_paths_present"], [(mid, "NOTE")],
                             "旧命名面必须被观测为 NOTE，且不参与实现判定")
            for x in data["modules"]:
                if x["status"] in POSITIVE_RUNGS or x["implemented"]:
                    self.assertTrue((root / x["target_dir"]).is_dir(),
                                    "%s 在迁移前形态下被判 %s，但其 target_dir 不存在"
                                    % (x["id"], x["status"]))

    def test_t25_no_facade_or_noop_judged_implemented(self):
        for m in self.data["modules"]:
            codes = {f["code"] for f in m["findings"]}
            if codes & {"facade_session_forward", "noop_entrypoint"}:
                self.assertNotIn(m["status"], {"IMPLEMENTED", "INSTALLED", "VERIFIED"}, m["id"])
                self.assertFalse(m["implemented"], m["id"])


class TestGapLedgerRatchet(unittest.TestCase):
    """FIX-404 缺口台账（50 路径 + 16 target + 能力缺口）的测试侧棘轮（当前规模见 T30 的沿革注释）。

    GATE-502 步骤 5 逐项甄别结论（当时 66 条路径/target + 72 条能力缺口；能力侧随后按
    「缺口落地 ⇒ 删登记」各减 1，现为 70，见 T30 沿革注释）：66 条路径/target 缺口与能力缺口**全部为
    「待实现」**（模块/schema/unit 未落地），owner = BLD-401（全门收口）或
    RELEASE-04/未覆盖（GUI/HiPS Browser，本包显式不做）
    ⇒ **标 DEFERRED，不进全绿集**：不为未实现模块写假绿测试（空骨架测试 = 假绿），
    也不放宽任何现有判据。本类只锁台账**诚实性**，四条都能红：
      T30 台账条数与机器实测 registered_gaps 必须相等（漏登记/删声明都红）；
      T31 每条缺口必须有可追责 owner + 理由，owner 必须命中 gap_owners，
          kind=task 的 owner 必须有任务书文件（fail-closed，与检查器同口径）；
      T32 棘轮：登记为缺口的路径/target **今天必须仍然缺失**（模块落地后不删登记 = 红）；
      T33 fake_paths == 0（不得靠删声明过门），且台账条目集合 == 机器实测条目集合。
    """

    @classmethod
    def setUpClass(cls):
        cls.doc = yaml.safe_load(MAP.read_text(encoding="utf-8"))
        cls.paths = cls.doc["declared_absent_paths"]
        cls.caps = cls.doc["declared_absent_capabilities"]
        cls.owners = {o["id"]: o for o in cls.doc["gap_owners"]}
        cls._td = tempfile.TemporaryDirectory(prefix="mod001-gap-")
        cls.proc, cls.data = run_real_repo_json(cls._td.name)

    @classmethod
    def tearDownClass(cls):
        cls._td.cleanup()

    def test_t30_ledger_counts_are_self_consistent(self):
        items = self.paths["items"]
        self.assertEqual(self.paths["path_gap_count"] + self.paths["target_gap_count"],
                         len(items), "path_gap_count + target_gap_count 必须等于条目数")
        n_target = len([i for i in items if i["key"] == "target"])
        self.assertEqual(self.paths["target_gap_count"], n_target)
        self.assertEqual(self.paths["path_gap_count"], len(items) - n_target)
        # 70 = 台账 declared_absent_capabilities 的当前规模（只减不增的棘轮：每次规模变化必须
        # 在此显式改登记，并写明依据）。沿革：72 → 71（2026-09-22 前台复核删除 noise_snr 的陈旧
        # header_missing_abi_version 登记）→ 70（2026-09-23 删除 star_detection 的陈旧
        # missing_co_located_tests 登记：该模块共址测试 tests/p1star/ 已落地，检查器
        # CHK-MODULE-MANIFEST 的 stale_declared_absent_capability 判红要求随落地删登记）。
        self.assertEqual(70, self.caps["capability_gap_count"], "能力缺口规模漂移须显式改登记")
        self.assertEqual(self.caps["capability_gap_count"], len(self.caps["items"]))
        self.assertEqual(66, self.data["gaps"]["registered"],
                         "台账 66 条必须与机器实测 registered_gaps 相等（双向对齐）")
        self.assertEqual(len(items), self.data["gaps"]["registered"])

    def test_t31_every_item_has_traceable_owner(self):
        for it in self.paths["items"] + self.caps["items"]:
            self.assertIn(it["owner"], self.owners,
                          "%s/%s 的 owner %r 未登记于 gap_owners" % (it["module"], it["key"] if "key" in it else it["code"], it["owner"]))
            self.assertTrue(str(it.get("reason", "")).strip(),
                            "%s 缺理由（不得只登记 owner 不写原因）" % it["module"])
        for oid, o in self.owners.items():
            if o.get("kind") == "task":
                self.assertTrue((REPO / o["authority"]).is_file(),
                                "task owner %s 的任务书不存在: %s" % (oid, o["authority"]))
            else:
                self.assertIn(o.get("kind"), {"out_of_scope"},
                              "gap_owners.kind 只允许 task/out_of_scope: %s" % oid)

    def test_t32_declared_absent_paths_are_still_absent(self):
        """棘轮：登记缺口一旦落地（路径出现 / target 定义）必须同步删登记 + 补测试。"""
        cmake = "\n".join(
            p.read_text(encoding="utf-8", errors="ignore")
            for p in REPO.rglob("CMakeLists.txt") if "build" not in p.parts)
        for it in self.paths["items"]:
            if it["key"] == "target":
                self.assertNotRegex(cmake, r"add_(?:library|executable)\s*\(\s*%s\b" % re.escape(it["path"]),
                                    "target %s 已定义：缺口落地后必须删登记并补测试" % it["path"])
            else:
                self.assertFalse((REPO / it["path"]).exists(),
                                 "%s 已存在：缺口落地后必须删登记并补测试" % it["path"])

    def test_t33_no_fake_paths_and_item_sets_match(self):
        self.assertEqual(0, self.data["gaps"]["fake_paths"],
                         "fake_paths 必须为 0（不得为过门删声明）")
        ledger = {(i["module"], i["key"], i["path"]) for i in self.paths["items"]}
        measured = {(i["module"], i["key"], i["path"]) for i in self.data["gaps"]["items"]}
        self.assertEqual(ledger, measured,
                         "台账条目与机器实测缺口必须逐条相等；差集=" + repr(sorted(ledger ^ measured)[:6]))


if __name__ == "__main__":
    unittest.main()

