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
        """未知滤镜名必败；且同位置换成库键必过（红绿双向，防 anyOf 判据静默失效）。"""
        schema_rel = "contracts/schemas/phase_config_normalize.schema.json"
        fixture = NEG + "unknown_filter.phase_config.json"
        errs = errors_for(schema_rel, fixture)
        self.assertTrue(errs, "未知滤镜名必须失败")
        self.assertIn("filter_passband", messages(errs), "错误须落在 filter_passband 字段")
        ok = C.load_json(fixture)
        ok["blocks"][0]["filter_passband"] = "Baader R"   # 逐字命中 config/filters.json
        self.assertEqual([], C.validate(C.load_json(schema_rel), ok),
                         "库内滤镜名必须通过（否则判据不是「未知滤镜」而是恒拒）")

    def test_negative_02_missing_output_dir(self):
        errs = errors_for("contracts/schemas/phase_config_normalize.schema.json",
                          NEG + "missing_output_dir.phase_config.json")
        self.assertTrue(errs, "缺 output_dir 必须失败")
        self.assertIn("required: missing 'output_dir'", messages(errs))

    def test_negative_03_precision_out_of_domain(self):
        """drizzle.precision_mode 越界必须失败（0=FP32/1=FP64 显式，无 silent 缺省）。"""
        errs = errors_for("contracts/schemas/phase_config_normalize.schema.json",
                          NEG + "precision_out_of_domain.phase_config.json")
        self.assertTrue(errs, "precision_mode 越界必须失败")
        self.assertIn("enum", messages(errs))
        self.assertIn("precision_mode", messages(errs))
        self.assertIn("2", messages(errs))

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
        """DRIZZLE.md:31 的 0 < pixfrac <= 1 必须是机器门（模板变异，不改模板本体）。

        GAP_AUDIT §9.68 后模板是多块形态：块内 algorithm_drizzle_pixfrac 与
        drizzle.pixfrac 两条路径都必须受 0 < pixfrac <= 1 约束。
        """
        schema = C.load_json("contracts/schemas/phase_config_normalize.schema.json")
        tpl = C.load_json("config/templates/normalize.phase_config.json")
        for field in ("algorithm_drizzle_pixfrac", "drizzle"):
            for bad in (0, 0.0, -0.1, 1.5):
                probe = C.load_json("config/templates/normalize.phase_config.json")
                if field == "drizzle":
                    probe["blocks"][0]["drizzle"]["pixfrac"] = bad
                else:
                    probe["blocks"][0][field] = bad
                self.assertTrue(C.validate(schema, probe),
                                "%s=%r 必须失败（越出 (0,1]）" % (field, bad))
            for good in (0.5, 0.8, 1.0):
                probe = C.load_json("config/templates/normalize.phase_config.json")
                if field == "drizzle":
                    probe["blocks"][0]["drizzle"]["pixfrac"] = good
                else:
                    probe["blocks"][0][field] = good
                self.assertEqual([], C.validate(schema, probe),
                                 "%s=%r 必须合法" % (field, good))
        self.assertIn("blocks", tpl, "模板必须已是多块形态（§9.68）")

    def test_cpu_profile_v1_bad_kernel_row_fails(self):
        """kernel_v1（v1 kernels[] 项约束）必须真被施加：size_class 越界必红。"""
        errs = errors_for("contracts/schemas/cpu_profile.schema.json",
                          NEG + "cpu_profile_v1_bad_kernel.json")
        self.assertTrue(errs, "v1 kernel 行 size_class=huge 必须失败")
        text = messages(errs)
        self.assertIn("enum", text)
        self.assertIn("huge", text)
        self.assertIn("kernels/0", text, "错误须落在 kernels[0] 行内：%s" % text)

    def test_cpu_profile_v2_bad_kernel_row_fails(self):
        """kernel_v2（v2 kernels{} 行约束）必须真被施加：provider 越界必红。"""
        errs = errors_for("contracts/schemas/cpu_profile.schema.json",
                          NEG + "cpu_profile_v2_bad_kernel.json")
        self.assertTrue(errs, "v2 kernel 行 provider=sse4 必须失败")
        text = messages(errs)
        self.assertIn("enum", text)
        self.assertIn("sse4", text)
        self.assertIn("kernels/calibration-pixel-transform/provider", text,
                      "错误须落在 kernels.<id>.provider：%s" % text)

    def test_cpu_profile_kernel_defs_are_wired_and_enforced(self):
        """$defs.kernel_v1/kernel_v2 不得是死定义（W5-CPU-001）：
        ① 两分支 def 必须 $ref 它们；② 顶层镜像与 $defs 逐字一致（防漂移）；
        ③ 剥离顶层镜像后仅靠分支 $ref 仍必判红（证明约束确由 kernel_v1/v2 施加）。"""
        import copy
        import json as _json
        schema = C.load_json("contracts/schemas/cpu_profile.schema.json")
        self.assertEqual("#/$defs/kernel_v1",
                         schema["$defs"]["legacy_v1"]["properties"]["kernels"]["items"]["$ref"])
        self.assertEqual("#/$defs/kernel_v2",
                         schema["$defs"]["profile_v2"]["properties"]["kernels"]
                         ["additionalProperties"]["$ref"])
        self.assertEqual(schema["properties"]["kernels"]["items"], schema["$defs"]["kernel_v1"],
                         "顶层 kernel 镜像与 $defs.kernel_v1 漂移")
        self.assertEqual(schema["properties"]["kernels"]["additionalProperties"],
                         schema["$defs"]["kernel_v2"],
                         "顶层 kernel 镜像与 $defs.kernel_v2 漂移")
        stripped = copy.deepcopy(schema)
        stripped["properties"]["kernels"].pop("items")
        stripped["properties"]["kernels"].pop("additionalProperties")
        validator = C.load_validator()
        cases = [("legacy_v1", "cpu_profile_v1_bad_kernel.json", "huge"),
                 ("profile_v2", "cpu_profile_v2_bad_kernel.json", "sse4")]
        for branch, fixture, token in cases:
            doc = C.load_json(NEG + fixture)
            errs = validator.validate(doc, stripped["$defs"][branch], root=stripped)
            self.assertTrue(errs, "剥离顶层镜像后 %s 负例仍必须由 $defs.kernel_* 判红" % fixture)
            self.assertIn(token, " | ".join(m for _, m in errs))
        # 缺必填方向：内存变异（不新增夹具），同样须由分支 $ref 判红
        v1 = C.load_json("tests/config/fixtures/positive/cpu_profile_v1_legacy.json")
        del v1["kernels"][0]["precision"]
        errs = validator.validate(v1, stripped["$defs"]["legacy_v1"], root=stripped)
        self.assertIn("required: missing 'precision'", " | ".join(m for _, m in errs))
        v2 = C.load_json("tests/config/fixtures/positive/cpu_profile_v2.json")
        kid = sorted(v2["kernels"])[0]
        del v2["kernels"][kid]["self_test_sha256"]
        errs = validator.validate(v2, stripped["$defs"]["profile_v2"], root=stripped)
        self.assertIn("required: missing 'self_test_sha256'", " | ".join(m for _, m in errs))

    def test_negative_fixture_inventory_is_exactly_registered(self):
        """负例清单按名登记（不是按数量）：新增负例必须显式登记在此，防漏测/防误删。"""
        import os
        expected = sorted([
            "unknown_filter.phase_config.json",              # ① 未知滤镜名
            "missing_output_dir.phase_config.json",          # ② 缺 output_dir（块级）
            "precision_out_of_domain.phase_config.json",     # ③ precision_mode 越界
            "hardware_fields_in_phase_config.json",          # ④ cpu_profile 字段混入
            "normalize_mixed_forms.phase_config.json",       # §9.68 ③ blocks 与平铺键互斥
            "normalize_block_unknown_key.phase_config.json", # §9.68 ⑤ 块内未知键
            "normalize_perframe_inputs.phase_config.json",   # §9.68 ⑥ 逐帧 inputs[] 已退役
            "mosaic_blocks_mixed_flat.phase_config.json",    # §9.71 裁决 2：blocks 与平铺键互斥
            "export_blocks_mixed_flat.phase_config.json",    # §9.71 裁决 2：blocks 与平铺键互斥
            "mosaic_block_weight_mode.phase_config.json",    # §9.73 A44：块内 weight_mode 必拒
            "export_block_phase_name.phase_config.json",     # §3.3：新分支不得出现阶段判别键
            "run_manifest_hardware_field.json",              # run_manifest 硬件字段
            "cpu_profile_v1_missing_required.json",          # legacy v1 缺必填
            "cpu_profile_v2_bad_os_abi.json",                # CFG-002：os_abi 越出冻结枚举
            "cpu_profile_v1_bad_kernel.json",                # W5-CPU-001：v1 kernel 行 size_class 越界
            "cpu_profile_v2_bad_kernel.json",                # W5-CPU-001：v2 kernel 行 provider 越界
        ])
        got = sorted(os.listdir(os.path.join(C.REPO, NEG)))
        self.assertEqual(expected, got, "负例清单与登记不一致: %s" % got)


if __name__ == "__main__":
    unittest.main(verbosity=2)