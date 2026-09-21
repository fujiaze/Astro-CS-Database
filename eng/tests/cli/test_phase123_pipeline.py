#!/usr/bin/env python3
"""E2E: 正式 CLI normalize → 持久化 → mosaic → 持久化 → export 真链路（CLI-002 重锚）。

数据流(与生产 M42 流程一致: 逐帧 normalize 产品 → mosaic 拼接 → export 投影):
  normalize(light_1) ─┐
  normalize(light_2) ─┴→ mosaic(hips_paths=[p1a,p1b]) ─→ export(读 p2out)
每阶段独立进程、只读上游**持久化 HiPS 产品**（无任何 fixture 顶替）; 断言
rc=0 / status=complete / artifacts 非空且 sha256+size 可独立复算。

CLI-002 重锚依据: CLI-001 唯一命令树 = normalize/mosaic/export（ASTROCS_DESIGN §6.2;
docs/api/CLI_PROTOCOL_V1.md §1 旧 phase1|2|3 run / verify / graph 均为已删除别名 → rc=2）。
旧用例的 verify --json --run-manifest 闭环改为**测试内独立复算** sha256/size（判据不变、
不依赖已删命令）; graph --preset 退役判据保留为负例（rc=2）。

负例矩阵(全非零且不写 complete):
  空 input_lights→2; 缺输入文件→3; 缺 HiPS 输入→3; 缺逐帧 ivar 产品→2;
  无 ivar fixture→2。
正例补充: 含 ivar 的合成 fixture → rc=0 且
manifest.uncertainty_available=true（真实不确定度面可达）。
Phase1 侧同源: light 帧用 --make-noisy（确定性噪声，校准后 σ≈1.8 ADU）⇒ 噪声模型
  有合格 patch（σ>0）⇒ normalize 产出 variance/ivar 子产品 ⇒ normalize→mosaic
  默认逐帧逆方差链可闭合（--make 的常量域帧 σ=0 ⇒ 整帧退化 ⇒ 默认链 fail-closed）。

权重口径（ASTROCS_DESIGN §2.1 + GAP_AUDIT §9.73 裁决 A44「不存在权重模式」）:
  HiPS 里**存**的是**帧级 SNR**（与稀疏相对 SNR 比值）; 权重是阶段二消费 SNR 时
  按覆盖该像素的帧集合**现场算出的派生量**，不是配置键 ⇒ 配置面**不得**出现
  weight_mode / legacy_allow_weight_fallback（CLI 白名单已摘除，出现即 rc=3）。
  生产唯一路径 = 逐帧逆方差; 缺逐帧 ivar 时按 DATA-UNC-001 §30.1 fail-closed
  （禁静默回退等权），故「无 ivar ⇒ rc=2」是本文件的负例判据。
"""
import hashlib, json, os, re, shutil, signal, subprocess, sys, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# ROOT-008: 唯一产品二进制 build/astrocs（旧 build/cli/astrocs 已退役）
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "astrocs"))


def _pick(*cands):
    for p in cands:
        if os.path.isdir(p):
            return p
    return cands[0]


AIO = _pick(os.path.join(REPO, "lib", "infrastructure", "aio"),
            os.path.join(REPO, "lib", "astro_image_io"))
SHARED = _pick(os.path.join(REPO, "lib", "algorithms", "shared"),
               os.path.join(REPO, "lib", "common"))
HEALPIX_SRC = os.path.join(SHARED, "healpix", "healpix_core.cpp")

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402

SKIP_FITS = r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|" \
            r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|" \
            r"imcopy|imarith|tabcompile|sortcol|tabselect"

# 合成 E2E WCS: 64² 帧覆盖 ~32° 天区(0.5°/px), 使 nside=512 的 HiPS 覆盖
# 足够 control 单元(8×8/tile)供 mosaic sampler 取得 >=2 clean frame 观测。
# 绝对天体测量正确性属 Batch 2（WcsTan oracle）; 此处只保证 WCS 自洽可 drizzle。
CD_DEG = 0.5
WCS_EXPLICIT = {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                "cd11": -CD_DEG, "cd12": 0.0, "cd21": 0.0, "cd22": CD_DEG}
DRIZZLE = {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1}


def _cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c") or re.search(SKIP_FITS, f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


def _common_incs():
    return [f"-I{os.path.join(REPO, 'lib', 'include')}",
            f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
            f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
            f"-I{SHARED}", f"-I{os.path.dirname(HEALPIX_SRC)}"]


