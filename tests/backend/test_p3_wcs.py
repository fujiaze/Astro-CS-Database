#!/usr/bin/env python3
"""P3-002 测试: TAN WCS — 独立参考(向量法) roundtrip/RA wrap/pole/rotation/CRPIX+CD 关键字。"""
import math, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "phase3_session")


def gnomonic_vector(ra, dec, ra0, dec0):
    """独立参考: 切平面单位向量法(Calabretta 三角法之外的推导路径), 返回 ξ,η (deg)。"""
    a, d, a0, d0 = map(math.radians, (ra, dec, ra0, dec0))
    v = (math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d))
    v0 = (math.cos(d0) * math.cos(a0), math.cos(d0) * math.sin(a0), math.sin(d0))
    e1 = (-math.sin(a0), math.cos(a0), 0.0)                       # 东
    e2 = (-math.sin(d0) * math.cos(a0), -math.sin(d0) * math.sin(a0), math.cos(d0))  # 北
    denom = sum(v[i] * v0[i] for i in range(3))
    if denom <= 0:
        return None
    xi = sum(v[i] * e1[i] for i in range(3)) / denom
    eta = sum(v[i] * e2[i] for i in range(3)) / denom
    return math.degrees(xi), math.degrees(eta)


def ungnomonic_vector(xi_deg, eta_deg, ra0, dec0):
    xi, eta = map(math.radians, (xi_deg, eta_deg))
    a0, d0 = map(math.radians, (ra0, dec0))
    r = math.hypot(xi, eta)
    theta = math.atan2(1.0, r)
    phi = math.atan2(-xi, eta)
    sd, cdv = math.sin(d0), math.cos(d0)
    st, ct = math.sin(theta), math.cos(theta)
    dec = math.asin(st * sd + ct * cdv * math.cos(phi))
    dra = math.atan2(-ct * math.sin(phi), cdv * st - sd * ct * math.cos(phi))
    ra = (math.degrees(a0 + dra)) % 360.0
    return ra, math.degrees(dec)


