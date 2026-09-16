#!/usr/bin/env python3
"""CLI 集成测试: normalize 进程内调用 — 无子进程/事件完整/错误映射/取消/数值 Oracle。

按 CLI-001 §6.2 新树同步（原文件用 phase1 run --config）:
  * 命令: normalize --json <cfg> [--events-jsonl] [-y]（§6.2 / §6.3）;
  * 二进制: 唯一 exe build/astrocs（ASTROCS_CLI_BIN 可覆盖）;
  * fixture 源码路径: ARCH-001 迁移后的 lib/infrastructure/aio（旧 lib/astro_image_io
    已不存在; 保留旧路径回退以便迁移中间态两侧都能构建）。

退役登记（依据 §6.2 唯一命令树 + CLI-001 rc 矩阵）:
  * test_01 内 "verify --json --run-manifest" 断言 → 退役: verify 命令删除;
    哈希链复算的新载体是 export resume 预检（cli/commands.cpp cmd_session3_run）,
    该载体当前被 export 预检/会话配置口径冲突阻塞（TEST-CLI-SYNC 报告已登记,
    归属 CLI-002）⇒ 本文件不再断言 verify, 缺口显式登记不静默;
  * test_06_ir_matches_frozen_chain（phase1 plan --json 逐节点对比 registry）→ 退役:
    plan 命令删除, IR 逐节点顺序无用户可见载体; 防「2 节点回退」的等价证据是本文件
    test_01 的 8 节点产物集断言（calibrated×2 + p1_sources/psf/wcs/flux/snr/final）。
"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REGISTRY = os.path.join(REPO, "runtime", "pipeline", "module_ports.registry.json")


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
from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402

# ARCH-001 迁移: 新布局优先, 旧路径回退（迁移未落盘的一侧仍可构建 fixture）。
AIO = next((p for p in (os.path.join(REPO, "lib", "infrastructure", "aio"),
                        os.path.join(REPO, "lib", "astro_image_io")) if os.path.isdir(p)),
           os.path.join(REPO, "lib", "infrastructure", "aio"))


def build_fixture(tmp):
    exe = os.path.join(tmp, "fixture")
    cf = [os.path.join(AIO, "third_party", "cfitsio", f) for f in os.listdir(
        os.path.join(AIO, "third_party", "cfitsio")) if f.endswith(".c")]
    objs = []
    for c in cf:
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", c):
            continue
        b = os.path.basename(c)[:-2] + ".o"
        o = os.path.join(tmp, b)
        subprocess.run(["gcc", "-O2", "-w", f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                        "-c", c, "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS",
                        f"-I{os.path.join(REPO, 'include')}",
                        f"-I{os.path.join(AIO, 'include')}",
                        f"-I{os.path.join(AIO, 'src')}",
                        f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                        os.path.join(REPO, "tests", "backend", "phase1_fixture_main.cpp"),
                        os.path.join(AIO, "src", "aio_fits.cpp"),
                        os.path.join(AIO, "src", "aio_api.cpp"),
                        os.path.join(AIO, "src", "aio_log.cpp"),
                        os.path.join(AIO, "src", "aio_compressor.cpp"),
                        *objs, "-lz", "-lzstd", "-llz4", "-o", exe],
                       capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr[-800:]
    return exe


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


class TestPhase1InProcess(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="p1int_")
        cls.fixture = build_fixture(cls.tmp)
        cls.data = os.path.join(cls.tmp, "data")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.data)
        os.makedirs(cls.out)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True, text=True,
                           timeout=120, cwd=run_cwd())
        assert "FIXTURES_OK" in r.stdout, r.stderr
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        # FIX-E2E B1-A1: 正式 phase1 链为 8 节点（cal→cos→psf→wcs→phot→snr→drz→wr）,
        # drizzle/wcs 为链上必填科学配置; Linux ipv stub 平台 wcs 走显式 WCS 配置路径。
        with open(cls.cfg, "w", encoding="utf-8") as fh:
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
            }, fh)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args, **kw):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              timeout=kw.pop("timeout", 120), cwd=run_cwd(), **kw)

    def test_01_run_complete_events_manifest(self):
        r = self._run("normalize", "--json", self.cfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        self.assertGreaterEqual(len(events), 4)
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))), "sequence 从 0 单调")
        self.assertEqual(events[0]["kind"], "stage_start")
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        self.assertEqual(events[-1]["phase"], "normalize", "事件 phase 用命令名(内部指代)")
        kinds = [e["kind"] for e in events]
        self.assertIn("artifact", kinds)
        # run manifest complete + 全链产物（8 节点事实面）入 manifest
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        with open(mpath, encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["status"], "complete")
        names = {os.path.basename(a["path"]) for a in m["artifacts"]}
        self.assertTrue({"calibrated_light_1.fits", "calibrated_light_2.fits"} <= names, names)
        self.assertTrue({"p1_sources.json", "p1_psf.json", "p1_wcs.json", "p1_flux.json",
                         "p1_snr.json", "p1_final.json"} <= names,
                        "phase1 冻结 8 节点链的产物必须全部入 manifest: %s" % sorted(names))
        for a in m["artifacts"]:
            self.assertTrue(a["sha256"] and os.path.isfile(a["path"]), a["path"])

    def test_02_calibrated_values_match_oracle(self):
        """数值 Oracle: (200-bias-k*(dark-bias))/flat = (200-100-50)/1.25 = 40(精确)。"""
        for f in ("calibrated_light_1.fits", "calibrated_light_2.fits"):
            r = subprocess.run([self.fixture, "--mean", os.path.join(self.out, f)],
                               capture_output=True, text=True, timeout=60, cwd=run_cwd())
            m = re.search(r"MEAN ([\d.]+)", r.stdout)
            self.assertIsNotNone(m)
            self.assertAlmostEqual(float(m.group(1)), 40.0, places=4, msg=f)

    def test_03_no_subprocess_during_run(self):
        """验收核心: 运行中进程树无子进程(取消钩子窗口内检查 /proc/<pid>/task/*/children)。"""
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="2500")
        p = subprocess.Popen([EXE, "normalize", "--json", self.cfg, "--events-jsonl", "-y"],
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
        self.assertEqual(children, [], "normalize 运行中不得产生子进程(纯进程内调用)")
        p.wait(timeout=60)
        self.assertEqual(p.returncode, 0)

    def test_04_error_mapping(self):
        # 缺失输入文件 → 3(INPUT)
        bad = os.path.join(self.tmp, "missing.json")
        with open(bad, "w", encoding="utf-8") as fh:
            json.dump({"input_lights": ["/nonexistent/x.fits"], "output_dir": self.out}, fh)
        r = self._run("normalize", "--json", bad, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 3, r.stderr[-200:])
        # 配置坏 JSON → 3(INPUT)
        bad2 = os.path.join(self.tmp, "badjson.json")
        with open(bad2, "w", encoding="utf-8") as fh:
            fh.write("{not json")
        r2 = self._run("normalize", "--json", bad2, "-y")
        self.assertEqual(r2.returncode, 3)
        # 无 master 校准路径仍合法（正式 8 节点链要求 drizzle/wcs 配置）
        cfg3 = os.path.join(self.tmp, "cfg3.json")
        with open(cfg3, "w", encoding="utf-8") as fh:
            json.dump({"input_lights": [os.path.join(self.data, "light_1.fits")],
                       "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                               "cd11": -2.7777777777777776e-4, "cd12": 0.0,
                               "cd21": 0.0, "cd22": 2.7777777777777776e-4},
                       "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0,
                                   "precision_mode": 1},
                       "output_dir": self.out}, fh)
        r3 = self._run("normalize", "--json", cfg3, "--events-jsonl", "-y")
        self.assertEqual(r3.returncode, 0, r3.stderr[-400:])

    def test_05_cancel_mid_run(self):
        """取消: rc=9 + 不落 complete manifest（§6.3 取消不得留下看似完整的产品）。"""
        out = os.path.join(self.tmp, "out_cancel")
        os.makedirs(out, exist_ok=True)
        with open(self.cfg, encoding="utf-8") as fh:
            doc = json.load(fh)
        doc["output_dir"] = out
        cfg = os.path.join(self.tmp, "cancel.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="3000")
        p = subprocess.Popen([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
                             cwd=run_cwd(), text=True)
        time.sleep(0.6)
        p.send_signal(signal.SIGINT)
        out_s, err_s = p.communicate(timeout=30)
        self.assertEqual(p.returncode, 9, "取消 → 9; got %s (%s)" % (p.returncode, err_s[-200:]))
        self.assertIn("cancel", err_s)
        for fn in os.listdir(out):
            if fn.startswith("astrocs_run_"):
                with open(os.path.join(out, fn), encoding="utf-8") as fh:
                    man = json.load(fh)
                self.assertNotEqual(man["status"], "complete", "取消不得写 complete manifest")

    def test_06_empty_input_lights_fail_closed(self):
        out = os.path.join(self.tmp, "out_empty")
        os.makedirs(out, exist_ok=True)
        cfg = os.path.join(self.tmp, "empty.json")
        with open(self.cfg, encoding="utf-8") as fh:
            d = json.load(fh)
        d["input_lights"] = []
        d["output_dir"] = out
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(d, fh)
        r = self._run("normalize", "--json", cfg, "--events-jsonl", "-y")
        self.assertNotEqual(r.returncode, 0, "空 input_lights 必须非零退出")
        for f in os.listdir(out):
            if f.startswith("astrocs_run_"):
                with open(os.path.join(out, f), encoding="utf-8") as fh:
                    man = json.load(fh)
                self.assertNotEqual(man["status"], "complete")

    def test_07_drizzle_nested_default_and_precision_required(self):
        # precision_mode 缺失 → 拒绝 (rc=2, 不 silent default)
        out = os.path.join(self.tmp, "out_nopm")
        os.makedirs(out, exist_ok=True)
        cfg = os.path.join(self.tmp, "nopm.json")
        with open(self.cfg, encoding="utf-8") as fh:
            d = json.load(fh)
        d["drizzle"] = {"nside": 512, "nested": 1, "pixfrac": 1.0}
        d["output_dir"] = out
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(d, fh)
        r = self._run("normalize", "--json", cfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        self.assertIn("precision_mode", r.stderr)
        # nested 缺失 → HiPS NESTED 合同缺省 1 (rc=0, HiPS 落盘)
        out2 = os.path.join(self.tmp, "out_nonested")
        os.makedirs(out2, exist_ok=True)
        cfg2 = os.path.join(self.tmp, "nonested.json")
        with open(self.cfg, encoding="utf-8") as fh:
            d2 = json.load(fh)
        d2["drizzle"] = {"nside": 512, "pixfrac": 1.0, "precision_mode": 1}
        d2["output_dir"] = out2
        with open(cfg2, "w", encoding="utf-8") as fh:
            json.dump(d2, fh)
        r2 = self._run("normalize", "--json", cfg2, "--events-jsonl", "-y", timeout=300)
        self.assertEqual(r2.returncode, 0, r2.stderr[-500:])
        self.assertTrue(os.path.isfile(os.path.join(out2, "signal", "properties")))

    def test_08_fp32_fp64_equivalence(self):
        finals = {}
        for pm in (0, 1):
            out = os.path.join(self.tmp, "out_pm%d" % pm)
            os.makedirs(out, exist_ok=True)
            cfg = os.path.join(self.tmp, "pm%d.json" % pm)
            with open(self.cfg, encoding="utf-8") as fh:
                d = json.load(fh)
            d["drizzle"] = {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": pm}
            d["output_dir"] = out
            with open(cfg, "w", encoding="utf-8") as fh:
                json.dump(d, fh)
            r = self._run("normalize", "--json", cfg, "--events-jsonl", "-y", timeout=300)
            self.assertEqual(r.returncode, 0, "precision_mode=%d: %s" % (pm, r.stderr[-400:]))
            with open(os.path.join(out, "p1_final.json"), encoding="utf-8") as fh:
                finals[pm] = json.load(fh)
        self.assertEqual(finals[0]["n_tiles"], finals[1]["n_tiles"])
        self.assertEqual(finals[0]["n_tiles_written"], finals[1]["n_tiles_written"])

    def test_09_invalid_master_flat_fail_closed(self):
        """RESCUE-P0-06 次生: p1_op_calibrate 不得消费退化 master flat。

        全零 / 负中位数 / 非有限 master flat 经 max(flat,0.1)=0.1 会恒 ×10 放大出
        看似正常的伪科学产品; 消费边界必须非零退出、不写 complete 且不留
        calibrated_*.fits 半成品 (SCI-CAL-001 §3 单位表 + §8 退化条件;
        ALG-CAL-003 消费口径)。
        """
        bad_dir = os.path.join(self.tmp, "badflat")
        os.makedirs(bad_dir, exist_ok=True)
        r0 = subprocess.run([self.fixture, "--make-bad-flat", bad_dir],
                            capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertIn("BAD_FLATS_OK", r0.stdout, r0.stderr)
        for name in ("flat_zero.fits", "flat_neg.fits", "flat_nan.fits"):
            bad = os.path.join(bad_dir, name)
            self.assertTrue(os.path.isfile(bad), bad)
            out = os.path.join(self.tmp, "out_badflat_" + name.split(".")[0])
            os.makedirs(out, exist_ok=True)
            cfg = os.path.join(self.tmp, "badflat_" + name + ".json")
            with open(self.cfg, encoding="utf-8") as fh:
                d = json.load(fh)
            d["master_flat"] = bad
            d["input_lights"] = [os.path.join(self.data, "light_1.fits")]
            d["output_dir"] = out
            with open(cfg, "w", encoding="utf-8") as fh:
                json.dump(d, fh)
            rr = self._run("normalize", "--json", cfg, "--events-jsonl", "-y")
            self.assertNotEqual(rr.returncode, 0,
                                "%s: 退化 master flat 必须非零退出" % name)
            self.assertIn("master_flat", rr.stderr,
                          "%s: stderr 须指明非法 master_flat\n%s" % (name, rr.stderr[-300:]))
            files = os.listdir(out)
            for f in files:
                if f.startswith("astrocs_run_"):
                    with open(os.path.join(out, f), encoding="utf-8") as fh:
                        man = json.load(fh)
                    self.assertNotEqual(man["status"], "complete",
                                        "%s: 不得写 complete manifest" % name)
            self.assertFalse(any(f.startswith("calibrated_") for f in files),
                             "%s: 不得留 calibrated_* 半成品: %s" % (name, files))

    def test_10_frozen_registry_chain_guard(self):
        """防「2 节点回退」: registry 冻结的 phase1 链必须是 8 模块, 且产物集覆盖其末端。

        （原 test_06 用 plan --json 逐节点对比; plan 已随 §6.2 删除, 等价守卫改为
        registry 冻结值 + test_01 的产物集断言。）
        """
        with open(REGISTRY, encoding="utf-8") as fh:
            reg = json.load(fh)
        p1 = [m["module_id"] for m in reg["modules"] if m.get("phase") == "phase1"]
        self.assertEqual(len(p1), 8,
                         "registry 冻结 phase1 模块数必须为 8(cal,cos,psf,wcs,phot,snr,drz,wr): %s"
                         % p1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
