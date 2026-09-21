#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P26 T3 产物比对口径自检(含阴性对照)。

覆盖:
  * 逐字节一致 → IDENTICAL;
  * 仅墙钟字段差异(properties 的 hips_creation_date / FITS 头 DATE) → 被忽略并单列,
    其余字节完全一致时结论为 IDENTICAL_AFTER_IGNORES;
  * --tolerance 模式下 1 ulp 级注入 → 不判差异(阴性对照: 不是恒 FAIL);
  * 大差异注入 → DIFFER(tolerance_exceeded)(阴性对照: 不是恒 PASS);
  * 结构差异(shape/数值长度) → DIFFER;
  * 缺文件/多文件 → DIFFER;
  * 非墙钟文本差异(如 properties 的 hips_order) → DIFFER(证明忽略集不是全盘放行)。
"""
from __future__ import annotations

import array
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
TOOL = REPO / "eng" / "tools" / "quality" / "compare_products.py"
sys.path.insert(0, str(REPO / "eng" / "tools" / "quality"))
import compare_products as cp  # noqa: E402


def card(key, value):
    if isinstance(value, str):
        return ("%-8s= %-20s" % (key, repr(value)))[:80].ljust(80)
    return ("%-8s= %20s" % (key, value))[:80].ljust(80)


def write_fits(path, values, date, bitpix=-32, fmt="f"):
    cards = [card("SIMPLE", "T"), card("BITPIX", bitpix), card("NAXIS", 1),
             card("NAXIS1", len(values)), card("DATE", date)]
    head = "".join(cards) + "END".ljust(80)
    hb = head.encode("ascii")
    hb += b" " * ((-len(hb)) % 2880)
    arr = array.array(fmt, values)
    if arr.itemsize > 1:
        arr.byteswap()
    data = arr.tobytes()
    data += b"\x00" * ((-len(data)) % 2880)
    Path(path).write_bytes(hb + data)


def build_tree(root, date, img_values, order="3", extra=None):
    sig = Path(root) / "signal"
    sig.mkdir(parents=True, exist_ok=True)
    (sig / "properties").write_text(
        "creator_did = astrocs\n"
        "hips_creation_date = %s\n"
        "hips_order = %s\n"
        "hips_frame = equatorial\n" % (date, order), encoding="utf-8")
    write_fits(sig / "img.fits", img_values, date)
    (Path(root) / "support").mkdir(parents=True, exist_ok=True)
    (Path(root) / "support" / "manifest.json").write_text(
        json.dumps({"kind": "astrocs_run_manifest", "phases": [3]}, sort_keys=True),
        encoding="utf-8")
    if extra:
        for rel, text in extra.items():
            p = Path(root) / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8")


class TestCompareProducts(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="p26_cmp_"))
        self.vals = [1.0, 2.0, 3.5, -4.25, 1e6]

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _trees(self, date_a="2026-09-15T10:00:00", date_b="2026-09-15T11:00:00",
               vals_b=None, order_b="3"):
        a = self.tmp / "A"
        b = self.tmp / "B"
        build_tree(a, date_a, self.vals)
        build_tree(b, date_b, vals_b if vals_b is not None else self.vals, order=order_b)
        return a, b

    def test_01_identical(self):
        a, b = self._trees(date_a="2026-09-15T10:00:00", date_b="2026-09-15T10:00:00")
        rep = cp.compare_trees(a, b)
        self.assertEqual(rep["verdict"], "IDENTICAL", rep["differences"])
        self.assertEqual(rep["counts"]["differing"], 0)

    def test_02_clock_only_ignored(self):
        a, b = self._trees()
        rep = cp.compare_trees(a, b)
        self.assertEqual(rep["verdict"], "IDENTICAL_AFTER_IGNORES", rep["differences"])
        self.assertEqual(rep["counts"]["differing"], 0)
        paths = {i["path"] for i in rep["ignored_differences"]}
        self.assertIn("signal/properties", paths)
        self.assertIn("signal/img.fits", paths)
        keys = {k for i in rep["ignored_differences"] for k in i["keys"]}
        self.assertIn("hips_creation_date", keys)
        self.assertIn("DATE", keys)
        self.assertTrue(rep["tolerance_rationale"])

    def test_03_ulp_within_tolerance_not_fail(self):
        # 1 ulp 级注入: 1e6 上 +1 ulp ≈ 6.25e-8 相对
        vals_b = list(self.vals)
        raw = struct.unpack("I", struct.pack("f", vals_b[4]))[0] + 1
        vals_b[4] = struct.unpack("f", struct.pack("I", raw))[0]
        a, b = self._trees(date_a="2026-09-15T10:00:00", date_b="2026-09-15T10:00:00",
                           vals_b=vals_b)
        rep = cp.compare_trees(a, b, rel_tol=1e-5)
        self.assertNotEqual(rep["verdict"], "DIFFER", rep["differences"])
        detail = [i for i in rep["within_tolerance"] if i["path"] == "signal/img.fits"]
        self.assertTrue(detail, "ulp 级差异应归入 within_tolerance")
        self.assertGreater(detail[0]["detail"]["max_abs"], 0.0)
        self.assertLess(detail[0]["detail"]["max_rel"], 1e-5)
        self.assertEqual(rep["mode"], "tolerance")
        self.assertEqual(rep["counts"]["numeric_within_tolerance"], 1)
        self.assertEqual(rep["counts"]["ignored_clock_only"], 0)

    def test_04_large_diff_fails(self):
        vals_b = list(self.vals)
        vals_b[2] = 30.5   # 3.5 -> 30.5, 远超 1e-5
        a, b = self._trees(vals_b=vals_b)
        rep = cp.compare_trees(a, b, rel_tol=1e-5)
        self.assertEqual(rep["verdict"], "DIFFER")
        d = [x for x in rep["differences"] if x["path"] == "signal/img.fits"]
        self.assertTrue(d)
        self.assertEqual(d[0]["reason"], "tolerance_exceeded")
        self.assertGreater(d[0]["detail"]["exceed"], 0)
        self.assertGreater(d[0]["detail"]["max_abs"], 1.0)

    def test_05_structural_and_membership_diffs(self):
        # 结构差异: 数据长度不同
        a, b = self._trees(vals_b=self.vals + [7.0])
        rep = cp.compare_trees(a, b, rel_tol=1e-5)
        self.assertEqual(rep["verdict"], "DIFFER")
        # 成员差异: B 多一个文件 / A 多一个文件
        a2, b2 = self._trees(date_a="same", date_b="same")
        (b2 / "signal" / "extra.dat").write_bytes(b"x")
        rep2 = cp.compare_trees(a2, b2)
        self.assertEqual(rep2["verdict"], "DIFFER")
        self.assertIn("signal/extra.dat", rep2["only_in_b"])

    def test_06_non_clock_text_diff_fails(self):
        a, b = self._trees(order_b="5")
        rep = cp.compare_trees(a, b)
        self.assertEqual(rep["verdict"], "DIFFER")
        d = {x["path"]: x for x in rep["differences"]}
        self.assertIn("signal/properties", d)
        self.assertEqual(d["signal/properties"]["reason"], "content_mismatch")

    def test_07_cli_exit_codes_and_report(self):
        a, b = self._trees()
        out = self.tmp / "rep"
        rc = cp.main(["--a", str(a), "--b", str(b), "--out", str(out)])
        self.assertEqual(rc, 0)
        rep = json.loads((out / "compare_report.json").read_text(encoding="utf-8"))
        self.assertEqual(rep["verdict"], "IDENTICAL_AFTER_IGNORES")
        self.assertTrue((out / "compare_summary.md").is_file())
        # 大差异 → CLI 退出码 1(与上一步 rc=0 互为阴性对照)
        vals_b = list(self.vals)
        vals_b[0] = 9.0
        c, d = self._trees(vals_b=vals_b, date_a="same", date_b="same")
        rc2 = cp.main(["--a", str(c), "--b", str(d), "--out", str(self.tmp / "rep2")])
        self.assertEqual(rc2, 1)
        # 清单自述可打印
        self.assertEqual(cp.main(["--list-ignored"]), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
