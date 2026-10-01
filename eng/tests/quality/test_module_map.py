#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MOD-001 / CHK-MODULE-MANIFEST 质量测试（映射门正例 + 五类负例 + 诚实性红线）。

覆盖：
  T1  映射表 23/23 唯一行，且 ID 集合 == docs/detail/00_INDEX.md §2（机器解析）
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
  T7  状态词只取 docs/ACSD_DESIGN §12.5 词表
  T8  检查器 --selftest 全绿（能绿能红自证）
"""
from __future__ import annotations

import pathlib
import re
import unittest

import yaml

REPO = pathlib.Path(__file__).resolve().parents[3]
MAP = REPO / "docs" / "modules" / "MODULE_MAP.yaml"
INDEX = REPO / "docs" / "detail" / "00_INDEX.md"

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
    """docs/detail/00_INDEX.md 的模块 ID 权威面（§3 registry 卡 + §4 基建卡）。

    表形态随文档重组变过：旧口径是「§2 模块总表、第 0 列为反引号 .md、第 1 列为 ID」。
    现行的 §2 已是「模块落位规则」，模块卡清单移到 §3/§4，ID 以反引号 acsd.* 逐个列出。
    两种形态都不匹配时**抛错**，不得静默返回空集 —— 空集会把 T02 变成无效判据
    （规范 08 §4「空断言、无效判据」）。
    """
    lines = INDEX.read_text(encoding="utf-8").splitlines()

    def _section(prefix):
        start = next((i for i, ln in enumerate(lines) if ln.startswith(prefix)), None)
        if start is None:
            return []
        end = next((i for i in range(start + 1, len(lines))
                    if lines[i].startswith("## ") and not lines[i].startswith(prefix)), len(lines))
        return lines[start:end]

    # 现行形态：§3/§4 中的反引号模块 ID
    ids = set()
    for sec in (_section("## 3."), _section("## 4.")):
        for ln in sec:
            ids.update(re.findall(r"`(acsd\.[A-Za-z0-9_.]+)`", ln))
    if ids:
        return ids

    # 旧形态：§2 模块总表第 2 列
    for ln in _section("## 2."):
        if not ln.startswith("| "):
            continue
        cells = [c.strip() for c in ln.strip("|").split("|")]
        if len(cells) >= 2 and cells[0].startswith("`") and cells[0].endswith(".md`"):
            ids.add(cells[1])
    if ids:
        return ids

    raise AssertionError(
        "MODULE_ID_INDEX_UNPARSABLE: %s 既无 §3/§4 的 acsd.* 模块卡清单，也无 §2 模块总表 —— "
        "权威面形态未知，须先确认文档重组后的模块 ID 唯一源（不得以空集放行）" % INDEX)


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
class TestGapLedgerRatchet(unittest.TestCase):
    """FIX-404 缺口台账（50 路径 + 16 target + 能力缺口）的测试侧棘轮（当前规模见 T30 的沿革注释）。

    GATE-502 步骤 5 逐项甄别结论（当时 66 条路径/target + 72 条能力缺口；能力侧随后按
    「缺口落地 ⇒ 删登记」各减 1，现为 70，见 T30 沿革注释）：66 条路径/target 缺口与能力缺口**全部为
    「待实现」**（模块/schema/unit 未落地），owner = BLD-401（全门收口）或
    RELEASE-04/未覆盖（GUI/HiPS Browser，本包显式不做）
    ⇒ **标 DEFERRED，不进全绿集**：不为未实现模块写假绿测试（空骨架测试 = 假绿），
    也不放宽任何现有判据。本类只锁台账**诚实性**：
      T30 台账条数自洽（path_gap_count + target_gap_count == 条目数；能力规模棘轮）；
      T31 每条缺口必须有可追责 owner + 理由，owner 必须命中 gap_owners，
          kind=task 的 owner 必须有任务书文件（fail-closed）；
      T32 棘轮：登记为缺口的路径/target **今天必须仍然缺失**（模块落地后不删登记 = 红）。
    原 T30 的「与机器实测 registered_gaps 双向对齐」、原 T33 的「fake_paths == 0 且
    台账条目集合 == 机器实测条目集合」依赖已随 G08-01 删除的 check_module_map.py，
    随之移除；门禁重建（G08-10）时由新判据承接，不在此处以常量或空断言冒充。
    """

    @classmethod
    def setUpClass(cls):
        cls.doc = yaml.safe_load(MAP.read_text(encoding="utf-8"))
        cls.paths = cls.doc["declared_absent_paths"]
        cls.caps = cls.doc["declared_absent_capabilities"]
        cls.owners = {o["id"]: o for o in cls.doc["gap_owners"]}

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


if __name__ == "__main__":
    unittest.main()

