#!/usr/bin/env python3
"""CLI-001 验收: 薄CLI phase1/2/3 validate|plan|inspect 语义冻结(宪章 §8.1 命令面补齐)。

权威: ASTROCS_PROJECT_CONSTITUTION.md §8.1 —
  `astrocs phaseN validate|plan|run|inspect <config>` 建议稳定命令面;
  `validate` 不执行科学重算; `plan` 生成 typed DAG/work units/并行计划;
  `inspect` 只读已有运行和产品。run 已由 CLI-002/003/004 冻结, 本文件只冻结缺口三命令。

冻结的语义(全部经生产二进制实测):
1. **validate(相级)**: session_mode 全量 config 校验 + PipelineIR 静态构建(零 Runtime
   实例化、零科学执行、零 I/O 产物); 深层拒绝与 run 同面(如 phase3 config 缺 phase3
   子对象 → 2, 诊断 "run config missing 'phase3' object"); config 不存在/坏 JSON → 3;
   schema_version 非法 → 2; 未知键 → 3。--json 恰一 JSON 文档(kind=astrocs_phase_validate)。
2. **plan(相级)**: 同 validate 前置 + 输出确定性 plan 文档
   (kind=astrocs_plan, pipeline.nodes=typed DAG 节点(node_id/module_id/resources),
    work_units.total/parallel/io, outputs; 无 run_id/时间戳 → 同 config 两次运行
    stdout 逐字节一致); --output <path> 落盘文件与 --json 文档一致; 零 output_dir 写入。
   节点数冻结: phase1=2(cal,cos) phase2=7(coverage..write) phase3=5(properties..verify)。
3. **inspect(相级)**: 只读 output_dir 下 astrocs_run_*.json(kind=astrocs_run_manifest,
   phases 含 N), 逐 run 呈现 {run_id,status,phases,summary,artifacts_count,config_sha256,
   path}; products 去重列出 run manifest 内 artifacts; malformed manifest 呈现为
   status="malformed" 不中断; 零写入(output_dir 快照不变); output_dir 缺失 → 3。
4. **parser 纪律**: 新命令严格旗标白名单(未知旗标 → 2); --help 列出新命令面。
"""
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")


def built():
    if not os.path.isfile(EXE):
        subprocess.run(["cmake", "-S", CLI, "-B", BUILD], check=True,
                       capture_output=True, timeout=300)
        subprocess.run(["cmake", "--build", BUILD, "-j2"], check=True,
                       capture_output=True, timeout=900)
    return EXE


