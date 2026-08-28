#!/usr/bin/env python3
"""P3-001 测试: HiPS properties 严格解析 — 合法/缺失/恶意路径/边界 order;无 silent default。"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "phase3_session")

GOOD = """creator_did=ivo://astrocs/test
obs_title=fixture
hips_order=0
hips_tile_width=512
hips_frame=equatorial
dataproduct_type=image
hips_tile_format=fits
hips_status=private master
"""


class TestHipsProperties(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3prop_")
        cls.exe = os.path.join(cls.tmp, "probe")
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
             f"-I{HOST}", os.path.join(REPO, "tests", "backend", "hips_properties_probe_main.cpp"),
             os.path.join(HOST, "hips_properties.cpp"), "-o", cls.exe],
            capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _probe(self, mode, arg):
        r = subprocess.run([self.exe, mode, arg], capture_output=True, text=True, timeout=60)
        return r.returncode, r.stdout.strip()

    def _make_hips(self, name, props_text, with_tile=True, order=0):
        root = os.path.join(self.tmp, name)
        os.makedirs(root, exist_ok=True)
        open(os.path.join(root, "properties"), "w", encoding="utf-8").write(props_text)
        if with_tile:
            d = os.path.join(root, f"Norder{order}", "Dir0")
            os.makedirs(d, exist_ok=True)
            open(os.path.join(d, "Npix0.fits"), "wb").write(b"SIMPLE  = T")
        return root

    def test_01_valid_properties(self):
        d = self._make_hips("good", GOOD)
        rc, out = self._probe("validate", d)
        self.assertEqual(rc, 0, out)
        self.assertEqual(out.split(), ["OK", "0", "512", "fits", "equatorial"])

    def test_02_missing_required_key_no_silent_default(self):
        """无 silent default: 每个必需键缺失都必须拒。"""
        for key in ("hips_order", "hips_tile_width", "hips_tile_format", "hips_frame",
                    "dataproduct_type"):
            text = "\n".join(l for l in GOOD.splitlines()
                             if not l.startswith(key + "=")) + "\n"
            rc, out = self._probe("parse", self._write_props(f"miss_{key}", text))
            self.assertEqual(rc, 1, f"缺 {key} 必须拒: {out}")
            self.assertIn("missing required", out, out)

    def _write_props(self, name, text):
        p = os.path.join(self.tmp, name + ".props")
        open(p, "w", encoding="utf-8").write(text)
        return p

    def test_03_invalid_values_rejected(self):
        cases = {
            "order_neg": GOOD.replace("hips_order=0", "hips_order=-3"),
            "order_huge": GOOD.replace("hips_order=0", "hips_order=99"),
            "order_nan": GOOD.replace("hips_order=0", "hips_order=abc"),
            "order_float": GOOD.replace("hips_order=0", "hips_order=2.5"),
            "width_256": GOOD.replace("hips_tile_width=512", "hips_tile_width=256"),
            "format_jpeg": GOOD.replace("hips_tile_format=fits", "hips_tile_format=jpeg"),
            "frame_galactic": GOOD.replace("hips_frame=equatorial", "hips_frame=galactic"),
            "dpt_catalog": GOOD.replace("dataproduct_type=image", "dataproduct_type=catalog"),
            "dup_order": GOOD + "hips_order=1\n",
            "no_eq": GOOD.replace("hips_order=0\n", ""),
        }
        for name, text in cases.items():
            rc, out = self._probe("parse", self._write_props(name, text))
            self.assertEqual(rc, 1, f"{name} 必须拒: {out}")

    def test_04_boundary_orders(self):
        """边界 order: 0 与 kMaxOrder=20 合法; 21 拒。"""
        for order, ok in ((0, True), (20, True), (21, False)):
            text = GOOD.replace("hips_order=0", f"hips_order={order}")
            rc, out = self._probe("parse", self._write_props(f"b{order}", text))
            self.assertEqual(rc, 0 if ok else 1, out)

    def test_05_missing_properties_and_missing_tiles(self):
        # properties 文件缺失
        empty = os.path.join(self.tmp, "empty_dir")
        os.makedirs(empty, exist_ok=True)
        rc, out = self._probe("validate", empty)
        self.assertEqual(rc, 1)
        self.assertIn("properties not found", out)
        # properties 合法但 Norder 目录缺失(缺 tile)
        notile = self._make_hips("notile", GOOD, with_tile=False)
        rc, out = self._probe("validate", notile)
        self.assertEqual(rc, 1)
        self.assertIn("no tiles", out, "缺 tile 必须拒(Norder 目录或 Npix 缺失)")

    def test_06_malicious_paths(self):
        """安全路径: '..'/反斜杠/空段/NUL 拒; 正常相对与绝对路径过。"""
        for bad in ("a/../b", "../escape", "/a/../b", "a\\..\\b", "a//b", ""):
            rc, out = self._probe("path", bad)
            self.assertEqual(rc, 1, f"恶意路径必须拒: {bad!r} → {out}")
        for good in ("run/phase2/x.hips/signal", "/tmp/astrocs/p3_test"):
            rc, out = self._probe("path", good)
            self.assertEqual(rc, 0, f"正常路径不应拒: {good!r} → {out}")

if __name__ == "__main__":
    unittest.main(verbosity=2)
