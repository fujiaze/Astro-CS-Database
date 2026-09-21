#!/usr/bin/env python3
"""FIX-208 验收 1/2：运行事件流 = **默认输出**；运行事件流 schema **唯一**。

权威（逐条）：
  * ASTROCS_DESIGN.md §6.3（运行事件走 JSONL：schema_version/event_id/run_id/.../kind 含
    progress/resource/artifact/backend/final；stdout 无日志污染）；
  * GAP_AUDIT.md(RELEASE-03) §4.3 Q6 裁决：唯一运行事件流 schema =
    lib/infrastructure/cli/protocol.h（ValidateEventV1）+ jsonl.h（JsonlEmitter）；
    LOG-001（lib/infrastructure/observability/logging/**，STRUCTURED_LOGGING_CONTRACT）是
    **另一份**「结构化日志」合同（键名 schema/seq/ts/run/level/event），**不得互相冒充**；
  * GAP_AUDIT.md(RELEASE-02) §9.74 裁决 7-a 定案 1（事件流 = 默认输出，不需要旗标开启；
    GUI 用其它语言直接捕获 CLI 输出）。

方法（外部 harness 视角；独立重实现协议文本，不链接库内部）：
  * spawn astrocs normalize --json CFG -y（**不带任何事件旗标**）→ 逐行读 stdout；
  * 每行恰一个 JSON 对象；10 必含字段名逐字；sequence 从 0 单调无空洞（**唯一**顺序键）；
  * 负例注入第二种字段命名（LOG-001 键名 / 额外 seq / 缺 kind / 乱序）⇒ 判据必须判红。
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO)

from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402

# §4 冻结字段（与 lib/infrastructure/cli/protocol.h kEventFieldsV1 逐字同集；独立重实现）
RUN_REQUIRED = {"schema_version", "event_id", "run_id", "timestamp_utc", "sequence",
                "kind", "severity", "phase", "stage", "message"}
# §4 kind 扩展字段冻结名册（protocol.h missing_required_extension_v1 对偶实现）
KIND_EXT = {
    "progress": {"completed", "total", "unit", "rate", "eta_seconds"},
    "resource": {"cpu_cores_used", "rss_bytes", "io_read_bytes", "io_write_bytes", "threads"},
    "artifact": {"role", "path", "sha256", "size_bytes"},
    "backend": {"kernel", "backend_id", "isa", "workers", "block_size", "reason"},
    "final": {"exit_code", "status", "run_manifest", "summary"},
    # FIX-405 G3-10: 10 类开放 kind 全登记（封闭注册表；未登记 kind 判红）
    "stage_start": set(),
    "stage_end": set(),
    "graph": {"path"},
    "resource_gate": {"diag", "enforcement", "strict", "enforced", "work_core_seconds",
                      "workload_floor_core_seconds", "workload_floor_reached"},
    "v6_mode_route": {"route_kind", "token", "surface", "source", "reason",
                      "implicit_phase_chain", "budget_source_owner",
                      "budget_allocated_cores", "one_budget_source_rule"},
}
BASE_KINDS = {"progress", "resource", "artifact", "backend", "final"}
# §4 kind 注册表（封闭枚举，10 类）—— 实现正本 = lib/infrastructure/cli/protocol.h
# registered_event_kinds_v1()；机器 schema = eng/contracts/schemas/jsonl_event_v1.schema.json。
REGISTERED_KINDS = set(KIND_EXT)
SCHEMA_PATH = os.path.join(REPO, "contracts", "schemas", "jsonl_event_v1.schema.json")
PROTOCOL_H = os.path.join(REPO, "lib", "infrastructure", "cli", "protocol.h")


def protocol_h_registered_kinds():
    """从实现正本解析 §4 kind 注册表（跨源一致性用，避免只信 schema）。"""
    with open(PROTOCOL_H, encoding="utf-8") as fh:
        src = fh.read()
    block = src.split("registered_event_kinds_v1()", 1)[1].split("};", 1)[0]
    return set(re.findall(r'"([a-z0-9_]+)"', block))


def schema_registered_kinds():
    with open(SCHEMA_PATH, encoding="utf-8") as fh:
        schema = json.load(fh)
    return set(schema["properties"]["kind"]["enum"])
EXIT_DOMAIN = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 70}          # 04 §2 冻结 11 条
# Q6：LOG-001「结构化日志」的标识键 —— 与运行事件流同现即「两种字段命名混用」⇒ 判红。
# （phase 是两份合同共有的字段名，不作标识键；见 Q6「字段名/枚举唯一」。）
LOG001_IDENTIFYING = {"schema", "seq", "ts", "run", "level", "event", "units",
                      "elapsed", "diagnostic", "task", "node", "module", "commit", "host"}
TS_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
RUNID_RE = re.compile(r"^[0-9a-f]{12}$")
SUMMARY_RE = re.compile(r"^\[(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)\] seq=(\d+) (\S+) (\S+)")
MAX_LINE_BYTES = 4096


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    return os.path.join(REPO, "build", "astrocs")


EXE = cli_binary()


def validate_run_event(ev, expect_seq):
    """运行事件流唯一 schema 的独立校验（读侧对偶实现）。返回错误串；None = 通过。"""
    if not isinstance(ev, dict):
        return "event is not an object"
    mixed = LOG001_IDENTIFYING & set(ev)
    if mixed:
        return "two field namings mixed (LOG-001 keys present): %s" % sorted(mixed)
    missing = RUN_REQUIRED - set(ev)
    if missing:
        return "missing required field(s): %s" % sorted(missing)
    if ev["schema_version"] != "1":
        return "schema_version must be one"
    if not RUNID_RE.match(ev["run_id"]):
        return "run_id malformed: %r" % ev["run_id"]
    if not TS_RE.match(ev["timestamp_utc"]):
        return "timestamp_utc malformed: %r" % ev["timestamp_utc"]
    if not isinstance(ev["sequence"], int) or ev["sequence"] != expect_seq:
        return "sequence %r != expected %s (monotonic from 0; only order key)" % (
            ev.get("sequence"), expect_seq)
    if not ev["event_id"].startswith("evt-%s-" % ev["run_id"]):
        return "event_id not run-bound: %r" % ev["event_id"]
    # FIX-405 G3-10: kind 必须是注册表内成员（未登记 kind 判红，不再是开放字符串）
    if ev["kind"] not in REGISTERED_KINDS:
        return "kind %r is not in the §4 registry (%d kinds)" % (
            ev["kind"], len(REGISTERED_KINDS))
    ext = KIND_EXT.get(ev["kind"])
    if ext:
        miss = ext - set(ev)
        if miss:
            return "kind %r missing frozen extension field(s): %s" % (ev["kind"], sorted(miss))
    if ev["kind"] == "final":
        if ev["exit_code"] not in EXIT_DOMAIN:
            return "final.exit_code %r outside frozen 04 §2 domain" % ev["exit_code"]
    return None


def validate_stream(lines):
    """返回 (errors, events)。errors 空 = 整条流合唯一 schema。"""
    errors, events = [], []
    for i, raw in enumerate(lines):
        if not raw.strip():
            continue
        try:
            ev = json.loads(raw)
        except Exception as exc:                       # stdout 混入非 JSON = 日志污染
            errors.append("line %d is not JSON (%s): %r" % (i, exc, raw[:80]))
            continue
        err = validate_run_event(ev, len(events))
        if err:
            errors.append("line %d: %s" % (i, err))
        events.append(ev)
    return errors, events


class TestRunEventStreamDefault(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="fix208_stream_")
        cache = os.environ.get("ASTROCS_FIX208_FIXTURE_DIR")
        if cache and os.path.isfile(os.path.join(cache, "fixture")) and \
                os.path.isdir(os.path.join(cache, "data")):
            cls.fixture = os.path.join(cache, "fixture")
            cls.data = os.path.join(cache, "data")
        else:
            from tests.cli.test_cli004_process_protocol import build_fixture
            cls.fixture = build_fixture(cls.tmp)
            cls.data = os.path.join(cls.tmp, "data")
            os.makedirs(cls.data, exist_ok=True)
            r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True,
                               text=True, timeout=300, cwd=run_cwd())
            assert "FIXTURES_OK" in r.stdout, r.stderr
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        with open(cls.cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1",
                       "input_lights": [os.path.join(cls.data, "light_1.fits"),
                                        os.path.join(cls.data, "light_2.fits")],
                       "master_bias": os.path.join(cls.data, "bias.fits"),
                       "master_dark": os.path.join(cls.data, "dark.fits"),
                       "master_flat": os.path.join(cls.data, "flat.fits"),
                       "dark_optimization": True,
                       "output_dir": cls.out,
                       "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0,
                               "crval2": 34.0, "cd11": -2.7777777777777776e-4,
                               "cd12": 0.0, "cd21": 0.0, "cd22": 2.7777777777777776e-4},
                       "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0,
                                   "precision_mode": 0}}, fh)
        # 默认运行：**不带 --events-jsonl**（§9.74 裁决 7-a 定案 1）
        cls.res = subprocess.run([EXE, "normalize", "--json", cls.cfg, "-y"],
                                 capture_output=True, text=True, timeout=600, cwd=run_cwd())
        cls.stdout_lines = [l for l in cls.res.stdout.splitlines() if l.strip()]
        cls.errors, cls.events = validate_stream(cls.stdout_lines)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_default_stream_present_and_valid(self):
        """不带任何旗标 ⇒ 有事件流输出（默认行为），且整条流合唯一 schema。"""
        self.assertEqual(self.res.returncode, 0, self.res.stderr[-600:])
        self.assertTrue(self.stdout_lines, "默认运行必须在 stdout 发事件流（无需旗标）")
        self.assertEqual(self.errors, [], "事件流违反唯一 schema: %s" % self.errors[:3])
        kinds = {e["kind"] for e in self.events}
        self.assertTrue(BASE_KINDS <= kinds,
                        "基础五类 kind 必须齐备（缺 %s）" % sorted(BASE_KINDS - kinds))
        self.assertEqual(self.events[-1]["kind"], "final")
        self.assertEqual(self.events[-1]["exit_code"], 0)
        self.assertEqual([e["sequence"] for e in self.events],
                         list(range(len(self.events))),
                         "sequence 必须从 0 单调无空洞（唯一顺序键）")

    def test_06_kind_registry_consistent_and_unregistered_rejected(self):
        """FIX-405 G3-10: 10 类 kind 全登记（三源同面），未登记 kind 被拒。"""
        proto = protocol_h_registered_kinds()
        schema = schema_registered_kinds()
        # (a) 三源同面: 读侧名册 / 实现正本 protocol.h / 机器 schema enum
        self.assertEqual(len(REGISTERED_KINDS), 10, "§4 kind 注册表必须是 10 类")
        self.assertEqual(proto, REGISTERED_KINDS,
                         "protocol.h registered_event_kinds_v1 与 §4 名册不同面")
        self.assertEqual(schema, REGISTERED_KINDS, "schema kind enum 与 §4 名册不同面")
        # (b) 真跑观测到的每个 kind 都必须在册（本任务前 5 类之外无人校验）
        observed = {e["kind"] for e in self.events}
        self.assertTrue(observed <= REGISTERED_KINDS,
                        "真实事件流出现未登记 kind: %s" % sorted(observed - REGISTERED_KINDS))
        self.assertTrue(BASE_KINDS <= observed,
                        "基础五类必须齐备（缺 %s）" % sorted(BASE_KINDS - observed))
        # (c) 负例（能红）: 未登记 kind 必须判红 —— 两个方向都验
        base = dict(self.events[0])
        self.assertIsNone(validate_run_event(base, 0), "未注入时必须判绿")
        bogus = dict(base, kind="bogus_kind")
        self.assertIsNotNone(validate_run_event(bogus, 0),
                             "未登记 kind 必须判红（封闭注册表）")
        self.assertNotIn("bogus_kind", proto)
        self.assertNotIn("bogus_kind", schema)
        # (d) 已登记 kind 缺冻结扩展字段也必须判红（注册表不是免检牌）
        g = dict(base, kind="graph")          # graph 必须带 path
        self.assertIsNotNone(validate_run_event(g, 0), "graph 缺 path 必须判红")
        g_ok = dict(g, path="run_graphs/x.dot")
        self.assertIsNone(validate_run_event(g_ok, 0), "graph 带 path 必须判绿")
        # (e) 事件流 schema 的 allOf 分支与注册表一一对应
        with open(SCHEMA_PATH, encoding="utf-8") as fh:
            sch = json.load(fh)
        branches = {c["if"]["properties"]["kind"]["const"] for c in sch["allOf"]}
        self.assertEqual(branches, REGISTERED_KINDS,
                         "schema allOf 分支集 != kind 注册表")

    def test_02_negative_second_field_naming_is_red(self):
        """负例注入第二种字段命名 ⇒ 判红（能红能绿）。"""
        base = self.events[0]
        self.assertIsNone(validate_run_event(base, 0), "未注入时必须判绿")
        # (a) 改写成 LOG-001「结构化日志」键名（另一份合同的字段命名）
        renamed = dict(base)
        renamed["schema"] = renamed.pop("schema_version")
        renamed["run"] = renamed.pop("run_id")
        renamed["ts"] = renamed.pop("timestamp_utc")
        renamed["seq"] = renamed.pop("sequence")
        renamed["level"] = renamed.pop("severity")
        renamed["event"] = renamed.pop("kind")
        self.assertIsNotNone(validate_run_event(renamed, 0),
                             "LOG-001 字段命名必须判红（两种 schema 不得互相冒充）")
        # (b) 两种命名并存（同一行同时有 sequence 与 seq）⇒ 判红
        mixed = dict(base, seq=0)
        self.assertIsNotNone(validate_run_event(mixed, 0), "并存两种顺序键必须判红")
        # (c) 缺 kind（事件枚举键名唯一）⇒ 判红
        no_kind = dict(base)
        del no_kind["kind"]
        self.assertIsNotNone(validate_run_event(no_kind, 0), "缺 kind 必须判红")
        # (d) 顺序键被破坏（只认 sequence，不认墙钟/其它键）⇒ 判红
        self.assertIsNotNone(validate_run_event(dict(base), 1), "sequence 乱序必须判红")
        # (e) 整条流注入一行 LOG-001 行 ⇒ 整流判红
        log001_line = json.dumps({"schema": "astrocs.log.event.v1", "seq": 1,
                                  "ts": "2026-09-20T00:00:00Z", "run": "r",
                                  "level": "info", "event": "end", "diagnostic": "d"})
        errors, _ = validate_stream(self.stdout_lines + [log001_line])
        self.assertTrue(errors, "混入 LOG-001 行后整流必须判红")

    def test_03_dual_channel_same_source_and_no_path_leak(self):
        """人可读摘要（stderr）与机器 JSONL（stdout）同源；不泄露绝对路径。"""
        summaries = [l for l in self.res.stderr.splitlines() if SUMMARY_RE.match(l)]
        self.assertEqual(len(summaries), len(self.events),
                         "每个事件必须有且只有一条同源人可读摘要（stderr）")
        for ev, line in zip(self.events, summaries):
            m = SUMMARY_RE.match(line)
            self.assertEqual(int(m.group(2)), ev["sequence"], "摘要 seq 必须与事件一致")
            self.assertEqual(m.group(4), ev["kind"], "摘要 kind 必须与事件一致")
            self.assertEqual(m.group(1), ev["timestamp_utc"], "摘要 ts 必须与事件同源")
        # 绝对路径不得进入人可读通道（脱敏纪律；结构化字段仍保留真值供 GUI 使用）
        man = [e for e in self.events if e["kind"] == "artifact"
               and e.get("role") == "run_manifest"]
        self.assertTrue(man, "必须发 run_manifest artifact 事件")
        abs_path = man[-1]["path"]
        self.assertTrue(os.path.isabs(abs_path))
        self.assertNotIn(abs_path, self.res.stderr, "人可读通道不得泄露绝对路径")
        for e in self.events:
            for key in ("path", "run_manifest"):
                v = e.get(key)
                if isinstance(v, str) and os.path.isabs(v):
                    self.assertNotIn(v, self.res.stderr,
                                     "事件结构化路径不得出现在人可读通道: %s" % key)

    def test_04_line_size_limit_and_stdout_purity(self):
        """单行 ≤ 4096 字节；stdout 无日志污染（每行恰一个 JSON 对象）。"""
        for raw in self.stdout_lines:
            self.assertLessEqual(len(raw.encode("utf-8")), MAX_LINE_BYTES,
                                 "事件行超 4096 字节: %r" % raw[:80])
        for line in self.res.stderr.splitlines():
            self.assertLessEqual(len(line.encode("utf-8")), MAX_LINE_BYTES,
                                 "摘要行超 4096 字节: %r" % line[:80])
        self.assertNotIn("astrocs:", self.res.stdout, "stdout 不得混入人类诊断文本")

    def test_05_json_commands_single_document_no_events(self):
        """--json 机器输出面：stdout 恰一个 JSON 文档，且不发事件流。"""
        r = subprocess.run([EXE, "--version", "--json"], capture_output=True, text=True,
                           timeout=60, cwd=run_cwd())
        self.assertEqual(r.returncode, 0)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["name"], "astrocs")
        self.assertNotIn("sequence", r.stdout)
        r2 = subprocess.run([EXE, "doctor", "--json"], capture_output=True, text=True,
                            timeout=120, cwd=run_cwd())
        self.assertEqual(r2.returncode, 0)
        doc2 = json.loads(r2.stdout)
        self.assertEqual(doc2["kind"], "astrocs_doctor")
        self.assertNotIn("sequence", r2.stdout)


class TestJsonlHelpersUnit(unittest.TestCase):
    """jsonl.h 的双通道/脱敏/4096 上限：共址 C++ 探针（能红能绿）。"""

    @classmethod
    def setUpClass(cls):
        if not shutil.which("g++"):
            raise unittest.SkipTest("需要 g++")
        cls.tmp = tempfile.mkdtemp(prefix="fix208_jsonl_")
        src = os.path.join(cls.tmp, "probe.cpp")
        with open(src, "w", encoding="utf-8") as fh:
            fh.write(r"""
