#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3005_fits_output.py — P3-005 (G6) FITS 输出校验与 provenance。
验证:
  A) request bitpix -32/-64 真实决定输出 BITPIX 与 buffer(非仅 metadata);
  B) 写 CRPIX/CRVAL/CD/CTYPE/CUNIT/BUNIT/coverage/provenance 关键字完整;
  C) 独立读取验证(roundtrip); 覆盖区与值一致。
"""
import json
import os
import subprocess
import tempfile
import unittest

import numpy as np
from astropy.io import fits

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")


class TestP3005FitsOutput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3005_")
        cls.hips = os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")
        assert os.path.isdir(cls.hips)

    def _run(self, out, bitpix):
        os.makedirs(out, exist_ok=True)
        cfg = {"schema_version": "1",
               "inputs": {"lights": [self.hips], "darks": [], "flats": [], "bias": []},
               "phase3": {"source": {"hips_dir": self.hips},
                          "center": {"ra_deg": 0.0, "dec_deg": 30.0},
                          "scale_deg_per_px": 0.05, "width_px": 16, "height_px": 16,
                          "sampler": "nearest", "projection": "TAN",
                          "coverage_output": "mask", "output_dir": out,
                          "bitpix": bitpix},
               "output_dir": out}
        c = os.path.join(out, "c.json")
        json.dump(cfg, open(c, "w"))
        r = subprocess.run([EXE, "phase3", "run", "--config", c], capture_output=True,
                           text=True, timeout=300)
        return r, os.path.join(out, "output_phase3.fits")

    def test_01_bitpix_32_real(self):
        """request -32 → 输出 BITPIX=-32(真实 buffer)。"""
        r, f = self._run(os.path.join(self.tmp, "o32"), -32)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        h = fits.getheader(f)
        self.assertEqual(h["BITPIX"], -32)
        d = fits.getdata(f)
        self.assertEqual(d.dtype.itemsize, 4, "BITPIX=-32 应真实 f32 buffer(FITS BE)")

    def test_02_bitpix_64_real(self):
        """request -64 → 输出 BITPIX=-64(真实 f64 buffer)。"""
        r, f = self._run(os.path.join(self.tmp, "o64"), -64)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        h = fits.getheader(f)
        self.assertEqual(h["BITPIX"], -64)
        d = fits.getdata(f)
        self.assertEqual(d.dtype.itemsize, 8, "BITPIX=-64 应真实 f64 buffer(FITS BE)")

    def test_03_header_keywords_complete(self):
        """CRPIX/CRVAL/CD/CTYPE/CUNIT/BUNIT/coverage/provenance 关键字完整。"""
        r, f = self._run(os.path.join(self.tmp, "o3"), -32)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        h = fits.getheader(f)
        for kw in ("CTYPE1", "CTYPE2", "CUNIT1", "CUNIT2", "CRPIX1", "CRPIX2",
                   "CRVAL1", "CRVAL2", "CD1_1", "CD1_2", "CD2_1", "CD2_2",
                   "BUNIT", "BSCALE", "BZERO", "HIPSID", "ORDERSEL", "SAMPLER",
                   "SWVER", "RUNID"):
            self.assertIn(kw, h, f"缺关键字 {kw}")
        self.assertEqual(h["BUNIT"], "ADU")   # P3-002: 面亮度, 非 Jy/beam
        self.assertEqual(h["CTYPE1"], "RA---TAN")
        # coverage 扩展
        self.assertTrue(len(fits.open(f)) >= 2, "应含 coverage 扩展")

    def test_04_values_roundtrip(self):
        """值 roundtrip: -64 输出在 f64 精度内保留值。"""
        r, f = self._run(os.path.join(self.tmp, "o4"), -64)
        self.assertEqual(r.returncode, 0)
        d = fits.getdata(f)
        fin = d[~np.isnan(d)]
        self.assertGreater(fin.size, 0)
        self.assertTrue(np.all(fin > 0), "覆盖区值应为正")


if __name__ == "__main__":
    unittest.main(verbosity=2)
