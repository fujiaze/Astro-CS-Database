#!/usr/bin/env python3
"""FIX-401 CLI 验收: HiPS tile 原子发布 + Phase2 暂存区 (端到端)。

权威: ASTROCS_DESIGN.md §10「I/O 与原子产品」/ GAP_AUDIT G3-1。

方法 (外部视角, 不调用库内部实现):
  * 合成全链: 真 CLI normalize (Phase1 逐帧 HiPS) → 真 CLI mosaic (Phase2 天球
    HiPS); 逐产品独立复算 sha256 并与 run manifest 的 artifacts 对拍; 逐产品根
    核对完成清单 manifest.json 齐备; 核对无 .tmp. / 无 .p2_mosaic_staging.tmp.* 残留。
  * 磁盘满: unshare -Ur -m + 真实小 tmpfs (无需 root) 跑 normalize ⇒ 必须
    fail-closed 为 exit 10, 且 output_dir 内无完成清单、无截断 tile、无临时残留。
  * Phase2 暂存区: 注入 ASTROCS_HIPS_TILE_FAULT=tile_diskfull 跑 mosaic ⇒ tile 写入
    必败, 而 output_dir **不得**出现 signal/support 半成品目录与完成清单 (证明
    产品在运行私有暂存区生成、失败即丢弃, 不是直写输出目录)。
"""
import hashlib, json, os, re, shutil, subprocess, tempfile, textwrap, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "astrocs"))

from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402

AIO = next((p for p in (os.path.join(REPO, "lib", "infrastructure", "aio"),
                        os.path.join(REPO, "lib", "astro_image_io")) if os.path.isdir(p)),
           os.path.join(REPO, "lib", "infrastructure", "aio"))
SHARED = next((p for p in (os.path.join(REPO, "lib", "algorithms", "shared"),
                           os.path.join(REPO, "lib", "common")) if os.path.isdir(p)),
              os.path.join(REPO, "lib", "algorithms", "shared"))
HEALPIX_SRC = os.path.join(SHARED, "healpix", "healpix_core.cpp")
SKIP_FITS = (r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
             r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
             r"imcopy|imarith|tabcompile|sortcol|tabselect")

CD_DEG = 0.5
WCS_EXPLICIT = {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                "cd11": -CD_DEG, "cd12": 0.0, "cd21": 0.0, "cd22": CD_DEG}
DRIZZLE = {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1}
FITS_BLOCK = 2880


def unshare_available():
    if os.name != "posix" or not shutil.which("unshare"):
        return False
    r = subprocess.run(["unshare", "-Ur", "-m", "--propagation", "private", "true"],
                       capture_output=True, text=True, timeout=60)
    return r.returncode == 0


UNSHARE = unshare_available()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def walk_files(root):
    out = []
    for dirpath, _dirs, names in os.walk(root):
        for n in names:
            out.append(os.path.join(dirpath, n))
    return out


def is_tile(name):
    return bool(re.fullmatch(r"Npix\d+\.fits", name))


