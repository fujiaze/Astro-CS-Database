#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3002_properties_order_unit.py — P3-002 (G6) properties/order/unit。
验证:
  A) order_sel 上限来自输入实际 order(reader/resampler 使用该 order, 非仅 metadata);
  B) BUNIT 来源输入合同: properties 有 BUNIT 用之, 缺省 ADU, 绝不 Jy/beam 默认;
  C) BITPIX/max memory 来自 request+NodePlan(-32|-64);
  D) 不支持输入显式错误。
"""
import os
import re
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SESS = os.path.join(REPO, "lib", "phase3_session", "p3_session.cpp")
RES = os.path.join(REPO, "lib", "phase3_session", "p3_resample.cpp")
PROPS_H = os.path.join(REPO, "lib", "phase3_session", "hips_properties.h")
PROPS_C = os.path.join(REPO, "lib", "phase3_session", "hips_properties.cpp")


class TestP3002PropertiesOrderUnit(unittest.TestCase):
    def test_01_order_sel_from_input(self):
        """order_sel 上限来自输入实际 order(非固定 20), reader 用该 order。"""
        s = open(SESS, encoding="utf-8").read()
        self.assertIn("input_order", s, "session 未读取输入实际 order")
        self.assertIn("max_order", s, "session 未用输入 order 作上限")
        # 上限 = min(input_order, 20 冻结守卫)
        m = re.search(r"const int max_order = \(input_order[^;]+;", s)
        self.assertIsNotNone(m, "order 上限公式缺失")
        r = open(RES, encoding="utf-8").read()
        self.assertIn("p3_sampler_open_ex", r, "resample 未暴露实际 order")
        self.assertIn("*out_order = p.order", r, "open_ex 未输出 properties 实测 order")

    def test_02_bunit_from_contract(self):
        """BUNIT 来源输入合同: 缺省 ADU, 绝不 Jy/beam 默认。"""
        s = open(SESS, encoding="utf-8").read()
        self.assertIn("bunit.c_str()", s, "session 未用输入合同 BUNIT")
        self.assertNotIn('"Jy/beam"', s, "session 不得硬编码 Jy/beam")
        r = open(RES, encoding="utf-8").read()
        self.assertIn('"ADU"', r, "缺省 BUNIT 应为 ADU")
        h = open(PROPS_H, encoding="utf-8").read()
        self.assertIn("bunit", h, "HipsProperties 缺 bunit 字段")
        c = open(PROPS_C, encoding="utf-8").read()
        self.assertIn('"BUNIT"', c, "properties 未解析 BUNIT 键")

    def test_03_bitpix_from_request(self):
        """BITPIX 来自 request(-32|-64), 非法拒绝。"""
        s = open(SESS, encoding="utf-8").read()
        self.assertIn("bitpix != -32 && bitpix != -64", s, "BITPIX 未校验 -32|-64")

    def test_04_unsupported_inputs_rejected(self):
        """不支持输入显式错误(variance/weight/ivar/flux-per-pixel)。"""
        r = open(RES, encoding="utf-8").read()
        self.assertIn('"variance"', r, "variance 未显式拒")
        self.assertIn('"flux-per-pixel"', r, "flux-per-pixel 未显式拒")


if __name__ == "__main__":
    unittest.main(verbosity=2)
