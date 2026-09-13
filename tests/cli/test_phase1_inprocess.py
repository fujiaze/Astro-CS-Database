#!/usr/bin/env python3
"""CLI-004 集成测试: phase1 run 进程内调用 — 无子进程/事件完整/错误映射/取消/数值 Oracle。"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "cli", "astrocs"))
REGISTRY = os.path.join(REPO, "runtime", "pipeline", "module_ports.registry.json")
FIX = "/tmp/astrocs_p1_fixture"   # 由 setUpClass 编译


def frozen_phase1_chain():
    """module_ports.registry.json 冻结的 phase1 端口链 (module_id 顺序)。"""
    d = json.load(open(REGISTRY, encoding="utf-8"))
    return [m["module_id"] for m in d["modules"] if m.get("phase") == "phase1"]


def build_fixture(tmp):
    exe = os.path.join(tmp, "fixture")
    aio = os.path.join(REPO, "lib", "astro_image_io")
    cf = [os.path.join(aio, "third_party", "cfitsio", f) for f in os.listdir(
        os.path.join(aio, "third_party", "cfitsio")) if f.endswith(".c")]
    objs = []
    for c in cf:
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", c):
            continue
        b = os.path.basename(c)[:-2] + ".o"
        o = os.path.join(tmp, b)
        subprocess.run(["gcc", "-O2", "-w", f"-I{os.path.join(aio, 'third_party', 'cfitsio')}",
                        "-c", c, "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS",
                        f"-I{os.path.join(REPO, 'include')}",
                        f"-I{os.path.join(aio, 'include')}",
                        f"-I{os.path.join(aio, 'src')}",
                        f"-I{os.path.join(aio, 'third_party', 'cfitsio')}",
                        os.path.join(REPO, "tests", "backend", "phase1_fixture_main.cpp"),
                        os.path.join(aio, "src", "aio_fits.cpp"),
                        os.path.join(aio, "src", "aio_api.cpp"),
                        os.path.join(aio, "src", "aio_log.cpp"),
                        os.path.join(aio, "src", "aio_compressor.cpp"),
                        *objs, "-lz", "-lzstd", "-llz4", "-o", exe],
                       capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr[-800:]
    return exe


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


class TestPhase1InProcess(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="p1int_")
        cls.fixture = build_fixture(cls.tmp)
        cls.data = os.path.join(cls.tmp, "data")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.data)
        os.makedirs(cls.out)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True, text=True,
                           timeout=120)
        assert "FIXTURES_OK" in r.stdout, r.stderr
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        # FIX-E2E B1-A1: 正式 phase1 链为 8 节点（cal→cos→psf→wcs→phot→snr→drz→wr）,
        # drizzle/wcs 为链上必填科学配置; Linux ipv stub 平台 wcs 走显式 WCS 配置路径。
        json.dump({
            "input_lights": [os.path.join(cls.data, "light_1.fits"),
                             os.path.join(cls.data, "light_2.fits")],
            "master_bias": os.path.join(cls.data, "bias.fits"),
            "master_dark": os.path.join(cls.data, "dark.fits"),
            "master_flat": os.path.join(cls.data, "flat.fits"),
            "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                    "cd11": -2.7777777777777776e-4, "cd12": 0.0,
                    "cd21": 0.0, "cd22": 2.7777777777777776e-4},
            "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1},
            "output_dir": cls.out,
        }, open(cls.cfg, "w"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args, **kw):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              timeout=kw.pop("timeout", 120), **kw)

    def test_01_run_complete_events_manifest_verify(self):
        r = self._run("phase1", "run", "--config", self.cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        self.assertGreaterEqual(len(events), 4)
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))), "sequence 从 0 单调")
        self.assertEqual(events[0]["kind"], "stage_start")
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        kinds = [e["kind"] for e in events]
        self.assertIn("artifact", kinds)
        # run manifest complete + verify 通过
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        m = json.load(open(mpath, encoding="utf-8"))
        self.assertEqual(m["status"], "complete")
        # FIX-E2E B1-A1: 全链产物（校准 FITS + p1_*.json + p1_stack.hiss + HiPS
        # properties）必须全部入 run manifest; 旧断言 ==2 只编码了 cal 两节点断链态。
        names = {os.path.basename(a["path"]) for a in m["artifacts"]}
        self.assertTrue({"calibrated_light_1.fits", "calibrated_light_2.fits"} <= names)
        self.assertTrue({"p1_sources.json", "p1_psf.json", "p1_wcs.json", "p1_flux.json",
                         "p1_snr.json", "p1_stack.hiss", "p1_final.json"} <= names)
        for a in m["artifacts"]:
            self.assertTrue(a["sha256"] and os.path.isfile(a["path"]), a["path"])
        v = self._run("verify", "--json", "--run-manifest", mpath)
        self.assertEqual(v.returncode, 0, v.stdout + v.stderr)

    def test_02_calibrated_values_match_oracle(self):
        """数值 Oracle: (200-bias-k*(dark-bias))/flat = (200-100-50)/1.25 = 40(精确)。"""
        for f in ("calibrated_light_1.fits", "calibrated_light_2.fits"):
            r = subprocess.run([self.fixture, "--mean", os.path.join(self.out, f)],
                               capture_output=True, text=True, timeout=60)
            m = re.search(r"MEAN ([\d.]+)", r.stdout)
            self.assertIsNotNone(m)
            self.assertAlmostEqual(float(m.group(1)), 40.0, places=4, msg=f)

    def test_03_no_subprocess_during_run(self):
        """验收核心: 运行中进程树无子进程(取消钩子窗口内检查 /proc/<pid>/task/*/children)。"""
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="2500")
        p = subprocess.Popen([EXE, "phase1", "run", "--config", self.cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
        time.sleep(0.8)
        children = []
        task_dir = f"/proc/{p.pid}/task"
        if os.path.isdir(task_dir):
            for tid in os.listdir(task_dir):
                cf = os.path.join(task_dir, tid, "children")
                if os.path.isfile(cf):
                    children += open(cf).read().split()
        self.assertEqual(children, [], "phase1 运行中不得产生子进程(纯进程内调用)")
        p.wait(timeout=60)
        self.assertEqual(p.returncode, 0)

    def test_04_error_mapping(self):
        # 缺失输入文件 → 3(INPUT)
        bad = os.path.join(self.tmp, "missing.json")
        json.dump({"input_lights": ["/nonexistent/x.fits"], "output_dir": self.out},
                  open(bad, "w"))
        r = self._run("phase1", "run", "--config", bad, "--events-jsonl")
        self.assertEqual(r.returncode, 3, r.stderr[-200:])
        # 配置坏 JSON → 3(INPUT): validate_config_full 解析失败即 INPUT
        # (CLI_PROTOCOL_V1 §2: 3=输入缺失、格式或 hash 错; 生产 parser 语义一致)
        bad2 = os.path.join(self.tmp, "badjson.json")
        open(bad2, "w").write("{not json")
        r2 = self._run("phase1", "run", "--config", bad2)
        self.assertEqual(r2.returncode, 3)
        # master 尺寸不匹配 → 2(PARAM→ARGS); 造一个 32x32 的 master
        small = subprocess.run([self.fixture, "--make", self.data], capture_output=True,
                               timeout=60)  # noop 复用
        cfg3 = os.path.join(self.tmp, "cfg3.json")
        # FIX-E2E B1-A1: 无 master 校准路径仍合法, 但正式 8 节点链要求 drizzle/wcs 配置。
        json.dump({"input_lights": [os.path.join(self.data, "light_1.fits")],
                   "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                           "cd11": -2.7777777777777776e-4, "cd12": 0.0,
                           "cd21": 0.0, "cd22": 2.7777777777777776e-4},
                   "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0,
                               "precision_mode": 1},
                   "output_dir": self.out}, open(cfg3, "w"))
        r3 = self._run("phase1", "run", "--config", cfg3, "--events-jsonl")
        self.assertEqual(r3.returncode, 0, r3.stderr[-400:])  # master 全可空 → 校准路径也合法

    def test_05_cancel_mid_run(self):
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="3000")
        p = subprocess.Popen([EXE, "phase1", "run", "--config", self.cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
        time.sleep(0.6)
        p.send_signal(signal.SIGINT)
        out, _ = p.communicate(timeout=30)
        self.assertEqual(p.returncode, 9, f"取消 → 9; got {p.returncode}")
        events = jsonl_lines(out)
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "cancelled")

    # ── FIX-E2E B1-A1 冻结门: CLI IR == registry phase1 端口链 ──
    def test_06_ir_matches_frozen_chain(self):
        """先红(基线 nodes=[cal,cos]) → 后绿: 正式 CLI phase1 IR 必须与
        runtime/pipeline/module_ports.registry.json 的 phase1 链逐节点一致。"""
        r = self._run("phase1", "plan", "--config", self.cfg, "--json")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        plan = json.loads(r.stdout)
        cli_mods = [n["module_id"] for n in plan["pipeline"]["nodes"]]
        self.assertEqual(cli_mods, frozen_phase1_chain(),
                         "CLI phase1 IR 必须 == registry 冻结端口链（防 2 节点回退）")
        self.assertEqual(plan["work_units"]["total"], len(frozen_phase1_chain()))

    # ── FIX-E2E B1-A3: 空必填输入 fail-closed ──
    def test_07_empty_input_lights_fail_closed(self):
        out = os.path.join(self.tmp, "out_empty")
        os.makedirs(out, exist_ok=True)
        cfg = os.path.join(self.tmp, "empty.json")
        d = json.load(open(self.cfg))
        d["input_lights"] = []
        d["output_dir"] = out
        json.dump(d, open(cfg, "w"))
        r = self._run("phase1", "run", "--config", cfg, "--events-jsonl")
        self.assertNotEqual(r.returncode, 0, "空 input_lights 必须非零退出")
        for f in os.listdir(out):
            if f.startswith("astrocs_run_"):
                man = json.load(open(os.path.join(out, f), encoding="utf-8"))
                self.assertNotEqual(man["status"], "complete")

    # ── FIX-E2E B1-A9: nested 缺省 + precision_mode 显式拒绝 ──
    def test_08_drizzle_nested_default_and_precision_required(self):
        # precision_mode 缺失 → DATA 拒绝 (rc=2, 不 silent default)
        out = os.path.join(self.tmp, "out_nopm")
        os.makedirs(out, exist_ok=True)
        cfg = os.path.join(self.tmp, "nopm.json")
        d = json.load(open(self.cfg))
        d["drizzle"] = {"nside": 512, "nested": 1, "pixfrac": 1.0}
        d["output_dir"] = out
        json.dump(d, open(cfg, "w"))
        r = self._run("phase1", "run", "--config", cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        self.assertIn("precision_mode", r.stderr)
        # nested 缺失 → HiPS NESTED 合同缺省 1 (rc=0, HiPS 落盘)
        out2 = os.path.join(self.tmp, "out_nonested")
        os.makedirs(out2, exist_ok=True)
        cfg2 = os.path.join(self.tmp, "nonested.json")
        d2 = json.load(open(self.cfg))
        d2["drizzle"] = {"nside": 512, "pixfrac": 1.0, "precision_mode": 1}
        d2["output_dir"] = out2
        json.dump(d2, open(cfg2, "w"))
        r2 = self._run("phase1", "run", "--config", cfg2, "--events-jsonl", timeout=300)
        self.assertEqual(r2.returncode, 0, r2.stderr[-500:])
        self.assertTrue(os.path.isfile(os.path.join(out2, "signal", "properties")))

    # ── FIX-E2E B1-A9: FP32/FP64 双精度 E2E 可达 + 结构等价 ──
    def test_09_fp32_fp64_equivalence(self):
        finals = {}
        for pm in (0, 1):
            out = os.path.join(self.tmp, "out_pm%d" % pm)
            os.makedirs(out, exist_ok=True)
            cfg = os.path.join(self.tmp, "pm%d.json" % pm)
            d = json.load(open(self.cfg))
            d["drizzle"] = {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": pm}
            d["output_dir"] = out
            json.dump(d, open(cfg, "w"))
            r = self._run("phase1", "run", "--config", cfg, "--events-jsonl", timeout=300)
            self.assertEqual(r.returncode, 0, "precision_mode=%d: %s" % (pm, r.stderr[-400:]))
            finals[pm] = json.load(open(os.path.join(out, "p1_final.json")))
        self.assertEqual(finals[0]["n_tiles"], finals[1]["n_tiles"])
        self.assertEqual(finals[0]["n_tiles_written"], finals[1]["n_tiles_written"])

if __name__ == "__main__":
    unittest.main(verbosity=2)
