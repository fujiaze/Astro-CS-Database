#!/usr/bin/env python3
"""CLI-001 测试: 单一 target 编译与 help/version stub golden(Linux GCC; Windows MSVC 实测记录于 LOG)。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402


def _repo_version():
    """版本单源: 根 VERSION 文件(cli/CMakeLists.txt 与 tools/gen_version.py 同源读取)。"""
    with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as fh:
        return fh.read().strip()

# CLI-001: help golden = ASTROCS_DESIGN §6.2 唯一命令树（逐行对照）。
# 旧命令（phase1/2/3、config *、modules *、selftest、test synthetic、verify*、
# drizzle、benchmark cpu|verify-profile、hardware inspect）不得出现在 help 里。
EXPECTED_HELP_LINES = [
    "astrocs --version [--json]",
    "astrocs normalize (--json <config.json> | --template [-o <path>] | --help)",
    "astrocs mosaic (--json <config.json> | --template [-o <path>] | --help)",
    "astrocs export (--json <config.json> | --template [-o <path>] | --help)",
    "astrocs help",
    "astrocs doctor [--json]",
    "astrocs benchmark",
]
# 旧用户命令（命令 + 别名）：help 里不得出现，运行必须 rc=2
LEGACY_COMMANDS = [
    ["phase1", "run"], ["phase2", "run"], ["phase3", "run"],
    ["phase1", "validate"], ["phase2", "plan"], ["phase3", "inspect"],
    ["phase1"], ["phase2"], ["phase3"], ["phase1-run"], ["Phase1"],
    ["version"], ["hardware", "inspect", "--json"], ["config", "validate", "--config", "x"],
    ["modules", "list", "--json"], ["selftest", "--json"],
    ["test", "synthetic", "--group", "all"],
    ["verify", "--run-manifest", "x", "--json"], ["drizzle", "--config", "x"],
    ["benchmark", "cpu", "--quick"], ["graph", "--preset", "1"], ["run", "--phases", "1"],
]

@unittest.skipUnless(shutil.which("cmake") and shutil.which("g++"), "需要 cmake/g++")
class TestCliBuild(unittest.TestCase):
    """被测对象 = **根产品图**产出的唯一 exe（build/astrocs; ASTROCS_CLI_BIN 可覆盖）。

    退役登记: 旧 setUpClass 以 cmake -S cli -B <tmp> 构建 lib/infrastructure/cli/ 独立图（compatibility
    target）; BLD-002 明确唯一产品事实源是根 CMakeLists.txt, 且该独立图在 ARCH-001
    lib/** 迁移中间态必然 configure 失败 ⇒ 该构建路径退役（不是产品缺陷, 也不再 skip）;
    cli/CMakeLists.txt 的 compat 属性仍由 test_05/test_06 静态断言覆盖。
    """

    @classmethod
    def setUpClass(cls):
        env = os.environ.get("ASTROCS_CLI_BIN")
        cands = [env] if env else []
        cands += [os.path.join(REPO, "build", "astrocs"),
                  os.path.join(REPO, "build", "cli", "astrocs")]
        for cand in cands:
            if cand and os.path.isfile(cand):
                cls.exe = cand
                return
        raise AssertionError("先构建根产品 exe: cmake -S . -B build && ninja -C build astrocs")

    def run_cli(self, *args):
        return subprocess.run([self.exe, *args], capture_output=True, text=True, timeout=30,
                              cwd=run_cwd())

    def test_01_version_format(self):
        r = self.run_cli("--version")
        self.assertEqual(r.returncode, 0)
        # 根图用 rev-parse HEAD 全 40 hex（旧 lib/infrastructure/cli/ 独立图才是 --short=12, 已退役）
        self.assertRegex(r.stdout.strip(),
                         r"^astrocs " + re.escape(_repo_version()) + r"\+g[0-9a-f]{12,40}(\.dirty)?$")

    def test_02_version_json_single_document(self):
        r = self.run_cli("--version", "--json")
        self.assertEqual(r.returncode, 0)
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertEqual(len(lines), 1, "stdout 恰一个 JSON 文档")
        doc = json.loads(lines[0])
        self.assertEqual(doc["name"], "astrocs")
        self.assertEqual(doc["schema_version"], "1")
        self.assertRegex(doc["version"],
                         r"^" + re.escape(_repo_version()) + r"\+g[0-9a-f]{12,40}(\.dirty)?$")

    def test_03_help_matches_contract(self):
        # §6.2 逐行对照: help 恰为本表（不多不少），且 `help` 与 `--help` 同文本
        r = self.run_cli("--help")
        self.assertEqual(r.returncode, 0)
        self.assertEqual([l for l in r.stdout.splitlines() if l.strip()],
                         EXPECTED_HELP_LINES, "help 文本必须与 §6.2 命令树逐行一致")
        r2 = self.run_cli("help")
        self.assertEqual(r2.returncode, 0)
        self.assertEqual(r2.stdout, r.stdout, "help 与 --help 必须同文本")

    def test_03b_legacy_commands_gone_exit_2(self):
        # 旧命令与别名全部消失且 rc=2（不保留兼容开关/隐藏别名）
        for args in LEGACY_COMMANDS:
            r = self.run_cli(*args)
            self.assertEqual(r.returncode, 2, f"旧命令必须 rc=2: astrocs {' '.join(args)}")
            self.assertNotIn("phase", r.stdout.lower(), "旧命令不得有任何可用输出")

    def test_03c_new_subcommands_template_and_help(self):
        for cmd in ("normalize", "mosaic", "export"):
            h = self.run_cli(cmd, "--help")
            self.assertEqual(h.returncode, 0, f"{cmd} --help")
            self.assertIn(f"astrocs {cmd}", h.stdout)
            t = self.run_cli(cmd, "--template")
            self.assertEqual(t.returncode, 0, f"{cmd} --template")
            doc = json.loads(t.stdout)          # 模板必须是可直接改的 JSON
            self.assertEqual(doc["schema_version"], "1")
            self.assertIn("output_dir", doc)

    def test_04_unknown_command_exit_2(self):
        r = self.run_cli("bogus")
        self.assertEqual(r.returncode, 2)
        self.assertIn("unknown command", r.stderr)

    def test_05_single_exe_rule(self):
        # 仅统计非注释行(文件头 BLD-002 注释含字面 add_executable(astrocs); 与 test_06 同式)
        cm = "\n".join(l for l in open(os.path.join(CLI, "CMakeLists.txt"), encoding="utf-8")
                       if not l.lstrip().startswith("#"))
        self.assertEqual(len(re.findall(r"add_executable\(", cm)), 1, "恰一个 target")
        # BLD-002: compatibility target 禁止正式 install (02_ABI_BUILD_CLI_TASKS §BLD-002)
        self.assertNotIn("install(TARGETS astrocs", cm)
        self.assertNotIn("march=native", cm)

    def test_06_no_global_arch_flags(self):
        # 仅检查非注释行(禁令注释本身含关键词, 与 checker 误报教训一致)
        code = "\n".join(l for l in open(os.path.join(CLI, "CMakeLists.txt"), encoding="utf-8")
                         if not l.lstrip().startswith("#"))
        for banned in ("-mavx", "arch:AVX", "march=native"):
            self.assertNotIn(banned, code, f"禁编译旗标 {banned}")

if __name__ == "__main__":
    unittest.main(verbosity=2)