def _aio_srcs():
    return [os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
            os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
            os.path.join(AIO, "src", "aio_fits.cpp"),
            os.path.join(AIO, "src", "aio_api.cpp"),
            os.path.join(AIO, "src", "aio_log.cpp"),
            os.path.join(AIO, "src", "aio_compressor.cpp"),
            HEALPIX_SRC]


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


def manifest_event(events):
    return [e for e in events if e["kind"] == "artifact" and
            e.get("role") == "run_manifest"][-1]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


class TestPhase123Pipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="syn009_")
        objs = _cfitsio_objs(cls.tmp)
        incs = _common_incs()
        cls.p1 = os.path.join(cls.tmp, "p1fx")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            os.path.join(REPO, "eng", "tests", "backend", "phase1_fixture_main.cpp"),
                            os.path.join(AIO, "src", "aio_fits.cpp"),
                            os.path.join(AIO, "src", "aio_api.cpp"),
                            os.path.join(AIO, "src", "aio_log.cpp"),
                            os.path.join(AIO, "src", "aio_compressor.cpp"),
                            *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p1],
                           capture_output=True, text=True, timeout=900)
        assert r.returncode == 0, r.stderr[-800:]
        cls.p2 = os.path.join(cls.tmp, "p2fx")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp"),
                            *_aio_srcs(), *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p2],
                           capture_output=True, text=True, timeout=900)
        assert r.returncode == 0, r.stderr[-800:]
        cls.p1data = os.path.join(cls.tmp, "p1data")
        os.makedirs(cls.p1data)
        # --make-noisy（非 --make）：常量域 light 帧 σ=0 ⇒ 噪声模型整帧退化 ⇒
        # Phase1 产品无 variance/ivar、无帧级 SNR ⇒ A44 后的默认（唯一）逐帧逆方差
        # 权重链按 DATA-UNC-001 §30.1 fail-closed（mosaic rc=2，禁静默等权）。
        # §2.1 要求 HiPS 存帧级 SNR，故端到端正例必须喂非退化噪声面。
        r = subprocess.run([cls.p1, "--make-noisy", cls.p1data], capture_output=True,
                           text=True, timeout=120, cwd=run_cwd())
        assert "FIXTURES_OK" in r.stdout, r.stderr
        # 含 variance/ivar 的 Phase2 fixture (B1-A5 真实不确定度面正例)
        cls.hips = os.path.join(cls.tmp, "hips")
        os.makedirs(cls.hips)
        for m in ("--make", "--make-field", "--make-nan"):
            subprocess.run([cls.p2, m, cls.hips], capture_output=True, text=True, timeout=120,
                           cwd=run_cwd())
        # 无 ivar 的 Phase2 fixture (默认逐帧逆方差负例; §2.1 / §9.73 A44)
        cls.noivar = os.path.join(cls.tmp, "noivar")
        os.makedirs(cls.noivar)
        subprocess.run([cls.p2, "--make-noivar", cls.noivar], capture_output=True,
                       text=True, timeout=120, cwd=run_cwd())
        # 逐帧 normalize 持久化产品目录
        cls.p1a = os.path.join(cls.tmp, "p1_light1")
        os.makedirs(cls.p1a)
        cls.p1b = os.path.join(cls.tmp, "p1_light2")
        os.makedirs(cls.p1b)
        cls.p2out = os.path.join(cls.tmp, "p2out")
        os.makedirs(cls.p2out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── config helpers（§6.2 命令面配置形态；与 --template 同源）──
    def _write(self, name, doc):
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    def _p1_cfg(self, out, lights, with_chain=True):
        doc = {"schema_version": "1", "input_lights": list(lights),
               "master_bias": os.path.join(self.p1data, "bias.fits"),
               "master_dark": os.path.join(self.p1data, "dark.fits"),
               "master_flat": os.path.join(self.p1data, "flat.fits"),
               # BIAS-001: 夹具 dark(含 bias) ⇒ 显式声明兼容式（否则默认标准式给 (200-100-150)/1.25=-40）。
               "dark_optimization": True,
               "output_dir": out}
        if with_chain:
            doc["wcs"] = dict(WCS_EXPLICIT)
            doc["drizzle"] = dict(DRIZZLE)
        return self._write("p1_%s.json" % os.path.basename(out), doc)

    def _p2_cfg(self, out, hips, extra=None):
        doc = {"schema_version": "1", "hips_paths": list(hips), "output_dir": out}
        if extra:
            doc.update(extra)
        return self._write("p2_%s.json" % os.path.basename(out), doc)

    def _p3_cfg(self, out, hips_dir):
        return self._write("p3_%s.json" % os.path.basename(out), {
            "schema_version": "1",
            "source": {"hips_dir": hips_dir},
            "output_dir": out,
            "center": {"ra_deg": 210.0, "dec_deg": 34.0},
            "scale_deg_per_px": 0.5, "width_px": 20, "height_px": 20,
            "sampler": "bilinear", "projection": "TAN",
            "coverage_output": "mask",
            # FZ-P3-MODES（FROZEN；docs/algorithms/v6/frozen/02_GATE_AND_MUTATION_FREEZE.md）：
            # phase3 resample 节点要求显式声明 output_mode（缺键即 REJECT）。
            "output_mode": "surface_brightness"})

    def _run(self, session, cfg, timeout=600, extra=None):
        return subprocess.run([EXE, session, "--json", cfg, "--events-jsonl", "-y",
                               *(extra or [])],
                              capture_output=True, text=True, timeout=timeout, cwd=run_cwd())

    @staticmethod
    def _artifact_names(man):
        return {os.path.basename(a["path"]) for a in man["artifacts"]}

    @staticmethod
    def _complete_manifests(d):
        out = []
        for f in os.listdir(d):
            if f.startswith("astrocs_run_") and f.endswith(".json"):
                try:
                    with open(os.path.join(d, f), encoding="utf-8") as fh:
                        m = json.load(fh)
                    if m.get("status") == "complete":
                        out.append(m)
                except Exception:  # noqa: BLE001
                    pass
        return out

    def _assert_manifest_closed(self, r, out, phase, min_art=1):
        """闭环判据: final ok + 唯一 complete manifest + 逐 artifact 独立复算 sha256/size。"""
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        mans = self._complete_manifests(out)
        self.assertEqual(len(mans), 1, "应有且仅有一个 complete manifest")
        man = mans[0]
        self.assertEqual(man["phases"], [phase])
        self.assertGreaterEqual(len(man["artifacts"]), min_art)
        for a in man["artifacts"]:
            self.assertTrue(os.path.isfile(a["path"]), "artifact 必须在盘: " + a["path"])
            self.assertGreater(a["size_bytes"], 0, a["path"])
            # CLI-002: 旧 verify --run-manifest 已删 → 测试内独立复算（判据不变）
            self.assertEqual(sha256_file(a["path"]), a["sha256"],
                             "artifact sha256 与 manifest 不符: " + a["path"])
            self.assertEqual(os.path.getsize(a["path"]), a["size_bytes"], a["path"])
        mf = manifest_event(events)["path"]
        with open(mf, encoding="utf-8") as fh:
            self.assertEqual(json.load(fh)["status"], "complete")
        return man

    # ── normalize: 逐帧持久化 + 校准数值 Oracle ──
    def test_01_normalize_products_persist_hips(self):
        for out, idx in ((self.p1a, 1), (self.p1b, 2)):
            cfg = self._p1_cfg(out, [os.path.join(self.p1data, "light_%d.fits" % idx)])
            r = self._run("normalize", cfg)
            self.assertEqual(r.returncode, 0, r.stderr[-500:])
            events = jsonl_lines(r.stdout)
            self.assertEqual([e["sequence"] for e in events], list(range(len(events))))
            self.assertEqual(events[-1]["kind"], "final")
            self.assertEqual(events[-1]["status"], "ok")
            kinds = [e["kind"] for e in events]
            self.assertIn("resource", kinds)
            self.assertIn("backend", kinds)
            man = self._assert_manifest_closed(r, out, 1, min_art=8)
            names = self._artifact_names(man)
            self.assertIn("calibrated_light_%d.fits" % idx, names)
            # 末端直写标准 HiPS（signal/properties + Moc/metadata）
            for want in ("p1_sources.json", "p1_psf.json", "p1_wcs.json", "p1_flux.json",
                         "p1_snr.json", "p1_final.json"):
                self.assertIn(want, names, "缺产物 " + want)
            # P0-21 §3.4: 每帧一个 HiPS 产品目录 output_dir/<frame_key>/（1 帧配置）。
            self.assertTrue(
                os.path.isfile(os.path.join(out, "light_%d" % idx, "signal", "properties")),
                "normalize 必须为每帧持久化 IVOA HiPS signal/properties")
            with open(os.path.join(out, "p1_products.json"), encoding="utf-8") as fh:
                prods = json.load(fh)
            self.assertEqual(prods["n_products"], 1)
            self.assertEqual(prods["n_frames"], 1)
            self.assertEqual(prods["hips_paths"],
                             [os.path.join(out, "light_%d" % idx)])
            # 数值 Oracle 归 eng/tests/cli/test_phase1_inprocess.py::test_02
            # （校准公式订正面 = calibration/BIAS-001 域；本用例只判跨命令链路:
            #  normalize 产物能否被 mosaic 只读消费、mosaic 产物能否被 export 只读消费）
            self.assertGreater(os.path.getsize(
                os.path.join(out, "calibrated_light_%d.fits" % idx)), 0)

    # ── mosaic: 只读两个 normalize 持久化产品（独立进程），默认逐帧逆方差 ──
    def test_02_mosaic_consumes_normalize_products(self):
        # P0-21 §3.4: normalize 产品 = 逐帧目录；mosaic 直接消费 p1_products.json
        # 的 hips_paths（可串行衔接）。
        # §2.1 / §9.73 裁决 A44: **无 weight_mode 配置键** —— 权重是阶段二消费帧级
        # SNR 时按覆盖该像素的帧集合现场算出的派生量；生产路径恒为逐帧逆方差
        # （normalize 产品已含 variance/ivar）。
        cfg = self._p2_cfg(self.p2out,
                           [os.path.join(self.p1a, "light_1"),
                            os.path.join(self.p1b, "light_2")])
        r = self._run("mosaic", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        # 真链路必须产生重叠控制点（>=2 clean frame/UPM 几何前提）
        with open(os.path.join(self.p2out, "p2_samples.json"), encoding="utf-8") as fh:
            smp = json.load(fh)
        self.assertGreater(smp.get("stats", {}).get("overlap_controls", 0), 0,
                           "mosaic 必须在两个 normalize 产品的重叠区取得控制点")
        man = self._assert_manifest_closed(r, self.p2out, 2, min_art=5)
        # B1-A5 + DATA-UNC-001 §30.1: 默认（唯一）生产路径 = 逐帧逆方差 ⇒
        # 不确定度面必须真实可达（强于旧的「显式等权 ⇒ false」判据）
        self.assertIs(man.get("uncertainty_available"), True)
        self.assertTrue(os.path.isfile(os.path.join(self.p2out, "signal", "properties")),
                        "mosaic 必须持久化 mosaic HiPS")

    # ── export: 只读 mosaic 持久化产品（独立进程） ──
    def test_03_export_consumes_mosaic_product(self):
        out = os.path.join(self.tmp, "p3out")
        os.makedirs(out, exist_ok=True)
        cfg = self._p3_cfg(out, self.p2out)
        r = self._run("export", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        man = self._assert_manifest_closed(r, out, 3, min_art=2)
        self.assertTrue(any(a.get("role") == "phase3_output" for a in man["artifacts"]))
        self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")))

    # ── 负例矩阵 ──
    def test_04_negative_matrix(self):
        D = self.tmp
        # a) normalize 空 input_lights → 2（预检结构门, 不可 -force 越）, 不写 complete
        d = os.path.join(D, "neg_empty")
        os.makedirs(d, exist_ok=True)
        r = self._run("normalize", self._p1_cfg(d, []))
        self.assertEqual(r.returncode, 2, r.stderr[-300:])
        self.assertIn("input_lights", r.stderr)
        self.assertEqual(self._complete_manifests(d), [], "空输入不得写 complete")
        # b) normalize 缺输入文件 → 3
        d = os.path.join(D, "neg_missing")
        os.makedirs(d, exist_ok=True)
        r = self._run("normalize", self._p1_cfg(d, [os.path.join(D, "nope.fits")]))
        self.assertEqual(r.returncode, 3, r.stderr[-300:])
        # c) mosaic 缺 HiPS 输入 → 3 (B1-A7 映射; SMOKE-001 D11)
        d = os.path.join(D, "neg_nohips")
        os.makedirs(d, exist_ok=True)
        r = self._run("mosaic", self._p2_cfg(d, ["/nonexistent/does_not_exist.hips"]))
        self.assertEqual(r.returncode, 3, r.stderr[-400:])
        # 红必须是「缺输入」，不得是配置键被 CLI 白名单摘除（§9.73 A44）
        self.assertNotIn("unknown key", r.stderr)
        # d) mosaic 默认逐帧逆方差对无 ivar 的产品 → 2, 不写 complete
        #    定案 2（逐像素方差接入）后 normalize 产品**已含** variance/ivar ⇒
        #    本负例改为消费「剥掉 variance/ivar 的副本」：判据与意图（缺 ivar ⇒
        #    fail-closed，不写 complete）逐字不变，只是不再依赖「生产不产方差」这一
        #    已被修复的缺陷。
        d = os.path.join(D, "neg_ivar")
        os.makedirs(d, exist_ok=True)
        novar = []
        for src in (os.path.join(self.p1a, "light_1"), os.path.join(self.p1b, "light_2")):
            dst = os.path.join(D, "neg_ivar_frames", os.path.basename(src) + "_noivar")
            if not os.path.isdir(dst):
                shutil.copytree(src, dst)
                for prod in ("variance", "ivar"):
                    shutil.rmtree(os.path.join(dst, prod), ignore_errors=True)
            novar.append(dst)
        r = self._run("mosaic", self._p2_cfg(d, novar))
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        self.assertIn("ivar", r.stderr)
        self.assertEqual(self._complete_manifests(d), [], "缺 ivar 不得写 complete")
        # e) 无 ivar fixture + 默认逐帧逆方差 → 2
        d = os.path.join(D, "neg_fxnoivar")
        os.makedirs(d, exist_ok=True)
        r = self._run("mosaic", self._p2_cfg(d, [os.path.join(self.noivar, "F1.hips"),
                                                 os.path.join(self.noivar, "F2.hips")]))
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        # f) 含 ivar fixture + 默认逐帧逆方差 → 0 且 uncertainty_available=true
        d = os.path.join(D, "pos_ivar")
        os.makedirs(d, exist_ok=True)
        r = self._run("mosaic", self._p2_cfg(d, [os.path.join(self.hips, "F1.hips"),
                                                 os.path.join(self.hips, "F2.hips")]))
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        man = self._assert_manifest_closed(r, d, 2, min_art=5)
        self.assertIs(man.get("uncertainty_available"), True)

    # ── mosaic/export 各自独立进程两次运行 ──
    def test_05_mosaic_export_isolated_commands(self):
        o2 = os.path.join(self.tmp, "iso2")
        os.makedirs(o2)
        o3 = os.path.join(self.tmp, "iso3")
        os.makedirs(o3)
        cfg2 = self._p2_cfg(o2, [os.path.join(self.hips, "F1.hips"),
                                 os.path.join(self.hips, "F2.hips")])
        cfg3 = self._p3_cfg(o3, os.path.join(self.hips, "FIELD.hips"))
        r2 = self._run("mosaic", cfg2)
        self.assertEqual(r2.returncode, 0, r2.stderr[-400:])
        r3 = self._run("export", cfg3)
        self.assertEqual(r3.returncode, 0, r3.stderr[-400:])
        ev2 = jsonl_lines(r2.stdout)
        ev3 = jsonl_lines(r3.stdout)
        self.assertEqual(ev2[-1]["status"], "ok")
        self.assertEqual(ev3[-1]["status"], "ok")
        rid2 = {e["run_id"] for e in ev2}
        rid3 = {e["run_id"] for e in ev3}
        self.assertEqual(len(rid2), 1)
        self.assertEqual(len(rid3), 1)
        self.assertNotEqual(rid2, rid3, "两次运行必须不同 run_id(进程隔离)")
        with open(manifest_event(ev2)["path"], encoding="utf-8") as fh:
            man2 = json.load(fh)
        with open(manifest_event(ev3)["path"], encoding="utf-8") as fh:
            man3 = json.load(fh)
        self.assertEqual(man2["status"], "complete")
        self.assertEqual(man3["status"], "complete")
        self.assertEqual(man2["phases"], [2])
        self.assertEqual(man3["phases"], [3])

    # ── 中断 → exit 9 + incomplete manifest ──
    def test_06_cancel_interrupt(self):
        """运行期取消 → rc=9 + final status=cancelled + 不留 complete manifest。

        CLI-002: 新命令树有两个同语义测试钩子（ASTROCS_TEST_SLEEP_MS）——
        CLI 入口等待（lib/infrastructure/cli/subcommand.h:156, 只回 rc=9 不产事件）
        与会话内等待（lib/infrastructure/cli/commands.cpp cmd_session1_run, 产
        incomplete manifest + final cancelled）。入口窗先耗尽，故 SIGINT 必须落在
        会话窗（入口窗之后）才测到**运行期**取消面（判据与原 phase3 用例一致）。
        """
        out = os.path.join(self.tmp, "outcancel")
        os.makedirs(out)
        cfg = self._p1_cfg(out, [os.path.join(self.p1data, "light_1.fits")])
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="6000")
        p = subprocess.Popen([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env,
                             cwd=run_cwd())
        time.sleep(8.0)          # 越过入口等待窗(0~6s), 落在会话等待窗(6~12s)
        p.send_signal(signal.SIGINT)
        p.wait(timeout=60)
        self.assertEqual(p.returncode, 9, "SIGINT → exit 9")
        ev = [json.loads(l) for l in p.stdout.read().decode().splitlines() if l.strip()]
        self.assertTrue(ev, "运行期取消必须发事件流（final=cancelled）")
        self.assertEqual(ev[-1]["kind"], "final")
        self.assertEqual(ev[-1]["status"], "cancelled")
        self.assertEqual(self._complete_manifests(out), [],
                         "取消不得留下看似完整的 run manifest")

    # ── resume/hash mismatch → exit 8 + incomplete ──
    def test_07_resume_hash_mismatch(self):
        out = os.path.join(self.tmp, "outhm")
        os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        r1 = self._run("export", cfg)
        self.assertEqual(r1.returncode, 0, r1.stderr[-400:])
        with open(manifest_event(jsonl_lines(r1.stdout))["path"], encoding="utf-8") as fh:
            man = json.load(fh)
        art = [a for a in man["artifacts"] if a.get("role") == "phase3_output"][0]
        with open(art["path"], "ab") as fh:
            fh.write(b"TAMPER")
        r2 = self._run("export", cfg)
        self.assertEqual(r2.returncode, 8, r2.stdout[-300:] + r2.stderr[-300:])
        ev = [e for e in jsonl_lines(r2.stdout) if e["kind"] == "final"]
        self.assertEqual(ev[-1]["status"], "resume_hash_mismatch")

    # ── RT-009: 运行图产物 ──
    def test_08_run_graphs(self):
        out = os.path.join(self.tmp, "outg")
        os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        r = self._run("export", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        gdir = os.path.join(out, "graph")
        for name in ("static_graph.json", "observed_trace.json", "graph_sidecar.json",
                     "static_graph.dot", "observed_graph.dot", "static_graph.svg",
                     "observed_graph.svg", "l0_graph.json", "l0_graph.dot"):
            self.assertTrue(os.path.isfile(os.path.join(gdir, name)), name)
        with open(os.path.join(gdir, "observed_trace.json"), encoding="utf-8") as fh:
            tr = json.load(fh)
        self.assertEqual(tr["schema"], "astrocs.observed-trace/v1")
        nodes = {n["node_id"]: n for n in tr["nodes"]}
        self.assertIn("properties", nodes)
        self.assertEqual(nodes["properties"]["status"], "COMPLETED")
        with open(os.path.join(gdir, "observed_trace.json"), encoding="utf-8") as fh:
            raw = fh.read()
        self.assertNotIn(REPO, raw)
        self.assertNotIn("/home/", raw)
        with open(os.path.join(gdir, "graph_sidecar.json"), encoding="utf-8") as fh:
            side = json.load(fh)
        self.assertEqual(side["schema"], "astrocs.graph-sidecar/v1")
        mods = os.path.join(self.tmp, "mods.json")
        with open(mods, "w", encoding="utf-8") as fh:
            json.dump({"astrocs.phase3.resample": {"module_id": "astrocs.phase3.resample",
                                                   "module_version": "1.x"}}, fh)
        c = subprocess.run([sys.executable, os.path.join(REPO, "eng", "tools", "quality",
                            "check_pipeline_graph.py"),
                            "--ir", os.path.join(gdir, "static_graph.json"),
                            "--module-index", mods,
                            "--trace", os.path.join(gdir, "observed_trace.json")],
                           capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertEqual(c.returncode, 0, c.stderr[-400:])
        self.assertIn("PIPELINE_GRAPH_PASS", c.stdout)
        # 退役面负例: graph 用户命令已删（CLI_PROTOCOL_V1 §1）→ rc=2 unknown command
        g = subprocess.run([EXE, "graph", "--preset", "1,2,3", "--config", cfg,
                            "--output", os.path.join(self.tmp, "gstatic")],
                           capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertEqual(g.returncode, 2, g.stderr[-200:])
        self.assertIn("unknown command", g.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