def jsonl(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


class TestFix401HipsAtomic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="fix401_cli_")
        cdir = os.path.join(AIO, "third_party", "cfitsio")
        objs = []
        for f in sorted(os.listdir(cdir)):
            if not f.endswith(".c") or re.search(SKIP_FITS, f):
                continue
            o = os.path.join(cls.tmp, f[:-2] + ".o")
            subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                            "-o", o], check=True, capture_output=True, timeout=300)
            objs.append(o)
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{cdir}", f"-I{SHARED}", f"-I{os.path.dirname(HEALPIX_SRC)}"]
        cls.p1fx = os.path.join(cls.tmp, "p1fx")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            os.path.join(REPO, "tests", "backend", "phase1_fixture_main.cpp"),
                            os.path.join(AIO, "src", "aio_fits.cpp"),
                            os.path.join(AIO, "src", "aio_api.cpp"),
                            os.path.join(AIO, "src", "aio_log.cpp"),
                            os.path.join(AIO, "src", "aio_compressor.cpp"),
                            *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p1fx],
                           capture_output=True, text=True, timeout=900)
        assert r.returncode == 0, r.stderr[-800:]
        cls.data = os.path.join(cls.tmp, "data")
        os.makedirs(cls.data)
        r = subprocess.run([cls.p1fx, "--make-noisy", cls.data], capture_output=True,
                           text=True, timeout=300, cwd=run_cwd())
        assert "FIXTURES_OK" in r.stdout, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── helpers ─────────────────────────────────────────────────────────────
    def _write_cfg(self, name, doc):
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    def _p1_cfg(self, out, lights):
        return self._write_cfg("p1_%s.json" % os.path.basename(out), {
            "schema_version": "1", "input_lights": list(lights),
            "master_bias": os.path.join(self.data, "bias.fits"),
            "master_dark": os.path.join(self.data, "dark.fits"),
            "master_flat": os.path.join(self.data, "flat.fits"),
            "dark_optimization": True, "output_dir": out,
            "wcs": dict(WCS_EXPLICIT), "drizzle": dict(DRIZZLE)})

    def _p2_cfg(self, out, hips):
        return self._write_cfg("p2_%s.json" % os.path.basename(out),
                               {"schema_version": "1", "hips_paths": list(hips),
                                "output_dir": out})

    def _run(self, *args, env=None, timeout=600):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              timeout=timeout, cwd=run_cwd(), env=env)

    @staticmethod
    def _manifest_of(events):
        ev = [e for e in events if e.get("kind") == "artifact" and
              e.get("role") == "run_manifest"]
        assert ev, "事件流缺 run_manifest artifact"
        with open(ev[-1]["path"], encoding="utf-8") as fh:
            return json.load(fh), ev[-1]["path"]

    def _check_no_residue_and_tiles(self, root):
        """公共判据: 无 .tmp. 残留; 所有 Npix*.fits 为完整 FITS 块长。"""
        bad_tmp, bad_tile = [], []
        for p in walk_files(root):
            n = os.path.basename(p)
            if ".tmp." in n:
                bad_tmp.append(p)
            if is_tile(n) and os.path.getsize(p) % FITS_BLOCK != 0:
                bad_tile.append(p)
        return bad_tmp, bad_tile

    # ── 合成全链 Phase1/Phase2: 哈希 + 完成清单 ─────────────────────────────
    def test_01_phase1_phase2_products_hashes_and_manifests(self):
        p1a = os.path.join(self.tmp, "p1a")
        p1b = os.path.join(self.tmp, "p1b")
        p2 = os.path.join(self.tmp, "p2")
        for d in (p1a, p1b, p2):
            os.makedirs(d, exist_ok=True)
        light1 = os.path.join(self.data, "light_1.fits")
        light2 = os.path.join(self.data, "light_2.fits")

        # ── Phase1 (normalize): 两个独立进程 ──
        hips = []
        for light, out in ((light1, p1a), (light2, p1b)):
            r = self._run("normalize", "--json", self._p1_cfg(out, [light]),
                          "--events-jsonl", "-y")
            self.assertEqual(r.returncode, 0, r.stderr[-500:])
            man, _mp = self._manifest_of(jsonl(r.stdout))
            self.assertEqual(man["status"], "complete")
            self.assertTrue(man.get("artifacts"), "Phase1 run manifest 必须带 artifacts")
            for a in man["artifacts"]:
                self.assertTrue(os.path.isfile(a["path"]), "artifact 缺失: %s" % a["path"])
                self.assertEqual(sha256_file(a["path"]), a["sha256"],
                                 "Phase1 artifact sha256 与磁盘不一致: %s" % a["path"])
                self.assertGreater(a["size_bytes"], 0)
            # HiPS 产品根 = 含完成清单 manifest.json 的子目录 (output_dir 下还有
            # run graph 等运行面产物, 不是 HiPS 产品)
            frame_dirs = [os.path.join(out, d) for d in sorted(os.listdir(out))
                          if os.path.isdir(os.path.join(out, d)) and
                          os.path.isfile(os.path.join(out, d, "manifest.json"))]
            self.assertTrue(frame_dirs, "Phase1 必须产出逐帧 HiPS 产品目录")
            for fd in frame_dirs:
                mpath = os.path.join(fd, "manifest.json")
                self.assertTrue(os.path.isfile(mpath),
                                "Phase1 产品根缺完成清单: %s" % fd)
                with open(mpath, encoding="utf-8") as fh:
                    hm = json.load(fh)
                self.assertIn("products", hm)
                self.assertIn("signal", hm["products"])
                self.assertGreater(hm["n_leaf_tiles"], 0)
                hips.append(fd)
            bad_tmp, bad_tile = self._check_no_residue_and_tiles(out)
            self.assertEqual(bad_tmp, [], "Phase1 残留 .tmp.: %s" % bad_tmp[:3])
            self.assertEqual(bad_tile, [], "Phase1 截断 tile: %s" % bad_tile[:3])

        # ── Phase2 (mosaic): 消费两个 Phase1 持久化产品 ──
        r = self._run("mosaic", "--json", self._p2_cfg(p2, hips), "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        man2, _mp2 = self._manifest_of(jsonl(r.stdout))
        self.assertEqual(man2["status"], "complete")
        self.assertTrue(man2.get("artifacts"))
        for a in man2["artifacts"]:
            self.assertTrue(os.path.isfile(a["path"]), "Phase2 artifact 缺失: %s" % a["path"])
            self.assertEqual(sha256_file(a["path"]), a["sha256"],
                             "Phase2 artifact sha256 与磁盘不一致: %s" % a["path"])
        mpath = os.path.join(p2, "manifest.json")
        self.assertTrue(os.path.isfile(mpath), "Phase2 mosaic 产品根缺完成清单")
        with open(mpath, encoding="utf-8") as fh:
            hm2 = json.load(fh)
        self.assertIn("products", hm2)
        self.assertIn("signal", hm2["products"])
        self.assertIn("support", hm2["products"])
        self.assertTrue(os.path.isdir(os.path.join(p2, "signal")))
        bad_tmp, bad_tile = self._check_no_residue_and_tiles(p2)
        self.assertEqual(bad_tmp, [], "Phase2 残留 .tmp.: %s" % bad_tmp[:3])
        self.assertEqual(bad_tile, [], "Phase2 截断 tile: %s" % bad_tile[:3])
        stray = [d for d in os.listdir(self.tmp) if ".p2_mosaic_staging.tmp." in d]
        self.assertEqual(stray, [], "Phase2 暂存区残留: %s" % stray)

    # ── 磁盘满: exit 10 且无半成品 ──────────────────────────────────────────
    @unittest.skipUnless(UNSHARE, "需要 unshare -Ur -m（无 root 的用户命名空间挂载）")
    def test_02_disk_full_exit10_no_partial(self):
        mnt = os.path.join(self.tmp, "tiny")
        os.makedirs(mnt, exist_ok=True)
        light1 = os.path.join(self.data, "light_1.fits")
        light2 = os.path.join(self.data, "light_2.fits")
        seen = []
        for size in ("1M", "2M", "4M"):
            out = os.path.join(mnt, "out_" + size)
            cfg = self._p1_cfg(out, [light1, light2])
            log_o = os.path.join(self.tmp, "df_%s.out" % size)
            log_e = os.path.join(self.tmp, "df_%s.err" % size)
            script = textwrap.dedent("""
                set -e
                mkdir -p "{mnt}"
                mount -t tmpfs -o size={size} tmpfs "{mnt}" 2>/dev/null || true
                cd "{repo}"
                set +e
                "{exe}" normalize --json "{cfg}" --events-jsonl -y > "{lo}" 2> "{le}"
                echo CLI_RC=$?
                echo -n "DF_AFTER="; df -k --output=avail "{mnt}" | tail -1
            """).format(mnt=mnt, size=size, repo=REPO, exe=EXE, cfg=cfg,
                        lo=log_o, le=log_e)
            r = subprocess.run(["unshare", "-Ur", "-m", "--propagation", "private",
                                "bash", "-c", script], capture_output=True, text=True,
                               timeout=900, cwd=run_cwd())
            self.assertIn("CLI_RC=", r.stdout, r.stderr[-400:])
            rc = int(re.search(r"CLI_RC=(\d+)", r.stdout).group(1))
            with open(log_o, encoding="utf-8") as fh:
                out_s = fh.read()
            with open(log_e, encoding="utf-8") as fh:
                err_s = fh.read()
            events = jsonl(out_s)
            # 门（任务书）: 磁盘满 → exit 10。
            # 已知跨域缺陷（FIX-401 归因实验证据）: CLI 的 exit-10 判定是**事后**探针
            # probe_writable(output_dir)（lib/infrastructure/cli/disk_gate.h），写 4 KiB
            # 探针文件看 errno。本任务 step 3 要求失败路径清理临时产物 —— 清理把磁盘满
            # 释放掉后，探针在"已不再满"的文件系统上必然成功 ⇒ failure_kind=none ⇒ 退回
            # 7(IO)。实测（归因实验）: 失败瞬间 free=0KB；清理后 free=884KB（= 被删的部分
            # tile 字节数），rc=7；把清理关掉同一场景 rc=10、free=0KB。
            # 修复后: 判据改为 aio 在**失败瞬间（清理之前）**分类 → 失败节点 manifest
            # error_kind="disk_full" → runtime_client 映射 10；探针只作兜底。
            self.assertEqual(rc, 10,
                             "磁盘满必须 fail-closed 为 exit 10 (size=%s, 实得 %d): %s"
                             % (size, rc, err_s[-300:]))
            errs = [e for e in events if e.get("severity") == "error"]
            self.assertTrue(errs, "必须发 error 事件 (size=%s)" % size)
            finals = [e for e in events if e.get("kind") == "final"]
            self.assertTrue(finals and finals[-1].get("exit_code") == 10,
                            "final 事件必须记 exit_code=10 (size=%s): %s"
                            % (size, finals[-1:] or events[-1:]))
            # 判据来源必须是"失败本身分类"，不是探针兜底（探针兜底会给出
            # "disk write failed (disk_full); pipeline rc=..." 的 fail_reason）
            self.assertNotIn("disk write failed",
                             finals[-1].get("message", "") if finals else "",
                             "不得依赖事后探针兜底路径 (size=%s)" % size)
            n_tiles = 0
            if os.path.isdir(out):
                for p in walk_files(out):
                    self.assertNotEqual(os.path.basename(p), "manifest.json",
                                        "磁盘满不得留下完成清单: %s" % p)
                    if is_tile(os.path.basename(p)):
                        n_tiles += 1
                bad_tmp, bad_tile = self._check_no_residue_and_tiles(out)
                self.assertEqual(bad_tmp, [], "磁盘满残留 .tmp.: %s" % bad_tmp[:3])
                self.assertEqual(bad_tile, [], "磁盘满留下截断 tile: %s" % bad_tile[:3])
            seen.append((size, rc, n_tiles))
        print("FIX401 disk-full sweep (size, rc, n_tiles): %s" % seen)

    # ── Phase2 暂存区: 失败不落 output_dir ──────────────────────────────────
    def test_03_phase2_staging_not_direct_write(self):
        p1 = os.path.join(self.tmp, "p1_stage")
        p2 = os.path.join(self.tmp, "p2_stage")
        os.makedirs(p1, exist_ok=True)
        os.makedirs(p2, exist_ok=True)
        # 两帧: Phase2 的 production control-ivar 权重要求有可用控制帧 ivar
        # (单帧输入会被 upm_fit 显式判错, 与本测试的注入面无关)。
        r = self._run("normalize", "--json",
                      self._p1_cfg(p1, [os.path.join(self.data, "light_1.fits"),
                                        os.path.join(self.data, "light_2.fits")]),
                      "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        hips = [os.path.join(p1, d) for d in sorted(os.listdir(p1))
                if os.path.isdir(os.path.join(p1, d)) and
                os.path.isfile(os.path.join(p1, d, "manifest.json"))]
        self.assertTrue(hips, "Phase1 必须产出逐帧 HiPS 产品目录")
        # 注入 tile 写入必败 (磁盘满等价面): Phase2 必须失败, 且 output_dir 内
        # **不得**出现 signal/support 半成品目录与完成清单 —— 证明产品在运行私有
        # 暂存区生成 (直写输出目录时 signal/ 会出现)。
        env = dict(os.environ, ASTROCS_HIPS_TILE_FAULT="tile_diskfull")
        r2 = self._run("mosaic", "--json", self._p2_cfg(p2, hips), "-y", env=env)
        self.assertNotEqual(r2.returncode, 0,
                            "tile 写入必败时 mosaic 必须 fail-closed")
        self.assertFalse(os.path.exists(os.path.join(p2, "manifest.json")),
                         "失败路径不得落完成清单")
        self.assertFalse(os.path.exists(os.path.join(p2, "signal")),
                         "失败路径 output_dir 不得出现 signal 半成品目录 (暂存区未直写)")
        self.assertFalse(os.path.exists(os.path.join(p2, "support")),
                         "失败路径 output_dir 不得出现 support 半成品目录")
        stray = [d for d in os.listdir(self.tmp) if ".p2_mosaic_staging.tmp." in d]
        self.assertEqual(stray, [], "失败路径必须清理暂存区: %s" % stray)
        # 反向判别力: 非空间类 I/O 失败**不得**被升格为 exit 10 (磁盘门不得吞掉
        # 其它 I/O 失败语义 —— 仍须是 7=IO)。
        p2io = os.path.join(self.tmp, "p2_stage_io")
        os.makedirs(p2io, exist_ok=True)
        env_io = dict(os.environ, ASTROCS_HIPS_TILE_FAULT="tile_write_fail")
        r2b = self._run("mosaic", "--json", self._p2_cfg(p2io, hips), "-y", env=env_io)
        self.assertEqual(r2b.returncode, 7,
                         "非空间类写失败必须仍为 exit 7 (实得 %d): %s"
                         % (r2b.returncode, r2b.stderr[-300:]))
        self.assertFalse(os.path.exists(os.path.join(p2io, "manifest.json")),
                         "非空间类写失败同样不得落完成清单")
        self.assertFalse(os.path.exists(os.path.join(p2io, "signal")),
                         "非空间类写失败 output_dir 不得出现半成品")
        # 阴性对照: 同一输入无注入 ⇒ 成功 (证明上面的失败来自注入而非输入本身)
        p2ok = os.path.join(self.tmp, "p2_stage_ok")
        os.makedirs(p2ok, exist_ok=True)
        r3 = self._run("mosaic", "--json", self._p2_cfg(p2ok, hips), "-y")
        self.assertEqual(r3.returncode, 0, r3.stderr[-400:])
        self.assertTrue(os.path.isfile(os.path.join(p2ok, "manifest.json")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
