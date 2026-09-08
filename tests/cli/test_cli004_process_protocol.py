#!/usr/bin/env python3
"""CLI-004 验收: GUI 可调用进程协议(JSON stream protocol) — 外部 harness 视角。

权威: docs/api/CLI_PROTOCOL_V1.md §3/§4/§5 (04 §3/§4/§5) + cli/protocol.h 生产侧硬闸。
方法(independent, 模拟外部 harness/GUI, 不调用库内部):
  - spawn `astrocs phaseN run --events-jsonl` 子进程, 流式逐行读 stdout;
  - 每行恰一个 UTF-8 JSON 事件(stdout 纪律), 独立重实现协议合同校验(防生产侧同源盲区);
  - 取消: SIGINT → exit 9 + final(status=cancelled) + incomplete manifest 可恢复读取;
  - run directory: manifest 落 config.output_dir, 文件名 astrocs_run_<run_id>.json;
  - 无 Qt/HiPS Browser 链接(源码 + 动态依赖双查)。
依赖: CLI 已构建(build/cli/astrocs)。phase1 fixture 同 test_phase1_inprocess 编译模式,
产物缓存到 /tmp/astrocs_cli004_fixture 避免重复编译。
"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "cli", "astrocs")
CLI_DIR = os.path.join(REPO, "cli")
SCHEMA = os.path.join(REPO, "schemas", "jsonl_event_v1.schema.json")

# §4 冻结 kind 扩展字段(独立重实现 — 与 cli/protocol.h 生产侧互为对偶)
REQUIRED_FIELDS = {"schema_version", "event_id", "run_id", "timestamp_utc", "sequence",
                   "kind", "severity", "phase", "stage", "message"}
KIND_EXT = {
    "progress": {"completed", "total", "unit", "rate", "eta_seconds"},
    "resource": {"cpu_cores_used", "rss_bytes", "io_read_bytes", "io_write_bytes", "threads"},
    "artifact": {"role", "path", "sha256", "size_bytes"},
    "backend": {"kernel", "backend_id", "isa", "workers", "block_size", "reason"},
    "final": {"exit_code", "status", "run_manifest", "summary"},
}
EXIT_DOMAIN = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 70}   # 04 §2 冻结 11 条
TS_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
RUNID_RE = re.compile(r"^[0-9a-f]{12}$")


def harness_validate(ev, expect_seq):
    """外部 harness 侧协议校验(读侧, 独立于 cli/protocol.h)。返回错误串, None=通过。"""
    if not isinstance(ev, dict):
        return "event is not an object"
    missing = REQUIRED_FIELDS - set(ev)
    if missing:
        return f"missing required field(s): {sorted(missing)}"
    if not RUNID_RE.match(ev["run_id"]):
        return f"run_id malformed: {ev['run_id']!r}"
    if not TS_RE.match(ev["timestamp_utc"]):
        return f"timestamp_utc malformed: {ev['timestamp_utc']!r}"
    if ev["sequence"] != expect_seq:
        return f"sequence {ev['sequence']} != expected {expect_seq} (monotonic from 0)"
    if not ev["event_id"].startswith(f"evt-{ev['run_id']}-"):
        return f"event_id not run-bound: {ev['event_id']!r}"
    ext = KIND_EXT.get(ev["kind"])
    if ext:
        miss = ext - set(ev)
        if miss:
            return f"kind {ev['kind']!r} missing frozen extension field(s): {sorted(miss)}"
    if ev["kind"] == "final":
        if ev["exit_code"] not in EXIT_DOMAIN:
            return f"final.exit_code {ev['exit_code']} outside frozen 04 §2 domain"
        if ev["exit_code"] < 2 and ev["status"] in ("cancelled",):
            return f"final exit_code {ev['exit_code']} inconsistent with status"
    return None


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


def build_fixture(tmp):
    """phase1 fixture(同 test_phase1_inprocess 模式); 产物缓存跨运行复用。"""
    cache = "/tmp/astrocs_cli004_fixture"
    if os.path.isfile(cache):
        return cache
    exe = os.path.join(tmp, "fixture")
    aio = os.path.join(REPO, "lib", "astro_image_io")
    cdir = os.path.join(aio, "third_party", "cfitsio")
    objs = []
    for c in sorted(os.listdir(cdir)):
        if not c.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", c):
            continue
        o = os.path.join(tmp, c[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, c),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS",
                        f"-I{os.path.join(REPO, 'include')}",
                        f"-I{os.path.join(aio, 'include')}",
                        f"-I{os.path.join(aio, 'src')}",
                        f"-I{cdir}",
                        os.path.join(REPO, "tests", "backend", "phase1_fixture_main.cpp"),
                        os.path.join(aio, "src", "aio_fits.cpp"),
                        os.path.join(aio, "src", "aio_api.cpp"),
                        os.path.join(aio, "src", "aio_log.cpp"),
                        os.path.join(aio, "src", "aio_compressor.cpp"),
                        *objs, "-lz", "-lzstd", "-llz4", "-o", exe],
                       capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr[-800:]
    shutil.copy2(exe, cache)
    return exe


class TestCli004ProcessProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI: cmake --build build"
        cls.tmp = tempfile.mkdtemp(prefix="cli004_")
        cls.fixture = build_fixture(cls.tmp)
        cls.data = os.path.join(cls.tmp, "data")
        os.makedirs(cls.data)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True,
                           text=True, timeout=120)
        assert "FIXTURES_OK" in r.stdout, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _cfg(self, out, lights):
        cfg = os.path.join(self.tmp, f"cfg_{os.path.basename(out)}.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": lights, "darks": [], "flats": [], "bias": []},
                   "output_dir": out}, open(cfg, "w"))
        return cfg

    def _empty_cfg(self, out):
        cfg = os.path.join(self.tmp, f"cfg_empty_{os.path.basename(out)}.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
                   "output_dir": out}, open(cfg, "w"))
        return cfg

    # ── 1. 外部 harness: spawn + 流式消费 + 全事件过协议合同(失败路径) ──
    def test_01_harness_stream_contract_failure_path(self):
        out = os.path.join(self.tmp, "o1"); os.makedirs(out)
        cfg = self._empty_cfg(out)
        errf = os.path.join(self.tmp, "o1_stderr.txt")
        with open(errf, "w") as ef:
            p = subprocess.Popen([EXE, "phase1", "run", "--config", cfg, "--events-jsonl"],
                                 stdout=subprocess.PIPE, stderr=ef, text=True)
            events, raw = [], []
            for line in p.stdout:          # 真流式: 逐行读取(非 communicate 后解析)
                raw.append(line)
                events.append(json.loads(line))   # stdout 纪律: 每行恰一 JSON, 否则异常
            p.wait(timeout=120)
        with open(errf, encoding="utf-8") as fh:
            err_text = fh.read()
        # 实测(2026-09-08 probe): 空 lights → Runtime INVALID → PARAM(2), 语义同
        # test_phase2_inprocess.test_04(INVALID→2); "文件缺失"才是 3。
        self.assertEqual(p.returncode, 2, "空输入 → 2(PARAM)")
        self.assertGreater(len(events), 0)
        for i, ev in enumerate(events):
            err = harness_validate(ev, i)
            self.assertIsNone(err, f"event[{i}] protocol violation: {err}")
        fin = events[-1]
        self.assertEqual(fin["kind"], "final")
        self.assertEqual(fin["exit_code"], p.returncode)
        self.assertIn(fin["status"], {"phase1_failed", "failed"})
        # §4: 重计算 stage 必发 stage_start/stage_end
        kinds = [e["kind"] for e in events]
        self.assertIn("stage_start", kinds); self.assertIn("stage_end", kinds)
        # progress 事件在场且字段冻结(§4)
        prog = [e for e in events if e["kind"] == "progress"]
        self.assertEqual(len(prog), 2, "run 级 progress 0/1 与 1/1")
        for e in prog:
            self.assertLessEqual(e["completed"], e["total"])
            self.assertEqual(e["unit"], "phases")
        # run manifest artifact 可恢复读取
        mf = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"]
        self.assertTrue(mf, "manifest 事件必发")
        self.assertTrue(os.path.isfile(mf[-1]["path"]))
        self.assertTrue(err_text.strip(), "诊断/日志必须在 stderr")

    # ── 2. 外部 harness: OK 路径 resource/backend/artifact 冻结扩展字段 ──
    def test_02_harness_ok_run_frozen_extension_fields(self):
        out = os.path.join(self.tmp, "o2"); os.makedirs(out)
        cfg = self._cfg(out, [os.path.join(self.data, "light_1.fits"),
                              os.path.join(self.data, "light_2.fits")])
        r = subprocess.run([EXE, "phase1", "run", "--config", cfg, "--events-jsonl"],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        for i, ev in enumerate(events):
            err = harness_validate(ev, i)
            self.assertIsNone(err, f"event[{i}] protocol violation: {err}")
        # backend §4 六字段(含真实 isa)
        bk = [e for e in events if e["kind"] == "backend"]
        self.assertTrue(bk, "OK run 必发 backend 事件")
        for e in bk:
            self.assertIn(e["isa"], {"baseline", "sse2", "avx", "avx2", "avx512"})
            self.assertGreaterEqual(e["workers"], 1)
        # resource §4 五字段(所有 resource 事件统一携带)
        for e in [e for e in events if e["kind"] == "resource"]:
            self.assertGreaterEqual(e["rss_bytes"], 0)
            self.assertIsInstance(e["threads"], int)
        # artifact §4 词表: 文件 artifact sha256 为 64hex + size_bytes>0
        for e in [e for e in events if e["kind"] == "artifact" and e.get("role") != "graph_dir"]:
            self.assertRegex(e["sha256"], r"^[0-9a-f]{64}$")
            self.assertGreater(e["size_bytes"], 0)
        # final + manifest 恢复读取闭环
        fin = events[-1]
        self.assertEqual((fin["kind"], fin["status"], fin["exit_code"]), ("final", "ok", 0))
        mf = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        man = json.load(open(mf["path"], encoding="utf-8"))
        self.assertEqual(man["status"], "complete")
        self.assertEqual(man["run_id"], fin["run_id"])
        v = subprocess.run([EXE, "verify", "--run-manifest", mf["path"], "--json"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(v.returncode, 0, v.stderr[-200:])

    # ── 3. 外部 harness: 取消 → 9 + cancelled + 恢复读取 incomplete manifest ──
    def test_03_harness_cancel_recover_incomplete_manifest(self):
        out = os.path.join(self.tmp, "o3"); os.makedirs(out)
        cfg = self._cfg(out, [os.path.join(self.data, "light_1.fits")])
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="8000")
        p = subprocess.Popen([EXE, "phase1", "run", "--config", cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             env=env)
        time.sleep(0.5)
        p.send_signal(signal.SIGINT)
        out_s, err_s = p.communicate(timeout=30)
        self.assertEqual(p.returncode, 9, err_s[-200:])
        events = jsonl_lines(out_s)
        for i, ev in enumerate(events):
            err = harness_validate(ev, i)
            self.assertIsNone(err, f"cancel stream event[{i}]: {err}")
        fin = events[-1]
        self.assertEqual(fin["kind"], "final")
        self.assertEqual(fin["exit_code"], 9)
        self.assertEqual(fin["status"], "cancelled")
        mf = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"]
        self.assertTrue(mf, "取消必须留 manifest(恢复读取)")
        man = json.load(open(mf[-1]["path"], encoding="utf-8"))
        self.assertEqual(man["status"], "incomplete")
        self.assertEqual(man["run_id"], fin["run_id"])
        v = subprocess.run([EXE, "verify", "--run-manifest", mf[-1]["path"], "--json"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(v.returncode, 8, "incomplete manifest verify → 8")

    # ── 4. run directory 布局合同 ──
    def test_04_run_directory_layout(self):
        out = os.path.join(self.tmp, "o4"); os.makedirs(out)
        cfg = self._empty_cfg(out)
        r = subprocess.run([EXE, "phase1", "run", "--config", cfg, "--events-jsonl"],
                           capture_output=True, text=True, timeout=120)
        events = jsonl_lines(r.stdout)
        mf = [e for e in events if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        self.assertEqual(os.path.dirname(os.path.abspath(mf["path"])),
                         os.path.abspath(out), "manifest 必落 config.output_dir")
        self.assertRegex(os.path.basename(mf["path"]), r"^astrocs_run_[0-9a-f]{12}\.json$")
        fin = events[-1]
        self.assertIn(fin["run_id"], os.path.basename(mf["path"]),
                      "manifest 文件名含 run_id(harness 按 run_id 归档)")

    # ── 5. stdout/stderr 纪律 + 协议面负向样例 ──
    def test_05_negative_protocol_surface(self):
        out = os.path.join(self.tmp, "o5"); os.makedirs(out)
        cfg = self._empty_cfg(out)
        # 负向: 未知协议旗标混入 → 2, stdout 无输出(无污染)
        r = subprocess.run([EXE, "phase1", "run", "--config", cfg,
                            "--events-jsonl", "--json"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 2)
        self.assertEqual(r.stdout, "")
        self.assertIn("astrocs:", r.stderr)
        # 负向: 非 events 模式 stdout 是人类文本, 不混 JSONL 事件流
        r2 = subprocess.run([EXE, "phase1", "run", "--config", cfg],
                            capture_output=True, text=True, timeout=120)
        self.assertEqual(r2.returncode, 2)
        self.assertNotIn('"kind"', r2.stdout)
        self.assertRegex(os.path.basename(r2.stdout.strip()), r"^astrocs_run_[0-9a-f]{12}\.json$")

    # ── 6. CLI 无 Qt/HiPS Browser 链接(源码 + 动态依赖) ──
    def test_06_no_qt_hips_browser_links(self):
        for fn in os.listdir(CLI_DIR):
            if not fn.endswith((".cpp", ".h")):
                continue
            text = open(os.path.join(CLI_DIR, fn), encoding="utf-8").read()
            self.assertNotIn("#include <Q", text, f"{fn} 含 Qt 头")
            self.assertNotIn("hipsbrowser", text.lower(), f"{fn} 含 HiPS Browser 链接")
            self.assertNotIn("QApplication", text, f"{fn} 含 QApplication")
        cm = open(os.path.join(CLI_DIR, "CMakeLists.txt"), encoding="utf-8").read()
        self.assertNotIn("Qt5", cm); self.assertNotIn("Qt6", cm)
        if os.name == "posix" and shutil.which("ldd"):
            deps = subprocess.run(["ldd", EXE], capture_output=True, text=True, timeout=60)
            for line in deps.stdout.splitlines():
                self.assertNotIn("libQt", line, f"动态链接 Qt: {line.strip()}")

    # ── 7. 负向样例: 违反冻结合同的事件流必须被 harness 校验拒绝 ──
    def test_07_negative_stream_rejected_by_contract(self):
        base = {"schema_version": "1", "event_id": "evt-000000000000-0",
                "run_id": "000000000000", "timestamp_utc": "2026-09-02T00:00:00Z",
                "sequence": 0, "kind": "progress", "severity": "info", "phase": "phase1",
                "stage": "progress", "message": "m",
                "completed": 0, "total": 1, "unit": "phases", "rate": None,
                "eta_seconds": None}
        # 7a. 缺必含字段
        bad = dict(base); del bad["stage"]
        self.assertIsNotNone(harness_validate(bad, 0))
        # 7b. sequence 跳跃(禁单调性破坏)
        self.assertIsNotNone(harness_validate(base, 1))
        # 7c. progress 缺冻结扩展字段
        bad = dict(base); del bad["eta_seconds"]
        self.assertIsNotNone(harness_validate(bad, 0))
        # 7d. final.exit_code 越冻结 11 条域(04 §2)
        bad = dict(base, kind="final", exit_code=11, status="ok",
                   run_manifest=None, summary="s")
        self.assertIsNotNone(harness_validate(bad, 0))
        # 7e. run_id 格式坏
        bad = dict(base, run_id="xyz")
        self.assertIsNotNone(harness_validate(bad, 0))
        # 7f. 合法流必须通过(对照)
        self.assertIsNone(harness_validate(base, 0))

    # ── 8. schema 文件在库且与冻结合同一致(§4: schemas/jsonl_event_v1.schema.json) ──
    def test_08_schema_file_present_and_consistent(self):
        self.assertTrue(os.path.isfile(SCHEMA), "§4 schema 必须在库")
        schema = json.load(open(SCHEMA, encoding="utf-8"))
        self.assertEqual(set(schema["required"]), REQUIRED_FIELDS)
        for kind, ext in KIND_EXT.items():
            cond = [c for c in schema["allOf"]
                    if c["if"]["properties"]["kind"]["const"] == kind]
            self.assertTrue(cond, f"schema 缺 {kind} 条件分支")
            self.assertEqual(set(cond[0]["then"]["required"]), ext)
        fin_enum = schema["properties"]["exit_code"]["enum"]
        self.assertEqual(set(fin_enum), EXIT_DOMAIN)


if __name__ == "__main__":
    unittest.main(verbosity=2)
