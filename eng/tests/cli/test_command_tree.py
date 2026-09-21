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
# CLI-MULTIBLOCK（GAP_AUDIT §9.68）: normalize 是多数据块形态，块内键在 --help 里
# 以 blocks[].<key> 列出（前缀由 session_commands.h 的 scope 派生，与 --template 同源）。
FIELDS_MIN = {
    "normalize": ("schema_version", "blocks", "blocks[].input_lights", "blocks[].master_bias",
                  "blocks[].master_dark", "blocks[].master_flat", "blocks[].output_dir",
                  "blocks[].drizzle", "blocks[].wcs"),
    # §9.73 裁决 A44: weight_mode 已从配置面摘除（模板/help/白名单同撤），
    # 故 mosaic 的 --help 不再列该键；此处只断言**仍在**的必列键。
    # FIX-203（GAP_AUDIT G05/N03）: 合同声明但 CLI 缺失的提升键落地 ⇒ 进必列集
    # （snr_path = mosaic；rotation_deg/crpix_px = export；键名逐字取合同声明名）。
    "mosaic": ("schema_version", "hips_paths", "output_dir", "algorithm_rejection_method",
               "snr_path"),
    "export": ("schema_version", "source", "output_dir", "center", "width_px",
               "height_px", "scale_deg_per_px", "rotation_deg", "crpix_px"),
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
            # §9.68: normalize 模板为多块形态（块级 output_dir）；mosaic/export 顶层。
            if "blocks" in outs[cmd]:
                self.assertIn("output_dir", outs[cmd]["blocks"][0])
            else:
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

    # ── FIX-203: 提升键落地（合同声明键 → CLI 键面） ──
    # 权威：ASTROCS_DESIGN §3.3「键名一律以命令行实际认的键为准」（三命令通用输入合同）；
    # GAP_AUDIT G05（snr_path 不在 CLI 白名单）/ N03（export 几何键）。
    # 断言四件：① --template 含新键且 --help 同源列出；② 模板能被自身 --json 接受
    # （无 unknown key；预检按输入缺失照常阻断——新键不得成为 unknown key 退出 3 的理由）；
    # ③ 结构完整配置下新键必须过键白名单（失败只应是「路径不存在」）；
    # ④ 负例对照：同一路径下真未知键**必须**被判 unknown key + rc=3（证明 ②③ 非恒真）。
    PROMOTED = {"mosaic": ("snr_path",), "export": ("rotation_deg", "crpix_px"),
                "normalize": ()}

    @staticmethod
    def _complete_config(cmd, doc, out_dir):
        """把模板补成「结构完整」（输入非空、output_dir 非空）——只有结构完整才会走到
        键白名单校验（validate_config_full）；路径故意指向不存在处，使失败原因是
        「输入路径不存在」（§3.5 error）而不是「unknown key」。"""
        d = json.loads(json.dumps(doc))
        if "blocks" in d:  # normalize 多块形态（块间 output_dir 不得重复）
            for i, b in enumerate(d["blocks"]):
                b["input_lights"] = ["/nonexistent/fix203_light.fits"]
                b["master_bias"] = "/nonexistent/fix203_bias.fits"
                b["master_dark"] = "/nonexistent/fix203_dark.fits"
                b["master_flat"] = "/nonexistent/fix203_flat.fits"
                b["output_dir"] = os.path.join(out_dir, f"norm{i}")
        else:
            d["output_dir"] = os.path.join(out_dir, cmd)
            if cmd == "mosaic":
                d["hips_paths"] = ["/nonexistent/fix203_hips"]
            elif cmd == "export":
                d["source"] = {"hips_dir": "/nonexistent/fix203_hips"}
        return d

    def _write_cfg(self, name, doc):
        path = os.path.join(self.tmp, name)
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return path

    def test_09_promoted_contract_keys_land(self):
        for cmd, keys in self.PROMOTED.items():
            r = self.cli(cmd, "--template")
            self.assertEqual(r.returncode, 0, r.stderr)
            doc = json.loads(r.stdout)
            for k in keys:
                self.assertIn(k, doc, f"{cmd} --template 缺提升键 {k}: {sorted(doc)}")
            # ① --help 同源：新键必须在字段表里（FIELDS_MIN 已同步，这里再独立核一次）
            h = self.cli(cmd, "--help")
            self.assertEqual(h.returncode, 0, h.stderr)
            for k in keys:
                self.assertIn(k, h.stdout, f"{cmd} --help 缺提升键 {k}")

            # ② 模板自往返：模板 → 文件 → 自身 --json（不得被判 unknown key；
            #    输入为空按预检照常阻断 rc=2/3）
            tpl = os.path.join(self.tmp, f"fix203_{cmd}.json")
            r = self.cli(cmd, "--template", "-o", tpl)
            self.assertEqual(r.returncode, 0, r.stderr)
            r = self.cli(cmd, "--json", tpl, "-y", timeout=120)
            self.assertNotIn("unknown key", r.stderr,
                             f"{cmd} 模板自往返被判 unknown key: {r.stderr[:400]}")
            self.assertIn(r.returncode, (2, 3),
                          f"{cmd} 模板自往返应被预检阻断（rc=2/3），实得 {r.returncode}")

            # ③ 结构完整的同类配置：提升键必须过键白名单，失败只应是「路径不存在」
            complete = self._complete_config(cmd, doc, self.tmp)
            good = self._write_cfg(f"fix203_{cmd}_complete.json", complete)
            rg = self.cli(cmd, "--json", good, "-y", timeout=120)
            self.assertNotIn("unknown key", rg.stderr,
                             f"{cmd} 提升键被判 unknown key（键白名单未落地）: {rg.stderr[:400]}")
            self.assertEqual(rg.returncode, 3,
                             f"{cmd} 路径不存在应 rc=3，实得 {rg.returncode}: {rg.stderr[:300]}")
            self.assertIn("不存在", rg.stderr,
                          f"{cmd} 未给出路径不存在诊断: {rg.stderr[:300]}")

            # ④ 负例对照：同一路径下注入真未知键必须被拒（rc=3 + unknown key）
            bad = json.loads(json.dumps(complete))
            if "blocks" in bad:
                bad["blocks"][0]["__fix203_unknown_key__"] = 1
            else:
                bad["__fix203_unknown_key__"] = 1
            bad_path = self._write_cfg(f"fix203_{cmd}_bad.json", bad)
            rb = self.cli(cmd, "--json", bad_path, "-y", timeout=120)
            self.assertEqual(rb.returncode, 3,
                             f"{cmd} 未知键必须 rc=3，实得 {rb.returncode}")
            self.assertIn("unknown key", rb.stderr,
                          f"{cmd} 未知键未被报出: {rb.stderr[:400]}")

    def test_10_precision_key_is_not_reinvented(self):
        """FIX-203 禁止项：不得新造同义键——precision(fp32/fp64) 不得进 CLI 键面。

        精度口径（ASTROCS_DESIGN §3.3:256）：阶段一 = drizzle.precision_mode(0/1)，
        阶段二/三 = 位深键 bitpix(-32/-64)。故配置里出现 precision 必须按 unknown key
        拒绝（rc=3），而不是被静默接受。
        """
        for cmd in ("normalize", "mosaic", "export"):
            # 结构必须完整，否则「缺 output_dir/输入为空」的结构错（rc=2）会先于键校验，
            # 掩盖 unknown key 诊断（subcommand.h 阻断优先级 ①→②）。
            tpl = json.loads(self.cli(cmd, "--template").stdout)
            doc = self._complete_config(cmd, tpl, self.tmp)
            doc["precision"] = "fp64"
            cfg = self._write_cfg(f"fix203_precision_{cmd}.json", doc)
            r = self.cli(cmd, "--json", cfg, "-y", timeout=120)
            self.assertEqual(r.returncode, 3,
                             f"{cmd} 不得接受 precision 同义键（rc={r.returncode}）")
            self.assertIn("unknown key", r.stderr)
            self.assertIn("precision", r.stderr)
        # 既有精度载体必须在键面上（复用、非新造；ASTROCS_DESIGN §3.3:256）：
        #   阶段一 drizzle.precision_mode(0/1)、阶段三 bitpix(-32/-64)。
        # 阶段二（mosaic）当前无被消费的精度键（实测登记于 eng/ci/ledgers/dead_config_keys.json
        # 的 dead_config_key:precision 残留缺口），故此处不断言 mosaic 载体。
        for cmd, carrier in (("normalize", "precision_mode"), ("export", "bitpix")):
            r = self.cli(cmd, "--help")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn(carrier, r.stdout, f"{cmd} --help 缺既有精度载体 {carrier}")
        # bitpix 是**既有**白名单键（不是新造）：配置里出现必须被识别（不得报 unknown key）
        cfg = os.path.join(self.tmp, "fix203_bitpix_export.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "bitpix": -64}, fh)
        r = self.cli("export", "--json", cfg, "-y", timeout=120)
        self.assertNotIn("unknown key", r.stderr,
                         f"bitpix 是既有键，不得被判 unknown key: {r.stderr[:300]}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