def run(*args, cwd=None, timeout=60):
    return subprocess.run([built(), *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout, cwd=cwd)


def jsonl_lines(stdout):
    return [json.loads(l) for l in stdout.splitlines() if l.strip()]


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


def sha256_file(p):
    with open(p, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


class TestCli001Vpi(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        built()
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_cli001_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)
        cls.light = os.path.join(cls.tmp, "light1.fits")
        with open(cls.light, "wb") as fh:
            fh.write(b"FAKE-FITS-DATA-0")
        cls.base = {"schema_version": "1",
                    "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
                    "output_dir": cls.out}
        cls.cfg_ok = cls._write(os.path.join(cls.tmp, "cfg_ok.json"), cls.base)

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

    # ── 1. help 命令面 ──
    def test_01_help_lists_validate_plan_inspect(self):
        r = run("--help")
        self.assertEqual(r.returncode, 0, r.stderr)
        for ph in (1, 2, 3):
            for sub in ("validate", "plan", "inspect"):
                self.assertIn(f"astrocs phase{ph} {sub} --config <path>", r.stdout,
                              f"--help 必须列出 phase{ph} {sub}")

    # ── 2. validate 正向 ──
    def test_02_validate_v1_config_ok_human_and_json(self):
        for ph in (1, 2):
            r = run(f"phase{ph}", "validate", "--config", self.cfg_ok)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("validate OK", r.stdout)
            rj = run(f"phase{ph}", "validate", "--config", self.cfg_ok, "--json")
            self.assertEqual(rj.returncode, 0, rj.stderr)
            self.assertEqual(len(rj.stdout.splitlines()), 1, "--json 恰一 JSON 文档")
            doc = json.loads(rj.stdout)
            self.assertEqual(doc["schema_version"], "1")
            self.assertEqual(doc["kind"], "astrocs_phase_validate")
            self.assertEqual(doc["phase"], ph)
            self.assertGreater(doc["node_count"], 0)

    def test_03_validate_flat_session_configs_ok(self):
        # phase1 平铺
        c1 = self._cfg("flat1.json", schema_version="1",
                       input_lights=[], output_dir=self.out)
        r = run("phase1", "validate", "--config", c1)
        self.assertEqual(r.returncode, 0, r.stderr)
        # phase2 平铺
        c2 = self._cfg("flat2.json", schema_version="1",
                       hips_paths=[], output_dir=self.out)
        r = run("phase2", "validate", "--config", c2)
        self.assertEqual(r.returncode, 0, r.stderr)
        # phase3 平铺直通
        c3 = self._cfg("flat3.json", schema_version="1", source={"hips_dir": "x"},
                       center={"ra_deg": 0.0, "dec_deg": 0.0},
                       scale_deg_per_px=0.1, width_px=8, height_px=8,
                       output_dir=self.out)
        r = run("phase3", "validate", "--config", c3)
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_04_validate_phase3_missing_object_rejected_like_run(self):
        # 深层拒绝与 run 同面: V1 config 无 phase3 子对象 → phase3 validate → 2
        r = run("phase3", "validate", "--config", self.cfg_ok)
        self.assertEqual(r.returncode, 2, r.stderr)
        self.assertIn("run config missing 'phase3' object", r.stderr)

    def test_05_validate_wrong_phase_flat_config_rejected(self):
        # 纯 phase1 平铺 config(无 inputs/无 output_dir 顶层 V1 面)驱动 phase2 → IR 构建缺输入 → 2
        c1 = self._write(os.path.join(self.tmp, "flat1b.json"),
                         {"schema_version": "1", "input_lights": [],
                          "output_dir": self.out})
        r = run("phase2", "validate", "--config", c1)
        self.assertEqual(r.returncode, 2, r.stderr)

    # ── 3. validate 错误面/纪律 ──
    def test_06_validate_error_codes(self):
        r = run("phase1", "validate", "--config",
                os.path.join(self.tmp, "nofile.json"))
        self.assertEqual(r.returncode, 3)
        r = run("phase1", "validate", "--config", self._cfg(
            "badsv.json", schema_version="2"))
        self.assertEqual(r.returncode, 2)
        r = run("phase1", "validate", "--config", self._cfg(
            "unknownkey.json", backend="x"))
        self.assertEqual(r.returncode, 3)
        self.assertIn("config has unknown key 'backend'", r.stderr)
        # 未知旗标 → 2(parser 白名单)
        r = run("phase1", "validate", "--config", self.cfg_ok, "--min-obs", "9")
        self.assertEqual(r.returncode, 2)

    def test_07_validate_zero_io_no_runtime(self):
        snap = tree_snapshot(self.out)
        r = run("phase1", "validate", "--config", self.cfg_ok, "--json")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(tree_snapshot(self.out), snap,
                         "validate 不得向 output_dir 写任何文件")
        self.assertEqual([f for f in os.listdir(self.out)
                          if f.startswith("astrocs_run_")], [],
                         "validate 不得写 run manifest")

    # ── 4. plan 正向/确定性/零 I/O ──
    def test_08_plan_json_structure_and_frozen_node_counts(self):
        for ph, n_nodes in ((1, 2), (2, 7), (3, 5)):
            cfg = self.cfg_ok
            if ph == 3:
                cfg = self._cfg(
                    "p3plan.json", **{"phase3": {
                        "source": {"hips_dir": "x"},
                        "center": {"ra_deg": 0.0, "dec_deg": 0.0},
                        "scale_deg_per_px": 0.1, "width_px": 8, "height_px": 8}})
            r = run(f"phase{ph}", "plan", "--config", cfg, "--json")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(len(r.stdout.splitlines()), 1, "--json 恰一 JSON 文档")
            doc = json.loads(r.stdout)
            self.assertEqual(doc["schema_version"], "1")
            self.assertEqual(doc["kind"], "astrocs_plan")
            self.assertEqual(doc["phase"], ph)
            self.assertEqual(doc["config"]["sha256"], sha256_file(cfg))
            nodes = doc["pipeline"]["nodes"]
            self.assertEqual(len(nodes), n_nodes,
                             f"phase{ph} plan 节点数冻结={n_nodes}")
            for nd in nodes:
                for k in ("node_id", "module_id", "inputs", "outputs", "resources"):
                    self.assertIn(k, nd, f"plan 节点缺 {k}: {nd}")
                self.assertIn(nd["resources"]["class"], ("cpu_heavy", "io"))
            self.assertEqual(doc["work_units"]["total"], n_nodes)
            self.assertEqual(doc["work_units"]["parallel"] + doc["work_units"]["io"],
                             n_nodes)
            self.assertGreaterEqual(doc["budget"]["cpu_cores"], 1)
            self.assertIn("outputs", doc["pipeline"])

    def test_09_plan_is_deterministic_byte_identical(self):
        a = run("phase1", "plan", "--config", self.cfg_ok, "--json")
        b = run("phase1", "plan", "--config", self.cfg_ok, "--json")
        self.assertEqual(a.returncode, 0, a.stderr)
        self.assertEqual(a.stdout, b.stdout,
                         "同 config 两次 plan --json 必须逐字节一致(无 run_id/时间戳)")

    def test_10_plan_output_flag_writes_same_document(self):
        p = os.path.join(self.tmp, "plan_p1.json")
        r = run("phase1", "plan", "--config", self.cfg_ok, "--json", "--output", p)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(os.path.isfile(p))
        with open(p, encoding="utf-8") as fh:
            on_disk = fh.read()
        self.assertEqual(json.loads(on_disk), json.loads(r.stdout),
                         "--output 落盘文档与 stdout 文档一致")

    def test_11_plan_zero_io_and_error(self):
        snap = tree_snapshot(self.out)
        r = run("phase1", "plan", "--config", self.cfg_ok, "--json")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(tree_snapshot(self.out), snap, "plan 不得写 output_dir")
        # plan 缺 config → 3; phase3 缺对象 → 2(与 validate/run 同面)
        self.assertEqual(run("phase1", "plan", "--config",
                             os.path.join(self.tmp, "nofile.json")).returncode, 3)
        self.assertEqual(run("phase3", "plan", "--config", self.cfg_ok).returncode, 2)

    # ── 5. inspect 只读呈现 ──
    def _mk_manifest(self, name, run_id, phases, status="complete",
                     artifacts=None, summary="t", mutate=None):
        ver = json.loads(run("--version", "--json").stdout)["version"]
        art = os.path.join(self.out, "out.fits")
        if not os.path.isfile(art):
            with open(art, "wb") as fh:
                fh.write(b"SCI-DATA-" + os.urandom(16))
        doc = {"schema_version": "1", "kind": "astrocs_run_manifest",
               "run_id": run_id, "astrocs_version": ver,
               "platform": {"os": "linux", "arch": "amd64"},
               "config_path": None, "config_sha256": None,
               "cpu_profile_path": None, "cpu_profile_sha256": None,
               "phases": phases,
               "artifacts": artifacts if artifacts is not None else
               [{"role": "phase3_output", "path": art,
                 "sha256": sha256_file(art),
                 "size_bytes": os.path.getsize(art)}],
               "status": status, "started_utc": "2026-01-01T00:00:00Z",
               "finished_utc": "2026-01-01T00:00:01Z", "summary": summary}
        if mutate:
            mutate(doc)
        return self._write(os.path.join(self.out, name), doc)

    def test_12_inspect_lists_matching_runs_and_products(self):
        self._mk_manifest("astrocs_run_aaaaaaaaaaaa.json", "a" * 12, [1],
                          summary="p1 ok")
        self._mk_manifest("astrocs_run_bbbbbbbbbbbb.json", "b" * 12, [2, 3],
                          summary="p2p3 ok")
        with open(os.path.join(self.out, "astrocs_run_cccccccccccc.json"),
                  "w", encoding="utf-8") as fh:
            fh.write("{not json")
        snap = tree_snapshot(self.out)
        r = run("phase1", "inspect", "--config", self.cfg_ok, "--json")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(len(r.stdout.splitlines()), 1)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["schema_version"], "1")
        self.assertEqual(doc["kind"], "astrocs_phase_inspect")
        self.assertEqual(doc["phase"], 1)
        run_ids = [x["run_id"] for x in doc["runs"]]
        self.assertIn("a" * 12, run_ids, "phases=[1] 的 run 必须列出")
        self.assertNotIn("b" * 12, run_ids,
                         "phases=[2,3] 的 run 不属于 phase1 inspect")
        row = doc["runs"][run_ids.index("a" * 12)]
        for k in ("status", "phases", "summary", "artifacts_count", "path",
                  "started_utc", "finished_utc", "config_sha256"):
            self.assertIn(k, row, f"inspect run 行缺 {k}")
        self.assertEqual(row["status"], "complete")
        malformed = [x for x in doc["runs"] if x["status"] == "malformed"]
        self.assertEqual(len(malformed), 1, "malformed manifest 必须呈现不中断")
        # products 去重呈现
        self.assertGreaterEqual(len(doc["products"]), 1)
        for pr in doc["products"]:
            self.assertIn("path", pr)
        # 零写入
        self.assertEqual(tree_snapshot(self.out), snap, "inspect 必须严格只读")
        # phase3 inspect 含 b 与 malformed
        r3 = run("phase3", "inspect", "--config", self.cfg_ok, "--json")
        self.assertEqual(r3.returncode, 0, r3.stderr)
        ids3 = [x["run_id"] for x in json.loads(r3.stdout)["runs"]]
        self.assertIn("b" * 12, ids3)
        self.assertNotIn("a" * 12, ids3)

    def test_13_inspect_output_dir_missing_3(self):
        d = os.path.join(self.tmp, "ins_nope")
        if os.path.isdir(d):
            shutil.rmtree(d)
        cfg = self._cfg("ins_nope.json", schema_version="1",
                        input_lights=[], output_dir=d)
        r = run("phase1", "inspect", "--config", cfg)
        self.assertEqual(r.returncode, 3, r.stderr)

    def test_14_inspect_human_mode_and_delete_injection(self):
        r = run("phase1", "inspect", "--config", self.cfg_ok)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("a" * 12, r.stdout, "人类模式须呈现 run_id")
        # 故障注入: 删除 manifest → inspect 列表同步减少(证明真扫描而非恒定输出)
        mf = os.path.join(self.out, "astrocs_run_aaaaaaaaaaaa.json")
        os.remove(mf)
        try:
            rj = run("phase1", "inspect", "--config", self.cfg_ok, "--json")
            self.assertEqual(rj.returncode, 0, rj.stderr)
            ids = [x["run_id"] for x in json.loads(rj.stdout)["runs"]]
            self.assertNotIn("a" * 12, ids, "删除 manifest 后不得再列出")
        finally:
            self._mk_manifest("astrocs_run_aaaaaaaaaaaa.json", "a" * 12, [1],
                              summary="p1 ok")

    # ── 6. run→inspect 闭环(真实 manifest 数据源) ──
    def test_15_run_failure_then_inspect_sees_incomplete(self):
        d = os.path.join(self.tmp, "r15")
        os.makedirs(d, exist_ok=True)
        cfg = self._cfg("r15.json", output_dir=d)   # 无 phase3 键 → run 深层拒
        rv = run("phase3", "run", "--config", cfg, cwd=self.tmp)
        self.assertEqual(rv.returncode, 2, rv.stderr)
        mans = [f for f in os.listdir(d) if f.startswith("astrocs_run_")]
        self.assertEqual(len(mans), 1, "失败 run 必须落 1 个 incomplete manifest")
        cfg_i = self._cfg("r15i.json", output_dir=d)
        ri = run("phase3", "inspect", "--config", cfg_i, "--json")
        self.assertEqual(ri.returncode, 0, ri.stderr)
        doc = json.loads(ri.stdout)
        self.assertEqual(len(doc["runs"]), 1)
        self.assertEqual(doc["runs"][0]["status"], "incomplete")
        self.assertEqual(doc["runs"][0]["phases"], [3])


if __name__ == "__main__":
    unittest.main(verbosity=2)
