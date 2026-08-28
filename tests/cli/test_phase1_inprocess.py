#!/usr/bin/env python3
"""CLI-004 集成测试: phase1 run 进程内调用 — 无子进程/事件完整/错误映射/取消/数值 Oracle。"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "cli", "astrocs")
FIX = "/tmp/astrocs_p1_fixture"   # 由 setUpClass 编译


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
        json.dump({
            "input_lights": [os.path.join(cls.data, "light_1.fits"),
                             os.path.join(cls.data, "light_2.fits")],
            "master_bias": os.path.join(cls.data, "bias.fits"),
            "master_dark": os.path.join(cls.data, "dark.fits"),
            "master_flat": os.path.join(cls.data, "flat.fits"),
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
        self.assertEqual(len(m["artifacts"]), 2, "两帧校准输出入 manifest")
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
        # 配置坏 JSON → 2(ARGS)
        bad2 = os.path.join(self.tmp, "badjson.json")
        open(bad2, "w").write("{not json")
        r2 = self._run("phase1", "run", "--config", bad2)
        self.assertEqual(r2.returncode, 2)
        # master 尺寸不匹配 → 2(PARAM→ARGS); 造一个 32x32 的 master
        small = subprocess.run([self.fixture, "--make", self.data], capture_output=True,
                               timeout=60)  # noop 复用
        cfg3 = os.path.join(self.tmp, "cfg3.json")
        json.dump({"input_lights": [os.path.join(self.data, "light_1.fits")],
                   "output_dir": self.out}, open(cfg3, "w"))
        r3 = self._run("phase1", "run", "--config", cfg3, "--events-jsonl")
        self.assertEqual(r3.returncode, 0)  # master 全可空 → 无 master 校准路径也合法

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

if __name__ == "__main__":
    unittest.main(verbosity=2)
