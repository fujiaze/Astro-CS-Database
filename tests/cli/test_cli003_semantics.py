#!/usr/bin/env python3
"""CLI-003 golden: 固化阶段子命令语义 — validate/plan(无 plan 命令, 以 validate/verify/run 现有面固化)/
run/inspect 语义。验收=调用计数 + I/O spy + validate 对大型 manifest 保持轻量 + 错误 schema 位置精确。

冻结的语义(全部经生产二进制实测, base de2d6d7f):
1. **validate 是浅校验 + 零科学执行**: `config validate` 只做顶层 schema 检查(白名单键/
   schema_version/inputs 四键存在且为字符串数组/output_dir), **不递归 phase3 内部**;
   不写 run manifest、不产生 FITS/artifact、不改 config 文件 mtime、不触发 Runtime。
   错误 schema 位置精确到键名: unknown key 'backend' → 3; 缺 schema_version → 3;
   schema_version≠"1" → 2(配置错, 与输入缺失区分); inputs 缺/错型 → 3; output_dir 缺/不存在 → 3。
2. **深层 phase3 拒绝属于 run 域**: validate 对深 garbage phase3 返回 OK; `phaseN run` 才做深层
   session 校验并失败(exit 2)+写 incomplete manifest。这证明 validate 与 execute 解耦。
3. **run 输出/overwrite/resume 语义**: 每次 `phaseN run` 是全新进程/Runtime/run_id(CLI-002),
   manifest 按 run_id 落盘(astrocs_run_<12hex>.json), **重跑绝不清写先前 manifest**(journal 语义);
   incomplete manifest 永不可 verify(8); 失败 run 不产出任何科学 FITS(无伪完整产物)。
   --force/--overwrite/--resume 不是 phase run 的 CLI 旗标(parser 白名单拒绝 → 2);
   resume/hash 校验属 session/domain 层(见 known_limits, SYN-009 域), 非本命令面旗标。
4. **调用计数 spy**: `config validate` 与 `verify` 零 stage_start/stage_end/resource/backend 事件,
   stdout 人类模式恰 "config OK" / --json 恰一 JSON 文档; 失败 run 事件面也只有
   {stage_start, stage_end, artifact(run_manifest), final} —— 无 resource/backend(科学 execute 未达)。
"""
import hashlib, json, os, shutil, subprocess, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")


def built():
    if not os.path.isfile(EXE):
        subprocess.run(["cmake", "-S", CLI, "-B", BUILD], check=True,
                       capture_output=True, timeout=120)
        subprocess.run(["cmake", "--build", BUILD, "-j2"], check=True,
                       capture_output=True, timeout=300)
    return EXE


