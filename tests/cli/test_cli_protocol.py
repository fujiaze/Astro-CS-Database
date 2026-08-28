#!/usr/bin/env python3
"""CLI-002 golden 测试: 04 协议合同 — parser/JSONL/退出码映射/cancel/crash boundary/Unicode。"""
import json, os, re, shutil, signal, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")

HELP_LINES = [
    "astrocs --version [--json]",
    "astrocs hardware inspect --json",
    "astrocs config init --output <path>",
    "astrocs config validate --config <path>",
    "astrocs config show-effective --config <path> [--cpu-profile <path>] --json",
    "astrocs benchmark cpu (--quick|--full) [--output <path>]",
    "astrocs doctor --json",
    "astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>",
    "astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs phase2 run --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs phase3 run --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs run --phases <1|2|3|1,2|1,2,3> --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs verify --run-manifest <path> --json",
]

def built():
    if not os.path.isfile(EXE):
        subprocess.run(["cmake", "-S", CLI, "-B", BUILD], check=True, capture_output=True, timeout=120)
        subprocess.run(["cmake", "--build", BUILD, "-j2"], check=True, capture_output=True, timeout=300)
    return EXE

def run(*args, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    return subprocess.run([built(), *args], capture_output=True, text=True, timeout=60, env=e)

def jsonl_lines(stdout):
    return [json.loads(l) for l in stdout.splitlines() if l.strip()]

class TestGolden(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        built()
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_proto_")
        cls.cfg = os.path.join(cls.tmp, "pipeline_config.json")
        r = run("config", "init", "--output", cls.cfg)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── help/version ──
    def test_01_help_exact_tree(self):
        r = run("--help")
        self.assertEqual(r.returncode, 0)
        self.assertEqual([l for l in r.stdout.splitlines() if l.strip()], HELP_LINES)
        self.assertEqual(r.stderr, "")

    def test_02_version_json_schema(self):
        r = run("--version", "--json")
        self.assertEqual(r.returncode, 0)
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertEqual(len(lines), 1)
        doc = json.loads(lines[0])
        self.assertEqual(doc["name"], "astrocs")
        self.assertRegex(doc["version"], r"^0\.9\.0-alpha\.1\+g[0-9a-f]{12}(\.dirty)?$")

    # ── parser 拒绝面(全部 → 2, 诊断在 stderr) ──
    def test_03_parser_rejects(self):
        cases = [
            ("bogus",),
            ("config",),                                   # 不完整命令
            ("config", "init"),                            # 缺 --output
            ("config", "init", "--output"),                # 缺值
            ("config", "validate", "--config", "--json"),  # 旗标当值
            ("phase1", "run", "--config", self.cfg, "--bogus", "x"),  # 未知旗标
            ("phase1", "run", "--config", self.cfg, "--config", self.cfg),  # 重复
            ("phase1", "run", "--json"),                   # 旗标不属该命令
            ("test", "synthetic", "--group", "nope"),      # 枚举外
            ("benchmark", "cpu"),                          # quick/full 皆无
            ("benchmark", "cpu", "--quick", "--full"),     # quick/full 皆有
            ("run", "--phases", "2,1", "--config", self.cfg),   # 非升序
            ("run", "--phases", "1,1", "--config", self.cfg),   # 重复
            ("run", "--phases", "4", "--config", self.cfg),     # 越界
        ]
        for case in cases:
            r = run(*case)
            self.assertEqual(r.returncode, 2, f"{case} → 期望 2, 得 {r.returncode}")
            self.assertIn("astrocs:", r.stderr, f"{case} 缺 stderr 诊断")
            self.assertEqual(r.stdout, "", f"{case} stdout 应无输出(污染)")

    # ── config init/validate 真实现 ──
    def test_04_config_init_writes_valid_json(self):
        p = os.path.join(self.tmp, "u", "cfg.json")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        r = run("config", "init", "--output", p)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), p)
        with open(p, encoding="utf-8") as fh:
            doc = json.loads(fh.read())
        self.assertEqual(doc["schema_version"], "1")

    def test_05_config_validate_mapping(self):
        r = run("config", "validate", "--config", self.cfg)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), "config OK")
        missing = run("config", "validate", "--config", os.path.join(self.tmp, "nope.json"))
        self.assertEqual(missing.returncode, 3, "文件缺失 → 3(输入缺失)")
        bad = os.path.join(self.tmp, "bad.json")
        with open(bad, "w", encoding="utf-8") as fh:
            fh.write("{not json")
        malformed = run("config", "validate", "--config", bad)
        self.assertEqual(malformed.returncode, 3, "格式错 → 3")
        arr = os.path.join(self.tmp, "arr.json")
        with open(arr, "w", encoding="utf-8") as fh:
            fh.write("[1,2]")
        nonobj = run("config", "validate", "--config", arr)
        self.assertEqual(nonobj.returncode, 3, "非对象 → 3")

    # ── JSONL 协议 ──
    def test_06_jsonl_contract(self):
        r = run("phase1", "run", "--config", self.cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 2)  # not-wired stub
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["kind"], "final")
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))), "sequence 从 0 单调")
        for e in events:
            for f in ("schema_version", "event_id", "run_id", "timestamp_utc", "sequence",
                      "kind", "severity", "phase", "stage", "message"):
                self.assertIn(f, e, f"缺必含字段 {f}")
            self.assertRegex(e["timestamp_utc"], r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
        fin = events[-1]
        for f in ("exit_code", "status", "run_manifest", "summary"):
            self.assertIn(f, fin, f"final 缺扩展字段 {f}")
        # stdout 无日志污染: 每行都是 JSON
        for line in r.stdout.splitlines():
            json.loads(line)
        self.assertTrue(r.stderr.strip(), "诊断/日志必须在 stderr")

    # ── cancel → 9 ──
    def test_07_cancel_exit_9_no_fake_artifacts(self):
        env = {"ASTROCS_TEST_SLEEP_MS": "8000"}
        p = subprocess.Popen([built(), "phase2", "run", "--config", self.cfg, "--events-jsonl"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             env={**os.environ, **env})
        time.sleep(0.4)
        p.send_signal(signal.SIGINT if os.name != "nt" else signal.SIGINT)
        out, err = p.communicate(timeout=15)
        self.assertEqual(p.returncode, 9, f"取消 → 9, 得 {p.returncode}; stderr={err[:200]}")
        events = jsonl_lines(out)
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "cancelled")
        self.assertEqual(events[-1]["exit_code"], 9)
        self.assertIn("cancelled", err)

    # ── crash boundary → 70 ──
    def test_08_crash_boundary_70_sanitized(self):
        r = run("phase3", "run", "--config", self.cfg, "--events-jsonl",
                env={"ASTROCS_TEST_CRASH": "1"})
        self.assertEqual(r.returncode, 70, "未捕获异常 → 70")
        self.assertIn("CRASH", r.stderr)
        self.assertRegex(r.stderr, r"run_id=[0-9a-f]{12}")
        self.assertIn("command='phase3 run'", r.stderr)
        self.assertIn("no credentials", r.stderr)
        self.assertNotIn(self.cfg, r.stderr, "crash report 不得含完整路径外泄")
        # 非 events 模式同样 70
        r2 = run("verify", "--run-manifest", self.cfg, env={"ASTROCS_TEST_CRASH": "1"})
        self.assertEqual(r2.returncode, 70)

    # ── Unicode 路径 ──
    def test_09_unicode_path(self):
        uni = os.path.join(self.tmp, "配置_β_test.json")
        r0 = run("config", "init", "--output", uni)
        self.assertEqual(r0.returncode, 0, r0.stderr)
        r = run("config", "validate", "--config", uni)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), "config OK")

    # ── 退出码单源(04 §6-3) ──
    def test_10_exit_codes_single_source(self):
        hits = []
        for fn in os.listdir(CLI):
            if fn.endswith((".cpp", ".h")) and fn != "exit_codes.h":
                with open(os.path.join(CLI, fn), encoding="utf-8") as fh:
                    text = fh.read()
                if re.search(r"=\s*(70|10)\s*[,;/)]", text) or "ARGS  = 2" in text:
                    hits.append(fn)
        self.assertEqual(hits, [], f"退出码数值表泄漏到: {hits}")
        with open(os.path.join(CLI, "exit_codes.h"), encoding="utf-8") as fh:
            self.assertIn("INTERNAL      = 70", fh.read())

if __name__ == "__main__":
    unittest.main(verbosity=2)
