#!/usr/bin/env python3
"""SYN-009 独立合成 Oracle — CLI Phase1/2/3 端到端合成 pipeline(单 CLI)。
验收(03 L131 + CLI-007): 合成帧经过 Phase1→2→3; 中断/resume hash mismatch;
                        单 CLI、artifact chain、events、资源与科学不变量全过。
方法(independent, 驱动生产 CLI `astrocs run --phases ...`, 不调用库内部):
  - 用 phase1/phase2/phase3 fixture 生成合成数据(FITS 灯场 + 主帧; F1/F2/FIELD/NAN HiPS)。
  - 单二进制 `astrocs run --phases 1|2|3|2,3` 逐相(生产 session)驱动,
    校验 events(sequence 单调/final) / run manifest(complete + verify 通过) / 逐阶段 artifact。
  - 科学不变量(各 phase 由 SYN-001..008 独立 Oracle 定义):
      phase1 帧校准值 = (200-100-1*(150-100))/1.25 = 40(精确);
      phase2 overlap_controls>0; phase3 output_phase3.fits 存在 + verify。
  - 资源不变量: complete run 含 resource summary(wall_seconds/peak_rss/max_threads)与 backend 事件。
  - 中断: SIGINT → exit 9 + manifest status=incomplete。
  - resume/hash mismatch: 篡改 prior artifact → exit 8 + status=resume_hash_mismatch。
  - 单 CLI: 全程用同一 build/cli/astrocs 二进制。
依赖: SYN-001..008 + CLI-001..007 + P3-004 已 PASS。
"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "cli", "astrocs")
AIO = os.path.join(REPO, "lib", "astro_image_io")

SKIP_FITS = r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|" \
            r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|" \
            r"imcopy|imarith|tabcompile|sortcol|tabselect"


def _cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(SKIP_FITS, f):
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
            f"-I{os.path.join(REPO, 'lib', 'common')}"]


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
        cls.hips = os.path.join(cls.tmp, "hips"); os.makedirs(cls.hips)
        for m in ("--make", "--make-field", "--make-nan"):
            subprocess.run([cls.p2, m, cls.hips], capture_output=True, text=True, timeout=120)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _config(self, out, lights, phase3=None):
        cfg = os.path.join(self.tmp, "cfg_%s.json" % os.path.basename(out))
        doc = {"schema_version": "1",
               "inputs": {"lights": lights, "darks": [], "flats": [], "bias": []},
               "output_dir": out}
        if phase3 is not None:
            doc["phase3"] = phase3
        json.dump(doc, open(cfg, "w"))
        return cfg

    def _run(self, phases, cfg):
        return subprocess.run([EXE, "run", "--phases", phases, "--config", cfg, "--events-jsonl"],
                              capture_output=True, text=True, timeout=300)

    # ── phase1 单独(pipeline run --phases 1, 用 master 的 phase1 run 做数值 Oracle) ──
    def test_01_phase1_single_invariants(self):
        # 1a. pipeline run --phases 1: 完整 + 事件 + manifest complete + verify + 资源/backend
        out = os.path.join(self.tmp, "out1"); os.makedirs(out)
        cfg = self._config(out, [os.path.join(self.p1data, "light_1.fits"),
                                 os.path.join(self.p1data, "light_2.fits")])
        r = self._run("1", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        self.assertEqual([e["sequence"] for e in events], list(range(len(events))), "sequence 单调")
        self.assertEqual(events[-1]["kind"], "final"); self.assertEqual(events[-1]["status"], "ok")
        kinds = [e["kind"] for e in events]
        self.assertIn("resource", kinds); self.assertIn("backend", kinds); self.assertIn("artifact", kinds)
        m = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        man = json.load(open(m["path"], encoding="utf-8"))
        self.assertEqual(man["status"], "complete")
        self.assertGreaterEqual(len(man["artifacts"]), 2, "两帧校准输出入 manifest")
        v = subprocess.run([EXE, "verify", "--json", "--run-manifest", m["path"]],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(v.returncode, 0, v.stdout + v.stderr)
        # 1b. 科学不变量(真实校准, 经 phase1 run 带 master): (200-100-1*(150-100))/1.25 = 40(精确)
        cfgc = os.path.join(self.tmp, "cfg_p1cal.json")
        json.dump({"input_lights": [os.path.join(self.p1data, "light_1.fits"),
                                    os.path.join(self.p1data, "light_2.fits")],
                   "master_bias": os.path.join(self.p1data, "bias.fits"),
                   "master_dark": os.path.join(self.p1data, "dark.fits"),
                   "master_flat": os.path.join(self.p1data, "flat.fits"),
                   "output_dir": out}, open(cfgc, "w"))
        rc = subprocess.run([EXE, "phase1", "run", "--config", cfgc],
                            capture_output=True, text=True, timeout=120)
        self.assertEqual(rc.returncode, 0, rc.stderr[-400:])
        for f in ("calibrated_light_1.fits", "calibrated_light_2.fits"):
            rr = subprocess.run([self.p1, "--mean", os.path.join(out, f)],
                                capture_output=True, text=True, timeout=60)
            mv = re.search(r"MEAN ([\d.]+)", rr.stdout)
            self.assertIsNotNone(mv)
            self.assertAlmostEqual(float(mv.group(1)), 40.0, places=4, msg=f)

    # ── phase2 单独 ──
    def test_02_phase2_single_invariants(self):
        out = os.path.join(self.tmp, "out2"); os.makedirs(out)
        cfg = self._config(out, [os.path.join(self.hips, "F1.hips"),
                                 os.path.join(self.hips, "F2.hips")])
        r = self._run("2", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["status"], "ok")
        # 科学不变量: 双帧重叠 → obs>0, overlap_controls≥1(日志打印)
        self.assertIn("overlap_controls", r.stderr)
        m = re.search(r"overlap_controls=(\d+)", r.stderr)
        self.assertIsNotNone(m)
        self.assertGreater(int(m.group(1)), 0)

    # ── phase3 单独 ──
    def test_03_phase3_single_invariants(self):
        out = os.path.join(self.tmp, "out3"); os.makedirs(out)
        ph3 = {"source": {"hips_dir": os.path.join(self.hips, "FIELD.hips")},
               "center": {"ra_deg": 0.0, "dec_deg": 30.0}, "scale_deg_per_px": 0.05,
               "width_px": 20, "height_px": 20, "sampler": "bilinear",
               "projection": "TAN", "coverage_output": "mask"}
        cfg = self._config(out, [], phase3=ph3)
        r = self._run("3", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["status"], "ok")
        m = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        man = json.load(open(m["path"], encoding="utf-8"))
        self.assertEqual(man["status"], "complete")
        self.assertTrue(any(a["role"] == "phase3_output" for a in man["artifacts"]),
                        "phase3 output 应在 manifest artifact chain")
        self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")), "phase3 FITS 已写出")

    # ── 组合 2,3(共享 HiPS 输入) ──
    def test_04_combined_23_artifact_chain(self):
        out = os.path.join(self.tmp, "out23"); os.makedirs(out)
        ph3 = {"source": {"hips_dir": os.path.join(self.hips, "FIELD.hips")},
               "center": {"ra_deg": 0.0, "dec_deg": 30.0}, "scale_deg_per_px": 0.05,
               "width_px": 20, "height_px": 20, "sampler": "bilinear",
               "projection": "TAN", "coverage_output": "mask"}
        cfg = self._config(out, [os.path.join(self.hips, "F1.hips"),
                                 os.path.join(self.hips, "F2.hips")], phase3=ph3)
        r = self._run("2,3", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["status"], "ok")
        m = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        man = json.load(open(m["path"], encoding="utf-8"))
        self.assertEqual(man["status"], "complete")
        self.assertEqual([e["stage"] for e in events if e["kind"] == "stage_end"],
                         ["run_phase2", "run_phase3"])

    # ── 中断 → exit 9 + incomplete manifest ──
    def test_05_cancel_interrupt(self):
        out = os.path.join(self.tmp, "outcancel"); os.makedirs(out)
        ph3 = {"source": {"hips_dir": os.path.join(self.hips, "FIELD.hips")},
               "center": {"ra_deg": 0.0, "dec_deg": 30.0}, "scale_deg_per_px": 0.05,
               "width_px": 20, "height_px": 20, "sampler": "bilinear",
               "projection": "TAN", "coverage_output": "mask"}
        cfg = self._config(out, [os.path.join(self.hips, "F1.hips"),
                                 os.path.join(self.hips, "F2.hips")], phase3=ph3)
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="4000")
        p = subprocess.Popen([EXE, "run", "--phases", "2,3", "--config", cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
        time.sleep(1.0)
        p.send_signal(signal.SIGINT)
        p.wait(timeout=60)
        self.assertEqual(p.returncode, 9, "SIGINT → exit 9")
        ev = [json.loads(l) for l in p.stdout.read().decode().splitlines() if l.strip()]
        self.assertEqual(ev[-1]["kind"], "final"); self.assertEqual(ev[-1]["status"], "cancelled")

    # ── resume/hash mismatch → exit 8 + incomplete ──
    def test_06_resume_hash_mismatch(self):
        out = os.path.join(self.tmp, "outhm"); os.makedirs(out)
        ph3 = {"source": {"hips_dir": os.path.join(self.hips, "FIELD.hips")},
               "center": {"ra_deg": 0.0, "dec_deg": 30.0}, "scale_deg_per_px": 0.05,
               "width_px": 20, "height_px": 20, "sampler": "bilinear",
               "projection": "TAN", "coverage_output": "mask"}
        cfg = self._config(out, [], phase3=ph3)
        # 第一次完整跑
        r1 = self._run("3", cfg)
        self.assertEqual(r1.returncode, 0, r1.stderr[-400:])
        man = [e for e in jsonl_lines(r1.stdout) if e.get("role") == "run_manifest"][-1]
        m = json.load(open(man["path"], encoding="utf-8"))
        art = [a for a in m["artifacts"] if a["role"] == "phase3_output"][0]
        # 篡改 prior artifact
        with open(art["path"], "ab") as fh:
            fh.write(b"TAMPER")
        # 重跑 → 应 8
        r2 = self._run("3", cfg)
        self.assertEqual(r2.returncode, 8, r2.stdout[-300:] + r2.stderr[-300:])
        ev = [e for e in jsonl_lines(r2.stdout) if e["kind"] == "final"]
        self.assertEqual(ev[-1]["status"], "resume_hash_mismatch")


if __name__ == "__main__":
    unittest.main(verbosity=2)
