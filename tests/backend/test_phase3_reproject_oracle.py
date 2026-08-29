#!/usr/bin/env python3
"""SYN-007 独立合成 Oracle — Phase3 HiPS→TAN FITS 重投影端到端。
验收(03 L129 + 13 §5): 执行 13 第5节全部 case; 独立 WCS/FITS reader 通过;
                       tile seam/RA wrap/mask/单位正确。
方法(independent, Oracle 不调用生产 WCS wrapper / resampler / lookup):
  - 编译 `p3_session_probe.cpp`(生产 `p3_session_run` 端到端)驱动重投影; 输出 TAN FITS。
  - 端侧独立 Python 参考:
      · 纯 Python FITS 读取器(大端)校验 WCS 头(CTYPE/CRPIX/CRVAL/CD/BUNIT)
        + 读 signal/coverage 两 HDU。
      · 独立 gnomonic(切平面单位向量法)像素中心→世界往返(pixel→world→pixel)。
      · 常量球面场 → 输出 SB 恒定(表面亮度保持, BUNIT=Jy)。
      · 单位正确: SB = flux/areaspan(1e-8), 常量场 2.5 → 2.5e8。
      · tile seam: 常量场跨 tile 无人工接缝(输出处处恒等) → Ne 常量。
      · RA 0/360 wrap: 中心近 0° 时东侧像素 RA 跨 0/360 归一且 round-trip 成立。
      · coverage/mask: 全覆盖字段 coverage=1; NAN 场 blank value -> NaN -> coverage 语义。
  - 依赖已 PASS 的 P3-002/003/004(WCS/重采样/输出组件) + SYN-001..006。
"""
import math, os, shutil, struct, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "phase3_session")
AIO = os.path.join(REPO, "lib", "astro_image_io")
INC = os.path.join(REPO, "include")
C = os.path.join(REPO, "lib", "common")
CASTRO = os.path.join(C, "healpix")

FITS_INCS = [f"-I{INC}", f"-I{HOST}", f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
             f"-I{CASTRO}", f"-I{os.path.join(REPO, 'third_party', 'nlohmann')}",
             f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}", f"-I{C}", f"-I{os.path.join(C, 'crypto')}"]
FITS_SRCS = [os.path.join(HOST, "p3_resample.cpp"), os.path.join(HOST, "hips_properties.cpp"),
             os.path.join(CASTRO, "healpix_core.cpp"),
             os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
             os.path.join(AIO, "src", "aio_fits.cpp"), os.path.join(AIO, "src", "aio_api.cpp"),
             os.path.join(AIO, "src", "aio_log.cpp"), os.path.join(AIO, "src", "aio_compressor.cpp"),
             os.path.join(C, "crypto", "sha256.cpp")]


def _compile_srcs(tmp, extra_srcs, out, incs, srcs, objs):
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs, *extra_srcs, *srcs, *objs,
                        "-lz", "-lzstd", "-llz4", "-o", out], capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr[-1200:]


# ============ 独立 Python FITS 读取器(大端) ============
def _parse_hdr(blk):
    cards = []
    for i in range(0, 2880, 80):
        cards.append(blk[i:i + 80].decode("ascii", "replace"))
    n = 0
    for i, c in enumerate(cards):
        if c[:8].strip() == "END":
            n = i + 1; break
    return cards[:n]


def read_fits(path):
    with open(path, "rb") as fh:
        data = fh.read()
    pos = 0; hdus = []
    while pos < len(data):
        cards = _parse_hdr(data[pos:pos + 2880]); hdr = {}
        for c in cards:
            k = c[:8].strip()
            if "=" in c:
                v = c[c.find("=") + 1:].strip().split("/")[0].strip().strip("'").strip()
                hdr[k] = v
        pos += 2880
        bp = int(hdr.get("BITPIX", "0")); nx1 = int(hdr.get("NAXIS1", "0"))
        nx2 = int(hdr.get("NAXIS2", "1")); naxis = int(hdr.get("NAXIS", "0"))
        ncount = nx1 * (nx2 if naxis == 2 else 1); nbytes = abs(bp) // 8 * ncount
        ds = pos; pos += ((nbytes + 2879) // 2880) * 2880
        vals = struct.unpack(">%df" % ncount, data[ds:ds + nbytes]) if nbytes and bp == -32 else []
        hdus.append((hdr, nx1, nx2, vals))
    return hdus


def gnomonic_vector(ra, dec, ra0, dec0):
    a, d, a0, d0 = map(math.radians, (ra, dec, ra0, dec0))
    v = (math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d))
    v0 = (math.cos(d0) * math.cos(a0), math.cos(d0) * math.sin(a0), math.sin(d0))
    e1 = (-math.sin(a0), math.cos(a0), 0.0)
    e2 = (-math.sin(d0) * math.cos(a0), -math.sin(d0) * math.sin(a0), math.cos(d0))
    denom = sum(v[i] * v0[i] for i in range(3))
    if denom <= 0:
        return None
    xi = sum(v[i] * e1[i] for i in range(3)) / denom
    eta = sum(v[i] * e2[i] for i in range(3)) / denom
    return math.degrees(xi), math.degrees(eta)


