#!/usr/bin/env python3
"""CLI golden 测试（按 CLI-001 §6.2 新树同步）: help 树/版本 JSON/parser 拒绝面/模板/
配置错误映射/stdout 纪律/crash boundary 70/Unicode/退出码单源 + incomplete manifest。

权威: ASTROCS_DESIGN §6.2（唯一命令树）、§6.3（stdout 纪律 + 退出码表）、§3.5（预检阻断）、
docs/api/CLI_PROTOCOL_V1.md §1-§3。

退役登记（旧命令面已被 CLI-001 删除，依据 §6.2 + CLI-001 rc 矩阵；原用例前提=命令存在）:
  * test_04_config_init_writes_valid_json  → 改写为 --template -o 写合法 JSON（同意图: 模板即完整可运行配置）;
  * test_05_config_validate_mapping        → 改写为运行入口的配置错误映射（文件缺失/坏 JSON/非对象 → 3）;
  * test_06_jsonl_contract（phase1 run 全程事件）→ 事件流全字段/单调 sequence 断言迁往
    tests/cli/test_phase1_inprocess.py（真实会话）; 本文件保留「阻断路径 stdout 无污染」;
  * test_07_cancel_exit_9_no_fake_artifacts → 取消语义迁往 tests/cli/test_phase{1,2,3}_inprocess.py
    （同一 ASTROCS_TEST_SLEEP_MS 钩子, 真实会话）;
  * test_08_crash_boundary_70_sanitized: test synthetic 已删除 → 改由 normalize + 生产
    ASTROCS_TEST_CRASH 钩子（subcommand.run 内）触发, 断言 70 + 脱敏 crash report;
  * test_09_unicode_path: config init/validate 已删除 → 改为 --template -o 与 --json 的
    非 ASCII 路径解析;
  * TestManifestVerify.test_01..test_07（config validate / show-effective / verify-profile /
    verify 闭环）→ **退役**: config */verify*/benchmark verify-profile 均不在 §6.2 命令树;
    其仍有效的数据面断言（incomplete manifest 字段/hash 链）见本文件 TestManifestIncomplete
    与 tests/cli/test_phase{1,2}_inprocess.py 的 resume 断言;
  * TestManifestVerify.test_08 的 manifest 字段断言 → 改写保留（本文件 TestManifestIncomplete）;
    test_09 verify 闭环 → 退役（verify 命令删除; 哈希链新载体是 export resume 预检,
    当前被 export 预检/会话口径冲突阻塞, 见 TEST-CLI-SYNC 报告）。
"""
import hashlib, json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# ROOT-008 迁移后 CLI 命令层在 lib/infrastructure/cli/（cli/ 只剩 compatibility 声明）。
CLI = os.path.join(REPO, "lib", "infrastructure", "cli")

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402

HELP_LINES = [
    "astrocs --version [--json]",
    "astrocs normalize (--json <config.json> | --template [-o <path>] | --help)",
    "astrocs mosaic (--json <config.json> | --template [-o <path>] | --help)",
    "astrocs export (--json <config.json> | --template [-o <path>] | --help)",
    "astrocs help",
    "astrocs doctor [--json]",
    "astrocs benchmark",
]


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


def _repo_version():
    """版本单源: 根 VERSION 文件（根 CMakeLists 与 tools/gen_version.py 同源读取）。"""
    with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as fh:
        return fh.read().strip()


