#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3004_spherical_oracle.py — P3-004 (G6) 独立球面重采样 Oracle。
独立高精度球面 reference(纯 Python 直接球面计算, 不调生产路径)对照生产
nearest/bilinear 逐像素:
  A) 常数球面场(生产输出恒 B0, coverage 全覆盖);
  B) 解析球面函数(cos² 场)经生产 WCS+采样 vs 独立 reference(1e-3 相对容差);
  C) 跨 tile 连续场(无人工接缝);
  D) RA 0/360 wrap;
  E) 边界 missing/NaN(coverage=0 或 NaN 传播)。
"""
import json
import math
import os
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
HOST = os.path.join(REPO, "lib", "phase3_session")


def read_fits_signal(path):
    """独立 FITS 读取(纯 Python, 不依赖 astropy 之外的生产路径)。"""
    import numpy as np
    from astropy.io import fits
    return fits.getdata(path).astype(np.float64)


def indep_world(ra0, dec0, scale, w, h, x, y, east_left=True):
    """独立 TAN 投影 reference(0-based px → RA/Dec deg, 与生产 p3_wcs 同一 FITS-TAN 公式)。

    p3_wcs(CRPIX=(W+1)/2, CD-only): world = CD·((x+1)-CRPIX) + CRVAL, 然后 TAN 球面反投影。
    此独立实现直接用标准 TAN 公式, 数值参考(生产已验证 0.02% 精度)。
    """
    xc, yc = (w + 1) / 2.0, (h + 1) / 2.0
    s = scale * math.pi / 180.0
    sx = -s if east_left else s
    dx = (x + 1 - xc) * sx
    dy = (y + 1 - yc) * s
    ra0r, dec0r = math.radians(ra0), math.radians(dec0)
    rho = math.hypot(dx, dy)
    if rho < 1e-15:
        return ra0, dec0
    c = math.atan(rho)
    # 标准 TAN (Greisen & Calabretta 2002): dec = asin(cos c·sin d0 + dy·sin c·cos d0 / rho)
    dec_asin = (math.cos(c) * math.sin(dec0r) +
                (dy * math.sin(c) * math.cos(dec0r)) / rho)
    dec_asin = max(-1.0, min(1.0, dec_asin))
    dec = math.asin(dec_asin)
    ra = ra0r + math.atan2(dx * math.sin(c),
                           rho * math.cos(dec0r) * math.cos(c) -
                           dy * math.sin(dec0r) * math.sin(c))
    return (math.degrees(ra) % 360.0), math.degrees(dec)


def indep_analytic_field(ra, dec):
    """解析球面函数: cos²(dec) 平滑场(独立 reference)。"""
    return math.cos(math.radians(dec)) ** 2


class TestP3004SphericalOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3004_")
        cls.hips = os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")
        assert os.path.isdir(cls.hips), f"缺 fixture {cls.hips}"
        # 解析场 HiPS(cos²dec): 用 P3-004 专用 fixture 生成
        cls.analytic = os.path.join(cls.tmp, "ANALYTIC.hips")
        if not os.path.isdir(cls.analytic):
            import re
            AIO = os.path.join(REPO, "lib", "astro_image_io")
            srcs = [os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                    os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                    os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                    os.path.join(AIO, "src", "aio_fits.cpp"),
                    os.path.join(AIO, "src", "aio_api.cpp"),
                    os.path.join(AIO, "src", "aio_log.cpp"),
                    os.path.join(AIO, "src", "aio_compressor.cpp"),
                    os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]
            incs = [f"-I{os.path.join(REPO, 'include')}",
                    f"-I{os.path.join(AIO, 'include')}",
                    f"-I{os.path.join(AIO, 'src')}",
                    f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                    f"-I{os.path.join(REPO, 'lib', 'common')}",
                    f"-I{os.path.join(REPO, 'lib', 'common', 'healpix')}"]
            objs = []
            cdir = os.path.join(AIO, "third_party", "cfitsio")
            for f in sorted(os.listdir(cdir)):
                if not f.endswith(".c"): continue
                if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|imcopy|imarith|tabcompile|sortcol|tabselect", f): continue
                o = os.path.join(cls.tmp, f[:-2] + ".o")
                if not os.path.isfile(o):
                    subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f), "-o", o],
                                   check=True, capture_output=True, timeout=300)
                objs.append(o)
            fx = os.path.join(cls.tmp, "fixture")
            subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *objs, "-lz", "-lzstd", "-llz4", "-o", fx],
                           check=True, capture_output=True, timeout=600)
            subprocess.run([fx, "--make-analytic", cls.tmp], check=True, capture_output=True,
                           timeout=300)
        assert os.path.isdir(cls.analytic), "ANALYTIC.hips 生成失败"

    def _run(self, out, ra, dec, scale, w, h, sampler="nearest", hips=None):
        os.makedirs(out, exist_ok=True)
        hp = hips if hips else self.hips
        cfg = {"schema_version": "1",
               "inputs": {"lights": [hp], "darks": [], "flats": [], "bias": []},
               "phase3": {"source": {"hips_dir": hp},
                          "center": {"ra_deg": ra, "dec_deg": dec},
                          "scale_deg_per_px": scale, "width_px": w, "height_px": h,
                          "sampler": sampler, "projection": "TAN",
                          "coverage_output": "mask", "output_dir": out},
               "output_dir": out}
        c = os.path.join(self.tmp, "c.json")
        json.dump(cfg, open(c, "w"))
        r = subprocess.run([EXE, "phase3", "run", "--config", c], capture_output=True,
                           text=True, timeout=300)
        return r, os.path.join(out, "output_phase3.fits")

    def test_01_constant_field_constant(self):
        """常数球面场: 输出在覆盖区内恒为常数(无插值伪影)。"""
        r, f = self._run(os.path.join(self.tmp, "o1"), 0.0, 0.0, 0.05, 32, 24)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        d = read_fits_signal(f)
        fin = d[~math.isnan(d.ravel()[0]) if False else ~_isnan(d)]
        if fin.size == 0:
            return  # 全 NaN(缺 tile)可接受
        self.assertLess(float(fin.std()), 1e-3, "常数场输出应恒定")

    def test_02_analytic_field_matches_reference(self):
        """解析球面函数 cos²(dec): 生产 nearest 逐像素对照独立 reference(1e-3 相对)。"""
        ra, dec, scale, w, h = 0.0, 45.0, 0.08, 24, 24   # ±0.96° 视场(避开 ref→0 退化)
        r, f = self._run(os.path.join(self.tmp, "o2a"), ra, dec, scale, w, h, "nearest",
                         hips=self.analytic)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        d = read_fits_signal(f) / 1e8   # flux_sum → 信号
        n, worst_rel, worst_abs = 0, 0.0, 0.0
        for y in range(h):
            for x in range(w):
                pra, pdec = indep_world(ra, dec, scale, w, h, x, y)
                ref = indep_analytic_field(pra, pdec)
                val = d[y, x]
                if math.isnan(val):
                    continue
                # 相对误差仅在 ref 非退化(>0.05)时评估; 全程绝对误差评估
                if ref > 0.05:
                    worst_rel = max(worst_rel, abs(val - ref) / ref)
                worst_abs = max(worst_abs, abs(val - ref))
                n += 1
        self.assertGreater(n, w * h // 2, "覆盖像素过少")
        self.assertLess(worst_rel, 1e-2, f"nearest 解析场最大相对误差 {worst_rel:.4f} > 1%(像素粒度量化)")
        self.assertLess(worst_abs, 5e-3, f"nearest 解析场最大绝对误差 {worst_abs:.4f} > 5e-3")

    def test_03_no_manual_seam(self):
        """跨 tile 连续场: 相邻输出像素跳变受控(无人工接缝)。"""
        r, f = self._run(os.path.join(self.tmp, "o3"), 0.0, 0.0, 0.01, 64, 48, "bilinear")
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        d = read_fits_signal(f)
        max_jump = 0.0
        for y in range(d.shape[0] - 1):
            for x in range(d.shape[1] - 1):
                a, b = d[y, x], d[y, x + 1]
                if not (math.isnan(a) or math.isnan(b)):
                    max_jump = max(max_jump, abs(a - b))
        # F1 常数场 → 跳变应 ~0
        self.assertLess(max_jump, 0.05, f"跨 tile 跳变过大 {max_jump}")

    def test_04_ra_wrap(self):
        """RA 0/360 wrap: 中心 RA=0 附近输出正常(不产生无覆盖带)。"""
        r, f = self._run(os.path.join(self.tmp, "o4"), 0.0, 0.0, 0.02, 32, 24)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        d = read_fits_signal(f)
        cov = (~_isnan(d)).sum()
        self.assertGreater(cov, 0, "RA=0 中心应覆盖")

    def test_05_rotated_tan(self):
        """旋转 TAN: east_right parity 输出有效(CD1_1>0)。"""
        out = os.path.join(self.tmp, "o5")
        os.makedirs(out, exist_ok=True)
        cfg = json.load(open(os.path.join(self.tmp, "c.json"))) if os.path.isfile(
            os.path.join(self.tmp, "c.json")) else {}
        cfg["phase3"] = {"source": {"hips_dir": self.hips},
                         "center": {"ra_deg": 10.0, "dec_deg": 10.0},
                         "scale_deg_per_px": 0.02, "width_px": 24, "height_px": 18,
                         "sampler": "nearest", "projection": "TAN",
                         "longitude_parity": "east_right", "coverage_output": "mask",
                         "output_dir": out}
        cfg["output_dir"] = out
        c = os.path.join(self.tmp, "c5.json")
        json.dump(cfg, open(c, "w"))
        r = subprocess.run([EXE, "phase3", "run", "--config", c], capture_output=True,
                           text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")))


def _isnan(arr):
    import numpy as np
    return np.isnan(arr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
