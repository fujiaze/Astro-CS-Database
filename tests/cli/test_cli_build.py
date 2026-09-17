#!/usr/bin/env python3
"""CLI-001 测试: 单一 target 编译与 help/version stub golden(Linux GCC; Windows MSVC 实测记录于 LOG)。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# 单一产品事实源 = 根 CMakeLists.txt（唯一 project()/唯一 add_executable(astrocs)）。
# 旧 cli/CMakeLists.txt（BLD-002 compatibility 声明，非产品事实源）退役后，静态
# 属性断言改锚到根文件 + 迁移后的生成头模板 lib/infrastructure/cli/version_generated.h.in。
ROOT_CMAKE = os.path.join(REPO, "CMakeLists.txt")
VERSION_H_IN = "lib/infrastructure/cli/version_generated.h.in"


def _code_lines(path):
    """非注释行（CMake 用 # 注释；禁令注释本身含关键词，沿用旧 filter 教训）。"""
    with open(path, encoding="utf-8") as fh:
        return "\n".join(l for l in fh if not l.lstrip().startswith("#"))


def _single_exe_violations(text):
    """唯一 project()/唯一 add_executable(astrocs) + 版本单源锚点（根文件口径）。

    返回违规列表（空 = 合规）；每条都可由注入式负例转红（见 test_05）。
    """
    bad = []
    if len(re.findall(r"^\s*project\(", text, re.M)) != 1:
        bad.append("根 CMakeLists 必须恰有一个 project()")
    if len(re.findall(r"add_executable\(", text)) != 1:
        bad.append("根 CMakeLists 必须恰有一个 add_executable()")
    if len(re.findall(r"add_executable\(astrocs\b", text)) != 1:
        bad.append("根 CMakeLists 必须恰有一个 add_executable(astrocs ...)")
    if "file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION" not in text:
        bad.append("版本单源必须是根 VERSION 文件（file(READ <root>/VERSION ...)）")
    if VERSION_H_IN not in text.replace(os.sep, "/"):
        bad.append("生成头模板必须锚在 " + VERSION_H_IN)
    # 退役锚点: 旧路径 <root>/cli/version_generated.h.in（不含 infrastructure/ 前缀）。
    if re.search(r"(?<!infrastructure/)cli/version_generated\.h\.in", text):
        bad.append("不得再引用已退役的 cli/version_generated.h.in")
    if "march=native" in text:
        bad.append("禁 march=native（全局 ISA 泄漏）")
    return bad


# ISA 高级旗标允许出现在 provider target 的 PRIVATE 作用域（ARCH-003 §2 / 根
# CMakeLists 的编译隔离合同）；被禁的是全局泄漏（add_compile_options /
# CMAKE_*_FLAGS / 丢失 PRIVATE 的目录级设置）。
_ARCH_FLAG_RE = re.compile(r"(-mavx\w*|-mfma|/arch:AVX\w*)")
_GLOBAL_SCOPE_RE = re.compile(r"(add_compile_options|CMAKE_CXX_FLAGS|CMAKE_C_FLAGS|"
                              r"add_definitions|include_directories)")


def _cmake_commands(text):
    """把 CMake 文本切成完整命令（跨行续行按括号配平合并），避免按行误判。"""
    out, buf, depth = [], [], 0
    for line in text.splitlines():
        buf.append(line)
        depth += line.count("(") - line.count(")")
        if depth <= 0:
            out.append(" ".join(x.strip() for x in buf))
            buf, depth = [], 0
    if buf:
        out.append(" ".join(x.strip() for x in buf))
    return out


