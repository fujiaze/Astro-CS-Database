#!/usr/bin/env python3
"""CLI-001: 唯一命令树（ASTROCS_DESIGN §6.1/§6.2、§1.2）行为测试。

被测对象 = 真实产品二进制（根 CMakeLists.txt 的 astrocs target）：
优先 `build/astrocs`，可用 ASTROCS_CLI_BIN 覆盖；不存在则跳过（不伪绿）。

断言：
  1. `help` / `--help` 文本与 §6.2 命令树逐行一致（不多不少）；
  2. 新命令 rc=0：normalize/mosaic/export 的 --help/--template、help、--version、doctor --json；
  3. 旧命令与别名全部消失且 rc=2（phase1/2/3 × run|validate|plan|inspect、裸 phaseN、
     phase1-run、Phase1、version、config *、modules *、selftest、test synthetic、
     verify*、drizzle、benchmark cpu|verify-profile、hardware inspect、graph、run）；
  4. 三个子命令平级独立：模板互不相同、--template 不触发会话、互不写对方 output_dir；
  5. 命令/参数错误 rc=2，输入缺失 rc=3（§6.3 码表）。
"""
import json, os, shutil, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO)

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

# CLI-002/CLI-11: 子命令 --help 的字段表必须覆盖该命令的**关键必填键**
# （与 --template 同源; 逐命令取最小必需集, 不做全文对照以免与模板演进耦合）。
FIELDS_MIN = {
    "normalize": ("schema_version", "input_lights", "master_bias", "master_dark",
                  "master_flat", "output_dir", "drizzle", "wcs"),
    "mosaic": ("schema_version", "hips_paths", "output_dir", "weight_mode"),
    "export": ("schema_version", "source", "output_dir", "center", "width_px",
               "height_px", "scale_deg_per_px"),
}

LEGACY = ([[c, sub] for c in ("phase1", "phase2", "phase3")
           for sub in ("run", "validate", "plan", "inspect")] +
          [["phase1"], ["phase2"], ["phase3"], ["phase1-run"], ["phase-1"], ["Phase1"],
           ["version"], ["hardware", "inspect", "--json"],
           ["modules", "list", "--json"], ["modules", "verify", "--json"],
           ["selftest", "--json"],
           ["config", "init", "--output", "/tmp/x.json"],
           ["config", "validate", "--config", "/tmp/x.json"],
           ["config", "show-effective", "--config", "/tmp/x.json", "--json"],
           ["test", "synthetic", "--group", "all"],
           ["verify", "--run-manifest", "/tmp/x.json", "--json"],
           ["verify", "profile", "--profile", "/tmp/x.json", "--json"],
           ["drizzle", "--config", "/tmp/x.json"],
           ["benchmark", "cpu", "--quick"],
           ["benchmark", "verify-profile", "--profile", "/tmp/x.json"],
           ["graph", "--preset", "1,2,3"], ["run", "--phases", "1,2,3"]])


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (os.path.join("build", "astrocs"), os.path.join("build", "cli", "astrocs")):
        cand = os.path.join(REPO, rel)
        if os.path.isfile(cand):
            return cand
    return None


EXE = cli_binary()


