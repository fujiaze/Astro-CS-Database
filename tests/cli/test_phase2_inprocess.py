#!/usr/bin/env python3
"""CLI-005 集成测试: phase2 run 进程内生产路由(coverage→sampler→UPM) — 无子进程/事件/错误映射/取消。"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "cli", "astrocs")
AIO = os.path.join(REPO, "lib", "astro_image_io")


def cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


class TestPhase2InProcess(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="p2int_")
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{os.path.join(REPO, 'lib', 'common')}"]
        srcs = [os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]
        cls.fixture = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                            "-o", cls.fixture], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        cls.data = os.path.join(cls.tmp, "data")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True, text=True,
                           timeout=300)
        assert "HIPS_FIXTURES_OK" in r.stdout, r.stderr
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        json.dump({
            "hips_paths": [os.path.join(cls.data, "F1.hips"),
                           os.path.join(cls.data, "F2.hips")],
            "output_dir": cls.out,
        }, open(cls.cfg, "w"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args, **kw):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              timeout=kw.pop("timeout", 300), **kw)

    def test_01_production_route_complete(self):
        """实际生产函数路由: coverage→sample→UPM; 事件+manifest complete+verify。"""
        r = self._run("phase2", "run", "--config", self.cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))))
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        m = json.load(open(mpath, encoding="utf-8"))
        self.assertEqual(m["status"], "complete")
        v = self._run("verify", "--json", "--run-manifest", mpath)
        self.assertEqual(v.returncode, 0)

    def test_02_session_manifest_route_values(self):
        """生产路由数值: 双帧重叠 → obs>0, overlap_controls≥1, coverage cells=12。"""
        r = self._run("phase2", "run", "--config", self.cfg, "--events-jsonl")
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        res = [e for e in events if e["kind"] == "resource"][-1]
        self.assertGreater(res["n_obs"], 0)
        self.assertEqual(res["n_inputs"], 2)

    def test_03_no_subprocess(self):
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="2500")
        p = subprocess.Popen([EXE, "phase2", "run", "--config", self.cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
        time.sleep(0.8)
        children = []
        task_dir = f"/proc/{p.pid}/task"
        if os.path.isdir(task_dir):
            for tid in os.listdir(task_dir):
                cf = os.path.join(task_dir, tid, "children")
                if os.path.isfile(cf):
                    children += open(cf).read().split()
        self.assertEqual(children, [], "phase2 运行中不得产生子进程(纯进程内调用)")
        p.wait(timeout=120)
        self.assertEqual(p.returncode, 0)

    def test_04_error_mapping(self):
        """API-P2 §4 映射: INVALID→2(PARAM); 缺失输入→3; production 缺 ivar rc=2→4(SCIENCE)。"""
        bad = os.path.join(self.tmp, "badpath.json")
        json.dump({"hips_paths": ["/nonexistent/does_not_exist.hips"],
                   "output_dir": self.out}, open(bad, "w"))
        r = self._run("phase2", "run", "--config", bad, "--events-jsonl")
        self.assertEqual(r.returncode, 3, f"缺失输入→3; got {r.returncode}: {r.stderr[-200:]}")
        bad2 = os.path.join(self.tmp, "badjson.json")
        open(bad2, "w").write("{oops")
        r2 = self._run("phase2", "run", "--config", bad2)
        self.assertEqual(r2.returncode, 2)
        # 缺 output_dir → 2(validate 拒绝, 无 silent default)
        bad3 = os.path.join(self.tmp, "nodir.json")
        json.dump({"hips_paths": [os.path.join(self.data, "F1.hips")]}, open(bad3, "w"))
        r3 = self._run("phase2", "run", "--config", bad3)
        self.assertEqual(r3.returncode, 2)

    def test_05_cancel_mid_run(self):
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="3000")
        p = subprocess.Popen([EXE, "phase2", "run", "--config", self.cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
        time.sleep(0.6)
        p.send_signal(signal.SIGINT)
        out, _ = p.communicate(timeout=30)
        self.assertEqual(p.returncode, 9)
        events = [json.loads(l) for l in out.splitlines() if l.strip()]
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "cancelled")

if __name__ == "__main__":
    unittest.main(verbosity=2)
