#!/usr/bin/env python3
"""CLI 进程协议验收: GUI 可调用 JSON stream protocol — 外部 harness 视角。

权威: docs/api/CLI_PROTOCOL_V1.md §3/§4/§5 + lib/infrastructure/cli/protocol.h 生产侧硬闸。
方法(independent, 模拟外部 harness/GUI, 不调用库内部):
  - spawn 'astrocs normalize --json <cfg> --events-jsonl -y' 子进程, 流式逐行读 stdout;
  - 每行恰一个 UTF-8 JSON 事件(stdout 纪律), 独立重实现协议合同校验(防生产侧同源盲区);
  - 取消: SIGINT → exit 9, 不落 complete manifest;
  - run directory: manifest 落 config.output_dir, 文件名 astrocs_run_<run_id>.json;
  - 无 Qt/HiPS Browser 链接(源码 + 动态依赖双查)。
依赖: CLI 已构建(build/astrocs; ASTROCS_CLI_BIN 可覆盖)。phase1 fixture 同
test_phase1_inprocess 编译模式; fixture 源码路径用 ARCH-001 迁移后布局
(lib/infrastructure/aio), 旧路径回退。

退役登记（依据 §6.2 唯一命令树 + CLI-001 rc 矩阵）:
  * test_02/test_03 内 'verify --run-manifest' 断言 → 退役: verify 命令删除;
    哈希链复算新载体是 export resume 预检, 当前被 export 预检/会话口径冲突阻塞
    （TEST-CLI-SYNC 报告, 归属 CLI-002）;
  * test_01 空 lights 的旧期望(rc=2 Runtime INVALID + 事件流) → 改写: 新树空输入在
    预检即阻断（rc=2, 无事件流, fail-closed）; 「失败路径仍须发完整合规事件流」改用
    「非空但文件缺失」场景（rc=3）证明;
  * test_03 旧期望 final(status=cancelled) + 事件流 → 改写: 新子命令层的确定性取消窗
    （ASTROCS_TEST_SLEEP_MS）位于会话启动之前; FIX-406 起该窗内取消也发**恰一个 final
    事件**（status=cancelled, exit_code=9, run_manifest=null —— 本次运行尚未建立
    output_dir/run_context，不造假清单），机器侧不再靠空事件流猜状态。断言: rc=9 +
    事件流合规（harness_validate 逐事件）+ 不落 complete manifest。
"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# ROOT-008: CLI 源目录已迁 lib/infrastructure/cli/（旧 REPO/cli 不存在 ⇒ test_06 自迁移起
# 恒 FileNotFoundError）。FIX-208 顺带订正扫描路径（扫描面/断言不变），并保留旧路径回退。
CLI_DIR = next((p for p in (os.path.join(REPO, "lib", "infrastructure", "cli"),
                            os.path.join(REPO, "lib", "infrastructure", "cli")) if os.path.isdir(p)),
               os.path.join(REPO, "lib", "infrastructure", "cli"))

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402
SCHEMA = os.path.join(REPO, "eng", "contracts", "schemas", "jsonl_event_v1.schema.json")


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
AIO = next((p for p in (os.path.join(REPO, "lib", "infrastructure", "aio"),
                        os.path.join(REPO, "lib", "astro_image_io")) if os.path.isdir(p)),
           os.path.join(REPO, "lib", "infrastructure", "aio"))

# §4 冻结 kind 扩展字段(独立重实现 — 与 lib/infrastructure/cli/protocol.h 生产侧互为对偶)
REQUIRED_FIELDS = {"schema_version", "event_id", "run_id", "timestamp_utc", "sequence",
                   "kind", "severity", "phase", "stage", "message"}
KIND_EXT = {
    "progress": {"completed", "total", "unit", "rate", "eta_seconds"},
    "resource": {"cpu_cores_used", "rss_bytes", "io_read_bytes", "io_write_bytes", "threads"},
    "artifact": {"role", "path", "sha256", "size_bytes"},
    "backend": {"kernel", "backend_id", "isa", "workers", "block_size", "reason"},
    "final": {"exit_code", "status", "run_manifest", "summary"},
    # FIX-405 G3-10: 10 类开放 kind 全登记（原 5 类之外补齐 5 类）
    "stage_start": set(),
    "stage_end": set(),
    "graph": {"path"},
    # SO-05 记录/裁决分离证据面（B4 登记；语义 = docs/plugins/infrastructure/
    # 21_observability.md §8.4）
    "resource_gate": {"diag", "enforcement", "strict", "enforced", "work_core_seconds",
                      "workload_floor_core_seconds", "workload_floor_reached",
                      "so05_signoff_id", "so05_signoff_status",
                      "auto_adjudication_allowed"},
    "v6_mode_route": {"route_kind", "token", "surface", "source", "reason",
                      "implicit_phase_chain", "budget_source_owner",
                      "budget_allocated_cores", "one_budget_source_rule"},
}
# §4 kind 注册表（10 类，封闭枚举；实现正本 = lib/infrastructure/cli/protocol.h
# registered_event_kinds_v1()）。未登记 kind 被拒。
REGISTERED_KINDS = set(KIND_EXT)
PROTOCOL_H = os.path.join(REPO, "lib", "infrastructure", "cli", "protocol.h")
EXIT_DOMAIN = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 70}   # 04 §2 冻结 11 条
TS_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
RUNID_RE = re.compile(r"^[0-9a-f]{12}$")


def harness_validate(ev, expect_seq):
    """外部 harness 侧协议校验(读侧, 独立于 lib/infrastructure/cli/protocol.h)。返回错误串, None=通过。"""
    if not isinstance(ev, dict):
        return "event is not an object"
    missing = REQUIRED_FIELDS - set(ev)
    if missing:
        return "missing required field(s): %s" % sorted(missing)
    if not RUNID_RE.match(ev["run_id"]):
        return "run_id malformed: %r" % ev["run_id"]
    if not TS_RE.match(ev["timestamp_utc"]):
        return "timestamp_utc malformed: %r" % ev["timestamp_utc"]
    if ev["sequence"] != expect_seq:
        return "sequence %s != expected %s (monotonic from 0)" % (ev["sequence"], expect_seq)
    if not ev["event_id"].startswith("evt-%s-" % ev["run_id"]):
        return "event_id not run-bound: %r" % ev["event_id"]
    ext = KIND_EXT.get(ev["kind"])
    if ext:
        miss = ext - set(ev)
        if miss:
            return "kind %r missing frozen extension field(s): %s" % (ev["kind"], sorted(miss))
    if ev["kind"] == "final":
        if ev["exit_code"] not in EXIT_DOMAIN:
            return "final.exit_code %s outside frozen 04 §2 domain" % ev["exit_code"]
        if ev["exit_code"] < 2 and ev["status"] in ("cancelled",):
            return "final exit_code %s inconsistent with status" % ev["exit_code"]
    return None


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


def build_fixture(tmp):
    """phase1 fixture(同 test_phase1_inprocess 模式)。

    不做 /tmp 缓存: fixture 与迁移中的 lib 源码同步, 缓存会复用旧路径编译的陈旧二进制。
    """
    exe = os.path.join(tmp, "fixture")
    cdir = os.path.join(AIO, "third_party", "cfitsio")
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
                        f"-I{os.path.join(REPO, 'lib', 'include')}",
                        f"-I{os.path.join(AIO, 'include')}",
                        f"-I{os.path.join(AIO, 'src')}",
                        f"-I{cdir}",
                        os.path.join(REPO, "eng", "tests", "backend", "phase1_fixture_main.cpp"),
                        os.path.join(AIO, "src", "aio_fits.cpp"),
                        os.path.join(AIO, "src", "aio_api.cpp"),
                        os.path.join(AIO, "src", "aio_log.cpp"),
                        os.path.join(AIO, "src", "aio_compressor.cpp"),
                        *objs, "-lz", "-lzstd", "-llz4", "-o", exe],
                       capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr[-800:]
    return exe


class TestCli004ProcessProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="cli004_")
        cls.fixture = build_fixture(cls.tmp)
        cls.data = os.path.join(cls.tmp, "data")
        os.makedirs(cls.data)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True,
                           text=True, timeout=120, cwd=run_cwd())
        assert "FIXTURES_OK" in r.stdout, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _cfg(self, out, lights):
        # FIX-E2E B1-A1/A9: phase1 正式链为 8 节点端口链, drizzle/wcs 为链上必填科学配置。
        # 新树扁平会话形态（input_lights 在顶层; 预检按键名计数）。
        cfg = os.path.join(self.tmp, "cfg_%s.json" % os.path.basename(out))
        # SMOKE-001 D4: normalize 预检对缺失标定帧判 error（§3.5，仅 -force 可越），
        # 故配置须显式给三个 master（fixture --make 真实产出 bias/dark/flat）。
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1",
                       "input_lights": lights,
                       "master_bias": os.path.join(self.data, "bias.fits"),
                       "master_dark": os.path.join(self.data, "dark.fits"),
                       "master_flat": os.path.join(self.data, "flat.fits"),
                       # BIAS-001: 夹具 dark.fits 含 bias ⇒ 显式声明兼容式标定。
                       "dark_optimization": True,
                       "output_dir": out,
                       "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0,
                               "crval2": 34.0, "cd11": -2.7777777777777776e-4,
                               "cd12": 0.0, "cd21": 0.0, "cd22": 2.7777777777777776e-4},
                       "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0,
                                   "precision_mode": 0}}, fh)
        return cfg

    def _empty_cfg(self, out):
        cfg = os.path.join(self.tmp, "cfg_empty_%s.json" % os.path.basename(out))
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "input_lights": [], "output_dir": out}, fh)
        return cfg

    # ── 1. 外部 harness: 预检阻断(无流) + 失败路径仍发完整合规流 ──
    def test_01_harness_stream_contract_failure_path(self):
        # 1a. 空输入 → 预检阻断: rc=2, stdout 无事件（§3 stdout 纪律: 无污染）
        out0 = os.path.join(self.tmp, "o1a"); os.makedirs(out0)
        r0 = subprocess.run([EXE, "normalize", "--json", self._empty_cfg(out0),
                             "--events-jsonl", "-y"],
                            capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertEqual(r0.returncode, 2, "空输入必须预检阻断")
        self.assertEqual(r0.stdout, "", "阻断路径 stdout 不得有非 JSON 文本")
        self.assertEqual([f for f in os.listdir(out0) if f.startswith("astrocs_run_")], [],
                         "预检阻断不得写 manifest")
        # 1b. 输入文件缺失 → 预检 rc=3（写盘前阻断）：无事件流、无 manifest。
        #     2026-09-18 预检 fail-closed 修复后，路径不存在/不可读在 precheck_config
        #     阶段即判 error（ASTROCS_DESIGN §3.5 + ENGINEERING_SPEC:122），-y 不可越；
        #     旧断言（完整事件流 + incomplete manifest）固化的是修复前 fail-open 行为。
        out = os.path.join(self.tmp, "o1"); os.makedirs(out)
        cfg = self._cfg(out, [os.path.join(self.data, "does_not_exist.fits")])
        r1 = subprocess.run([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                            capture_output=True, text=True, cwd=run_cwd(), timeout=120)
        self.assertEqual(r1.returncode, 3, "输入文件缺失 → 3(INPUT)")
        self.assertEqual(r1.stdout, "",
                         "预检阻断不得发运行期事件流（stdout 纪律）")
        self.assertEqual([f for f in os.listdir(out) if f.startswith("astrocs_run_")], [],
                         "预检阻断不得写 manifest")
        self.assertTrue(r1.stderr.strip(), "诊断/日志必须在 stderr")
        self.assertIn("missing/unreadable input path", r1.stderr,
                      "必须点名输入路径缺失原因（不许静默）")

    # ── 2. 外部 harness: OK 路径 resource/backend/artifact 冻结扩展字段 ──
    def test_02_harness_ok_run_frozen_extension_fields(self):
        out = os.path.join(self.tmp, "o2"); os.makedirs(out)
        cfg = self._cfg(out, [os.path.join(self.data, "light_1.fits"),
                              os.path.join(self.data, "light_2.fits")])
        r = subprocess.run([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                           capture_output=True, text=True, timeout=300, cwd=run_cwd())
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = jsonl_lines(r.stdout)
        for i, ev in enumerate(events):
            err = harness_validate(ev, i)
            self.assertIsNone(err, "event[%d] protocol violation: %s" % (i, err))
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
        with open(mf["path"], encoding="utf-8") as fh:
            man = json.load(fh)
        self.assertEqual(man["status"], "complete")
        self.assertEqual(man["run_id"], fin["run_id"])

    # ── 3. 外部 harness: 取消 → 9 + 不落 complete manifest ──
    def test_03_harness_cancel_no_complete_manifest(self):
        out = os.path.join(self.tmp, "o3"); os.makedirs(out)
        cfg = self._cfg(out, [os.path.join(self.data, "light_1.fits")])
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="8000")
        p = subprocess.Popen([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             cwd=run_cwd(), env=env)
        time.sleep(0.5)
        p.send_signal(signal.SIGINT)
        out_s, err_s = p.communicate(timeout=30)
        self.assertEqual(p.returncode, 9, err_s[-200:])
        self.assertIn("cancel", err_s)
        for i, ev in enumerate(jsonl_lines(out_s)):
            self.assertIsNone(harness_validate(ev, i), "cancel stream event[%d]" % i)
        for fn in os.listdir(out):
            if fn.startswith("astrocs_run_"):
                with open(os.path.join(out, fn), encoding="utf-8") as fh:
                    man = json.load(fh)
                self.assertNotEqual(man["status"], "complete",
                                    "取消不得留看似完整的 manifest")

    # ── 4. run directory 布局合同 ──
    def test_04_run_directory_layout(self):
        # FIX-208: 预检 fail-closed 后「输入缺失」在预检即阻断（无事件、无 manifest）
        # ⇒ 布局合同必须用**真实输入**证明（旧写法用 does_not_exist.fits，自预检修复起
        # 恒无事件 → IndexError；本次一并改正）。
        out = os.path.join(self.tmp, "o4"); os.makedirs(out)
        cfg = self._cfg(out, [os.path.join(self.data, "light_1.fits"),
                              os.path.join(self.data, "light_2.fits")])
        r = subprocess.run([EXE, "normalize", "--json", cfg, "-y"],
                           capture_output=True, text=True, timeout=300, cwd=run_cwd())
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
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
        cfg = self._cfg(out, [os.path.join(self.data, "does_not_exist.fits")])
        # 负向: 旧旗标混入 → 2, stdout 无输出(无污染)
        r = subprocess.run([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y",
                            "--config", cfg],
                           capture_output=True, text=True, timeout=60, cwd=run_cwd())
        self.assertEqual(r.returncode, 2)
        self.assertEqual(r.stdout, "")
        self.assertIn("astrocs:", r.stderr)
        # 负向: 运行/模板互斥 → 2
        r_mutex = subprocess.run([EXE, "normalize", "--json", cfg, "--template"],
                                 capture_output=True, text=True, timeout=60, cwd=run_cwd())
        self.assertEqual(r_mutex.returncode, 2)
        # FIX-208（§9.74 裁决 7-a 定案 1）: 事件流 = **默认输出** ⇒ stdout 恒为纯 JSONL
        # 或空；人类摘要只走 stderr（不再有「非 events 模式 stdout 打 manifest 路径」）。
        r2 = subprocess.run([EXE, "normalize", "--json", cfg, "-y"],
                            capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertEqual(r2.returncode, 3)
        # 预检阻断（输入缺失）⇒ 无事件、无 manifest（fail-closed，不落看似完整的产物）
        self.assertEqual(r2.stdout, "", "预检阻断不得在 stdout 打印任何文本/事件")
        self.assertIn("astrocs:", r2.stderr)
        self.assertEqual([f for f in os.listdir(out) if f.startswith("astrocs_run_")], [],
                         "预检阻断不得写 run manifest")

    # ── 6. CLI 无 Qt/HiPS Browser 链接(源码 + 动态依赖) ──
    def test_06_no_qt_hips_browser_links(self):
        for fn in os.listdir(CLI_DIR):
            if not fn.endswith((".cpp", ".h")):
                continue
            with open(os.path.join(CLI_DIR, fn), encoding="utf-8") as fh:
                text = fh.read()
            self.assertNotIn("#include <Q", text, "%s 含 Qt 头" % fn)
            self.assertNotIn("hipsbrowser", text.lower(), "%s 含 HiPS Browser 链接" % fn)
            self.assertNotIn("QApplication", text, "%s 含 QApplication" % fn)
        with open(os.path.join(CLI_DIR, "CMakeLists.txt"), encoding="utf-8") as fh:
            cm = fh.read()
        self.assertNotIn("Qt5", cm)
        self.assertNotIn("Qt6", cm)
        if os.name == "posix" and shutil.which("ldd"):
            deps = subprocess.run(["ldd", EXE], capture_output=True, text=True, timeout=60)
            for line in deps.stdout.splitlines():
                self.assertNotIn("libQt", line, "动态链接 Qt: %s" % line.strip())

    # ── 7. 负向样例: 违反冻结合同的事件流必须被 harness 校验拒绝 ──
    def test_07_negative_stream_rejected_by_contract(self):
        base = {"schema_version": "1", "event_id": "evt-000000000000-0",
                "run_id": "000000000000", "timestamp_utc": "2026-09-02T00:00:00Z",
                "sequence": 0, "kind": "progress", "severity": "info", "phase": "normalize",
                "stage": "progress", "message": "m",
                "completed": 0, "total": 1, "unit": "phases", "rate": None,
                "eta_seconds": None}
        bad = dict(base); del bad["stage"]
        self.assertIsNotNone(harness_validate(bad, 0))
        self.assertIsNotNone(harness_validate(base, 1))
        bad = dict(base); del bad["eta_seconds"]
        self.assertIsNotNone(harness_validate(bad, 0))
        bad = dict(base, kind="final", exit_code=11, status="ok",
                   run_manifest=None, summary="s")
        self.assertIsNotNone(harness_validate(bad, 0))
        bad = dict(base, run_id="xyz")
        self.assertIsNotNone(harness_validate(bad, 0))
        self.assertIsNone(harness_validate(base, 0))

    # ── 8. schema 文件在库且与冻结合同一致(§4: schemas/jsonl_event_v1.schema.json) ──
    def test_08_schema_file_present_and_consistent(self):
        self.assertTrue(os.path.isfile(SCHEMA), "§4 schema 必须在库")
        with open(SCHEMA, encoding="utf-8") as fh:
            schema = json.load(fh)
        self.assertEqual(set(schema["required"]), REQUIRED_FIELDS)
        for kind, ext in KIND_EXT.items():
            cond = [c for c in schema["allOf"]
                    if c["if"]["properties"]["kind"]["const"] == kind]
            self.assertTrue(cond, "schema 缺 %s 条件分支" % kind)
            self.assertEqual(set(cond[0]["then"]["required"]), ext)
        fin_enum = schema["properties"]["exit_code"]["enum"]
        self.assertEqual(set(fin_enum), EXIT_DOMAIN)

    # ── 9. FIX-405 G3-10: kind 注册表封闭（10 类全登记；未登记 kind 被拒） ──
    def test_09_kind_registry_closed_and_unregistered_rejected(self):
        with open(SCHEMA, encoding="utf-8") as fh:
            schema = json.load(fh)
        enum = schema["properties"]["kind"]["enum"]
        # (a) 10 类全登记，且与读侧独立重实现（KIND_EXT）同面
        self.assertEqual(len(enum), 10, "§4 kind 注册表必须是 10 类")
        self.assertEqual(set(enum), REGISTERED_KINDS,
                         "schema kind enum 与 §4 冻结名册不同面")
        # (b) 每个登记 kind 恰有一个 allOf 分支，其 required 与扩展字段集逐字一致
        branches = {c["if"]["properties"]["kind"]["const"]: set(c["then"]["required"])
                    for c in schema["allOf"]}
        self.assertEqual(set(branches), REGISTERED_KINDS,
                         "schema allOf 分支集 != kind 注册表")
        for kind, ext in KIND_EXT.items():
            self.assertEqual(branches[kind], ext, "%s 扩展字段集不同面" % kind)
        # (c) 机器注册表块与 enum 同面（防第二份定义漂移）
        reg = schema["x-astrocs-event-kind-registry"]
        self.assertEqual(set(reg["kinds"]), REGISTERED_KINDS)
        for kind, ext in KIND_EXT.items():
            self.assertEqual(set(reg["kinds"][kind]), ext)
        # (d) 实现正本 protocol.h 的注册表与 schema enum 同面（跨源一致性）
        with open(PROTOCOL_H, encoding="utf-8") as fh:
            src = fh.read()
        block = src.split("registered_event_kinds_v1()", 1)[1]
        block = block.split("};", 1)[0]
        proto_kinds = set(re.findall(r'"([a-z0-9_]+)"', block))
        self.assertEqual(proto_kinds, REGISTERED_KINDS,
                         "protocol.h registered_event_kinds_v1 与 schema enum 不同面")
        # (e) 负例：未登记 kind 不在 enum 内 ⇒ 判据可红（不是恒真门）
        self.assertNotIn("bogus_kind", enum)
        self.assertNotIn("bogus_kind", proto_kinds)


if __name__ == "__main__":
    unittest.main(verbosity=2)