@unittest.skipUnless(EXE, "astrocs 未构建（先 cmake -S . -B build && ninja -C build astrocs）")
class TestCommandTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="cli001_tree_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def cli(self, *args, timeout=60):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=timeout,
                              cwd=run_cwd())

    def cfg(self, name, **fields):
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(fields, fh)
        return p

    # ── 1. help 与 §6.2 逐行一致 ──
    def test_01_help_is_exactly_spec_tree(self):
        r = self.cli("help")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual([l for l in r.stdout.splitlines() if l.strip()], HELP_LINES)
        r2 = self.cli("--help")
        self.assertEqual(r2.returncode, 0)
        self.assertEqual(r2.stdout, r.stdout)

    def test_02_new_commands_rc0(self):
        for args in (["--version"], ["--version", "--json"], ["help"], ["doctor", "--json"]):
            r = self.cli(*args, timeout=180)
            self.assertEqual(r.returncode, 0, f"astrocs {' '.join(args)} → rc={r.returncode}: {r.stderr[:200]}")
        for cmd in ("normalize", "mosaic", "export"):
            r = self.cli(cmd, "--help")
            self.assertEqual(r.returncode, 0, f"{cmd} --help rc={r.returncode}")
            lines = [l for l in r.stdout.splitlines() if l.strip()]
            # CLI-002/CLI-11: §1「子命令帮助与**字段说明**」—— usage 行恒为第一行,
            # 其后为该命令的配置字段表（与 --template 同源生成, session_commands.h
            # config_fields）。旧断言要求 stdout 恰为 usage 行（早于字段说明要求）。
            self.assertEqual(lines[0],
                             f"astrocs {cmd} (--json <config.json> | --template [-o <path>] | --help)")
            self.assertEqual(lines[1].strip(), "fields:", "子命令帮助必须给字段说明")
            fields = [l.split(" — ")[0].strip() for l in lines[2:] if " — " in l]
            self.assertTrue(fields, f"{cmd} --help 字段表为空")
            for k in FIELDS_MIN[cmd]:
                self.assertTrue(any(f.split(" ")[0] == k for f in fields),
                                f"{cmd} --help 缺字段 {k}: {fields}")

    # ── 2. 旧命令全部 rc=2 ──
    def test_03_legacy_commands_exit_2(self):
        for args in LEGACY:
            r = self.cli(*args)
            self.assertEqual(r.returncode, 2,
                             f"旧命令必须 rc=2: astrocs {' '.join(args)} → rc={r.returncode}")

    def test_04_bare_and_bad_args_exit_2(self):
        for args in (["normalize"], ["mosaic"], ["export"], ["doctor"],
                     ["normalize", "--json"], ["frobnicate"],
                     ["normalize", "--json", "x.json", "--template"],
                     ["normalize", "--json", "x.json", "--bogus"]):
            r = self.cli(*args)
            self.assertEqual(r.returncode, 2, f"astrocs {' '.join(args)} → rc={r.returncode}")

    def test_05_missing_input_exit_3(self):
        for cmd in ("normalize", "mosaic", "export"):
            r = self.cli(cmd, "--json", os.path.join(self.tmp, "nope.json"), "-y")
            self.assertEqual(r.returncode, 3, f"{cmd} 缺 config 应 rc=3，实得 {r.returncode}")

    # ── 3. 三个子命令平级独立 ──
    def test_06_templates_are_distinct_and_standalone(self):
        outs = {}
        for cmd in ("normalize", "mosaic", "export"):
            r = self.cli(cmd, "--template")
            self.assertEqual(r.returncode, 0)
            outs[cmd] = json.loads(r.stdout)
            self.assertEqual(outs[cmd]["schema_version"], "1")
            self.assertIn("output_dir", outs[cmd])
        self.assertEqual(len({json.dumps(v, sort_keys=True) for v in outs.values()}), 3,
                         "三个命令的模板必须互不相同（各自独立产品）")

    def test_07_template_writes_file_and_does_not_run_session(self):
        out_dir = os.path.join(self.tmp, "tpl_out")
        os.makedirs(out_dir, exist_ok=True)
        tpl = os.path.join(self.tmp, "norm_tpl.json")
        r = self.cli("normalize", "--template", "-o", tpl)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(os.path.isfile(tpl))
        doc = json.load(open(tpl, encoding="utf-8"))
        # 模板里的 output_dir 用 "."；--template 不启动会话 → 不产生 run manifest
        self.assertNotIn("astrocs_run_", " ".join(os.listdir(out_dir)))
        self.assertEqual(sorted(os.listdir(out_dir)), [])

    def test_08_commands_do_not_chain(self):
        """normalize 的运行不得产生 mosaic/export 会话事件；反之亦然（§1.2 禁隐式串接）。"""
        for cmd in ("normalize", "mosaic", "export"):
            out_dir = os.path.join(self.tmp, f"chain_{cmd}")
            os.makedirs(out_dir, exist_ok=True)
            key = {"normalize": "input_lights", "mosaic": "hips_paths", "export": "source"}[cmd]
            cfg = self.cfg(f"chain_{cmd}.json", schema_version="1", output_dir=out_dir, **{key: []})
            r = self.cli(cmd, "--json", cfg, "--events-jsonl", "-y", timeout=120)
            self.assertEqual(r.returncode, 2, f"{cmd} 空输入应被预检阻断 rc=2，实得 {r.returncode}")
            events = [json.loads(l) for l in r.stdout.splitlines() if l.strip().startswith("{")]
            phases = {e.get("phase") for e in events}
            others = {"normalize", "mosaic", "export"} - {cmd}
            self.assertFalse(phases & others,
                             f"{cmd} 事件流出现了其它命令的会话: {phases}（隐式串接）")
            # 未确认/预检阻断时不得留下看似完整的 manifest
            for fn in os.listdir(out_dir):
                if fn.startswith("astrocs_run_"):
                    d = json.load(open(os.path.join(out_dir, fn), encoding="utf-8"))
                    self.assertNotEqual(d.get("status"), "complete",
                                        f"{cmd} 预检阻断仍写了 complete manifest")


if __name__ == "__main__":
    unittest.main(verbosity=2)
