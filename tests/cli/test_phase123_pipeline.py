#!/usr/bin/env python3
"""FIX-E2E B1-A7: 正式 CLI Phase1 → 持久化 → Phase2 → 持久化 → Phase3 真链路。

数据流(与生产 M42 流程一致: 逐帧 Phase1 产品 → Phase2 拼接):
  phase1 run(light_1) ─┐
  phase1 run(light_2) ─┴→ phase2 run(hips_paths=[p1a,p1b]) ─→ phase3 run(读 p2out)
每阶段独立进程、只读上游**持久化 HiPS 产品**（无任何 fixture 顶替）; 断言
rc=0 / status=complete / artifacts 非空且 sha256+size 可核验 / verify rc=0。

负例矩阵(全非零且不写 complete):
  空 input_lights→2; 缺输入文件→3; 缺 HiPS 输入→3; 默认 weight_mode=2 对无 ivar
  产品→2; 无 ivar fixture mode=2→2。
正例补充: 含 ivar 的合成 fixture + 默认 weight_mode=2 → rc=0 且
manifest.uncertainty_available=true（真实不确定度面可达）。

B1-A5: E2E 机器闭环用**显式** weight_mode=1（等权; 法定科学模式之一）,
manifest 记 uncertainty_available=false; **不得**当作 §B 科学闭环通过。
"""
import json, os, re, shutil, signal, subprocess, sys, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "cli", "astrocs"))
AIO = os.path.join(REPO, "lib", "astro_image_io")

SKIP_FITS = r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|" \
            r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|" \
            r"imcopy|imarith|tabcompile|sortcol|tabselect"

# 合成 E2E WCS: 64² 帧覆盖 ~32° 天区(0.5°/px), 使 nside=512 的 HiPS 覆盖
# 足够 control 单元(8×8/tile)供 Phase2 sampler 取得 >=2 clean frame 观测。
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
    return [f"-I{os.path.join(REPO, 'include')}",
            f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
            f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
            f"-I{os.path.join(REPO, 'lib', 'common')}",
            f"-I{os.path.join(REPO, 'lib', 'common', 'healpix')}"]


