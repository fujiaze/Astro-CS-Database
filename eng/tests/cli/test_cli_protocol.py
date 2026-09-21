#!/usr/bin/env python3
"""CLI golden 测试（按 CLI-001 §6.2 新树同步）: help 树/版本 JSON/parser 拒绝面/模板/
配置错误映射/stdout 纪律/crash boundary 70/Unicode/退出码单源 + incomplete manifest。

权威: ASTROCS_DESIGN §6.2（唯一命令树）、§6.3（stdout 纪律 + 退出码表）、§3.5（预检阻断）、
docs/api/CLI_PROTOCOL_V1.md §1-§3。

退役登记（旧命令面已被 CLI-001 删除，依据 §6.2 + CLI-001 rc 矩阵；原用例前提=命令存在）:
  * test_04_config_init_writes_valid_json  → 改写为 --template -o 写合法 JSON（同意图: 模板即完整可运行配置）;
  * test_05_config_validate_mapping        → 改写为运行入口的配置错误映射（文件缺失/坏 JSON/非对象 → 3）;
  * test_06_jsonl_contract（phase1 run 全程事件）→ 事件流全字段/单调 sequence 断言迁往
    eng/tests/cli/test_phase1_inprocess.py（真实会话）; 本文件保留「阻断路径 stdout 无污染」;
  * test_07_cancel_exit_9_no_fake_artifacts → 取消语义迁往 eng/tests/cli/test_phase{1,2,3}_inprocess.py
    （同一 ASTROCS_TEST_SLEEP_MS 钩子, 真实会话）;
  * test_08_crash_boundary_70_sanitized: test synthetic 已删除 → 改由 normalize + 生产
    ASTROCS_TEST_CRASH 钩子（subcommand.run 内）触发, 断言 70 + 脱敏 crash report;
  * test_09_unicode_path: config init/validate 已删除 → 改为 --template -o 与 --json 的
    非 ASCII 路径解析;
  * TestManifestVerify.test_01..test_07（config validate / show-effective / verify-profile /
    verify 闭环）→ **退役**: config */verify*/benchmark verify-profile 均不在 §6.2 命令树;
    其仍有效的数据面断言（incomplete manifest 字段/hash 链）见本文件 TestManifestIncomplete
    与 eng/tests/cli/test_phase{1,2}_inprocess.py 的 resume 断言;
  * TestManifestVerify.test_08 的 manifest 字段断言 → 改写保留（本文件 TestManifestIncomplete）;
    test_09 verify 闭环 → 退役（verify 命令删除; 哈希链新载体是 export resume 预检,
    当前被 export 预检/会话口径冲突阻塞, 见 TEST-CLI-SYNC 报告）。
"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# ROOT-008 迁移后 CLI 命令层在 lib/infrastructure/cli/（cli/ 只剩 compatibility 声明）。
CLI = os.path.join(REPO, "lib", "infrastructure", "cli")

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402

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
    """版本单源: 根 VERSION 文件（根 CMakeLists 与 eng/tools/gen_version.py 同源读取）。"""
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
        # CLI-MULTIBLOCK（§9.68）: normalize 模板为多块形态 ⇒ output_dir 在块级
        self.assertIn("blocks", doc)
        self.assertIn("output_dir", doc["blocks"][0])
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
        # 非 ASCII 配置路径可被解析（预检读到块级 input_lights, 而不是 config not found）:
        # 模板占位路径不在盘上 ⇒ 路径门 rc=3（§3.5 文件找不到），且诊断必须落在 blocks[0]
        r = run("normalize", "--json", uni, "-y")
        self.assertEqual(r.returncode, 3, r.stderr[-300:])
        self.assertNotIn("config not found", r.stderr)
        self.assertIn("blocks[0].input_lights", r.stderr)

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
    """预检 fail-closed 数据面: 输入路径不存在 → rc=3, 写 manifest 之前阻断。

    2026-09-18 CLI 预检修复（ASTROCS_DESIGN §3.5 + ENGINEERING_SPEC:122 fail-closed）：
    路径不存在/不可读在 precheck_config 阶段即判 error，`-y` 不可越，进程在
    session_dispatch（任何产品/manifest 落盘）之前返回 rc=3。
    旧断言（落 1 个 incomplete manifest + final 事件 exit_code=3）固化的是修复前的
    fail-open 行为，已同步反转为新行为。manifest 数据面（incomplete 禁 complete）
    由运行期失败路径覆盖，不再由「输入路径缺失」这一预检场景覆盖。
    """

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_manifest_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_missing_input_blocked_before_manifest(self):
        cfg = os.path.join(self.tmp, "cfg.json")
        # 标定帧与 light 均给出但盘上不存在 ⇒ precheck_config 的 input_path_errors
        # 判 error（§3.5 fail-closed），-y 不可越，rc=3 且写盘前返回。
        na = os.path.join(self.tmp, "nope.fits")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1",
                       "input_lights": [os.path.join(self.tmp, "nope_light.fits")],
                       "master_bias": na, "master_dark": na, "master_flat": na,
                       "output_dir": self.out}, fh)
        r = run("normalize", "--json", cfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 3, r.stderr[-300:])
        mans = [f for f in os.listdir(self.out) if f.startswith("astrocs_run_")]
        self.assertEqual(mans, [],
                         "预检阻断必须发生在写 manifest 之前（不得落 incomplete 冒充）")
        # 预检阻断路径不进入运行期事件流：stdout 无事件，诊断落 stderr。
        self.assertEqual(r.stdout.strip(), "",
                         "预检阻断不得发运行期事件流（stdout 纪律）")
        self.assertIn("missing/unreadable input path", r.stderr,
                      "必须点名输入路径缺失原因（不许静默）")


# =====================================================================
# FIX-405 G3-11: verify 能力纳入命令树（doctor 机器旗标 --run-manifest）
#
# 权威: ASTROCS_DESIGN §7.1 唯一命令树（无独立 verify 命令；verify* 属已删别名
# → rc=2）+ docs/api/CLI_PROTOCOL_V1.md §1/§3（--json 恰一个 JSON 文档；退出码
# 2 参数 / 3 输入 / 5 版本 / 8 完整性）。
# 落位: command_tree.h 把 --run-manifest 登记为 doctor 的**内部/机器旗标**
# （不写进 help ⇒ help golden 行 "astrocs doctor [--json]" 不变），dispatch 的
# doctor 分支在该旗标在位时走 cmd_verify（manifest→status→version→输入 hash→
# 逐 artifact 存在/sha256/size）。
# =====================================================================
class TestDoctorVerifyLocus(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_doctor_verify_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @staticmethod
    def _cli_version():
        """版本锚必须取**二进制自身**的版本串（VERSION 文件不含 git 后缀，
        而 CLI 的 --version 带 +g<sha>；用 VERSION 会被版本门正确判 5）。"""
        r = run("--version", "--json")
        return json.loads(r.stdout)["version"]

    def _manifest(self, name, **over):
        doc = {"kind": "astrocs_run_manifest", "schema_version": "1",
               "status": "complete", "astrocs_version": self._cli_version(),
               "artifacts": [], "phases": []}
        doc.update(over)
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    def test_01_doctor_run_manifest_verifies_complete_manifest(self):
        m = self._manifest("ok.json")
        r = run("doctor", "--json", "--run-manifest", m)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertEqual(len(lines), 1, "stdout 恰一个 JSON 文档（§3 stdout 纪律）")
        doc = json.loads(lines[0])
        self.assertEqual(doc.get("verify"), "ok")
        self.assertGreaterEqual(doc.get("checked", 0), 1)

    def test_02_missing_manifest_is_input_error(self):
        r = run("doctor", "--json", "--run-manifest",
                os.path.join(self.tmp, "nope.json"))
        self.assertEqual(r.returncode, 3, r.stderr[-300:])

    def test_03_malformed_manifest_is_input_error(self):
        p = os.path.join(self.tmp, "bad.json")
        with open(p, "w", encoding="utf-8") as fh:
            fh.write("{not json")
        r = run("doctor", "--json", "--run-manifest", p)
        self.assertEqual(r.returncode, 3, r.stderr[-300:])

    def test_04_incomplete_manifest_is_integrity_error(self):
        m = self._manifest("incomplete.json", status="incomplete")
        r = run("doctor", "--json", "--run-manifest", m)
        self.assertEqual(r.returncode, 8, r.stderr[-300:])

    def test_05_doctor_without_flag_still_emits_doctor_document(self):
        r = run("doctor", "--json")
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        doc = json.loads(r.stdout)
        self.assertEqual(doc.get("kind"), "astrocs_doctor",
                         "无 --run-manifest 时 doctor 语义不变")

    def test_06_doctor_without_json_is_args_error(self):
        m = self._manifest("ok2.json")
        r = run("doctor", "--run-manifest", m)
        self.assertEqual(r.returncode, 2, r.stderr[-300:])

    def test_07_standalone_verify_command_stays_deleted(self):
        m = self._manifest("ok3.json")
        r = run("verify", "--run-manifest", m, "--json")
        self.assertEqual(r.returncode, 2,
                         "verify 仍是 §7.1 已删别名（能力只在 doctor 下）")


if __name__ == "__main__":
    unittest.main(verbosity=2)

