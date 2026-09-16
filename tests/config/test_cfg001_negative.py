#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CFG-001 负例（必须真实必败）：四门 + run_manifest/cpu_profile 回归负例。

跑法：python3 -m unittest discover -s tests/config -t tests/config
"""
import unittest

import cfg_common as C

NEG = "tests/config/fixtures/negative/"


def errors_for(schema_rel, instance_rel):
    return C.validate(C.load_json(schema_rel), C.load_json(instance_rel))


def messages(errors):
    return " | ".join("%s: %s" % ("/".join(str(p) for p in path) or "<root>", msg) for path, msg in errors)


class TestNegativeFixturesMustFail(unittest.TestCase):
    def test_negative_01_unknown_filter_name(self):
        errs = errors_for("contracts/schemas/phase_config_normalize.schema.json",
                          NEG + "unknown_filter.phase_config.json")
        self.assertTrue(errs, "未知滤镜名必须失败")
        self.assertIn("enum", messages(errs))
        self.assertIn("bader r", messages(errs))

    def test_negative_02_missing_output_dir(self):
        errs = errors_for("contracts/schemas/phase_config_normalize.schema.json",
                          NEG + "missing_output_dir.phase_config.json")
        self.assertTrue(errs, "缺 output_dir 必须失败")
        self.assertIn("required: missing 'output_dir'", messages(errs))

    def test_negative_03_precision_out_of_domain(self):
        errs = errors_for("contracts/schemas/phase_config_normalize.schema.json",
                          NEG + "precision_out_of_domain.phase_config.json")
        self.assertTrue(errs, "precision 越界必须失败")
        self.assertIn("enum", messages(errs))
        self.assertIn("fp128", messages(errs))

    def test_negative_04_hardware_fields_in_phase_config(self):
        errs = errors_for("contracts/schemas/phase_config_normalize.schema.json",
                          NEG + "hardware_fields_in_phase_config.json")
        self.assertTrue(errs, "cpu_profile 的 workers/isa/block 出现在 phase_config 必须失败")
        text = messages(errs)
        for name in ("workers", "isa", "block_size", "isa_level"):
            self.assertIn("additionalProperties: %r unexpected" % name, text,
                          "未拒绝硬件字段 %s：%s" % (name, text))

    def test_run_manifest_rejects_hardware_field(self):
        errs = errors_for("contracts/schemas/run_manifest.schema.json",
                          NEG + "run_manifest_hardware_field.json")
        self.assertTrue(errs, "run_manifest 出现 workers 必须失败")
        self.assertIn("additionalProperties: 'workers' unexpected", messages(errs))

    def test_cpu_profile_v1_missing_required_still_fails(self):
        """迁移后 legacy 分支仍强制原 v1 必填集（顶层 required 取两分支共有键，完整性由分支承担）。"""
        errs = errors_for("contracts/schemas/cpu_profile.schema.json",
                          NEG + "cpu_profile_v1_missing_required.json")
        self.assertTrue(errs, "缺 verdict 的 v1 profile 必须失败")
        self.assertIn("oneOf: 0 branches matched", messages(errs))

    def test_cpu_profile_v2_missing_writer_binding_fails(self):
        """v2 缺 provider_build_ids（provider hash 绑定）必须失败。"""
        doc = C.load_json("tests/config/fixtures/positive/cpu_profile_v2.json")
        del doc["build"]["provider_build_ids"]
        errs = C.validate(C.load_json("contracts/schemas/cpu_profile.schema.json"), doc)
        self.assertTrue(errs, "v2 缺 provider_build_ids 必须失败")

    def test_drizzle_pixfrac_domain_is_enforced(self):
        """DRIZZLE.md:31 的 0 < pixfrac <= 1 必须是机器门（模板变异，不改模板本体）。"""
        schema = C.load_json("contracts/schemas/phase_config_normalize.schema.json")
        tpl = C.load_json("config/templates/normalize.phase_config.json")
        for bad in (0, 0.0, -0.1, 1.5):
            tpl["config"]["algorithm_drizzle_pixfrac"] = bad
            self.assertTrue(C.validate(schema, tpl), "pixfrac=%r 必须失败（越出 (0,1]）" % bad)
        for good in (0.5, 0.8, 1.0):
            tpl["config"]["algorithm_drizzle_pixfrac"] = good
            self.assertEqual([], C.validate(schema, tpl), "pixfrac=%r 必须合法" % good)

    def test_all_six_negative_fixtures_exist(self):
        import os
        got = sorted(os.listdir(os.path.join(C.REPO, NEG)))
        self.assertEqual(6, len(got), "负例数量不为 6: %s" % got)


if __name__ == "__main__":
    unittest.main(verbosity=2)
