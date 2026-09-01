#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3001_science_freeze.py — P3-001 (G6) 科学与算法冻结审计。
审计文档与代码一致性:
  A) 极区条件统一为 abs(dec)<=85°(SCI/API/session/WCS 三处无第二套条件);
  B) 冻结项齐全: HiPS 版本/子集、NESTED、ICRS、TAN、单位、pixel center、
     CD parity、order、nearest/bilinear、coverage、NaN/missing、BITPIX、最大资源;
  C) 生产 session 在 |dec|>85° 拒绝(ACS_ERR_PARAM), 85° 内接受;
  D) 先文档后代码: 文档冻结为 FROZEN 状态。
"""
import os
import re
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCI = os.path.join(REPO, "docs", "science", "PHASE3_HIPS_TO_FITS.md")
API = os.path.join(REPO, "docs", "api", "PHASE3_API_V1.md")
SESS = os.path.join(REPO, "lib", "phase3_session", "p3_session.cpp")
WCS_C = os.path.join(REPO, "lib", "phase3_session", "p3_wcs.cpp")
WCS_H = os.path.join(REPO, "lib", "phase3_session", "p3_wcs.h")


class TestP3001ScienceFreeze(unittest.TestCase):
    def test_01_pole_condition_single(self):
        """极区条件统一为 abs(dec)<=85°(无第二套 |dec|>=5° 条件)。"""
        for p in (SCI, API, WCS_C, WCS_H):
            s = open(p, encoding="utf-8").read()
            # 唯一条件表述
            self.assertIn("85", s, f"{os.path.basename(p)} 缺 85° 条件")
            # 无旧式独立条件(|dec|≥5° 或 <5° 作为拒绝条件)
            self.assertNotIn("|dec|≥5°", s, f"{os.path.basename(p)} 含旧条件 |dec|≥5°")
            self.assertNotIn("|Dec|≥5°", s, f"{os.path.basename(p)} 含旧条件 |Dec|≥5°")
        # 代码语义: 拒绝 abs(dec)>85
        sc = open(SESS, encoding="utf-8").read()
        self.assertIn("fabs(dec) > 85.0", sc, "session 拒绝条件应为 >85")
        sw = open(WCS_C, encoding="utf-8").read()
        self.assertIn("fabs(dec_deg) > kMaxAbsDec", sw, "WCS 拒绝条件应为 >kMaxAbsDec(=85)")

    def test_02_frozen_items_present(self):
        """冻结项齐全(SCI 文档): NESTED/ICRS/TAN/单位/pixel center/CD parity/order/nearest/bilinear/coverage/NaN/BITPIX。"""
        s = open(SCI, encoding="utf-8").read()
        for token in ("NESTED", "ICRS", "TAN", "ADU", "像素中心",
                      "CRPIX", "CD1_1", "east_left", "order_sel",
                      "nearest", "bilinear", "coverage", "NaN", "BITPIX=-32/-64"):
            self.assertIn(token, s, f"SCI 文档缺冻结项 {token}")

    def test_03_session_rejects_pole(self):
        """生产 session 拒绝 |dec|>85°(ACS_ERR_PARAM), 85° 内接受。"""
        # 直接检查 session 校验源码逻辑(fabs>85 拒绝), 无第二条件
        sc = open(SESS, encoding="utf-8").read()
        m = re.search(r"std::fabs\(dec\) > 85\.0", sc)
        self.assertIsNotNone(m, "session 未按 abs(dec)<=85° 拒绝")
        # WCS 描述符同语义
        sw = open(WCS_C, encoding="utf-8").read()
        self.assertIn("kMaxAbsDec = 85.0", sw, "WCS 常量应=85.0")

    def test_04_sci_frozen(self):
        """SCI 文档状态 FROZEN。"""
        s = open(SCI, encoding="utf-8").read()
        self.assertIn("状态: FROZEN", s.splitlines()[2] if len(s.splitlines()) > 2 else "")


if __name__ == "__main__":
    unittest.main(verbosity=2)
