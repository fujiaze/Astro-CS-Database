#!/usr/bin/env python3
"""P3-004 测试: 输出 FITS 原子写 — header/data/hash/coverage 回环、取消不留假文件、失败清理。"""
import hashlib, math, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "phase3_session")
AIO = os.path.join(REPO, "lib", "astro_image_io")
CFITSIO = os.path.join(AIO, "third_party", "cfitsio")


class TestP3Output(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3out_")
        incs = [f"-I{os.path.join(REPO, 'include')}", f"-I{HOST}",
                f"-I{os.path.join(REPO, 'lib', 'common')}",
                f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{CFITSIO}"]
        objs = []
        for f in sorted(os.listdir(CFITSIO)):
            if not f.endswith(".c"):
                continue
            if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                         r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                         r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
                continue
            o = os.path.join(cls.tmp, f[:-2] + ".o")
            subprocess.run(["gcc", "-O2", "-w", f"-I{CFITSIO}", "-c",
                            os.path.join(CFITSIO, f), "-o", o], check=True,
                           capture_output=True, timeout=300)
            objs.append(o)
        srcs = [os.path.join(REPO, "tests", "backend", "p3_output_probe_main.cpp"),
                os.path.join(HOST, "p3_output.cpp"), os.path.join(HOST, "p3_wcs.cpp"),
                os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp")]
        cls.exe = os.path.join(cls.tmp, "probe")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *objs, "-lz", "-lzstd", "-llz4", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-800:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args):
        r = subprocess.run([self.exe, *[str(a) for a in args]],
                           capture_output=True, text=True, timeout=120)
        return r.returncode, r.stdout.strip()

    def _fill_ref(self, W, H, seed):
        sig = [float("nan")] * (W * H)
        cov = [0.0] * (W * H)
        for y in range(H):
            for x in range(W):
                i = y * W + x
                c = (W // 4 <= x < 3 * W // 4) and (H // 4 <= y < 3 * H // 4)
                if c:
                    sig[i] = seed * 0.001 + i
                    cov[i] = 1.0
        return sig, cov

    def test_01_write_verify_roundtrip(self):
        """write→verify: header/data/hash/coverage 回环 全 PASS、非平凡覆盖、sha256 与文件一致。"""
        out = os.path.join(self.tmp, "a.fits")
        W, H, seed = 64, 48, 7
        rc, o = self._run("write", out, W, H, -1, seed)
        self.assertEqual(rc, 0, o)
        t = o.split()
        self.assertEqual(t[0], "OK")
        sha, covered, total = t[1], int(t[2]), int(t[3])
        self.assertEqual(total, W * H)
        # coverage 半幅矩形 = (W/2)*(H/2) = 1536
        self.assertEqual(covered, (W // 2) * (H // 2), "非平凡 coverage 计数")
        # sha256 与文件实际一致
        h = hashlib.sha256(open(out, "rb").read()).hexdigest()
        self.assertEqual(sha, h, "checksum 与文件字节一致")
        # verify 回环
        rc, o = self._run("verify", out, W, H, seed)
        self.assertEqual(rc, 0, o)
        t = o.split()
        self.assertEqual(int(t[1]), 1, "reopen_ok=1 (header+data 回环)")
        self.assertEqual(int(t[2]), 1, "coverage_ok=1")
        self.assertEqual(int(t[3]), (W // 2) * (H // 2))
        self.assertEqual(int(t[4]), W * H)

    def test_02_header_keywords_and_provenance(self):
        """独立 FITS reader: CTYPE/CRPIX/CRVAL/CD/BUNIT/BSCALE/BZERO + HIPSID 等 provenance。"""
        out = os.path.join(self.tmp, "b.fits")
        W, H, seed = 32, 20, 3
        self._run("write", out, W, H, -1, seed)
        # 用 CFITSIO 探针读头(通过 probe verify 源码同源, 但这里直接检查文件头字节)
        data = open(out, "rb").read()
        # primary 头含关键字文本
        head = data[:2880].decode("latin1", "ignore")
        for kw in ("CTYPE1","CTYPE2","CRPIX1","CRVAL1","CD1_1","BUNIT","BSCALE","BZERO",
                   "HIPSID","ORDERSEL","SAMPLER","SWVER","HIS"):
            self.assertIn(kw, head, f"missing keyword {kw}")
        # provenance 值
        self.assertIn("ivo://astrocs/test_p3", head)
        # P3-002: BUNIT 来源输入合同(缺省 ADU 面亮度), 绝不 Jy/beam 默认
        self.assertIn("ADU", head)
        self.assertNotIn("Jy/beam", head)
        self.assertIn("bilinear", head)

    def test_03_cancel_no_partial_file(self):
        """取消: cancelled_at_row≥0 → 返回 CANCELLED, 输出文件不存在, 无 .tmp 残留。"""
        out = os.path.join(self.tmp, "c.fits")
        rc, o = self._run("write", out, 40, 30, 10, 5)
        # probe 对 cancel 返回 0 + "CANCELLED"
        self.assertEqual(o, "CANCELLED", o)
        self.assertFalse(os.path.exists(out), "取消后不得落盘完整假文件")
        # 无 tmp 残留
        leftovers = [f for f in os.listdir(self.tmp) if f.startswith("c.fits")]
        self.assertEqual(leftovers, [], f"不应有残留: {leftovers}")

    def test_04_verify_rejects_wrong_shape(self):
        """verify 对不正确 shape → 不误报 (reopen_ok=0) 或 FAIL; 绝不谎报通过。"""
        out = os.path.join(self.tmp, "d.fits")
        self._run("write", out, 50, 40, -1, 2)
        rc, o = self._run("verify", out, 64, 48, 2)   # 错误 W/H
        t = o.split()
        # probe verify 在 shape 不符时对 signal 比较失败 → reopen=0
        self.assertEqual(t[0], "OK")
        self.assertEqual(int(t[1]), 0, "wrong shape → reopen_ok=0")

    def test_05_preserves_values_within_float(self):
        """信号值在 float32 内精确回环(原子落盘无损耗)。"""
        out = os.path.join(self.tmp, "e.fits")
        W, H, seed = 100, 60, 123
        self._run("write", out, W, H, -1, seed)
        # verify 用相同 seed 重建参考 → 回环
        rc, o = self._run("verify", out, W, H, seed)
        t = o.split()
        self.assertEqual(int(t[1]), 1, "float 值精确回环")

    def test_06_overwrite_and_fresh_checksum(self):
        """覆盖写: 同路径第二次写 → 新 checksum(原子替换), 非损坏。"""
        out = os.path.join(self.tmp, "f.fits")
        self._run("write", out, 32, 20, -1, 1)
        sha1 = hashlib.sha256(open(out, "rb").read()).hexdigest()
        self._run("write", out, 32, 20, -1, 99)
        sha2 = hashlib.sha256(open(out, "rb").read()).hexdigest()
        self.assertNotEqual(sha1, sha2, "覆盖写应产生不同 checksum(数据不同)")

if __name__ == "__main__":
    unittest.main(verbosity=2)