def _aio_srcs():
    return [os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
            os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
            os.path.join(AIO, "src", "aio_fits.cpp"),
            os.path.join(AIO, "src", "aio_api.cpp"),
            os.path.join(AIO, "src", "aio_log.cpp"),
            os.path.join(AIO, "src", "aio_compressor.cpp"),
            os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


def manifest_event(events):
    return [e for e in events if e["kind"] == "artifact" and
            e.get("role") == "run_manifest"][-1]


@unittest.skipUnless(os.path.isfile(EXE), "需要已构建 CLI build/cli/astrocs")
class TestPhase123Pipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn009_")
        objs = _cfitsio_objs(cls.tmp)
        incs = _common_incs()
        cls.p1 = os.path.join(cls.tmp, "p1fx")
        subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                        os.path.join(REPO, "tests", "backend", "phase1_fixture_main.cpp"),
                        os.path.join(AIO, "src", "aio_fits.cpp"),
                        os.path.join(AIO, "src", "aio_api.cpp"),
                        os.path.join(AIO, "src", "aio_log.cpp"),
                        os.path.join(AIO, "src", "aio_compressor.cpp"),
                        *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p1],
                       capture_output=True, text=True, timeout=600)
        cls.p2 = os.path.join(cls.tmp, "p2fx")
        subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                        os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                        *_aio_srcs(), *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p2],
                       capture_output=True, text=True, timeout=600)
        cls.p1data = os.path.join(cls.tmp, "p1data"); os.makedirs(cls.p1data)
        r = subprocess.run([cls.p1, "--make", cls.p1data], capture_output=True, text=True, timeout=120)
        assert "FIXTURES_OK" in r.stdout, r.stderr
        # 含 variance/ivar 的 Phase2 fixture (B1-A5 真实不确定度面正例)
        cls.hips = os.path.join(cls.tmp, "hips"); os.makedirs(cls.hips)
        for m in ("--make", "--make-field", "--make-nan"):
            subprocess.run([cls.p2, m, cls.hips], capture_output=True, text=True, timeout=120)
        # 无 ivar 的 Phase2 fixture (默认 weight_mode=2 负例)
        cls.noivar = os.path.join(cls.tmp, "noivar"); os.makedirs(cls.noivar)
        subprocess.run([cls.p2, "--make-noivar", cls.noivar], capture_output=True,
                       text=True, timeout=120)
        # 逐帧 Phase1 持久化产品目录
        cls.p1a = os.path.join(cls.tmp, "p1_light1"); os.makedirs(cls.p1a)
        cls.p1b = os.path.join(cls.tmp, "p1_light2"); os.makedirs(cls.p1b)
        cls.p2out = os.path.join(cls.tmp, "p2out"); os.makedirs(cls.p2out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── config helpers ──
    def _write(self, name, doc):
        p = os.path.join(self.tmp, name)
        json.dump(doc, open(p, "w"))
        return p

    def _p1_cfg(self, out, lights, with_chain=True):
        doc = {"schema_version": "1", "input_lights": list(lights),
               "master_bias": os.path.join(self.p1data, "bias.fits"),
               "master_dark": os.path.join(self.p1data, "dark.fits"),
               "master_flat": os.path.join(self.p1data, "flat.fits"),
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
            "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
            "output_dir": out,
            "phase3": {"source": {"hips_dir": hips_dir},
                       "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                       "scale_deg_per_px": 0.5, "width_px": 20, "height_px": 20,
                       "sampler": "bilinear", "projection": "TAN",
                       "coverage_output": "mask", "max_tiles": 16}})

    def _run(self, phase_cmd, cfg, timeout=300):
        return subprocess.run([EXE, phase_cmd, "run", "--config", cfg, "--events-jsonl"],
                              capture_output=True, text=True, timeout=timeout)

    @staticmethod
    def _artifact_names(man):
        return {os.path.basename(a["path"]) for a in man["artifacts"]}

    @staticmethod
    def _complete_manifests(d):
        out = []
        for f in os.listdir(d):
            if f.startswith("astrocs_run_") and f.endswith(".json"):
                try:
                    m = json.load(open(os.path.join(d, f), encoding="utf-8"))
                    if m.get("status") == "complete":
                        out.append(m)
                except Exception:
                    pass
        return out

    def _assert_manifest_closed(self, r, out, phase, min_art=1):
        self.assertEqual(jsonl_lines(r.stdout)[-1]["status"], "ok")
        mans = self._complete_manifests(out)
        self.assertEqual(len(mans), 1, "应有且仅有一个 complete manifest")
        man = mans[0]
        self.assertEqual(man["phases"], [phase])
        self.assertGreaterEqual(len(man["artifacts"]), min_art)
        for a in man["artifacts"]:
            self.assertTrue(a["sha256"], "artifact 必带 sha256: " + a["path"])
            self.assertTrue(os.path.isfile(a["path"]), "artifact 必须在盘: " + a["path"])
            self.assertGreater(a["size_bytes"], 0, a["path"])
        mf = manifest_event(jsonl_lines(r.stdout))["path"]
        v = subprocess.run([EXE, "verify", "--json", "--run-manifest", mf],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(v.returncode, 0, v.stdout + v.stderr)
        return man

    # ── Phase1: 逐帧持久化 + 校准数值 Oracle ──
    def test_01_phase1_products_persist_hips(self):
        for out, idx in ((self.p1a, 1), (self.p1b, 2)):
            cfg = self._p1_cfg(out, [os.path.join(self.p1data, "light_%d.fits" % idx)])
            r = self._run("phase1", cfg)
            self.assertEqual(r.returncode, 0, r.stderr[-500:])
            events = jsonl_lines(r.stdout)
            self.assertEqual([e["sequence"] for e in events], list(range(len(events))))
            self.assertEqual(events[-1]["kind"], "final")
            self.assertEqual(events[-1]["status"], "ok")
            kinds = [e["kind"] for e in events]
            self.assertIn("resource", kinds); self.assertIn("backend", kinds)
            man = self._assert_manifest_closed(r, out, 1, min_art=8)
            names = self._artifact_names(man)
            self.assertIn("calibrated_light_%d.fits" % idx, names)
            for want in ("p1_sources.json", "p1_psf.json", "p1_wcs.json", "p1_flux.json",
                         "p1_snr.json", "p1_stack.hiss", "p1_final.json"):
                self.assertIn(want, names, "缺产物 " + want)
            self.assertTrue(os.path.isfile(os.path.join(out, "signal", "properties")),
                            "Phase1 必须持久化 IVOA HiPS signal/properties")
            rr = subprocess.run([self.p1, "--mean", os.path.join(out, "calibrated_light_%d.fits" % idx)],
                                capture_output=True, text=True, timeout=60)
            mv = re.search(r"MEAN ([\d.]+)", rr.stdout)
            self.assertIsNotNone(mv)
            # (200-100-1*(150-100))/1.25 = 40 精确
            self.assertAlmostEqual(float(mv.group(1)), 40.0, places=4)

    # ── Phase2: 只读两个 Phase1 持久化产品（独立进程），显式等权闭合 ──
    def test_02_phase2_consumes_phase1_products(self):
        cfg = self._p2_cfg(self.p2out, [self.p1a, self.p1b], {"weight_mode": 1})
        r = self._run("phase2", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        # 真链路必须产生重叠控制点（>=2 clean frame/UPM 几何前提）
        with open(os.path.join(self.p2out, "p2_samples.json"), encoding="utf-8") as fh:
            smp = json.load(fh)
        self.assertGreater(smp.get("stats", {}).get("overlap_controls", 0), 0,
                           "Phase2 必须在两个 Phase1 产品的重叠区取得控制点")
        man = self._assert_manifest_closed(r, self.p2out, 2, min_art=5)
        # B1-A5: 显式 mode=1 → 等权, 无不确定度面（机器闭环, 非科学闭环）
        self.assertIs(man.get("uncertainty_available"), False)
        self.assertTrue(os.path.isfile(os.path.join(self.p2out, "signal", "properties")),
                        "Phase2 必须持久化 mosaic HiPS")

    # ── Phase3: 只读 Phase2 持久化产品（独立进程） ──
    def test_03_phase3_consumes_phase2_product(self):
        out = os.path.join(self.tmp, "p3out"); os.makedirs(out, exist_ok=True)
        cfg = self._p3_cfg(out, self.p2out)
        r = self._run("phase3", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        man = self._assert_manifest_closed(r, out, 3, min_art=2)
        self.assertTrue(any(a.get("role") == "phase3_output" for a in man["artifacts"]))
        self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")))

    # ── 负例矩阵 ──
    def test_04_negative_matrix(self):
        D = self.tmp
        # a) phase1 空 input_lights → 2, 不写 complete
        d = os.path.join(D, "neg_empty"); os.makedirs(d, exist_ok=True)
        r = self._run("phase1", self._p1_cfg(d, []))
        self.assertEqual(r.returncode, 2, r.stderr[-300:])
        self.assertIn("input_lights must be non-empty array", r.stderr)
        self.assertEqual(self._complete_manifests(d), [], "空输入不得写 complete")
        # b) phase1 缺输入文件 → 3
        d = os.path.join(D, "neg_missing"); os.makedirs(d, exist_ok=True)
        r = self._run("phase1", self._p1_cfg(d, [os.path.join(D, "nope.fits")]))
        self.assertEqual(r.returncode, 3, r.stderr[-300:])
        # c) phase2 缺 HiPS 输入 → 3 (B1-A7 映射)
        d = os.path.join(D, "neg_nohips"); os.makedirs(d, exist_ok=True)
        r = self._run("phase2", self._p2_cfg(d, ["/nonexistent/does_not_exist.hips"],
                                             {"weight_mode": 1}))
        self.assertEqual(r.returncode, 3, r.stderr[-400:])
        # d) phase2 默认 weight_mode=2 对无 ivar 的 Phase1 产品 → 2, 不写 complete
        d = os.path.join(D, "neg_ivar"); os.makedirs(d, exist_ok=True)
        r = self._run("phase2", self._p2_cfg(d, [self.p1a, self.p1b]))
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        self.assertIn("ivar", r.stderr)
        self.assertEqual(self._complete_manifests(d), [], "缺 ivar 不得写 complete")
        # e) 无 ivar fixture + 默认 weight_mode=2 → 2
        d = os.path.join(D, "neg_fxnoivar"); os.makedirs(d, exist_ok=True)
        r = self._run("phase2", self._p2_cfg(d, [os.path.join(self.noivar, "F1.hips"),
                                                 os.path.join(self.noivar, "F2.hips")]))
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        # f) 含 ivar fixture + 默认 weight_mode=2 → 0 且 uncertainty_available=true
        d = os.path.join(D, "pos_ivar"); os.makedirs(d, exist_ok=True)
        r = self._run("phase2", self._p2_cfg(d, [os.path.join(self.hips, "F1.hips"),
                                                 os.path.join(self.hips, "F2.hips")]))
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        man = self._assert_manifest_closed(r, d, 2, min_art=5)
        self.assertIs(man.get("uncertainty_available"), True)

    # ── Phase2/Phase3 各自独立进程两次运行 ──
    def test_05_phase2_phase3_isolated_commands(self):
        o2 = os.path.join(self.tmp, "iso2"); os.makedirs(o2)
        o3 = os.path.join(self.tmp, "iso3"); os.makedirs(o3)
        cfg2 = self._p2_cfg(o2, [os.path.join(self.hips, "F1.hips"),
                                 os.path.join(self.hips, "F2.hips")])
        cfg3 = self._p3_cfg(o3, os.path.join(self.hips, "FIELD.hips"))
        r2 = self._run("phase2", cfg2)
        self.assertEqual(r2.returncode, 0, r2.stderr[-400:])
        r3 = self._run("phase3", cfg3)
        self.assertEqual(r3.returncode, 0, r3.stderr[-400:])
        ev2 = jsonl_lines(r2.stdout); ev3 = jsonl_lines(r3.stdout)
        self.assertEqual(ev2[-1]["status"], "ok"); self.assertEqual(ev3[-1]["status"], "ok")
        rid2 = {e["run_id"] for e in ev2}; rid3 = {e["run_id"] for e in ev3}
        self.assertEqual(len(rid2), 1); self.assertEqual(len(rid3), 1)
        self.assertNotEqual(rid2, rid3, "CLI-002: 两次运行必须不同 run_id(进程隔离)")
        man2 = json.load(open(manifest_event(ev2)["path"], encoding="utf-8"))
        man3 = json.load(open(manifest_event(ev3)["path"], encoding="utf-8"))
        self.assertEqual(man2["status"], "complete")
        self.assertEqual(man3["status"], "complete")
        self.assertEqual(man2["phases"], [2]); self.assertEqual(man3["phases"], [3])

    # ── 中断 → exit 9 + incomplete manifest ──
    def test_06_cancel_interrupt(self):
        out = os.path.join(self.tmp, "outcancel"); os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="4000")
        p = subprocess.Popen([EXE, "phase3", "run", "--config", cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
        time.sleep(1.0)
        p.send_signal(signal.SIGINT)
        p.wait(timeout=60)
        self.assertEqual(p.returncode, 9, "SIGINT → exit 9")
        ev = [json.loads(l) for l in p.stdout.read().decode().splitlines() if l.strip()]
        self.assertEqual(ev[-1]["kind"], "final"); self.assertEqual(ev[-1]["status"], "cancelled")

    # ── resume/hash mismatch → exit 8 + incomplete ──
    def test_07_resume_hash_mismatch(self):
        out = os.path.join(self.tmp, "outhm"); os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        r1 = self._run("phase3", cfg)
        self.assertEqual(r1.returncode, 0, r1.stderr[-400:])
        man = json.load(open(manifest_event(jsonl_lines(r1.stdout))["path"], encoding="utf-8"))
        art = [a for a in man["artifacts"] if a.get("role") == "phase3_output"][0]
        with open(art["path"], "ab") as fh:
            fh.write(b"TAMPER")
        r2 = self._run("phase3", cfg)
        self.assertEqual(r2.returncode, 8, r2.stdout[-300:] + r2.stderr[-300:])
        ev = [e for e in jsonl_lines(r2.stdout) if e["kind"] == "final"]
        self.assertEqual(ev[-1]["status"], "resume_hash_mismatch")

    # ── RT-009: 运行图产物 ──
    def test_08_run_graphs(self):
        out = os.path.join(self.tmp, "outg"); os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        r = self._run("phase3", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        gdir = os.path.join(out, "graph")
        for name in ("static_graph.json", "observed_trace.json", "graph_sidecar.json",
                     "static_graph.dot", "observed_graph.dot", "static_graph.svg",
                     "observed_graph.svg", "l0_graph.json", "l0_graph.dot"):
            self.assertTrue(os.path.isfile(os.path.join(gdir, name)), name)
        tr = json.load(open(os.path.join(gdir, "observed_trace.json"), encoding="utf-8"))
        self.assertEqual(tr["schema"], "astrocs.observed-trace/v1")
        nodes = {n["node_id"]: n for n in tr["nodes"]}
        self.assertIn("properties", nodes)
        self.assertEqual(nodes["properties"]["status"], "COMPLETED")
        raw = open(os.path.join(gdir, "observed_trace.json"), encoding="utf-8").read()
        self.assertNotIn(REPO, raw); self.assertNotIn("/home/", raw)
        side = json.load(open(os.path.join(gdir, "graph_sidecar.json"), encoding="utf-8"))
        self.assertEqual(side["schema"], "astrocs.graph-sidecar/v1")
        mods = os.path.join(self.tmp, "mods.json")
        json.dump({"astrocs.phase3.resample": {"module_id": "astrocs.phase3.resample",
                                               "module_version": "1.x"}}, open(mods, "w"))
        c = subprocess.run([sys.executable, os.path.join(REPO, "tools", "quality",
                            "check_pipeline_graph.py"),
                            "--ir", os.path.join(gdir, "static_graph.json"),
                            "--module-index", mods,
                            "--trace", os.path.join(gdir, "observed_trace.json")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(c.returncode, 0, c.stderr[-400:])
        self.assertIn("PIPELINE_GRAPH_PASS", c.stdout)
        g = subprocess.run([EXE, "graph", "--preset", "1,2,3", "--config", cfg,
                            "--output", os.path.join(self.tmp, "gstatic")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(g.returncode, 2, g.stderr[-200:])
        self.assertIn("unknown command", g.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