def _global_arch_violations(text):
    """全局 ISA 旗标泄漏（返回违规命令；空 = 合规）。"""
    bad = []
    if "march=native" in text:
        bad.append("march=native")
    for cmd in _cmake_commands(text):
        if not _ARCH_FLAG_RE.search(cmd):
            continue
        if _GLOBAL_SCOPE_RE.search(cmd) or not re.search(r"PRIVATE", cmd):
            bad.append(cmd.strip()[:120])
    return bad

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

    def test_03d_templates_key_complete_and_accepted(self):
        """§6.3 / E2E-D02: 三命令 --template 的键必须全部是运行期接受键（无 unknown key），
        且结构完整（填好路径即可运行）。负例自证: 注入未登记键必须被判出。"""
        tmp = tempfile.mkdtemp(prefix="astrocs_tpl_")
        self.addCleanup(shutil.rmtree, tmp, True)
        # 各命令「只填路径」的最小填充（不填补即被预检阻断，无法走到键校验面）
        fill = {
            "normalize": lambda d: d.update({
                "input_lights": [os.path.join(tmp, "light.fits")],
                "master_bias": os.path.join(tmp, "b.fits"),
                "master_dark": os.path.join(tmp, "d.fits"),
                "master_flat": os.path.join(tmp, "f.fits")}),
            "mosaic": lambda d: d.update({"hips_paths": [os.path.join(tmp, "F.hips")]}),
            "export": lambda d: (d["source"].__setitem__("hips_dir", os.path.join(tmp, "F.hips"))),
        }
        for cmd in ("normalize", "mosaic", "export"):
            t = self.run_cli(cmd, "--template")
            self.assertEqual(t.returncode, 0, f"{cmd} --template")
            doc = json.loads(t.stdout)
            self.assertEqual(doc["schema_version"], "1")
            out = os.path.join(tmp, cmd + "_out")
            os.makedirs(out, exist_ok=True)
            doc["output_dir"] = out
            fill[cmd](doc)
            cfg = os.path.join(tmp, cmd + ".json")
            with open(cfg, "w", encoding="utf-8") as fh:
                json.dump(doc, fh)
            r = self.run_cli(cmd, "--json", cfg, "-y")
            self.assertNotIn("unknown key", r.stderr,
                             f"{cmd} 模板含运行期不接受的键:\n{r.stderr[-300:]}")
            self.assertNotEqual(r.returncode, 70, f"{cmd} 模板触发未分类错误")
            # 负例自证（判别力）: 注入未登记键 ⇒ 同一检查必须报 unknown key。
            doc["definitely_not_a_key"] = 1
            with open(cfg, "w", encoding="utf-8") as fh:
                json.dump(doc, fh)
            bad = self.run_cli(cmd, "--json", cfg, "-y")
            self.assertIn("unknown key", bad.stderr,
                          f"{cmd}: 未登记键未被判出（检查失去判别力）")
        # E2E-D02 回归锚: normalize 模板必须带 drizzle.precision_mode（无 silent 缺省）
        n = json.loads(self.run_cli("normalize", "--template").stdout)
        self.assertIn("precision_mode", n["drizzle"])
        # D3 形态锚: export 模板的 source/center 必须是运行期对象形态
        e = json.loads(self.run_cli("export", "--template").stdout)
        self.assertIsInstance(e["source"], dict)
        self.assertIsInstance(e["center"], dict)

    def test_04_unknown_command_exit_2(self):
        r = self.run_cli("bogus")
        self.assertEqual(r.returncode, 2)
        self.assertIn("unknown command", r.stderr)

    def test_05_single_exe_rule(self):
        # 单一产品事实源 = 根 CMakeLists.txt（旧 cli/CMakeLists.txt compatibility
        # 声明已退役）；判据: 唯一 project()/唯一 add_executable(astrocs) + 版本单源
        # 锚在根 VERSION 与 lib/infrastructure/cli/version_generated.h.in。
        text = _code_lines(ROOT_CMAKE)
        self.assertEqual(_single_exe_violations(text), [],
                         "根 CMakeLists 违反唯一产品 exe 规则")
        # 判别力自证（负例，注入式）: 改掉对应行 ⇒ 同一判据必须转红。
        self.assertTrue(_single_exe_violations(
            text.replace("add_executable(astrocs", "add_executable(astrocs_dup", 1)),
            "负例: target 改名未被判出（判据失去判别力）")
        self.assertTrue(_single_exe_violations(text.replace(
            "lib/infrastructure/cli/version_generated.h.in",
            "cli/version_generated.h.in", 1)),
            "负例: 版本头模板锚点漂回已退役路径未被判出")

    def test_06_no_global_arch_flags(self):
        # ARCH-003 §2: 禁全局 ISA 旗标（可加载性/可移植性）。provider target 的
        # PRIVATE 高级旗标（astrocs_cpu_avx2 等）是合同允许的隔离形态。
        text = _code_lines(ROOT_CMAKE)
        self.assertEqual(_global_arch_violations(text), [],
                         "根 CMakeLists 出现全局 ISA 旗标泄漏")
        # 判别力自证（负例，注入式）: 注入全局旗标与游标旗标 ⇒ 必须判出。
        self.assertTrue(_global_arch_violations("add_compile_options(-mavx2)\n"),
                        "负例: 全局 add_compile_options(-mavx2) 未被判出")
        self.assertTrue(_global_arch_violations("target_compile_options(x -mavx2)\n"),
                        "负例: 丢失 PRIVATE 的目标级旗标未被判出")
        self.assertTrue(_global_arch_violations("set(CMAKE_CXX_FLAGS \"-march=native\")\n"),
                        "负例: march=native 未被判出")

if __name__ == "__main__":
    unittest.main(verbosity=2)