def run(*args, cwd=None, env=None, timeout=60):
    e = dict(os.environ)
    if env:
        e.update(env)
    return subprocess.run([built(), *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout,
                          cwd=cwd, env=e)


def jsonl_lines(stdout):
    return [json.loads(l) for l in stdout.splitlines() if l.strip()]


def tree_snapshot(d):
    """递归文件快照: {abs_path: (mtime_ns, size)}。目录不存在 → {}。"""
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


def sha256_file(p):
    with open(p, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


@unittest.skipUnless(os.path.isfile(EXE), "需要已构建 CLI build/cli/astrocs")
class TestCli003Semantics(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        built()
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_cli003_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)
        # 真实存在的输入文件(validate 路径存在性检查用)
        cls.light = os.path.join(cls.tmp, "light1.fits")
        with open(cls.light, "wb") as fh:
            fh.write(b"FAKE-FITS-DATA-0")
        cls.base = {
            "schema_version": "1",
            "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
            "output_dir": cls.out,
        }
        cls.cfg_ok = os.path.join(cls.tmp, "cfg_ok.json")
        cls._write(cls.cfg_ok, cls.base)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @classmethod
    def _write(cls, path, doc):
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return path

    def _cfg(self, name, **over):
        doc = dict(self.base)
        doc.update(over)
        return self._write(os.path.join(self.tmp, name), doc)

    # ── 1. validate 精确 schema 位置 + exit code ──
    def test_01_valid_config_validate_ok(self):
        r = run("config", "validate", "--config", self.cfg_ok)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), "config OK", "人类模式 stdout 恰 'config OK'")
        self.assertEqual(r.stderr, "", "合法 config 无诊断")

    def test_02_unknown_key_reports_exact_key(self):
        r = run("config", "validate", "--config",
                self._cfg("unknown.json", backend="x"))
        self.assertEqual(r.returncode, 3)
        self.assertIn("config has unknown key 'backend'", r.stderr,
                      "错误必须带具体键名(防拼写静默忽略)")
        # 第二个未知键同样精确
        r2 = run("config", "validate", "--config",
                 self._cfg("unknown2.json", hips_dir="y"))
        self.assertEqual(r2.returncode, 3)
        self.assertIn("config has unknown key 'hips_dir'", r2.stderr)

    def test_03_unknown_key_checked_before_missing_schema_version(self):
        # 校验序确定(不猜测): 白名单键先查 → 即使同时缺 schema_version 也先报 unknown key
        doc = {"inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
               "output_dir": self.out, "backend": "x"}
        r = run("config", "validate", "--config",
                self._write(os.path.join(self.tmp, "order.json"), doc))
        self.assertEqual(r.returncode, 3)
        self.assertIn("config has unknown key 'backend'", r.stderr)

    def test_04_missing_schema_version_input_error_3(self):
        doc = {"inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
               "output_dir": self.out}
        r = run("config", "validate", "--config", self._write(
            os.path.join(self.tmp, "nosv.json"), doc))
        self.assertEqual(r.returncode, 3, "缺 schema_version = 输入缺失 → 3")
        self.assertIn("config missing 'schema_version'", r.stderr)

    def test_05_schema_version_not_1_args_error_2(self):
        for idx, v in enumerate(("2", 1, ["1"])):
            r = run("config", "validate", "--config",
                    self._cfg(f"sv_{idx}.json", schema_version=v))
            self.assertEqual(r.returncode, 2, f"schema_version={v!r} → 2(配置错)")
            self.assertIn('config schema_version must be "1"', r.stderr)

    def test_06_inputs_missing_or_malformed_3(self):
        # inputs 对象整体缺失
        doc = {"schema_version": "1", "output_dir": self.out}
        r = run("config", "validate", "--config", self._write(
            os.path.join(self.tmp, "noinputs.json"), doc))
        self.assertEqual(r.returncode, 3)
        self.assertIn("config missing 'inputs' object", r.stderr)
        # lights 键缺失
        r2 = run("config", "validate", "--config", self._cfg(
            "nolights.json", **{"inputs": {"darks": [], "flats": [], "bias": []}}))
        self.assertEqual(r2.returncode, 3)
        self.assertIn("config inputs.lights must be an array", r2.stderr)
        # lights 非数组
        r3 = run("config", "validate", "--config", self._cfg(
            "lightsstr.json", **{"inputs": {"lights": "x", "darks": [], "flats": [],
                                            "bias": []}}))
        self.assertEqual(r3.returncode, 3)
        self.assertIn("config inputs.lights must be an array", r3.stderr)

    def test_07_input_path_errors_positioned_3(self):
        # 空路径 → 明确键位
        r = run("config", "validate", "--config", self._cfg(
            "emptypath.json", **{"inputs": {"lights": [""], "darks": [], "flats": [],
                                            "bias": []}}))
        self.assertEqual(r.returncode, 3)
        self.assertIn("config inputs.lights has empty path", r.stderr)
        # 文件不存在 → 路径入诊断(精确到文件)
        r2 = run("config", "validate", "--config", self._cfg(
            "nofile.json", **{"inputs": {"lights": [os.path.join(self.tmp, "nofile.fits")],
                                         "darks": [], "flats": [], "bias": []}}))
        self.assertEqual(r2.returncode, 3)
        self.assertIn("config input not found", r2.stderr)
        self.assertIn("nofile.fits", r2.stderr)

    def test_08_output_dir_errors_positioned_3(self):
        # output_dir 键整体缺失 → 3(需绕开 base 的 output_dir, 手写完整 doc)
        doc = {"schema_version": "1",
               "inputs": {"lights": [], "darks": [], "flats": [], "bias": []}}
        r = run("config", "validate", "--config", self._write(
            os.path.join(self.tmp, "noout.json"), doc))
        self.assertEqual(r.returncode, 3)
        self.assertIn("config missing 'output_dir'", r.stderr)
        r2 = run("config", "validate", "--config", self._cfg(
            "badout.json", output_dir=os.path.join(self.tmp, "nope_dir")))
        self.assertEqual(r2.returncode, 3)
        self.assertIn("config output_dir not found", r2.stderr)

    def test_09_validate_is_shallow_no_phase3_recurse(self):
        # CLI-003 冻结: validate 不递归 phase3 内部(浅校验=轻量)。
        # 深 garbage / 非法 projection / 不存在 hips source 在 validate 一律通过,
        # 其拒绝属于 run 域(见 test_12/13 系列) —— validate 不触发科学 execute。
        deep = {"phase3": {"garbage": 42, "nested": {"a": [1, 2]},
                           "projection": "SIN",
                           "source": {"hips_dir": os.path.join(self.tmp, "nope.hips")}}}
        r = run("config", "validate", "--config", self._cfg("deep.json", **deep))
        self.assertEqual(r.returncode, 0, "深 phase3 内容不属于 validate 浅面")
        self.assertEqual(r.stdout.strip(), "config OK")
        self.assertEqual(r.stderr, "")

    # ── 2. validate I/O spy: 零输出 / 轻量(大型 manifest) / config mtime 不变 ──
    def test_10_validate_io_spy_no_outputs_large_manifest_light(self):
        snap = tree_snapshot(self.out)
        cfg_mtime = os.stat(self.cfg_ok).st_mtime_ns
        # 大型 manifest: 300 个 light 项(路径存在性逐项查) → 仍轻量(<10s), 零 I/O 产物
        big = dict(self.base)
        big["inputs"] = {"lights": [self.light] * 300, "darks": [], "flats": [], "bias": []}
        big_cfg = self._cfg("big300.json", **big)
        t0 = time.time()
        r = run("config", "validate", "--config", big_cfg, timeout=120)
        wall = time.time() - t0
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), "config OK")
        self.assertLess(wall, 10.0, "大型 manifest 校验必须轻量")
        # 重复校验合法小 config 也不产生任何产物
        self.assertEqual(run("config", "validate", "--config", self.cfg_ok).returncode, 0)
        after = tree_snapshot(self.out)
        self.assertEqual(after, snap, "validate 不得向 output_dir 写任何文件")
        self.assertEqual(os.stat(self.cfg_ok).st_mtime_ns, cfg_mtime,
                         "validate 不得触碰 config 文件本身")
        # 零 run manifest / fits / artifact
        self.assertEqual(glob_no_run(self.out), [], "validate 不得写 astrocs_run_*")
        self.assertEqual([f for f in walk_files(self.out) if f.endswith(".fits")], [],
                         "validate 不得产出 FITS")

    def test_11_verify_is_recompute_only_no_io_no_rerun(self):
        # verify 只重算 hash(不运行科学/不写产物); 手工 complete manifest 闭环。
        art = os.path.join(self.out, "out.fits")
        payload = b"SCI-DATA-" + os.urandom(32)
        with open(art, "wb") as fh:
            fh.write(payload)
        ver = json.loads(run("--version", "--json").stdout)["version"]
        mf = os.path.join(self.out, "run_ok.json")
        doc = {"schema_version": "1", "kind": "astrocs_run_manifest",
               "run_id": "0" * 12, "astrocs_version": ver,
               "platform": {"os": "linux", "arch": "amd64"},
               "config_path": None, "config_sha256": None,
               "cpu_profile_path": None, "cpu_profile_sha256": None,
               "phases": [3],
               "artifacts": [{"role": "phase3_output", "path": art,
                              "sha256": sha256_file(art),
                              "size_bytes": os.path.getsize(art)}],
               "status": "complete", "started_utc": "2026-01-01T00:00:00Z",
               "finished_utc": "2026-01-01T00:00:01Z", "summary": "t"}
        self._write(mf, doc)
        snap = tree_snapshot(self.out)
        before_art = sha256_file(art)
        r = run("verify", "--run-manifest", mf, "--json")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(len(r.stdout.splitlines()), 1, "verify stdout 恰一 JSON 文档")
        out = json.loads(r.stdout)
        self.assertEqual(out["verify"], "ok")
        self.assertGreaterEqual(out["checked"], 2)
        # 调用计数: verify 不写任何新文件、不改 artifact、不产 run manifest
        self.assertEqual(tree_snapshot(self.out), snap, "verify 不得产生任何 I/O 变化")
        self.assertEqual(sha256_file(art), before_art, "verify 不得触碰 artifact 内容")
        # mutation → 精确退出码(重算闭环)
        doc["artifacts"][0]["sha256"] = "0" * 64
        self._write(mf, doc)
        r2 = run("verify", "--run-manifest", mf, "--json")
        self.assertEqual(r2.returncode, 8, "artifact sha 篡改 → 8(完整性)")
        self.assertIn("sha256 mismatch", r2.stderr)
        doc["artifacts"][0]["sha256"] = sha256_file(art)
        doc["kind"] = "other"
        self._write(mf, doc)
        r3 = run("verify", "--run-manifest", mf, "--json")
        self.assertEqual(r3.returncode, 3, "非 v1 manifest → 3")
        self.assertIn("not a v1 astrocs_run_manifest", r3.stderr)

    # ── 3. run output / overwrite / resume ──
    def test_12_run_missing_phase3_writes_incomplete_manifest(self):
        # run 缺 phase3 子对象: validate 通过(浅)但 run 深层拒 → 2 + incomplete manifest。
        d = os.path.join(self.tmp, "r12"); os.makedirs(d, exist_ok=True)
        cfg = self._cfg("r12.json", output_dir=d)   # 无 phase3 键
        rv = run("config", "validate", "--config", cfg)
        self.assertEqual(rv.returncode, 0, "浅 validate 对缺 phase3 不报错(phase3 可选顶层键)")
        r = run("phase3", "run", "--config", cfg, "--events-jsonl", cwd=self.tmp)
        self.assertEqual(r.returncode, 2, r.stderr)
        self.assertIn("run config missing 'phase3' object", r.stderr)
        events = jsonl_lines(r.stdout)
        self.assertEqual([e["sequence"] for e in events],
                         list(range(len(events))), "sequence 从 0 单调")
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "phase3_failed")
        self.assertEqual(events[-1]["exit_code"], 2)
        # 调用计数: 失败 run 事件面 = stage_start/stage_end + artifact(run_manifest) + final;
        # 无 resource/backend(科学 execute 未达, Runtime 未产生资源事件)
        kinds = [e["kind"] for e in events]
        self.assertNotIn("resource", kinds)
        self.assertNotIn("backend", kinds)
        self.assertEqual(sorted(set(kinds)), ["artifact", "final", "stage_end", "stage_start"])
        arts = [e for e in events if e["kind"] == "artifact" and
                e.get("role") == "run_manifest"]
        self.assertEqual(len(arts), 1)
        mpath = arts[0]["path"]
        self.assertTrue(os.path.isfile(mpath), "run 必须落盘 run manifest")
        with open(mpath, encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["kind"], "astrocs_run_manifest")
        self.assertEqual(m["status"], "incomplete", "失败 run 禁止 complete")
        self.assertEqual(m["phases"], [3])
        self.assertEqual(m["config_sha256"], sha256_file(cfg),
                         "manifest 记录 config 字节 hash(与独立 sha256 一致)")
        self.assertRegex(m["run_id"], r"^[0-9a-f]{12}$")
        self.assertEqual(events[-1]["run_id"], m["run_id"])
        # 无科学 FITS 产物(无伪完整结果)
        self.assertEqual([f for f in os.listdir(d) if f.endswith(".fits")], [])

    def test_13_rerun_fresh_run_id_no_overwrite_incomplete_never_verify(self):
        # CLI-003/CLI-002 冻结: 每次 run = 全新进程/Runtime/run_id → manifest journal,
        # 重跑绝不覆盖先前 manifest(该文件字节不变), 各自 run_id 唯一。
        d = os.path.join(self.tmp, "r13"); os.makedirs(d, exist_ok=True)
        cfg = self._cfg("r13.json", output_dir=d)
        r1 = run("phase3", "run", "--config", cfg, "--events-jsonl", cwd=self.tmp)
        self.assertEqual(r1.returncode, 2)
        m1_path = [e for e in jsonl_lines(r1.stdout)
                   if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]["path"]
        with open(m1_path, encoding="utf-8") as fh:
            m1 = json.load(fh)
        with open(m1_path, "rb") as fh:
            m1_bytes = fh.read()
        r2 = run("phase3", "run", "--config", cfg, "--events-jsonl", cwd=self.tmp)
        self.assertEqual(r2.returncode, 2)
        m2_path = [e for e in jsonl_lines(r2.stdout)
                   if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]["path"]
        with open(m2_path, encoding="utf-8") as fh:
            m2 = json.load(fh)
        self.assertNotEqual(m1_path, m2_path, "重跑必须写新 manifest(不覆盖先前文件)")
        self.assertNotEqual(m1["run_id"], m2["run_id"], "两次 run 必须不同 run_id(进程隔离)")
        with open(m1_path, "rb") as fh:
            self.assertEqual(fh.read(), m1_bytes,
                         "先前 manifest 不得被重跑清写(journal 语义)")
        self.assertEqual(m1["status"], "incomplete")
        self.assertEqual(m2["status"], "incomplete")
        # incomplete manifest 永不可 verify → 8
        rv = run("verify", "--run-manifest", m1_path, "--json")
        self.assertEqual(rv.returncode, 8)
        self.assertIn("incomplete", rv.stderr)
        # --force/--overwrite/--resume 不是 phase run 命令面旗标(parser 白名单拒绝 → 2)
        for flag in ("--force", "--overwrite", "--resume"):
            rr = run("phase3", "run", "--config", cfg, flag, cwd=self.tmp)
            self.assertEqual(rr.returncode, 2, f"{flag} 必须被 parser 拒绝")
            self.assertIn("unknown flag", rr.stderr)

    def test_14_phase1_run_empty_lights_call_count(self):
        # phase1 run 空 inputs.lights: 同一 config validate=OK(浅) 但 run 深层拒 → 2 +
        # incomplete manifest; 事件面仍无 resource/backend(科学执行未启动)。
        d = os.path.join(self.tmp, "r14"); os.makedirs(d, exist_ok=True)
        cfg = self._cfg("r14.json", output_dir=d)
        rv = run("config", "validate", "--config", cfg)
        self.assertEqual(rv.returncode, 0, "空 lights 属合法 config(validate 不查内容语义)")
        r = run("phase1", "run", "--config", cfg, "--events-jsonl", cwd=self.tmp)
        self.assertEqual(r.returncode, 2, r.stderr)
        self.assertIn("input_lights must be non-empty array", r.stderr)
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "phase1_failed")
        self.assertEqual(events[-1]["exit_code"], 2)
        kinds = [e["kind"] for e in events]
        self.assertNotIn("resource", kinds)
        self.assertNotIn("backend", kinds)
        self.assertEqual(sorted(set(kinds)), ["artifact", "final", "stage_end", "stage_start"])
        arts = [e for e in events if e["kind"] == "artifact" and
                e.get("role") == "run_manifest"]
        self.assertEqual(len(arts), 1)
        with open(arts[0]["path"], encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["status"], "incomplete")
        self.assertEqual(m["phases"], [1])

    def test_15_deep_phase3_rejection_deferred_to_run(self):
        # validate 对非法 projection / 不存在 hips source 一律 OK(浅); run 才拒(2)。
        # 冻结 validate/run 分界: validate 不触发科学 execute、不做大 I/O(不打开 HiPS)。
        d = os.path.join(self.tmp, "r15"); os.makedirs(d, exist_ok=True)
        deep = {"phase3": {"source": {"hips_dir": os.path.join(self.tmp, "nope.hips")},
                           "center": {"ra_deg": 0.0, "dec_deg": 0.0},
                           "scale_deg_per_px": 0.1, "width_px": 8, "height_px": 8,
                           "projection": "SIN"}}
        cfg = self._cfg("r15.json", output_dir=d, **deep)
        rv = run("config", "validate", "--config", cfg)
        self.assertEqual(rv.returncode, 0, "深 phase3 键不属于 validate 面")
        r = run("phase3", "run", "--config", cfg, "--events-jsonl", cwd=self.tmp)
        self.assertEqual(r.returncode, 2, "run 域深层拒 → 2")
        self.assertIn("phase3 failed", r.stderr)
        arts = [e for e in jsonl_lines(r.stdout)
                if e["kind"] == "artifact" and e.get("role") == "run_manifest"]
        self.assertEqual(len(arts), 1, "深层拒也落 incomplete manifest")
        with open(arts[0]["path"], encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["status"], "incomplete")
        # 不存在 hips source 的合法 TAN 深配置: run 也拒(不打开缺失 HiPS 前先失败 PARAM)
        deep2 = dict(deep)
        deep2["phase3"]["projection"] = "TAN"
        cfg2 = self._cfg("r15b.json", output_dir=d, **deep2)
        r2 = run("phase3", "run", "--config", cfg2, "--events-jsonl", cwd=self.tmp)
        self.assertEqual(r2.returncode, 2, r2.stderr)
        self.assertIn("phase3 failed", r2.stderr)

    def test_16_no_separate_validate_plan_inspect_subcommands(self):
        # CLI-003 冻结面: validate/plan/run/inspect 语义固化在当前命令树内 —— 不存在
        # `phaseN validate/plan/inspect` 独立子命令; parser 白名单(CLI-001 冻结)拒绝 → 2,
        # stdout 零污染。validate 语义 = config validate(浅); run 语义 = phaseN run;
        # verify = manifest 复算(inspect 不重算的机器落地, test_11)。
        cfg = self._cfg("sub.json", output_dir=os.path.join(self.tmp, "sub_out"))
        for sub in ("phase1 validate", "phase2 plan", "phase3 plan",
                    "phase3 inspect", "phase3 resume"):
            r = run(*sub.split(), "--config", cfg)
            self.assertEqual(r.returncode, 2, f"{sub} 必须 unknown command → 2")
            self.assertIn("unknown command", r.stderr)
            self.assertEqual(r.stdout, "", f"{sub} stdout 应无输出(污染)")


def walk_files(d):
    out = []
    for root, _dirs, files in os.walk(d):
        for f in files:
            out.append(os.path.join(root, f))
    return out


def glob_no_run(d):
    return [f for f in walk_files(d) if os.path.basename(f).startswith("astrocs_run_")]


if __name__ == "__main__":
    unittest.main(verbosity=2)