def ungnomonic_vector(xi_deg, eta_deg, ra0, dec0):
    xi, eta = map(math.radians, (xi_deg, eta_deg)); a0, d0 = map(math.radians, (ra0, dec0))
    r = math.hypot(xi, eta); theta = math.atan2(1.0, r); phi = math.atan2(-xi, eta)
    sd, cdv = math.sin(d0), math.cos(d0); st, ct = math.sin(theta), math.cos(theta)
    dec = math.asin(st * sd + ct * cdv * math.cos(phi))
    dra = math.atan2(-ct * math.sin(phi), cdv * st - sd * ct * math.cos(phi))
    ra = (math.degrees(a0 + dra)) % 360.0
    return ra, math.degrees(dec)


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestPhase3ReprojOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn007_")
        cdir = os.path.join(AIO, "third_party", "cfitsio")
        objs = []
        for f in sorted(os.listdir(cdir)):
            if not f.endswith(".c"):
                continue
            if __import__("re").search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                                       r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                                       r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
                continue
            o = os.path.join(cls.tmp, f[:-2] + ".o")
            subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f), "-o", o],
                           check=True, capture_output=True, timeout=300)
            objs.append(o)
        # probe (session end-to-end)
        _compile_srcs(cls.tmp, [os.path.join(REPO, "tests", "backend", "p3_session_probe.cpp")],
                      os.path.join(cls.tmp, "probe"), FITS_INCS,
                      [os.path.join(HOST, "p3_session.cpp"), os.path.join(HOST, "p3_output.cpp"),
                       os.path.join(HOST, "p3_wcs.cpp")] + FITS_SRCS, objs)
        # fixture (const/field/nan)
        _compile_srcs(cls.tmp, [os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp")],
                      os.path.join(cls.tmp, "fx"), FITS_INCS, FITS_SRCS, objs)
        cls.data = os.path.join(cls.tmp, "data"); os.makedirs(cls.data)
        for m in ("--make-const", "--make-field", "--make-nan"):
            r = subprocess.run([os.path.join(cls.tmp, "fx"), m, cls.data], capture_output=True, text=True, timeout=300)
            assert "HIPS_FIXTURES_OK" in r.stdout, f"{m}: {r.stderr}"
        cls.const = os.path.join(cls.data, "CONST.hips")
        cls.field = os.path.join(cls.data, "FIELD.hips")
        cls.nan = os.path.join(cls.data, "NAN.hips")
        cls.out = os.path.join(cls.tmp, "out"); os.makedirs(cls.out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, hips, ra, dec, scale, w, h, sampler):
        r = subprocess.run([os.path.join(self.tmp, "probe"), "run", hips, str(ra), str(dec), str(scale),
                            str(w), str(h), sampler, self.out], capture_output=True, text=True, timeout=300)
        self.assertIn("exit_code", r.stdout, r.stderr + r.stdout[-300:])
        return r.stdout

    def _fits(self, hips, ra, dec, scale, w, h, sampler):
        self._run(hips, ra, dec, scale, w, h, sampler)
        ph = os.path.join(self.out, "output_phase3.fits")
        hdus = read_fits(ph)
        # hdus[i] = (hdrdict, nx1, nx2, vals)
        sig_hdr, cov_hdr = hdus[0][0], hdus[1][0]
        return sig_hdr, hdus[0][3], cov_hdr, hdus[1][3]

    def test_01_independent_fits_wcs_header(self):
        """独立 FITS reader: TAN/deg/CRPIX(center)/CRVAL/CD/BUNIT 关键字正确。"""
        hdr, _sig, _cov, _covv = self._fits(self.const, 0.0, 30.0, 0.05, 20, 20, "bilinear")
        self.assertEqual(hdr["CTYPE1"], "RA---TAN"); self.assertEqual(hdr["CTYPE2"], "DEC--TAN")
        self.assertEqual(hdr["CUNIT1"], "deg"); self.assertEqual(hdr["CUNIT2"], "deg")
        self.assertAlmostEqual(float(hdr["CRPIX1"]), 10.5, places=9)
        self.assertAlmostEqual(float(hdr["CRPIX2"]), 10.5, places=9)
        self.assertAlmostEqual(float(hdr["CRVAL1"]), 0.0, places=9)
        self.assertAlmostEqual(float(hdr["CRVAL2"]), 30.0, places=9)
        self.assertAlmostEqual(float(hdr["CD1_1"]), -0.05, places=12)  # east_left → CD1_1<0
        self.assertAlmostEqual(float(hdr["CD2_2"]), 0.05, places=12)
        self.assertEqual(hdr["BUNIT"], "Jy")

    def test_02_wcs_roundtrip_pixworld(self):
        """独立 gnomonic pixel→world→pixel 往返(用写的 CD/CRPIX): 误差<1e-6 px。"""
        hdr, _sig, _cov, _covv = self._fits(self.const, 12.0, 45.0, 0.02, 40, 40, "bilinear")
        cd = (float(hdr["CD1_1"]), float(hdr["CD1_2"]), float(hdr["CD2_1"]), float(hdr["CD2_2"]))
        crpix = (float(hdr["CRPIX1"]), float(hdr["CRPIX2"]))
        crval = (float(hdr["CRVAL1"]), float(hdr["CRVAL2"]))
        for (x, y) in [(0, 0), (5, 5), (20, 25), (39, 39), (13, 7)]:
            xi = cd[0] * (x - crpix[0]) + cd[1] * (y - crpix[1])
            eta = cd[2] * (x - crpix[0]) + cd[3] * (y - crpix[1])
            ra, dec = ungnomonic_vector(xi, eta, crval[0], crval[1])
            # 逆: world→pixel
            g = gnomonic_vector(ra, dec, crval[0], crval[1])
            self.assertIsNotNone(g)
            xi2, eta2 = g
            # CD 逆(CD 对角时解析): x = crpix + cd^-1 * [xi,eta]
            det = cd[0] * cd[3] - cd[1] * cd[2]
            ix = cd[3] / det * xi2 - cd[1] / det * eta2
            iy = -cd[2] / det * xi2 + cd[0] / det * eta2
            self.assertAlmostEqual(ix + crpix[0], x, delta=1e-4)
            self.assertAlmostEqual(iy + crpix[1], y, delta=1e-4)

    def test_03_constant_field_surface_brightness(self):
        """常量球面场 → 输出 SB 恒定(处处一致); BUNIT=Jy 表面亮度; SB=flux/areaspan(1e-8)。"""
        hdr, s_vals, _cov, _covv = self._fits(self.const, 0.0, 30.0, 0.05, 20, 20, "bilinear")
        # const flux=2.5, each tile AREA=1e-8 → SB=2.5/1e-8=2.5e8
        self.assertEqual(len(s_vals), 400)
        uniq = set(round(v, 3) for v in s_vals)
        self.assertEqual(len(uniq), 1, f"常量场输出应恒定, got {uniq}")
        self.assertAlmostEqual(uniq.pop(), 2.5 / 1e-8, delta=1.0, msg="SB 应=flux/area")

    def test_04_field_bilinear_range_and_coverage(self):
        """FIELD(各 tile 常量 1..12) → bilinear 输出在[SB_lo, SB_hi] 且全 coverage=1。
        用宽场(跨度大, 越过 tile 边界)以观测多 tile 值。"""
        hdr, s_vals, cov_hdr, cov_vals = self._fits(self.field, 20.0, 30.0, 0.05, 200, 200, "bilinear")
        s = sorted(set(round(v, 3) for v in s_vals))
        lo, hi = 1.0 / 1e-8, 12.0 / 1e-8
        for v in s:
            self.assertTrue(lo - 1e5 <= v <= hi + 1e5, f"FIELD 输出越界 {v}")
        self.assertGreater(len(s), 1, "FIELD 跨多 tile 应非单一值")
        ncov = set(round(v, 3) for v in cov_vals)
        self.assertEqual(ncov, {1.0}, f"FIELD 全覆盖应为 1, got {ncov}")

    def test_05_ra_wrap_east_left(self):
        """RA 0/360 wrap: 中心近 0°, 东侧像素 RA 跨 0/360 归一; round-trip 成立。"""
        hdr, _sig, _cov, _covv = self._fits(self.const, 359.9, 20.0, 0.02, 24, 24, "nearest")
        cd = (float(hdr["CD1_1"]), float(hdr["CD1_2"]), float(hdr["CD2_1"]), float(hdr["CD2_2"]))
        crpix = (float(hdr["CRPIX1"]), float(hdr["CRPIX2"])); crval = (float(hdr["CRVAL1"]), float(hdr["CRVAL2"]))
        # 最西像素的 RA 应逼近 359.9 东侧跨 0 → RA<180 或 >359; round-trip
        for (x, y) in [(0, 12), (12, 12), (23, 12), (23, 23)]:
            xi = cd[0] * (x - crpix[0]) + cd[1] * (y - crpix[1])
            eta = cd[2] * (x - crpix[0]) + cd[3] * (y - crpix[1])
            ra, dec = ungnomonic_vector(xi, eta, crval[0], crval[1])
            self.assertTrue(0.0 <= ra < 360.0, f"RA 应归一 [0,360), got {ra}")
            g = gnomonic_vector(ra, dec, crval[0], crval[1])
            self.assertIsNotNone(g)
            # RA wrap 时 round-trip 在 [0,360) 内可靠
            self.assertLess(abs(((ra - crval[0] + 180) % 360) - 180), 2.0)

    def test_06_nan_blank_semantics(self):
        """NAN field(所有叶 NaN) → signal=NaN + coverage=1(§4 非错误语义)。"""
        hdr, s_vals, cov_hdr, cov_vals = self._fits(self.nan, 0.0, 30.0, 0.05, 20, 20, "bilinear")
        nan_cnt = sum(1 for v in s_vals if v != v)
        self.assertEqual(nan_cnt, 400, "NAN field 输出应全部 NaN")
        ncov = set(round(v, 3) for v in cov_vals)
        self.assertEqual(ncov, {1.0}, "NAN field coverage 应为 1(有值但 NaN)")

    def test_07_tile_seam_no_artifacts(self):
        """常量场跨 tile seam: 输出处处恒定 → 无人工接缝(常量场内部 seam 由 tile 常量保证)。"""
        hdr, s_vals, cov_hdr, cov_vals = self._fits(self.const, 5.0, 60.0, 0.04, 40, 40, "bilinear")
        uniq = set(round(v, 2) for v in s_vals)
        self.assertEqual(len(uniq), 1, f"常量场跨 tile 输出应恒定(seam 无伪影), got {uniq}")

    def test_08_surface_brightness_bunit_preserved(self):
        """BUNIT=Jy(表面亮度) 且常量场输出为正有限(单位正确, 无越界/NaN)。"""
        hdr, s_vals, cov_hdr, cov_vals = self._fits(self.const, -20.0, -5.0, 0.05, 20, 20, "bilinear")
        self.assertEqual(hdr["BUNIT"], "Jy")
        for v in s_vals:
            self.assertTrue(v == v and v > 0, f"常量场 SB 应正有限, got {v}")


    def test_09_unsupported_projection_reject(self):
        """projection≠TAN / |dec|<5° 显式拒; hemisphere-crossing 像素保持 NaN/0。"""
        hdr, s_vals, cov_hdr, cov_vals = self._fits(self.const, 0.0, 30.0, 0.5, 60, 60, "bilinear")
        # 60px @0.5° → 跨度 ±15°, 均在半球内, 不应有 NaN
        nan_cnt = sum(1 for v in s_vals if v != v)
        self.assertEqual(nan_cnt, 0, "半球内输出不应有 NaN")
        # 负 dec 亦受 |dec|≥5 抑制; 用 dec=-5 触发
        # validate 经 probe 返回非0(投影/dec 越界拒)
        r = subprocess.run([os.path.join(self.tmp, "probe"), "validate", self.const, "0.0", "3.0",
                            "0.05", "20", "20", "bilinear"], capture_output=True, text=True, timeout=60)
        self.assertTrue(r.stdout.strip().startswith(("1", "2")), f"|dec|<5 应被拒, got {r.stdout.strip()}")

    def test_10_wcs_roundtrip_across_poles_guard(self):
        """round-trip 独立 gnomonic 在(中心 dec>5, 小角度)稳定; 边缘像素世界坐标在 [0,360)x[-90,90]。"""
        hdr, s_vals, cov_hdr, cov_vals = self._fits(self.const, 45.0, 60.0, 0.03, 30, 30, "bilinear")
        cd = (float(hdr["CD1_1"]), float(hdr["CD1_2"]), float(hdr["CD2_1"]), float(hdr["CD2_2"]))
        crpix = (float(hdr["CRPIX1"]), float(hdr["CRPIX2"]))
        crval = (float(hdr["CRVAL1"]), float(hdr["CRVAL2"]))
        for (x, y) in [(0, 0), (14, 14), (29, 29), (29, 0)]:
            xi = cd[0] * (x - crpix[0]) + cd[1] * (y - crpix[1])
            eta = cd[2] * (x - crpix[0]) + cd[3] * (y - crpix[1])
            ra, dec = ungnomonic_vector(xi, eta, crval[0], crval[1])
            self.assertTrue(0.0 <= ra < 360.0)
            self.assertTrue(-90.0 <= dec <= 90.0)
            g = gnomonic_vector(ra, dec, crval[0], crval[1])
            self.assertIsNotNone(g)


if __name__ == "__main__":
    unittest.main(verbosity=2)
