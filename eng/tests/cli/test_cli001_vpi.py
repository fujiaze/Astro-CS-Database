#!/usr/bin/env python3
"""CLI 命令面语义（原 CLI-001 validate|plan|inspect 冻结文件，按 §6.2 新树同步）。

权威: ASTROCS_DESIGN §6.2（唯一七行命令树; phase 仅为内部指代）、§3.5（预检阻断）、
§6.3（stdout/退出码）、docs/api/CLI_PROTOCOL_V1.md §1-§3。

退役登记（依据 §6.2 唯一命令树 + CLI-001 rc 矩阵）:
  * 原文件冻结的 phase1|2|3 的 validate|plan|inspect 子命令 **已被 CLI-001 整体删除**
    （rc=2）。原权威 ASTROCS_PROJECT_CONSTITUTION.md 已不存在（仓库零命中），
    其「§8.1 命令面」不再是权威；本文件改写为「这些子命令不存在且被拒」+ 新树
    仍然成立的零 I/O / stdout 纪律断言。
  * 原 test_01(help 列出 validate|plan|inspect)、test_02..test_15（validate/plan/inspect
    的 --json 文档、plan 节点数、inspect run 列表、run→inspect 闭环）→ **退役**：
    被断言的能力（命令）已删除; help 树断言由 eng/tests/cli/test_command_tree.py::
    test_01 与 eng/tests/cli/test_cli_protocol.py::test_01 覆盖; validate/plan 的
    「零科学执行/零产物」语义改由 --template 零 I/O 断言承接（本文件 test_03）;
    inspect 只读呈现无 §6.2 载体 → 能力随命令删除, 如需恢复须先改 §6.2。
"""
import json, os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "acsd"), ("build", "cli", "acsd")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "acsd")


EXE = cli_binary()
HELP_LINES = [
    "acsd --version [--json]",
    "acsd normalize (--json <config.json> | --template [-o <path>] | --help)",
    "acsd mosaic (--json <config.json> | --template [-o <path>] | --help)",
    "acsd export (--json <config.json> | --template [-o <path>] | --help)",
    "acsd help",
    "acsd doctor [--json]",
    "acsd benchmark",
]


def run(*args, timeout=60):
    return subprocess.run([EXE, *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout, cwd=run_cwd())


def tree_snapshot(d):
    out = {}
    if not os.path.isdir(d):
        return out
    for root, _dirs, files in os.walk(d):
        for f in files:
            fp = os.path.join(root, f)
            try:
                st = os.stat(fp)
            except OSError:
                continue
            out[fp] = (st.st_mtime_ns, st.st_size)
    return out


class TestCliCommandSurface(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build acsd）"
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_vpi_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _cfg(self, name, **over):
        doc = {"schema_version": "1", "input_lights": [], "output_dir": self.out}
        doc.update(over)
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    # ── 1. 旧 validate|plan|inspect 子命令全部 rc=2（不存在即被拒） ──
    def test_01_legacy_vpi_subcommands_exit_2(self):
        cfg = self._cfg("ok.json")
        for ph in (1, 2, 3):
            for sub in ("validate", "plan", "inspect"):
                for extra in ([], ["--config", cfg], ["--json"]):
                    r = run("phase%d" % ph, sub, *extra)
                    self.assertEqual(r.returncode, 2,
                                     "phase%d %s %s → 应 rc=2, 得 %s"
                                     % (ph, sub, extra, r.returncode))
                    self.assertEqual(r.stdout, "")
                    self.assertIn("unknown command", r.stderr)
        # 旧 --config 旗标不属于新树（新树用 --json <path>）
        r = run("normalize", "--config", cfg)
        self.assertEqual(r.returncode, 2)
        self.assertIn("unknown flag", r.stderr)

    # ── 2. help 只列 §6.2 七行（无 validate|plan|inspect 行） ──
    def test_02_help_lists_only_spec_tree(self):
        r = run("help")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual([l for l in r.stdout.splitlines() if l.strip()], HELP_LINES)
        for token in ("validate", "plan", "inspect"):
            self.assertNotIn(token, r.stdout, "help 不得再列旧 %s 子命令" % token)

    # ── 3. 模板/帮助零 I/O：不写 output_dir、不起会话 ──
    def test_03_template_and_help_zero_io(self):
        snap = tree_snapshot(self.out)
        for cmd in ("normalize", "mosaic", "export"):
            for args in ([cmd, "--template"], [cmd, "--help"]):
                r = run(*args)
                self.assertEqual(r.returncode, 0, "%s → %s: %s" % (args, r.returncode, r.stderr))
            tpl = os.path.join(self.tmp, "%s_tpl.json" % cmd)
            r = run(cmd, "--template", "-o", tpl)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertTrue(os.path.isfile(tpl))
        self.assertEqual(tree_snapshot(self.out), snap, "模板/帮助不得写 output_dir")
        self.assertEqual([f for f in os.listdir(self.out) if f.startswith("astrocs_run_")], [],
                         "模板/帮助不得写 run manifest")

    # ── 4. 预检阻断：error 时 rc=2、零产物（-y 也不能越过, §3.5） ──
    def test_04_precheck_error_blocks_even_with_yes(self):
        cfg = self._cfg("empty.json")
        snap = tree_snapshot(self.out)
        r = run("normalize", "--json", cfg, "-y")
        self.assertEqual(r.returncode, 2, "-y 不得越过预检 error")
        self.assertIn("[error]", r.stderr)
        self.assertEqual(tree_snapshot(self.out), snap, "阻断运行不得写任何产物")
        self.assertEqual([f for f in os.listdir(self.out) if f.startswith("astrocs_run_")], [])

    # ── 5. --json 机器输出恰一 JSON 文档（stdout 无日志污染） ──
    def test_05_json_single_document_discipline(self):
        cases = [("--version", "--json")]
        for cmd in ("normalize", "mosaic", "export"):
            cases.append((cmd, "--template", "--json"))
        for case in cases:
            r = run(*case)
            self.assertEqual(r.returncode, 0, "%s: %s" % (case, r.stderr))
            # 恰一个 JSON 文档: 整段 stdout 必须可整体解析（模板为多行 pretty-print,
            # 仍是一个文档; 无日志行/无第二个文档即"无污染"）。
            doc = json.loads(r.stdout)
            self.assertIsInstance(doc, dict, "%s stdout 必须是 JSON 对象文档" % (case,))
        # 单行严格形态: --version --json
        r = run("--version", "--json")
        self.assertEqual(len([l for l in r.stdout.splitlines() if l.strip()]), 1)

    # ── 6. 内部会话号仍可用作内部指代, 但不得再作为用户命令（§6.2） ──
    def test_06_session_numbers_not_user_commands(self):
        for tok in ("phase1", "phase2", "phase3", "1", "2", "3"):
            r = run(tok)
            self.assertEqual(r.returncode, 2, "%s 不得是用户命令" % tok)
        for tok in ("phase1-run", "phase-1", "Phase1"):
            self.assertEqual(run(tok).returncode, 2, "%s 不得是用户命令" % tok)


if __name__ == "__main__":
    unittest.main(verbosity=2)