def run(*args, env=None, timeout=90):
    e = dict(os.environ)
    if env:
        e.update(env)
    return subprocess.run([EXE, *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout,
                          cwd=run_cwd(), env=e)


class TestGolden(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_proto_")
        cls.cfg = os.path.join(cls.tmp, "cfg_valid.json")
        with open(cls.cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "input_lights": [], "output_dir": cls.tmp}, fh)

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
        self.assertEqual(doc["schema_version"], "1")
        self.assertRegex(doc["version"],
                         r"^" + re.escape(_repo_version()) + r"\+g[0-9a-f]{12,40}(\.dirty)?$")

    # ── parser 拒绝面（全部 → 2, 诊断在 stderr, stdout 无污染） ──
    def test_03_parser_rejects(self):
        cases = [
            ("bogus",),
            ("normalize",),                                  # 缺动作
            ("normalize", "--json"),                         # 取值旗标缺值
            ("normalize", "--json", self.cfg, "--bogus"),    # 未知旗标
            ("normalize", "--json", self.cfg, "--template"),  # 运行/模板互斥
            ("mosaic",), ("export",), ("doctor",),           # doctor 只登记 --json
            ("benchmark", "cpu"),                            # 旧子命令
            ("benchmark", "verify-profile", self.cfg),       # 旧子命令
            ("hardware", "inspect", "--json"),               # 旧命令
            ("config", "init", "--output", os.path.join(self.tmp, "x.json")),
            ("test", "synthetic", "--group", "calibration"),
            ("selftest", "--json"),
            ("verify", "--run-manifest", self.cfg, "--json"),
            ("drizzle",),
            ("phase1", "run", "--config", self.cfg),
            ("run", "--phases", "2,1", "--config", self.cfg),
            ("graph", "--preset", "1,2,3"),
        ]
        for case in cases:
            r = run(*case)
            self.assertEqual(r.returncode, 2, "%s → 期望 2, 得 %s" % (case, r.returncode))
            self.assertIn("astrocs:", r.stderr, "%s 缺 stderr 诊断" % (case,))
            self.assertEqual(r.stdout, "", "%s stdout 应无输出(污染)" % (case,))

    # ── 模板 = 完整可运行 JSON（原 config init 的等价面） ──
    def test_04_template_writes_valid_json(self):
        p = os.path.join(self.tmp, "tpl", "norm.json")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        r = run("normalize", "--template", "-o", p)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, "", "-o 落盘时 stdout 不重复输出")
        with open(p, encoding="utf-8") as fh:
            doc = json.loads(fh.read())
        self.assertEqual(doc["schema_version"], "1")
        self.assertIn("output_dir", doc)
        # 三命令模板互不相同（各自独立产品）
        outs = {}
        for cmd in ("normalize", "mosaic", "export"):
            rr = run(cmd, "--template")
            self.assertEqual(rr.returncode, 0, rr.stderr)
            outs[cmd] = json.loads(rr.stdout)
        self.assertEqual(len({json.dumps(v, sort_keys=True) for v in outs.values()}), 3)

    # ── 配置错误映射（运行入口, 非空输入以越过预检到达会话校验） ──
    def test_05_config_error_mapping(self):
        def cfg(name, doc):
            p = os.path.join(self.tmp, name)
            with open(p, "w", encoding="utf-8") as fh:
                json.dump(doc, fh)
            return p

        base = {"schema_version": "1", "input_lights": ["/nonexistent/x.fits"],
                "output_dir": self.tmp}
        missing = run("normalize", "--json", os.path.join(self.tmp, "nope.json"), "-y")
        self.assertEqual(missing.returncode, 3, "文件缺失 → 3(输入缺失)")
        bad = os.path.join(self.tmp, "bad.json")
        with open(bad, "w", encoding="utf-8") as fh:
            fh.write("{not json")
        self.assertEqual(run("normalize", "--json", bad, "-y").returncode, 3, "格式错 → 3")
        arr = os.path.join(self.tmp, "arr.json")
        with open(arr, "w", encoding="utf-8") as fh:
            fh.write("[1,2]")
        self.assertEqual(run("normalize", "--json", arr, "-y").returncode, 3, "非对象 → 3")
        unk = run("normalize", "--json", cfg("unknown.json", dict(base, backend="x")), "-y")
        self.assertEqual(unk.returncode, 3, "白名单外键 → 3(防拼写静默忽略)")
        self.assertIn("unknown key", unk.stderr)
        sv = run("normalize", "--json", cfg("sv2.json", dict(base, schema_version="2")), "-y")
        self.assertEqual(sv.returncode, 2, "schema_version 非法 → 2(配置错, 与输入缺失 3 区分)")
        nodir = run("normalize", "--json", cfg("nodir.json",
                                               {"schema_version": "1",
                                                "input_lights": ["/nonexistent/x.fits"]}), "-y")
        self.assertEqual(nodir.returncode, 2, "缺 output_dir → 2(预检阻断, 无 silent default)")
        self.assertIn("output_dir", nodir.stderr)

    # ── 预检阻断: stdout 无污染 + 不落 complete ──
    def test_06_blocked_preflight_stdout_purity(self):
        out = os.path.join(self.tmp, "blocked_out")
        os.makedirs(out, exist_ok=True)
        cfg = os.path.join(self.tmp, "empty.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "input_lights": [], "output_dir": out}, fh)
        r = run("normalize", "--json", cfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 2, "空输入必须预检阻断")
        self.assertEqual(r.stdout, "", "--events-jsonl 阻断路径 stdout 不得有非 JSON 文本")
        self.assertIn("[error]", r.stderr)
        for fn in os.listdir(out):
            self.assertFalse(fn.startswith("astrocs_run_"),
                             "预检阻断不得写 run manifest: %s" % fn)

    # ── crash boundary → 70 + 脱敏 crash report ──
    def test_07_crash_boundary_70_sanitized(self):
        r = run("normalize", "--json", self.cfg, "-y", env={"ASTROCS_TEST_CRASH": "1"})
        self.assertEqual(r.returncode, 70, "未捕获异常 → 70")
        self.assertIn("CRASH", r.stderr)
        self.assertRegex(r.stderr, r"run_id=[0-9a-f]{12}")
        self.assertIn("command='normalize'", r.stderr)
        self.assertIn("no credentials", r.stderr)
        self.assertNotIn(self.cfg, r.stderr, "crash report 不得含完整路径外泄")

    # ── Unicode 路径 ──
    def test_08_unicode_path(self):
        uni = os.path.join(self.tmp, "配置_β_test.json")
        r0 = run("normalize", "--template", "-o", uni)
        self.assertEqual(r0.returncode, 0, r0.stderr)
        with open(uni, encoding="utf-8") as fh:
            self.assertEqual(json.load(fh)["schema_version"], "1")
        # 非 ASCII 配置路径可被解析（预检读到 input_lights, 而不是 config not found）
        r = run("normalize", "--json", uni, "-y")
        self.assertEqual(r.returncode, 2)
        self.assertNotIn("config not found", r.stderr)

    # ── 退出码单源(04 §6-3) ──
    def test_09_exit_codes_single_source(self):
        hits = []
        for fn in os.listdir(CLI):
            if fn.endswith((".cpp", ".h")) and fn != "exit_codes.h":
                with open(os.path.join(CLI, fn), encoding="utf-8") as fh:
                    text = fh.read()
                if re.search(r"=\s*(70|10)\s*[,;/)]", text) or "ARGS  = 2" in text:
                    hits.append(fn)
        self.assertEqual(hits, [], "退出码数值表泄漏到: %s" % hits)
        with open(os.path.join(CLI, "exit_codes.h"), encoding="utf-8") as fh:
            self.assertIn("INTERNAL      = 70", fh.read())


class TestManifestIncomplete(unittest.TestCase):
    """run manifest 数据面: 输入缺失 → 3 + incomplete（禁 complete 冒充）。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_manifest_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_missing_input_writes_incomplete_manifest(self):
        cfg = os.path.join(self.tmp, "cfg.json")
        # 标定帧必须显式给出（SMOKE-001 D4：normalize 预检对缺失标定帧判 error 并阻断
        # rc=2，仅 -force 可越）。本用例判的是「输入文件找不到 → 3」，故配置须越过
        # 预检（文件不存在属运行期输入缺失，由节点报 error_kind=input）。
        na = os.path.join(self.tmp, "nope.fits")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1",
                       "input_lights": [os.path.join(self.tmp, "nope_light.fits")],
                       "master_bias": na, "master_dark": na, "master_flat": na,
                       "output_dir": self.out}, fh)
        r = run("normalize", "--json", cfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 3, r.stderr[-300:])
        mans = [f for f in os.listdir(self.out) if f.startswith("astrocs_run_")]
        self.assertEqual(len(mans), 1, "失败 run 必须落恰 1 个 manifest")
        with open(os.path.join(self.out, mans[0]), encoding="utf-8") as fh:
            doc = json.load(fh)
        self.assertEqual(doc["kind"], "astrocs_run_manifest")
        self.assertEqual(doc["schema_version"], "1")
        self.assertEqual(doc["status"], "incomplete", "不完整运行禁止 complete")
        self.assertEqual(doc["platform"]["arch"], "amd64")
        self.assertEqual(doc["phases"], [1])
        with open(cfg, "rb") as fh:
            want = hashlib.sha256(fh.read()).hexdigest()
        self.assertEqual(doc["config_sha256"], want, "config_sha256 必须是配置实测 hash")
        # 事件流末事件 final/exit_code 与进程退出码单源
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertTrue(events, "--events-jsonl 必须落事件")
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["exit_code"], 3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
