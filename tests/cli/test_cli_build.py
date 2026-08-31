#!/usr/bin/env python3
"""CLI-001 测试: 单一 target 编译与 help/version stub golden(Linux GCC; Windows MSVC 实测记录于 LOG)。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")

EXPECTED_HELP_LINES = [
    "astrocs --version [--json]",
    "astrocs hardware inspect --json",
    "astrocs config init --output <path>",
    "astrocs config validate --config <path>",
    "astrocs config show-effective --config <path> [--cpu-profile <path>] --json",
    "astrocs benchmark cpu (--quick|--full) [--output <path>] [--events-jsonl]",
    "astrocs verify profile --profile <path> [--json]",
    "astrocs doctor --json",
    "astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>",
    "astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs phase2 run --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs phase3 run --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs run --phases <1|2|3|1,2|1,2,3> --config <path> [--cpu-profile <path>] [--events-jsonl]",
    "astrocs verify --run-manifest <path> --json",
]

@unittest.skipUnless(shutil.which("cmake") and shutil.which("g++"), "需要 cmake/g++")
class TestCliBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bdir = tempfile.mkdtemp(prefix="astrocs_cli_build_")
        subprocess.run(["cmake", "-S", CLI, "-B", cls.bdir], check=True, capture_output=True, timeout=120)
        subprocess.run(["cmake", "--build", cls.bdir, "-j2"], check=True, capture_output=True, timeout=900)
        exe = os.path.join(cls.bdir, "astrocs")
        assert os.path.isfile(exe), exe
        cls.exe = exe

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.bdir, ignore_errors=True)

    def run_cli(self, *args):
        return subprocess.run([self.exe, *args], capture_output=True, text=True, timeout=30)

    def test_01_version_format(self):
        r = self.run_cli("--version")
        self.assertEqual(r.returncode, 0)
        self.assertRegex(r.stdout.strip(),
                         r"^astrocs 0\.10\.0-alpha\.2\+g[0-9a-f]{12}(\.dirty)?$")

    def test_02_version_json_single_document(self):
        r = self.run_cli("--version", "--json")
        self.assertEqual(r.returncode, 0)
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertEqual(len(lines), 1, "stdout 恰一个 JSON 文档")
        doc = json.loads(lines[0])
        self.assertEqual(doc["name"], "astrocs")
        self.assertEqual(doc["schema_version"], "1")
        self.assertRegex(doc["version"], r"^0\.10\.0-alpha\.2\+g[0-9a-f]{12}")

    def test_03_help_matches_contract(self):
        r = self.run_cli("--help")
        self.assertEqual(r.returncode, 0)
        for line in EXPECTED_HELP_LINES:
            self.assertIn(line, r.stdout, f"help 缺命令行: {line}")

    def test_04_unknown_command_exit_2(self):
        r = self.run_cli("bogus")
        self.assertEqual(r.returncode, 2)
        self.assertIn("unknown command", r.stderr)

    def test_05_single_exe_rule(self):
        cm = open(os.path.join(CLI, "CMakeLists.txt"), encoding="utf-8").read()
        self.assertEqual(len(re.findall(r"add_executable\(", cm)), 1, "恰一个 target")
        self.assertIn("install(TARGETS astrocs", cm)
        self.assertNotIn("march=native", cm)

    def test_06_no_global_arch_flags(self):
        # 仅检查非注释行(禁令注释本身含关键词, 与 checker 误报教训一致)
        code = "\n".join(l for l in open(os.path.join(CLI, "CMakeLists.txt"), encoding="utf-8")
                         if not l.lstrip().startswith("#"))
        for banned in ("-mavx", "arch:AVX", "march=native"):
            self.assertNotIn(banned, code, f"禁编译旗标 {banned}")

if __name__ == "__main__":
    unittest.main(verbosity=2)
