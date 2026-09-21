#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CLI-MULTIBLOCK（GAP_AUDIT §9.68 负责人裁决 2026-09-20）机器门。

裁决语义（逐字）：「如果多套设备，多个通道需要不同的校准场以及运行参数的话。
可以在一个 json 里写很多块。就像一个 main 下面可以写很多个函数一样。不需要每条
都详细写出校准帧。**同一组校准帧和运行参数应该支持一组 light**」。

本模块锁 schema 面（CLI 运行面见 eng/tests/cli/test_multiblock_normalize.py）：
  ① 多块形态通过（eng/packaging/config/templates/normalize.phase_config.json 与正例夹具）
  ② 平铺单块简写仍通过（向后兼容）
  ③ blocks 与平铺键同时出现 → 必败（互斥，不静默取一）
  ④ 块缺 output_dir → 必败（块级必填，禁 silent default）
  ⑤ 块内未知键 → 必败
  ⑥ 逐帧 inputs[] 形态（已否决）→ 必败，且 $defs/inputs 面已删除
  ⑦ 每块一套母版 + 一组 light：同一母版路径可在多块复用（不得要求逐帧重复写校准帧）

跑法：python3 -m unittest discover -s eng/tests/config -t eng/tests/config
"""
import json
import unittest

import cfg_common as C

SCHEMA = "eng/contracts/schemas/phase_config_normalize.schema.json"
TEMPLATE = "eng/packaging/config/templates/normalize.phase_config.json"
POS_BLOCKS = "eng/tests/config/fixtures/positive/normalize_blocks.phase_config.json"
POS_FLAT = "eng/tests/config/fixtures/positive/normalize_flat.phase_config.json"
NEG = "eng/tests/config/fixtures/negative/"


def errs_for(instance):
    return C.validate(C.load_json(SCHEMA), instance)


def messages(errors):
    return " | ".join("%s: %s" % ("/".join(str(p) for p in path) or "<root>", msg)
                      for path, msg in errors)


class TestMultiBlockForm(unittest.TestCase):
    def test_01_multi_block_template_and_fixture_pass(self):
        schema = C.load_json(SCHEMA)
        for rel in (TEMPLATE, POS_BLOCKS):
            doc = C.load_json(rel)
            self.assertIn("blocks", doc, "%s 必须是多块形态" % rel)
            self.assertGreaterEqual(len(doc["blocks"]), 2,
                                    "%s 必须含两块示例（§9.68：一个 json 里写很多块）" % rel)
            self.assertEqual([], C.validate(schema, doc), "%s 未通过 schema" % rel)
        # 块级必填：每块自带一组 light + 块级 output_dir
        for i, blk in enumerate(C.load_json(POS_BLOCKS)["blocks"]):
            self.assertTrue(blk["input_lights"], "blocks[%d] 必须有非空 input_lights" % i)
            self.assertTrue(blk["output_dir"], "blocks[%d] 必须有块级 output_dir" % i)

    def test_02_flat_single_block_shorthand_still_passes(self):
        self.assertEqual([], errs_for(C.load_json(POS_FLAT)),
                         "平铺单块简写必须保留（向后兼容 §9.68）")

    def test_03_blocks_and_flat_keys_are_mutually_exclusive(self):
        errs = errs_for(C.load_json(NEG + "normalize_mixed_forms.phase_config.json"))
        self.assertTrue(errs, "blocks 与平铺键同时出现必须失败（不静默取一）")
        self.assertIn("blocks", messages(errs))

    def test_04_block_missing_output_dir_fails(self):
        errs = errs_for(C.load_json(NEG + "missing_output_dir.phase_config.json"))
        self.assertTrue(errs, "块缺 output_dir 必须失败")
        self.assertIn("required: missing 'output_dir'", messages(errs))

    def test_05_block_unknown_key_fails(self):
        errs = errs_for(C.load_json(NEG + "normalize_block_unknown_key.phase_config.json"))
        self.assertTrue(errs, "块内未知键必须失败")
        self.assertIn("unknown_knob", messages(errs))

    def test_06_retired_per_frame_inputs_form_is_rejected(self):
        """§9.68 否决 {phase_name, config, inputs[]} 逐帧形态。"""
        doc = C.load_json(NEG + "normalize_perframe_inputs.phase_config.json")
        errs = errs_for(doc)
        self.assertTrue(errs, "逐帧 inputs[] 形态必须失败")
        text = messages(errs)
        self.assertIn("phase_name", text)
        self.assertIn("inputs", text)
        # schema 面已无逐帧 inputs 定义（不得留第二形态）
        schema = C.load_json(SCHEMA)
        self.assertNotIn("inputs", schema.get("properties", {}),
                         "schema 顶层不得再有逐帧 inputs[]")
        self.assertNotIn("normalize_inputs", schema.get("$defs", {}),
                         "schema 不得再保留逐帧 normalize_inputs 定义")
        self.assertNotIn("phase_name", C.property_names(schema),
                         "schema 不得再声明 phase_name（逐帧形态判别键）")
        self.assertNotIn("light", C.property_names(schema))
        self.assertNotIn("filter", C.property_names(schema))

    def test_07_one_master_set_serves_a_group_of_lights(self):
        """同一组校准帧 + 运行参数支持一组 light：母版路径可在多块间复用，
        且单块内多帧共用同一套母版（不逐帧重复写校准帧）。"""
        doc = C.load_json(POS_BLOCKS)
        self.assertEqual([], errs_for(doc))
        b0, b1 = doc["blocks"]
        self.assertEqual(b0["master_bias"], b1["master_bias"],
                         "同一 bias 母版必须可被多块复用（§9.68）")
        self.assertEqual(b0["master_dark"], b1["master_dark"])
        self.assertGreaterEqual(len(b0["input_lights"]), 2,
                                "一块内多帧共用同一套母版（不逐帧重复写校准帧）")
        self.assertNotEqual(b0["master_flat"], b1["master_flat"],
                            "不同通道各自平场是裁决的用例（多套设备/多个通道）")

    def test_08_filter_passband_keeps_library_enum_gate(self):
        """块级 filter_passband 仍受滤镜库逐字命中门约束（不得因改形态而放宽）。"""
        schema = C.load_json(SCHEMA)
        self.assertIn("#/$defs/filter_name", C.load_text(SCHEMA),
                      "normalize 必须仍消费 $defs/filter_name（phases_consuming_filter 登记）")
        lib = list(C.load_json("eng/packaging/config/filters.json")["filters"].keys())
        self.assertEqual(lib, schema["$defs"]["filter_name"]["enum"])
        ok = C.load_json(POS_FLAT)
        ok["filter_passband"] = ""
        self.assertEqual([], C.validate(schema, ok), "空串 = 显式无 filter 必须合法")
        for bad in ("bader r", "baader r", "Baader  R"):
            probe = C.load_json(POS_FLAT)
            probe["filter_passband"] = bad
            self.assertTrue(C.validate(schema, probe), "未知/变体滤镜 %r 必须失败" % bad)


if __name__ == "__main__":
    unittest.main(verbosity=2)