#include "jsonl.h"
#include <cstdio>
#include <string>
int main() {
    using namespace astrocs;
    // 脱敏（LOG-001 §5 同规则）: 绝对路径/盘符/UNC/凭据
    std::printf("r1=%s\n", redact_free_text("write /home/alice/x.fits ok").c_str());
    std::printf("r2=%s\n", redact_free_text("token=abc123").c_str());
    std::printf("r3=%s\n", redact_free_text("C:\\Users\\bob\\a.fits").c_str());
    std::printf("r4=%s\n", redact_free_text("share \\\\srv\\share\\t.fits").c_str());
    std::printf("r5=%s\n", redact_free_text("plain message").c_str());
    // 单行上限（4096 字节，UTF-8 边界，保持合法 JSON）
    nlohmann::json ev = {{"schema_version", "1"}, {"message", std::string(9000, 'x')},
                         {"sequence", 0}};
    const std::string line = fit_event_line(ev);
    std::printf("len=%zu\n", line.size());
    std::printf("json_ok=%d\n", nlohmann::json::accept(line) ? 1 : 0);
    // 人可读摘要与机器行同源（同一事件对象）
    std::printf("summary=%s", human_summary_of(ev).c_str());
    return 0;
}
""")
        cls.exe = os.path.join(cls.tmp, "probe")
        r = subprocess.run(["g++", "-std=c++17", "-O1", "-w",
                            "-I" + os.path.join(REPO, "lib", "infrastructure", "cli"),
                            "-I" + os.path.join(REPO, "third_party"),
                            src, "-o", cls.exe], capture_output=True, text=True, timeout=300)
        assert r.returncode == 0, r.stderr[-800:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_redact_and_size(self):
        r = subprocess.run([self.exe], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = dict(l.split("=", 1) for l in r.stdout.splitlines() if "=" in l)
        self.assertEqual(out["r1"], "write <redacted> ok")
        self.assertEqual(out["r2"], "<redacted>")
        self.assertEqual(out["r3"], "<redacted>")
        self.assertEqual(out["r4"], "share <redacted>")   # 前缀文本保留, UNC 段整体脱敏
        self.assertEqual(out["r5"], "plain message")           # 阴性对照: 普通文本不动
        self.assertLessEqual(int(out["len"]), MAX_LINE_BYTES)   # 4096 含换行
        self.assertEqual(out["json_ok"], "1", "截断后必须仍是合法 JSON")
        self.assertTrue(out["summary"].startswith("["))


if __name__ == "__main__":
    unittest.main(verbosity=2)
