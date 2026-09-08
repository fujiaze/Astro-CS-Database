#!/usr/bin/env python3
"""P3-004 测试: 输出 FITS 原子写 — header/data/hash/coverage 回环、取消不留假文件、失败清理。

R10-C (bughunt p2 batchL) 增补:
- test_07..08: fsync 时序断言 (LD_PRELOAD interposer 记录 open/fsync/rename 事件序,
  断言 flush(数据写出)→fsync→rename; ASTROCS_FAIL_FSYNC=1 注入 fsync 失败 →
  错误传播且无产物发布)。断电/崩溃语义无法真测, 以调用序断言替代。
- test_09..11: sha256 失败分支 (读失败注入 / 不存在 / 不可读) — 失败必须错误传播,
  绝不写空串/前缀假哈希; 写路径已发布产物哈希失败 → 产物回滚删除。
"""
import hashlib, math, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "phase3_session")
AIO = os.path.join(REPO, "lib", "astro_image_io")
CFITSIO = os.path.join(AIO, "third_party", "cfitsio")


class TestP3Output(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3out_")
        cls.fsync_probe = None
        cls.interposer_so = None
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
        cls.cfitsio_objs = objs
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

    # ---- R10-C 增补: fsync probe (失败注入构建) + interposer ----------------
    @classmethod
    def _build_fsync_probe(cls):
        """构建带 hash-fail 注入钩子的探针 (正常路径行为与 probe_main 同源)。"""
        if getattr(cls, "fsync_probe", None):
            return cls.fsync_probe
        incs = [f"-I{os.path.join(REPO, 'include')}", f"-I{HOST}",
                f"-I{os.path.join(REPO, 'lib', 'common')}",
                f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{CFITSIO}"]
        srcs = [os.path.join(REPO, "tests", "backend", "p3_output_fsync_probe.cpp"),
                os.path.join(HOST, "p3_output.cpp"), os.path.join(HOST, "p3_wcs.cpp"),
                os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp")]
        probe = os.path.join(cls.tmp, "fsync_probe")
        # -Dastrocs_hash_fail_inject: 编入测试钩子; 无 env 时不改变任何行为
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS",
                            "-Dastrocs_hash_fail_inject", *incs, *srcs,
                            *cls.cfitsio_objs, "-lz", "-lzstd", "-llz4", "-o", probe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-800:]
        cls.fsync_probe = probe
        return probe

    @classmethod
    def _build_interposer(cls):
        if getattr(cls, "interposer_so", None):
            return cls.interposer_so
        so = os.path.join(cls.tmp, "fsync_interposer.so")
        r = subprocess.run(["g++", "-shared", "-fPIC", "-O2", "-w",
                            os.path.join(REPO, "tests", "backend",
                                         "p3_output_fsync_interposer.cpp"),
                            "-ldl", "-o", so],
                           capture_output=True, text=True, timeout=120)
        assert r.returncode == 0, r.stderr[-800:]
        cls.interposer_so = so
        return so

    @classmethod
    def _run_fsync_probe(cls, *args, extra_env=None, preload=None):
        env = os.environ.copy()
        env.pop("ASTROCS_HASH_FAIL_INJECT", None)
        env.pop("ASTROCS_FAIL_FSYNC", None)
        if extra_env:
            env.update(extra_env)
        if preload:
            env["LD_PRELOAD"] = preload
        cmd = [cls.fsync_probe, *[str(a) for a in args]]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=120, env=env)
        return r.returncode, r.stdout.strip(), r.stderr

    @staticmethod
    def _parse_events(stderr_text):
        evs = []
        for line in stderr_text.splitlines():
            if line.startswith("EVENT "):
                _, kind, arg = line.split(" ", 2)
                evs.append((kind, arg))
        return evs

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

    # ================= R10-C (bughunt p2 batchL) 增补 =====================

    def test_07_fsync_order_flush_before_fsync_before_rename(self):
        """时序断言: flush(向 tmp 的最后一次 fwrite/fflush) → FSYNC(tmp fd) → RENAME。

        LD_PRELOAD interposer 记录 libc open/fwrite/fflush/fsync/rename 事件序:
        fsync 的 fd 必须是 open(tmp) 的 fd; fsync 之前必须已发生对同一 fd 的
        fwrite/fflush (cfitsio 缓冲已写出); rename 必须在 fsync 之后。
        断电/崩溃语义无法真测, 以调用序断言替代。"""
        out = os.path.join(self.tmp, "g.fits")
        so = self._build_interposer()
        self._build_fsync_probe()
        rc, o, err = self._run_fsync_probe("write", out, 32, 20, 5, preload=so)
        self.assertEqual(rc, 0, f"{o}\n{err[-2000:]}")
        evs = self._parse_events(err)
        tmp_name = os.path.basename(out) + "."
        # OPEN(tmp) 事件 arg = "<path> fd=<n> mode=<m>"; FSYNC/FWRITE/FFLUSH arg = "<fd>"
        def _fd_of(arg: str) -> str:
            return arg.split(" fd=")[1].split(" ")[0]
        opens_tmp = [(i, _fd_of(a)) for i, (k, a) in enumerate(evs)
                     if k == "FOPEN" and os.path.basename(a.split(" fd=")[0]).startswith(tmp_name)
                     and "w+" in a.split("mode=")[-1]]
        self.assertTrue(opens_tmp, f"未见对 tmp 的 OPEN: {evs}")
        tmp_fds = {fd for _, fd in opens_tmp}
        fsyncs = [i for i, (k, _) in enumerate(evs) if k == "FSYNC"]
        renames = [i for i, (k, a) in enumerate(evs) if k == "RENAME"
                   and os.path.basename(a).startswith(tmp_name)]
        self.assertTrue(fsyncs, f"未见 FSYNC: {evs}")
        self.assertTrue(renames, f"未见 RENAME(tmp→out): {evs}")
        # fsync 的 fd 必须等于最后 w+b 打开 tmp 的 fd (fsync 作用于 tmp 本身)
        self.assertEqual(_fd_of(evs[max(i for i, _ in opens_tmp)][1]),
                         evs[max(fsyncs)][1],
                         f"fsync(fd) 必须作用于 tmp: {evs}")
        # flush→fsync: RENAME(发布点)之前, 最后一次对 tmp fd 的数据写出
        # (fwrite/fflush) 必须在 FSYNC 之前 (RENAME 后 fd 可能被 verify 读端
        # 复用, 其 flush 不属于 tmp 写窗口)
        pre_rename = evs[:min(renames)]
        writes_tmp = [i for i, (k, a) in enumerate(pre_rename)
                      if k in ("FWRITE", "FFLUSH") and a in tmp_fds]
        self.assertTrue(writes_tmp, f"未见对 tmp 的数据写出: {evs}")
        self.assertLess(max(writes_tmp), min(fsyncs),
                        f"FSYNC 必须在最后一次 flush(fwrite/fflush) 之后: {evs}")
        self.assertLess(max(fsyncs), min(renames),
                        f"RENAME 必须在 FSYNC 之后: {evs}")
        # 恰好一次 fsync (flush 后不再有第二次 fd fsync)
        self.assertEqual(len(fsyncs), 1, f"应恰好一次 fsync(fd): {evs}")
        # 产物存在且哈希一致
        self.assertEqual(o.split()[0], "OK")
        h = hashlib.sha256(open(out, "rb").read()).hexdigest()
        self.assertEqual(o.split()[1], h)

    def test_08_fsync_failure_propagates_no_publish(self):
        """fsync 失败注入 → 写路径必须报 IO 错误, 不 rename 发布任何产物。"""
        out = os.path.join(self.tmp, "h.fits")
        so = self._build_interposer()
        self._build_fsync_probe()
        rc, o, err = self._run_fsync_probe("write", out, 32, 20, 6, preload=so,
                                           extra_env={"ASTROCS_FAIL_FSYNC": "1"})
        self.assertNotEqual(rc, 0, "fsync 失败必须错误传播")
        self.assertIn("FAIL 2", o, f"应为 P3_OUT_IO: {o}")  # P3_OUT_IO=2
        self.assertFalse(os.path.exists(out), "fsync 失败不得发布产物")
        evs = self._parse_events(err)
        renames = [a for k, a in evs if k == "RENAME"]
        self.assertEqual(renames, [], f"fsync 失败后不得 RENAME: {evs}")
        # 无 tmp 残留
        leftovers = [f for f in os.listdir(self.tmp) if f.startswith("h.fits")]
        self.assertEqual(leftovers, [], f"不应有残留: {leftovers}")

    def test_09_write_hash_fail_no_publish_no_fake_anchor(self):
        """写路径 sha256 读失败注入 → P3_OUT_IO, 产物回滚, 无假哈希发布。"""
        out = os.path.join(self.tmp, "i.fits")
        self._build_fsync_probe()
        rc, o, err = self._run_fsync_probe("write", out, 32, 20, 7,
                                           extra_env={"ASTROCS_HASH_FAIL_INJECT": "1"})
        self.assertNotEqual(rc, 0, "哈希失败必须错误传播, 禁止静默成功")
        self.assertIn("FAIL 2", o, f"应为 P3_OUT_IO: {o}")
        self.assertFalse(os.path.exists(out),
                         "已发布产物哈希失败 → 必须回滚, 不得留下无完整性锚的输出")

    def test_10_verify_missing_file_fails_not_fake_hash(self):
        """verify 对不存在的文件 → P3_OUT_IO, sha256 字段不得是 64 位假哈希。"""
        self._build_fsync_probe()
        missing = os.path.join(self.tmp, "no_such_file.fits")
        rc, o, err = self._run_fsync_probe("verify_missing", missing)
        self.assertEqual(rc, 0, err[-2000:])
        self.assertTrue(o.startswith("IO "), f"应为 P3_OUT_IO: {o}")
        sha_len = int(o.split("sha_len=")[1])
        self.assertNotEqual(sha_len, 64, "禁止对失败读产出 64 位假哈希")

    def test_11_verify_unreadable_file_fails_not_fake_hash(self):
        """verify 对不可读(权限)文件 → P3_OUT_IO, 不产假哈希 (root 环境跳过)。"""
        if os.geteuid() == 0:
            self.skipTest("root 下 chmod 000 不可模拟不可读")
        self._build_fsync_probe()
        out = os.path.join(self.tmp, "j.fits")
        rc, o, err = self._run_fsync_probe("write", out, 32, 20, 8)
        self.assertEqual(rc, 0, f"{o}\n{err[-2000:]}")
        os.chmod(out, 0o000)
        try:
            rc, o, err = self._run_fsync_probe("verify_missing", out)
            # 以非 root 运行本测试进程; fork 的子进程同 uid → 读取被拒
            self.assertTrue(o.startswith("IO "), f"不可读文件应为 P3_OUT_IO: {o}")
            sha_len = int(o.split("sha_len=")[1])
            self.assertNotEqual(sha_len, 64, "禁止对不可读文件产出 64 位假哈希")
        finally:
            os.chmod(out, 0o644)

if __name__ == "__main__":
    unittest.main(verbosity=2)
