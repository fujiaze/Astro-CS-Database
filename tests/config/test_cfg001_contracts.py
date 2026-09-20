#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CFG-001 验收门（正向 + 机器断言）。

跑法（控制包指定）：
  python3 -m unittest discover -s tests/config -t tests/config
所有断言都直接读仓库文件，零第三方依赖（jsonschema 子集用 tests/contracts/v6/jsonschema_min.py）。
"""
import json
import os
import re
import unittest

import cfg_common as C

PHASE_SCHEMAS = {
    "normalize": "contracts/schemas/phase_config_normalize.schema.json",
    "mosaic": "contracts/schemas/phase_config_mosaic.schema.json",
    "export": "contracts/schemas/phase_config_export.schema.json",
}
TEMPLATES = {
    "normalize": "config/templates/normalize.phase_config.json",
    "mosaic": "config/templates/mosaic.phase_config.json",
    "export": "config/templates/export.phase_config.json",
}
CPU_SCHEMA = "contracts/schemas/cpu_profile.schema.json"
MANIFEST_SCHEMA = "contracts/schemas/run_manifest.schema.json"
ANCHORS = "docs/contracts/config_separation_anchors.json"
DEFAULTS = "config/defaults.json"
FILTERS = "config/filters.json"
FIELDS_TRANSCRIBED = ["name", "channel", "wavelength_nm", "value", "n_points"]
MIN_EXPECTED_FILTERS = 45   # 编制时实测条数；少于该数说明转录丢失（滤镜库可扩充，多于不算失败）

# 负责人实测指认的权威锚点（仍须逐条核对原文）——(字段, 文件, 行, 该行必须出现的 token)
KEY_ANCHORS = [
    ("detection.threshold_sigma", "docs/science/STAR_DETECTION.md", 19, "5.0"),
    ("psf.default_model", "docs/science/PSF.md", 7, "Moffat4"),
    ("psf.moffat_beta", "docs/science/PSF.md", 92, "4"),
    ("noise.source_mask_radius_px", "docs/science/NOISE_MODEL.md", 84, "rmax"),
    ("noise.variance_floor", "docs/science/NOISE_MODEL.md", 21, "1e-12"),
    ("rejection.sigma.lower_sigma", "docs/science/REJECTION.md", 66, "4.0/3.0/8"),
    ("photometry.mag_tolerance", "docs/science/PHOTOMETRY.md", 28, "3.0 mag"),
    ("weight.default_mode", "docs/science/PSF_SIGNAL_WEIGHT.md", 12, "psf_information_weight"),
    ("precision.default", "docs/science/SCIENCE_SCOPE.md", 53, "FP64"),
    ("upm.k_corr", "docs/science/PHASE2_UPM.md", 22, "1.4"),
    ("hips.tile_width", "docs/science/PHASE3_HIPS_TO_FITS.md", 39, "512"),
    ("drizzle.pixfrac", "docs/science/DRIZZLE.md", 33, "pixfrac"),
]
PENDING_EXPECTED = {
    # drizzle.pixfrac 已于 DOC-SCI-001 §3 裁决落地（defaults.json value=1.0，
    # authority_status=owner_adjudicated），不再是 pending 项。
    "sparse_snr.density": "SCI-RES-01/R-001",
    "scalar_gate.rd": "SCI-RES-01/R-002",
    "scalar_gate.trend": "SCI-RES-01/R-002",
}


def defaults_fields():
    doc = C.load_json(DEFAULTS)
    return doc, {f["key"]: f for f in doc["fields"]}


class TestPhaseConfigFamily(unittest.TestCase):
    """门 1：三份模板必须通过各自 schema；schema 只含科学参数/路径/output_dir/算法选择。"""

    def test_templates_pass_their_schema(self):
        for phase, tpl_rel in TEMPLATES.items():
            schema = C.load_json(PHASE_SCHEMAS[phase])
            tpl = C.load_json(tpl_rel)
            errs = C.validate(schema, tpl)
            self.assertEqual([], errs, "%s 模板未通过 %s: %s" % (tpl_rel, PHASE_SCHEMAS[phase], errs))
            # phase 身份：mosaic/export 用模板的 phase_name 判别键；normalize 已按
            # GAP_AUDIT §9.68 改为多数据块形态（无 phase_name），身份由 schema 的
            # x-astrocs-phase 承载 + 模板必须给出非空 blocks[]。
            self.assertEqual(phase, schema["x-astrocs-phase"])
            if "phase_name" in tpl:
                self.assertEqual(phase, tpl["phase_name"])
            else:
                self.assertIn("blocks", tpl, "%s 模板既无 phase_name 也无 blocks" % tpl_rel)
                self.assertTrue(tpl["blocks"], "%s 模板 blocks 为空" % tpl_rel)

    def test_templates_have_no_hardware_field_names(self):
        banned = self._banned_names()
        banned_re = re.compile(r"^(isa|isa_[a-z0-9_]+|workers?|worker_count|block|block_[a-z0-9_]+|cpu_[a-z0-9_]+|thread_budget|affinity_mask)$")
        for phase, tpl_rel in TEMPLATES.items():
            tpl = C.load_json(tpl_rel)
            for key in C.json_key_paths(tpl):
                leaf = key.rsplit("/", 1)[-1]
                self.assertNotIn(leaf, banned, "%s 含硬件字段名 %s（%s）" % (tpl_rel, leaf, key))
                self.assertIsNone(banned_re.match(leaf), "%s 含硬件族字段名 %s（%s）" % (tpl_rel, leaf, key))

    def test_schemas_have_no_hardware_field_names(self):
        banned = self._banned_names()
        banned_re = re.compile(r"^(isa|isa_[a-z0-9_]+|workers?|worker_count|block|block_[a-z0-9_]+|cpu_[a-z0-9_]+|thread_budget|affinity_mask)$")
        for phase, rel in PHASE_SCHEMAS.items():
            names = C.property_names(C.load_json(rel))
            for name in names:
                self.assertNotIn(name, banned, "%s 定义了硬件字段名 %s" % (rel, name))
                self.assertIsNone(banned_re.match(name), "%s 定义了硬件族字段名 %s" % (rel, name))

    def test_filter_enum_equals_library_keys(self):
        lib = list(C.load_json(FILTERS)["filters"].keys())
        for phase, rel in PHASE_SCHEMAS.items():
            schema = C.load_json(rel)
            enum = schema["$defs"]["filter_name"]["enum"]
            self.assertEqual(lib, enum, "%s 的滤镜枚举与 config/filters.json 不一致" % rel)

    def test_phase_config_not_writable_by_benchmark(self):
        for phase, rel in PHASE_SCHEMAS.items():
            schema = C.load_json(rel)
            self.assertEqual("phase_config", schema["x-astrocs-config-class"])
            self.assertNotIn("benchmark", schema["x-astrocs-writer"])
            self.assertIn("benchmark", schema["x-astrocs-not-writable-by"])

    def test_no_aggregate_second_definition(self):
        """锚点保留路径 phase_config.schema.json 实现为三份 phase 专属 schema，不另留聚合等价定义。"""
        self.assertFalse(os.path.exists(os.path.join(C.REPO, "contracts/schemas/phase_config.schema.json")))

    def _banned_names(self):
        anchors = C.load_json(ANCHORS)
        return set(anchors["cross_class_forbidden"]["cpu_profile_keys_in_phase_config"])


class TestCrossClassDisjointness(unittest.TestCase):
    """门 7：cpu_profile 字段名 ∩ phase_config 字段名 == 空集（附 legacy 同名登记）。"""

    def setUp(self):
        self.phase = set()
        for rel in PHASE_SCHEMAS.values():
            self.phase |= C.property_names(C.load_json(rel))
        self.cpu = C.load_json(CPU_SCHEMA)
        self.manifest = set(C.property_names(C.load_json(MANIFEST_SCHEMA)))

    def test_phase_config_vs_current_cpu_profile_is_empty(self):
        c_v2 = C.property_names(self.cpu["$defs"]["profile_v2"])
        self.assertEqual(set(), self.phase & c_v2, "phase_config 与 cpu_profile v2 字段名冲突: %s" % (self.phase & c_v2))

    def test_phase_config_vs_legacy_v1_intersection_is_documented(self):
        c_legacy = C.property_names(self.cpu["$defs"]["legacy_v1"]) | C.property_names(self.cpu["$defs"]["kernel_v1"])
        self.assertEqual({"precision"}, self.phase & c_legacy,
                         "legacy v1 与 phase_config 的同名集合发生变化（需负责人裁决）: %s" % (self.phase & c_legacy))

    def test_phase_config_vs_whole_cpu_profile_intersection_is_documented(self):
        c_all = C.property_names(self.cpu)
        self.assertEqual({"precision"}, self.phase & c_all,
                         "cpu_profile ∩ phase_config 除已登记的 precision 外出现新冲突: %s" % (self.phase & c_all))

    def test_phase_config_vs_run_manifest_is_empty(self):
        self.assertEqual(set(), self.phase & self.manifest, "phase_config 与 run_manifest 字段名冲突: %s" % (self.phase & self.manifest))

    def test_cpu_profile_has_no_phase_config_reserved_names(self):
        anchors = C.load_json(ANCHORS)
        reserved = set(anchors["cross_class_forbidden"]["phase_config_keys_in_cpu_profile"])
        self.assertEqual(set(), reserved & C.property_names(self.cpu))


class TestDefaultsContract(unittest.TestCase):
    """门 3：字段数 == 带 unit 数 == 带 source 或 pending_authority 数；来源不明 == 0。"""

    def test_counts_and_no_unknown_source(self):
        doc, fields = defaults_fields()
        total = len(doc["fields"])
        self.assertEqual(total, doc["field_count"], "field_count 与实际字段数不一致")
        self.assertEqual(len(fields), total, "字段 key 重复")
        with_unit = [k for k, f in fields.items() if isinstance(f.get("unit"), str) and f["unit"].strip()]
        attributed = [k for k, f in fields.items()
                      if (isinstance(f.get("source"), str) and f["source"].strip())
                      or (f.get("authority_status") == "pending_authority" and f.get("pending_task"))]
        unknown = [k for k, f in fields.items() if k not in attributed]
        self.assertEqual(total, len(with_unit), "缺 unit 的字段: %s" % sorted(set(fields) - set(with_unit)))
        self.assertEqual(total, len(attributed), "来源不明的字段: %s" % unknown)
        self.assertEqual([], unknown)
        # 非 pending 字段不得用 unspecified 单位蒙混
        for k, f in fields.items():
            if f["authority_status"] != "pending_authority":
                self.assertFalse(f["unit"].startswith("unspecified"), "%s 的 unit 未定义" % k)

    def test_pending_items_are_the_adjudicated_gap_set(self):
        _doc, fields = defaults_fields()
        pending = {k: f.get("pending_task") for k, f in fields.items() if f["authority_status"] == "pending_authority"}
        self.assertEqual(set(PENDING_EXPECTED), set(pending), "pending_authority 集合与裁决不一致")
        for key, task in PENDING_EXPECTED.items():
            self.assertIn(task, pending[key])
            self.assertIsNone(fields[key]["value"], "%s 无权威数值，value 必须为 null（禁止编造）" % key)
            self.assertIsNone(fields[key]["source"])

    def test_dark_light_tolerance_is_owner_adjudicated_5s(self):
        _doc, fields = defaults_fields()
        f = fields["calibration.dark_light_exposure_tolerance"]
        self.assertEqual(5, f["value"])
        self.assertEqual("s", f["unit"])
        self.assertEqual("owner_adjudicated", f["authority_status"])
        self.assertIn("负责人裁决", f["source"])
        self.assertIn("SCI-RES-01/R-004", f["source"])
        self.assertIn("SCI-RES-01/R-003", f["source"])

    def test_every_source_ref_resolves_and_key_anchors_hold(self):
        _doc, fields = defaults_fields()
        for key, f in fields.items():
            ref = f.get("source_ref")
            if ref is None:
                self.assertNotEqual("sourced", f["authority_status"],
                                    "%s 标 sourced 但没有 source_ref（文件:行）" % key)
                continue
            lines = C.read_lines(ref["path"])
            self.assertGreaterEqual(ref["line"], 1)
            self.assertLessEqual(ref["line"], len(lines), "%s 的 source_ref 行号越界" % key)
        # 一次报告全部锚点失败（fail-fast 会掩盖后续漂移：CFG-002 实测 3 处漂移只报第 1 处）
        failures = []
        for key, path, line, token in KEY_ANCHORS:
            if key not in fields:
                failures.append("权威锚点字段缺失: %s" % key)
                continue
            if not C.grep_line(path, line, token):
                failures.append("%s 的 source_ref 不成立：%s:%d 不含 %r" % (key, path, line, token))
            if path != fields[key]["source_ref"]["path"]:
                failures.append("%s 的 source_ref.path 漂移：测试期望 %s，登记 %s"
                                % (key, path, fields[key]["source_ref"]["path"]))
            if line != fields[key]["source_ref"]["line"]:
                failures.append("%s 的 source_ref.line 漂移：测试期望 %d，登记 %d"
                                % (key, line, fields[key]["source_ref"]["line"]))
        self.assertEqual([], failures, "锚点失败 %d 条：\n  - %s" % (len(failures), "\n  - ".join(failures)))

    def test_design_named_defaults_all_present(self):
        """ASTROCS_DESIGN §3.3 点名的默认值项必须出现（含三项无权威数值者）。"""
        _doc, fields = defaults_fields()
        for key in ["calibration.dark_light_exposure_tolerance", "psf.default_model",
                    "detection.threshold_sigma", "scalar_gate.rd", "scalar_gate.trend",
                    "sparse_snr.density"]:
            self.assertIn(key, fields, "设计点名项缺失: %s" % key)

    def test_calibration_doc_has_no_numeric_tolerance(self):
        """5 s 来自负责人裁决：docs/science/CALIBRATION.md 只有 K=t_light/t_dark 语义（上限自证）。"""
        lines = C.read_lines("docs/science/CALIBRATION.md")
        self.assertTrue(any("t_light/t_dark" in ln for ln in lines), "CALIBRATION.md 应含 K=t_light/t_dark 语义")
        self.assertFalse(any(re.search(r"容差.*\b\d+(\.\d+)?\s*s\b", ln) for ln in lines),
                         "CALIBRATION.md 若已有容差数值，则本条 source 需改为文档来源")


class TestFiltersLibrary(unittest.TestCase):
    """门：逐字转录 + provenance 标注 + 无零点列 + 未知滤镜 error。"""

    def setUp(self):
        self.doc = C.load_json(FILTERS)
        # 定位符由 config/filters.json 自己声明（仓库目录迁移后仍成立），内容由 sha256 冻结
        self.filter_src_rel = self.doc["transcription"]["source_path"]
        self.prov_src_rel = self.doc["provenance"]["source_path"]
        self.src = C.load_json(self.filter_src_rel)
        self.prov = C.load_json(self.prov_src_rel)

    def test_source_locators_resolve_and_are_hash_frozen(self):
        self.assertTrue(self.filter_src_rel.startswith("lib/"), "转录源必须位于 lib/ 下")
        self.assertTrue(self.prov_src_rel.startswith("lib/"), "provenance 源必须位于 lib/ 下")
        self.assertEqual("filters.json", os.path.basename(self.filter_src_rel))
        self.assertEqual("filter_qe_provenance.json", os.path.basename(self.prov_src_rel))
        self.assertEqual(self.doc["transcription"]["source_sha256"], C.sha256_file(self.filter_src_rel),
                         "滤镜库源文件内容已变（sha256 不匹配）——转录必须重做")
        self.assertEqual(self.doc["transcription"]["source_bytes"], C.file_size(self.filter_src_rel))
        self.assertEqual(self.doc["provenance"]["source_sha256"], C.sha256_file(self.prov_src_rel),
                         "provenance 源文件内容已变（sha256 不匹配）")

    def test_verbatim_transcription(self):
        self.assertEqual(len(self.src), self.doc["transcription"]["filter_count"])
        self.assertGreaterEqual(len(self.doc["filters"]), MIN_EXPECTED_FILTERS,
                                "滤镜条数少于编制时实测的 %d 条" % MIN_EXPECTED_FILTERS)
        for name, rec in self.src.items():
            self.assertIn(name, self.doc["filters"])
            got = self.doc["filters"][name]
            self.assertEqual({k: rec[k] for k in FIELDS_TRANSCRIBED}, got,
                             "%s 的转录与原文件不一致（禁止重采样/改数值）" % name)

    def test_each_filter_record_has_only_transcribed_fields(self):
        for name, rec in self.doc["filters"].items():
            self.assertEqual(sorted(FIELDS_TRANSCRIBED), sorted(rec.keys()),
                             "%s 出现了转录字段之外的列" % name)

    def test_provenance_marks_unverified_and_gap025(self):
        prov = self.doc["provenance"]
        self.assertEqual("unverified", prov["status"])
        self.assertEqual("GAP-025", prov["gap_id"])
        self.assertEqual(self.prov_src_rel, prov["source_path"])
        self.assertEqual(len(self.src), len(prov["per_filter"]))
        for name in self.src:
            entry = prov["per_filter"][name]
            self.assertEqual(self.prov["filters"][name]["source"], entry["source"])
            self.assertTrue(entry["source"].startswith("unverified"), "%s provenance 未如实标注未验证" % name)
            self.assertFalse(entry["verified"])
            self.assertIsNone(entry["url"])

    def test_curve_stats_match_curves(self):
        for name, rec in self.doc["filters"].items():
            stats = self.doc["provenance"]["per_filter"][name]["curve_stats"]
            self.assertEqual(rec["n_points"], stats["n_points"])
            self.assertEqual(len(rec["wavelength_nm"]), rec["n_points"])
            self.assertEqual(len(rec["value"]), rec["n_points"])
            self.assertEqual(min(rec["wavelength_nm"]), stats["wl_min"])
            self.assertEqual(max(rec["wavelength_nm"]), stats["wl_max"])
            self.assertEqual(min(rec["value"]), stats["val_min"])
            self.assertEqual(max(rec["value"]), stats["val_max"])

    def test_no_zero_point_column_anywhere(self):
        for path in C.json_key_paths(self.doc):
            self.assertIsNone(C.ZERO_RE.search(path), "滤镜库出现禁用键位: %s" % path)
        text = C.load_text(FILTERS)
        self.assertIsNone(C.ZERO_RE.search(text),
                          "滤镜库文本出现 zero 字样（零点列必须完全不出现）")

    def test_lookup_policy_unknown_filter_is_error(self):
        self.assertEqual("error", self.doc["lookup"]["unknown_filter"])
        self.assertEqual("exact", self.doc["lookup"]["match"])


class TestCpuProfileMigration(unittest.TestCase):
    """裁决四：cpu_profile 单一文件（v1 legacy + v2 现行），只有 benchmark 可写。"""

    def setUp(self):
        self.schema = C.load_json(CPU_SCHEMA)

    def test_single_definition(self):
        schemas_dir = os.path.join(C.REPO, "contracts", "schemas")
        matches = [f for f in os.listdir(schemas_dir) if "profile" in f and f.endswith(".json")]
        self.assertEqual(["cpu_profile.schema.json"], sorted(matches),
                         "contracts/schemas 下出现第二份 profile 定义")

    def test_writer_is_benchmark_only(self):
        self.assertEqual("cpu_profile", self.schema["x-astrocs-config-class"])
        self.assertEqual("benchmark", self.schema["x-astrocs-writer"])
        for who in ("用户", "cli --template", "phase_config"):
            self.assertIn(who, self.schema["x-astrocs-not-writable-by"])

    def test_legacy_accessors_preserved(self):
        """既有读取面（tests/backend + tools/validate_cpu_profile.py）不得失去访问路径。"""
        self.assertIsInstance(self.schema["required"], list)
        self.assertIn("build", self.schema["required"])
        self.assertIn("kernels", self.schema["required"])
        items_required = self.schema["properties"]["kernels"]["items"]["required"]
        for k in ("kernel_id", "kernel_version", "precision", "size_class", "backend_id",
                  "workers", "block_size", "oracle_status", "measurements"):
            self.assertIn(k, items_required)
        legacy_required = self.schema["$defs"]["legacy_v1"]["required"]
        for k in ("schema_version", "created_at_utc", "mode", "hardware", "build",
                  "memory_benchmark", "kernels", "verdict"):
            self.assertIn(k, legacy_required)

    def test_both_branches_validate(self):
        for rel in ("tests/config/fixtures/positive/cpu_profile_v1_legacy.json",
                    "tests/config/fixtures/positive/cpu_profile_v2.json"):
            errs = C.validate(self.schema, C.load_json(rel))
            self.assertEqual([], errs, "%s 未通过 cpu_profile schema: %s" % (rel, errs))

    def test_v2_binds_hardware_and_software_identity(self):
        v2 = self.schema["$defs"]["profile_v2"]
        host = v2["properties"]["host"]["required"]
        for k in ("vendor", "family", "model", "stepping", "os_abi", "features", "xcr0", "logical_available"):
            self.assertIn(k, host, "v2 host 未绑定 %s" % k)
        build = v2["properties"]["build"]["required"]
        for k in ("astrocs_version", "source_commit", "benchmark_binary_sha256", "runtime_build_id", "provider_build_ids"):
            self.assertIn(k, build, "v2 build 未绑定 %s" % k)
        kernel = self.schema["$defs"]["kernel_v2"]
        for k in ("provider", "workers", "block", "self_test_sha256"):
            self.assertIn(k, kernel["required"], "v2 kernel 未绑定 %s" % k)
        self.assertEqual(["baseline", "avx2", "avx512"], kernel["properties"]["provider"]["enum"])


class TestRunManifest(unittest.TestCase):
    def test_positive_fixture_passes(self):
        errs = C.validate(C.load_json(MANIFEST_SCHEMA),
                          C.load_json("tests/config/fixtures/positive/run_manifest.example.json"))
        self.assertEqual([], errs, errs)

    def test_freezes_hashes_and_toolchain(self):
        schema = C.load_json(MANIFEST_SCHEMA)
        for k in ("run_id", "software_sha", "config_hash", "manifest_input_hashes",
                  "manifest_output_hashes", "toolchain_version"):
            self.assertIn(k, schema["required"], "run_manifest 未冻结 %s" % k)


if __name__ == "__main__":
    unittest.main(verbosity=2)
