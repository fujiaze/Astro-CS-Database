#!/usr/bin/env python3
"""P3-003 测试: order selector/nearest/bilinear/跨 tile seam Oracle/coverage+NaN/显式拒。"""
import math, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "phase3_session")
AIO = os.path.join(REPO, "lib", "astro_image_io")


class TestP3Resample(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3rs_")
        incs = [f"-I{os.path.join(REPO, 'include')}", f"-I{HOST}",
                f"-I{os.path.join(REPO, 'lib', 'common')}",
                f"-I{os.path.join(REPO, 'lib', 'common', 'healpix')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}"]
        srcs = [os.path.join(REPO, "tests", "backend", "p3_resample_probe_main.cpp"),
                os.path.join(HOST, "p3_resample.cpp"),
                os.path.join(HOST, "hips_properties.cpp"),
                os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp")]
        objs = []
        cdir = os.path.join(AIO, "third_party", "cfitsio")
        for f in sorted(os.listdir(cdir)):
            if not f.endswith(".c"):
                continue
            if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                         r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                         r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
                continue
            o = os.path.join(cls.tmp, f[:-2] + ".o")
            subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c",
                            os.path.join(cdir, f), "-o", o], check=True,
                           capture_output=True, timeout=300)
            objs.append(o)
        cls.exe = os.path.join(cls.tmp, "probe")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *objs, "-lz", "-lzstd", "-llz4", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-800:]
        # fixture(常量域 FIELD/NAN)
        fx = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                            os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                            *srcs[3:], *objs, "-lz", "-lzstd", "-llz4", "-o", fx],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        cls.data = os.path.join(cls.tmp, "data")
        os.makedirs(cls.data)
        for m in ("--make-field", "--make-nan"):
            r = subprocess.run([fx, m, cls.data], capture_output=True, text=True, timeout=300)
            assert "HIPS_FIXTURES_OK" in r.stdout, r.stderr
        cls.field = os.path.join(cls.data, "FIELD.hips")
        cls.nan = os.path.join(cls.data, "NAN.hips")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args):
        r = subprocess.run([self.exe, *[str(a) for a in args]],
                           capture_output=True, text=True, timeout=120)
        return r.returncode, r.stdout.strip()

    def test_01_order_selector_deterministic(self):
        # res(order k) = 0.1125/2^k deg; 选最小 k 使 res ≤ scale, 否则 max_order
        cases = [((3, 0.001), 3), ((3, 10.0), 0), ((3, 0.05), 2), ((0, 0.001), 0),
                 ((20, 1e-6), 17), ((5, 0.1125), 1), ((5, 0.1124), 1)]
        for (maxo, scale), want in cases:
            rc, out = self._run("order", maxo, scale)
            self.assertEqual((rc, out), (0, f"OK {want}"), f"max={maxo} scale={scale}")
        # 重复调用确定性
        rc1, o1 = self._run("order", 5, 0.02)
        rc2, o2 = self._run("order", 5, 0.02)
        self.assertEqual(o1, o2)

    def test_02_unsupported_modes_explicit_reject(self):
        """§4: variance/ivar/weight/flux-per-pixel 输入模式 → UNSUPPORTED(2)。"""
        for m in ("variance", "ivar", "weight", "flux-per-pixel"):
            rc, out = self._run("mode", m)
            self.assertEqual((rc, out), (1, "FAIL 2"), f"{m} 必须显式拒")
        rc, out = self._run("mode", "surface_brightness")
        self.assertEqual((rc, out), (0, "OK"))
        rc, out = self._run("mode", "bogus")
        self.assertEqual(rc, 1)

    def test_03_nearest_tile_constant_oracle(self):
        """Oracle: 12 tile 中心 → nearest 恒等该 tile 常量(逐 tile 不同), coverage=1。"""
        consts = []
        for ipix in range(12):
            rc, out = self._run("pix2ang", ipix)
            ra, dec = map(float, out.split()[1:])
            rc, out = self._run("nearest", self.field, f"{ra:.9f}", f"{dec:.9f}")
            self.assertEqual(rc, 0, out)
            t = out.split()
            v, cov = float(t[1]), int(t[2])
            self.assertEqual(cov, 1)
            self.assertAlmostEqual(v, (ipix + 1) * 1e8, delta=8.0,
                                   msg=f"tile {ipix} 常量不匹配: {v}")  # f32 ulp@1e8=8
            consts.append(v)
        self.assertEqual(len(set(consts)), 12, "12 tile 值必须互异(seam Oracle 前提)")

    def test_04_bilinear_interior_exact_and_seam_bounded(self):
        """bilinear: tile 内=常量; 跨 seam 值有界且 1e-5° 连续(无跳变)。"""
        rc, out = self._run("pix2ang", 3)
        ra, dec = map(float, out.split()[1:])
        rc, out = self._run("bilinear", self.field, f"{ra:.9f}", f"{dec:.9f}")
        t = out.splitlines()[-1].split()
        self.assertEqual(int(t[2]), 1)
        self.assertAlmostEqual(float(t[1]), 4 * 1e8, delta=8.0, msg="tile 内 bilinear=常量")
        # 跨 seam: 球面密集网格上 bilinear ∈ [1e8, 12e8] + 局部连续
        vals = []
        for i in range(400):
            ra = 360.0 * ((i * 2654435761) % 4294967296) / 4294967296.0
            dec = 90.0 - 180.0 * ((i * 40503) % 4294967296) / 4294967296.0 * 0.999
            rc, out = self._run("bilinear", self.field, f"{ra:.9f}", f"{dec:.9f}")
            out = out.splitlines()[-1]
            if not out.startswith("OK"):
                continue
            t = out.split()
            self.assertEqual(int(t[2]), 1, f"全覆盖域 coverage 必须=1: {out}")
            vals.append(float(t[1]))
        self.assertEqual(len(vals), 400)
        self.assertTrue(all(1e8 - 1 <= v <= 12e8 + 1 for v in vals),
                        "bilinear 全域必须界于 tile 常量范围(无 NaN/无越界)")
        # 连续性: 1e-5° 位移 → 值变化 << tile 常量步长
        rc, out = self._run("bilinear", self.field, "210.0", "34.0")
        v1 = float(out.splitlines()[-1].split()[1])
        rc, out = self._run("bilinear", self.field, "210.00001", "34.0")
        v2 = float(out.splitlines()[-1].split()[1])
        self.assertLess(abs(v2 - v1), 1e6, "跨 tile 采样在微小位移下必须连续")

    def test_05_nan_semantics(self):
        """§4: tile 内 NaN → S=NaN + C=1(非错误)。"""
        rc, out = self._run("nearest", self.nan, "210.0", "34.0")
        out = out.splitlines()[-1]
        t = out.split()
        self.assertEqual(rc, 0)
        self.assertEqual(int(t[2]), 1, "NaN 像素 coverage=1")
        self.assertTrue(math.isnan(float(t[1])), "值=NaN")

    def test_06_no_silent_default_open(self):
        rc, out = self._run("open", os.path.join(self.tmp, "no_such_hips"))
        self.assertEqual(rc, 1)
        self.assertIn("FAIL", out)

if __name__ == "__main__":
    unittest.main(verbosity=2)
