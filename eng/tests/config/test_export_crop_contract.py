#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXPORT-CROP-01：导出裁剪范围 crop 的合同面回归锁（能红能绿）。

权威：负责人裁决 2026-09-23（「默认导出的话是要求边框不得裁剪任何有效像素，然后可以导出
一些黑边。到平面后我自己手动剪裁。然后支持手动输入裁剪范围。这样我以后 gui 的 HiPS 浏览器
里面我可以直接导出框选。需要保留接口。」）；
设计正本 = docs/design/PHASE3_DETAILED_DESIGN.md §8；
字段合同 = eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop。

本模块只锁**合同面**（schema 三分支 + CLI 键表同源 + 判别键不撞跨类面）。几何判据
（越界/宽高非正/两形式同时给/裁剪后为空）的唯一实现 = lib/algorithms/projection/p3_wcs.h，
其 C++ 探针与 M42 端到端实证见 run/EXPORT-CROP-01/（不入 CI 面，避免 CI 依赖大产品）。

跑法：python3 -m unittest discover -s eng/tests/config -t eng/tests/config
"""
import json
import re
import unittest

import cfg_common as C

SCHEMA = "eng/contracts/schemas/phase_config_export.schema.json"
SESSION_H = "lib/infrastructure/cli/session_commands.h"
PARSER_CPP = "lib/infrastructure/cli/parser.cpp"
BLOCK = "eng/tests/config/fixtures/positive/export_blocks.phase_config.json"
FLAT = "eng/tests/config/fixtures/positive/export_flat.phase_config.json"

PIXELS = {"crop_form": "pixels", "pixels": {"x0": 1, "y0": 1, "x1": 512, "y1": 512}}
SKY = {"crop_form": "sky",
       "sky": {"ra_min_deg": 83.5, "ra_max_deg": 84.0,
               "dec_min_deg": -5.6, "dec_max_deg": -5.2}}


def errs(doc):
    return C.validate(C.load_json(SCHEMA), doc)


def messages(errors):
    return " | ".join("%s: %s" % ("/".join(str(p) for p in path) or "<root>", msg)
                      for path, msg in errors)


def with_crop(doc, crop):
    """把 crop 写进块内（blocks[] 形态）或平铺顶层（平铺形态）。"""
    d = json.loads(json.dumps(doc))
    if "blocks" in d:
        d["blocks"][0]["crop"] = crop
    else:
        d["crop"] = crop
    return d


class TestExportCropContract(unittest.TestCase):
    # ── 绿：两种形式在两种形态下都合法 ──
    def test_01_both_forms_accepted_in_block_and_flat(self):
        for rel in (BLOCK, FLAT):
            base = C.load_json(rel)
            for name, crop in (("pixels", PIXELS), ("sky", SKY)):
                self.assertEqual([], errs(with_crop(base, crop)),
                                 "%s + crop(%s) 必须合法：%s"
                                 % (rel, name, messages(errs(with_crop(base, crop)))))

    def test_02_absent_crop_is_valid_default(self):
        """默认不裁剪：crop 键缺失 ⇒ 合法（整幅导出，允许黑边）。"""
        for rel in (BLOCK, FLAT):
            self.assertEqual([], errs(C.load_json(rel)),
                             "%s 不带 crop 必须合法（默认不裁剪）" % rel)

    # ── 红：全部非法形态必须被合同面拒绝（判据非退化）──
    def test_03_illegal_forms_rejected(self):
        base = C.load_json(BLOCK)
        cases = {
            "两种形式同时给": {"crop_form": "pixels", "pixels": PIXELS["pixels"],
                              "sky": SKY["sky"]},
            "缺判别键": {"pixels": PIXELS["pixels"]},
            "判别键越域": {"crop_form": "rect", "pixels": PIXELS["pixels"]},
            "判别键指向的形式缺失": {"crop_form": "sky", "pixels": PIXELS["pixels"]},
            "未知键": dict(PIXELS, bogus_knob=1),
            "形式内未知键": {"crop_form": "pixels",
                            "pixels": dict(PIXELS["pixels"], x2=9)},
            "像素坐标 0（非 FITS 1-based）": {"crop_form": "pixels",
                                             "pixels": {"x0": 0, "y0": 1, "x1": 512, "y1": 512}},
            "像素坐标非整数": {"crop_form": "pixels",
                              "pixels": {"x0": 1.5, "y0": 1, "x1": 512, "y1": 512}},
            "天球形式缺字段": {"crop_form": "sky",
                              "sky": {"ra_min_deg": 83.5, "ra_max_deg": 84.0,
                                      "dec_min_deg": -5.6}},
            "dec 越 TAN 冻结域": {"crop_form": "sky",
                                 "sky": {"ra_min_deg": 83.5, "ra_max_deg": 84.0,
                                         "dec_min_deg": -5.6, "dec_max_deg": 86.0}},
        }
        for name, crop in cases.items():
            e = errs(with_crop(base, crop))
            self.assertTrue(e, "负例「%s」必须判红（合同面判据不得恒真）" % name)

    def test_04_flat_branch_shares_the_same_key_set(self):
        """平铺分支 propertyNames 必须含 crop（与块内键集等价）。"""
        s = C.load_json(SCHEMA)
        block = set(s["$defs"]["export_block"]["properties"])
        flat = set(s["else"]["else"]["propertyNames"]["enum"]) - {"schema_version"}
        self.assertIn("crop", block)
        self.assertEqual(block - {"name"}, flat,
                         "平铺分支与块内键集不等价（crop 只加了一边）")

    def test_05_cli_key_tables_declare_crop(self):
        """CLI 两面（config_fields + session_keys）都必须认 crop，否则用户拿到 rc=3。"""
        text = C.load_text(SESSION_H)
        m = re.search(r"static const std::vector<ConfigField>\s+kExport\s*=\s*\{(.*?)\n    \};",
                      text, re.S)
        self.assertIsNotNone(m, "session_commands.h 找不到 kExport（唯一键集声明已漂移）")
        self.assertIn(chr(123) + chr(34) + "crop" + chr(34), m.group(1),
                      "config_fields(SESSION_EXPORT) 缺 crop")
        ptext = C.load_text(PARSER_CPP)
        pm = re.search(r"session_keys\(\)\s*\{.*?=\s*\{(.*?)\n    \};", ptext, re.S)
        self.assertIsNotNone(pm, "parser.cpp 找不到 session_keys()")
        self.assertIn(chr(34) + "crop" + chr(34), pm.group(1),
                      "session_keys() 白名单缺 crop（配置不可达）")

    def test_06_discriminator_key_does_not_collide_cross_class(self):
        """判别键名不得撞 cpu_profile/legacy_v1：mode 已被占用（UNIFIED_MODEL §3 禁同名异义）。"""
        cpu = C.load_json("eng/contracts/schemas/cpu_profile.schema.json")
        taken = (C.property_names(cpu["$defs"]["legacy_v1"])
                 | C.property_names(cpu["$defs"]["kernel_v1"]))
        self.assertIn("mode", taken, "legacy_v1 的 mode 是既有事实（判据锚点）")
        self.assertNotIn("crop_form", taken)
        crop = C.load_json(SCHEMA)["$defs"]["export_crop"]
        self.assertEqual(["crop_form"], crop["required"],
                         "判别键名变化必须同步本测试与 docs/design/PHASE3_DETAILED_DESIGN.md §8.2")


if __name__ == "__main__":
    unittest.main(verbosity=2)
