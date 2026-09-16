#!/usr/bin/env python3
"""§6.2 唯一命令面行为测试（原 V7 统一命令面骨架契约文件，按 CLI-001 新树同步）。

权威: ASTROCS_DESIGN §6.2（命令树只有 normalize/mosaic/export ×(--json|--template|
--help) + help/--version/doctor/benchmark；phase 仅内部指代）、docs/api/CLI_PROTOCOL_V1.md §1。

退役登记（旧 surface 已被 CLI-001 删除，依据 §6.2 唯一命令树 + CLI-001 rc 矩阵；
原断言「modules/selftest 骨架可用」的前提——命令存在——已被权威删除）:
  * TestModulesSurface（modules list|verify：manifest 扫描/缺 DLL→5/UTF-8 安装树/
    stdout 纯净，7 用例）→ **退役**；模块清单/生命周期面不在 §6.2 命令树，
    contracts/config/cli_modules_list.schema.json 的 CLI 消费者已不存在；
  * TestSelftestSurface（selftest [--module|--provider] 宿主自检）→ **退役**；
    宿主自检的 §6.2 载体是 doctor [--json]（本文件 TestVersionSurface 旁的
    tests/cli/test_command_tree.py 与 tests/cli/test_bench_cli.py::test_04 覆盖其 JSON 合同）；
  * TestVersionSurface（version 子命令）→ **改写**：version 子命令删除，
    等价面是 --version [--json]（同一版本单源）。
"""
import json, os, re, subprocess, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

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

LEGACY_SURFACE = [
    ["version"], ["version", "--json"],
    ["modules", "list", "--json"], ["modules", "verify", "--json"], ["modules", "bogus"],
    ["selftest"], ["selftest", "--module", "astrocs.noop", "--json"],
    ["hardware", "inspect", "--json"],
    ["config", "init", "--output", "/tmp/x.json"],
    ["config", "validate", "--config", "/tmp/x.json"],
    ["config", "show-effective", "--config", "/tmp/x.json", "--json"],
    ["test", "synthetic", "--group", "all"],
    ["verify", "--run-manifest", "/tmp/x.json", "--json"],
    ["verify", "profile", "--profile", "/tmp/x.json", "--json"],
    ["drizzle", "--config", "/tmp/x.json"],
    ["benchmark", "cpu", "--quick"],
    ["benchmark", "verify-profile", "/tmp/x.json"],
    ["graph", "--preset", "1,2,3"],
    ["run", "--phases", "1,2,3"],
    ["phase1", "run"], ["phase2", "run"], ["phase3", "run"],
    ["phase1", "validate"], ["phase2", "plan"], ["phase3", "inspect"],
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
    with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as fh:
        return fh.read().strip()


def run(*args, timeout=90):
    return subprocess.run([EXE, *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout, cwd=run_cwd())


class TestCommandSurface(unittest.TestCase):
    """§6.2 命令面: help 逐行 + 旧 surface 全部 rc=2（正确行为，不是缺陷）。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"

    def test_01_help_lists_exactly_spec_tree(self):
        a = run("help")
        self.assertEqual(a.returncode, 0, a.stderr)
        self.assertEqual([l for l in a.stdout.splitlines() if l.strip()], HELP_LINES)
        b = run("--help")
        self.assertEqual(b.returncode, 0)
        self.assertEqual(b.stdout, a.stdout)

    def test_02_legacy_surface_all_exit_2(self):
        for case in LEGACY_SURFACE:
            r = run(*case)
            self.assertEqual(r.returncode, 2,
                             "旧命令必须 rc=2: astrocs %s → %s" % (" ".join(case), r.returncode))
            self.assertEqual(r.stdout, "", "参数错误 stdout 应无输出: %s" % (case,))
            self.assertIn("astrocs", r.stderr)


class TestVersionSurface(unittest.TestCase):
    """--version: 稳定输出 + JSON 合同 + 单一版本源（原 version 子命令的等价面）。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"

    def test_01_version_plain(self):
        r = run("--version")
        self.assertEqual(r.returncode, 0, r.stderr)
        # 版本串单源 = 根 CMakeLists（产品事实源, BLD-002）: VERSION+g<commit>;
        # 根图用 rev-parse HEAD 全 40 hex（旧 cli/ 独立图才是 --short=12, 已退役）。
        self.assertRegex(r.stdout.strip(),
                         r"^astrocs " + re.escape(_repo_version()) + r"\+g[0-9a-f]{12,40}(\.dirty)?$")
        self.assertEqual(r.stderr, "")

    def test_02_version_json_single_document(self):
        r = run("--version", "--json")
        self.assertEqual(r.returncode, 0)
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertEqual(len(lines), 1, "stdout 恰一个 JSON 文档(无日志混杂)")
        doc = json.loads(lines[0])
        self.assertEqual(doc["name"], "astrocs")
        self.assertEqual(doc["schema_version"], "1")
        self.assertRegex(doc["version"],
                         r"^" + re.escape(_repo_version()) + r"\+g[0-9a-f]{12,40}(\.dirty)?$")

    def test_03_version_json_matches_plain(self):
        a = run("--version")
        b = run("--version", "--json")
        self.assertEqual(b.returncode, 0)
        self.assertEqual(json.loads(b.stdout)["version"], a.stdout.strip().split()[-1],
                         "--version 与 --version --json 必须同一版本源")

    def test_04_version_rejects_unknown_flag(self):
        r = run("--version", "--bogus")
        self.assertEqual(r.returncode, 2)
        self.assertIn("astrocs:", r.stderr)
        self.assertEqual(r.stdout, "")


if __name__ == "__main__":
    unittest.main(verbosity=2)