class TestP3Wcs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3wcs_")
        cls.exe = os.path.join(cls.tmp, "p3wcs")
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
             f"-I{HOST}", os.path.join(REPO, "tests", "backend", "p3_wcs_main.cpp"),
             os.path.join(HOST, "p3_wcs.cpp"), "-o", cls.exe],
            capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args):
        r = subprocess.run([self.exe, *args], capture_output=True, text=True, timeout=60)
        return r.stdout.strip()

    def _make(self, ra, dec, scale, w, h, parity, pa):
        out = self._run("make", str(ra), str(dec), str(scale), str(w), str(h), parity, str(pa))
        if out.startswith("OK"):
            t = out.split()
            return {"crpix_x": float(t[1]), "crpix_y": float(t[2]),
                    "cd": [float(t[3]), float(t[4]), float(t[5]), float(t[6])]}
        raise AssertionError(out)

    def test_01_roundtrip_grid_and_independent_ref(self):
        """roundtrip(pix→world→pix 恒等)+C++ 与独立向量法参考一致(网格 7×7)。"""
        ra0, dec0, scale, w, h = 210.0, 34.0, 0.001, 101, 77
        for pa, parity in ((0.0, "east_left"), (30.0, "east_left"), (-45.0, "east_right")):
            d = self._make(ra0, dec0, scale, w, h, parity, pa)
            cd = d["cd"]
            for ix in range(7):
                for iy in range(7):
                    x = ix * (w - 1) / 6.0
                    y = iy * (h - 1) / 6.0
                    out = self._run("p2w", str(ra0), str(dec0), str(scale), str(w), str(h),
                                    parity, str(pa), str(x), str(y))
                    self.assertTrue(out.startswith("OK"), out)
                    ra, dec = map(float, out.split()[1:])
                    # 独立参考: 向量法切平面 → 反解像素(CD⁻¹)
                    xi, eta = gnomonic_vector(ra, dec, ra0, dec0)
                    ref_ra, ref_dec = ungnomonic_vector(
                        (cd[0] * (x + 1 - d["crpix_x"]) + cd[1] * (y + 1 - d["crpix_y"])),
                        (cd[2] * (x + 1 - d["crpix_x"]) + cd[3] * (y + 1 - d["crpix_y"])),
                        ra0, dec0)
                    # C++ p2w 与 参考链(world 域)一致
                    self.assertAlmostEqual(((ra - ref_ra + 180) % 360) - 180, 0.0, places=8,
                                           msg=f"pa={pa} parity={parity} ({x},{y})")
                    self.assertAlmostEqual(dec, ref_dec, places=8)
                    # roundtrip
                    out2 = self._run("w2p", str(ra0), str(dec0), str(scale), str(w), str(h),
                                     parity, str(pa), str(ra), str(dec))
                    self.assertTrue(out2.startswith("OK"), out2)
                    x2, y2 = map(float, out2.split()[1:])
                    self.assertAlmostEqual(x2, x, places=7)
                    self.assertAlmostEqual(y2, y, places=7)

    def test_02_ra_wrap(self):
        """RA wrap: 中心 359.9°/0.1°, 东侧像素 RA 应跨 0/360 并归一。"""
        d = self._make(359.9, 20.0, 0.001, 201, 201, "east_right", 0.0)
        self.assertGreater(d["cd"][0], 0, "east_right → CD1_1>0")
        out = self._run("p2w", "359.9", "20.0", "0.001", "201", "201", "east_right",
                        "0.0", "200", "100")
        ra, dec = map(float, out.split()[1:])
        self.assertLess(ra, 1.0, "RA 应跨 0° 归一(小正值)")
        # roundtrip 穿越边界
        out2 = self._run("w2p", "359.9", "20.0", "0.001", "201", "201", "east_right",
                         "0.0", str(ra), str(dec))
        x2, y2 = map(float, out2.split()[1:])
        self.assertAlmostEqual(x2, 200.0, places=7)
        self.assertAlmostEqual(y2, 100.0, places=7)

    def test_03_pole_guard(self):
        """|dec|≥5°: 中心距极点 <5°(dec=88)拒; dec=84 合法; dec=-88 拒。"""
        self.assertIn("FAIL 1", self._run("make", "210", "88", "0.001", "50", "50",
                                          "east_left", "0"))
        self.assertIn("FAIL 1", self._run("make", "210", "-88", "0.001", "50", "50",
                                          "east_left", "0"))
        self.assertTrue(self._run("make", "210", "84", "0.001", "50", "50",
                                  "east_left", "0").startswith("OK"))
        # 反变换极点守卫
        self.assertIn("FAIL 1", self._run("w2p", "210", "84", "0.001", "50", "50",
                                          "east_left", "0", "210", "89.5"))

    def test_04_hemisphere_crossing_rejected(self):
        """输出跨 TAN 半球 → 拒(scale 大到某角落过半球)。"""
        self.assertIn("FAIL 3", self._run("make", "0", "10", "45.0", "400", "400",
                                          "east_left", "0"))
        # 合法: 小图大 scale 不跨
        self.assertTrue(self._run("make", "0", "10", "0.01", "400", "400",
                                  "east_left", "0").startswith("OK"))

    def test_05_crpix_cd_keywords(self):
        """CRPIX=(W+1)/2 pixel-center; CD 矩阵/符号( east_left→CD1_1<0)/旋转项。"""
        out = self._run("kw", "210.5", "34.25", "0.002", "101", "77", "east_left", "0")
        kws = dict(re.match(r"(\w+\d?)\s*=\s*(.*)", l).groups()
                   for l in out.splitlines() if "=" in l)
        self.assertEqual(kws["CTYPE1"], "'RA---TAN'")
        self.assertEqual(kws["CTYPE2"], "'DEC--TAN'")
        self.assertAlmostEqual(float(kws["CRPIX1"]), 51.0, places=9)
        self.assertAlmostEqual(float(kws["CRPIX2"]), 39.0, places=9)
        self.assertAlmostEqual(float(kws["CRVAL1"]), 210.5, places=9)
        self.assertAlmostEqual(float(kws["CD1_1"]), -0.002, delta=1e-12)
        self.assertAlmostEqual(float(kws["CD2_2"]), 0.002, delta=1e-12)
        self.assertAlmostEqual(float(kws["CD1_2"]), 0.0, delta=1e-15)
        # 旋转 PA=30°: CD1_2/CD2_1 非零
        out2 = self._run("kw", "210.5", "34.25", "0.002", "101", "77", "east_left", "30")
        k2 = dict(re.match(r"(\w+\d?)\s*=\s*(.*)", l).groups()
                  for l in out2.splitlines() if "=" in l)
        self.assertAlmostEqual(float(k2["CD1_2"]), 0.002 * math.sin(math.radians(30)),
                               delta=1e-12)
        self.assertAlmostEqual(float(k2["CD1_1"]), -0.002 * math.cos(math.radians(30)),
                               delta=1e-12)

    def test_06_unsupported_and_bounds(self):
        """显式拒绝: W/H 越界(0/20001)→PARAM; scale≤0→PARAM。"""
        self.assertIn("FAIL 1", self._run("make", "10", "10", "0.001", "0", "50",
                                          "east_left", "0"))
        self.assertIn("FAIL 1", self._run("make", "10", "10", "0.001", "20001", "50",
                                          "east_left", "0"))
        self.assertIn("FAIL 1", self._run("make", "10", "10", "0", "50", "50",
                                          "east_left", "0"))

if __name__ == "__main__":
    unittest.main(verbosity=2)
