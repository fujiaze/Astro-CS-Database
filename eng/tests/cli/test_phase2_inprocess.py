#!/usr/bin/env python3
"""CLI 集成测试: mosaic 进程内生产路由(coverage→sampler→UPM) — 无子进程/事件/错误映射/取消。

按 CLI-001 §6.2 新树同步（原文件用 phase2 run --config）:
  * 命令: mosaic --json <cfg> [--events-jsonl] [-y]（§6.2 / §6.3）;
  * 二进制: 唯一 exe build/astrocs（ASTROCS_CLI_BIN 可覆盖）;
  * fixture 源码路径: ARCH-001 迁移后布局（lib/infrastructure/aio +
    lib/algorithms/shared/healpix），旧路径回退以便迁移中间态两侧可构建。

退役登记:
  * test_01 内 "verify --json --run-manifest" 断言 → 退役: verify 命令删除（§6.2）;
    哈希链复算新载体是 export resume 预检, 当前被 export 预检/会话口径冲突阻塞
    （TEST-CLI-SYNC 报告, 归属 CLI-002）, 缺口显式登记不静默。
"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "astrocs"), ("build", "cli", "astrocs")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "astrocs")


EXE = cli_binary()

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402


def _pick(*cands):
    for p in cands:
        if os.path.isdir(p):
            return p
    return cands[0]


AIO = _pick(os.path.join(REPO, "lib", "infrastructure", "aio"),
            os.path.join(REPO, "lib", "astro_image_io"))
SHARED = _pick(os.path.join(REPO, "lib", "algorithms", "shared"),
               os.path.join(REPO, "lib", "common"))
HEALPIX_SRC = _pick(os.path.join(SHARED, "healpix", "healpix_core.cpp"),
                    os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp"))


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
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="p2int_")
        incs = [f"-I{os.path.join(REPO, 'lib', 'include')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{SHARED}",
                f"-I{os.path.dirname(HEALPIX_SRC)}"]
        srcs = [os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                HEALPIX_SRC]
        cls.fixture = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                            "-o", cls.fixture], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        cls.data = os.path.join(cls.tmp, "data")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True, text=True,
                           timeout=300, cwd=run_cwd())
        assert "HIPS_FIXTURES_OK" in r.stdout, r.stderr
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        with open(cls.cfg, "w", encoding="utf-8") as fh:
            json.dump({
                "hips_paths": [os.path.join(cls.data, "F1.hips"),
                               os.path.join(cls.data, "F2.hips")],
                "output_dir": cls.out,
            }, fh)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args, **kw):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              timeout=kw.pop("timeout", 300), cwd=run_cwd(), **kw)

    def test_01_production_route_complete(self):
        """实际生产函数路由: coverage→sample→UPM; 事件+manifest complete。"""
        r = self._run("mosaic", "--json", self.cfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))))
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        self.assertEqual(events[-1]["phase"], "mosaic")
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        with open(mpath, encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["status"], "complete")
        # P1-5 回归: manifest artifacts 逐条带真实产物证据（path 存在 + sha256
        # 为 64 位十六进制 + size_bytes>0）。默认 config 无文件产物时允许空列表,
        # 但出现条目时证据字段必须完整（防"恒空/伪造证据"回归）。
        for a in m.get("artifacts", []):
            self.assertTrue(os.path.isfile(a["path"]), "artifact 缺失: %s" % a["path"])
            self.assertTrue(re.fullmatch(r"[0-9a-f]{64}", a["sha256"]),
                            "artifact sha256 非真实证据: %s" % a)
            self.assertGreater(a["size_bytes"], 0)

    def test_02_session_manifest_route_values(self):
        """生产路由数值: 双帧重叠 → obs>0, n_inputs=2。"""
        r = self._run("mosaic", "--json", self.cfg, "--events-jsonl", "-y")
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        res = [e for e in events if e["kind"] == "resource"][-1]
        self.assertGreater(res["n_obs"], 0)
        self.assertEqual(res["n_inputs"], 2)

    def test_03_no_subprocess(self):
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="2500")
        p = subprocess.Popen([EXE, "mosaic", "--json", self.cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env,
                             cwd=run_cwd())
        time.sleep(0.8)
        children = []
        task_dir = f"/proc/{p.pid}/task"
        if os.path.isdir(task_dir):
            for tid in os.listdir(task_dir):
                cf = os.path.join(task_dir, tid, "children")
                if os.path.isfile(cf):
                    with open(cf) as fh:
                        children += fh.read().split()
        self.assertEqual(children, [], "mosaic 运行中不得产生子进程(纯进程内调用)")
        p.wait(timeout=120)
        self.assertEqual(p.returncode, 0)

    def test_04_error_mapping(self):
        """API-P2 §4 映射: 缺失输入→3; 坏 JSON→3; 缺 output_dir→2(无 silent default)。"""
        bad = os.path.join(self.tmp, "badpath.json")
        with open(bad, "w", encoding="utf-8") as fh:
            json.dump({"hips_paths": ["/nonexistent/does_not_exist.hips"],
                       "output_dir": self.out}, fh)
        r = self._run("mosaic", "--json", bad, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 3, "缺失输入→3; got %s: %s" % (r.returncode, r.stderr[-200:]))
        bad2 = os.path.join(self.tmp, "badjson.json")
        with open(bad2, "w", encoding="utf-8") as fh:
            fh.write("{oops")
        r2 = self._run("mosaic", "--json", bad2, "-y")
        self.assertEqual(r2.returncode, 3)
        bad3 = os.path.join(self.tmp, "nodir.json")
        with open(bad3, "w", encoding="utf-8") as fh:
            json.dump({"hips_paths": [os.path.join(self.data, "F1.hips")]}, fh)
        r3 = self._run("mosaic", "--json", bad3, "-y")
        self.assertEqual(r3.returncode, 2)

    def test_05_cancel_mid_run(self):
        out = os.path.join(self.tmp, "out_cancel")
        os.makedirs(out, exist_ok=True)
        with open(self.cfg, encoding="utf-8") as fh:
            doc = json.load(fh)
        doc["output_dir"] = out
        cfg = os.path.join(self.tmp, "cancel.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="3000")
        p = subprocess.Popen([EXE, "mosaic", "--json", cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
                             cwd=run_cwd(), text=True)
        time.sleep(0.6)
        p.send_signal(signal.SIGINT)
        _out, err = p.communicate(timeout=30)
        self.assertEqual(p.returncode, 9, "取消 → 9; got %s (%s)" % (p.returncode, err[-200:]))
        self.assertIn("cancel", err)
        for fn in os.listdir(out):
            if fn.startswith("astrocs_run_"):
                with open(os.path.join(out, fn), encoding="utf-8") as fh:
                    man = json.load(fh)
                self.assertNotEqual(man["status"], "complete", "取消不得写 complete manifest")

    def test_06_manifest_artifacts_nonempty_evidence(self):
        """P1-5 回归: mosaic run manifest artifacts 非空且含真实产物证据。

        以 persist_upm+upm_save_path 让 session 真实落盘 UPM 模型（唯一文件产物）,
        断言 run manifest artifacts 收到该产物且 sha256/size_bytes 与磁盘文件一致。
        取舍: 资源门禁 MON-002 对共享 CI 机负载敏感（upm_build 阶段等效核数低
        时可能判 low_avg_cores → rc=10）; artifacts 收集在门禁判定之后执行且
        complete/incomplete 两路径均入 manifest, 故证据断言不依赖门禁放行。
        """
        cfg = os.path.join(self.tmp, "cfg_upm.json")
        upm_path = os.path.join(self.out, "upm_model_test.bin")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"hips_paths": [os.path.join(self.data, "F1.hips"),
                                      os.path.join(self.data, "F2.hips")],
                       "output_dir": self.out,
                       "persist_upm": True,
                       "upm_save_path": upm_path}, fh)
        r = self._run("mosaic", "--json", cfg, "--events-jsonl", "-y")
        self.assertIn(r.returncode, (0, 10), r.stderr[-500:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        with open(mpath, encoding="utf-8") as fh:
            m = json.load(fh)
        arts = m.get("artifacts", [])
        self.assertTrue(arts, "run manifest artifacts 不应为空(P1-5 恒空回归)")
        hit = [a for a in arts if a["path"] == upm_path]
        self.assertTrue(hit, "UPM 产物应入 run manifest: %s" % arts)
        self.assertTrue(os.path.isfile(upm_path))
        import hashlib
        with open(upm_path, "rb") as fh:
            want_sha = hashlib.sha256(fh.read()).hexdigest()
        self.assertEqual(hit[0]["sha256"], want_sha, "sha256 应为真实文件证据")
        self.assertEqual(hit[0]["size_bytes"], os.path.getsize(upm_path))


if __name__ == "__main__":
    unittest.main(verbosity=2)
