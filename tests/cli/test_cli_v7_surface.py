#!/usr/bin/env python3
"""CLI-001 验收: V7 统一命令面(03 §3) — version/modules/selftest 骨架契约。

规格: 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md CLI-001
  实现 version/doctor/modules/config/phase1/phase2/phase3/benchmark/selftest 命令骨架;
  定义 stdout JSON 与 stderr 诊断、稳定退出码、UTF-8/UTF-16 path。
验收:
  - CLI contract tests;
  - 未知命令/坏配置/缺 DLL 明确非零;
  - 机器输出无混杂进度文字。

本文件覆盖 CLI-001 骨架层(version/modules/selftest):
  * `astrocs version [--json]` — 稳定输出(schema 与 --version --json 一致);
  * `astrocs modules list|verify [--json]` — 从 exe 旁 astrocs.product.json 扫描;
    无 manifest(纯开发单 exe)→ 5; 缺 DLL/缺文件 → 5 + issues; 完整树 → 0;
  * `astrocs selftest [--module ID] [--json]` — 宿主自检; 定向未装配 → 5;
  * 机器输出纪律: --json stdout 恰一个 JSON 文档, 无日志/进度文字混杂;
  * 未知命令 → 2; stderr 诊断带 "astrocs:" 前缀。

构建: 复用 tests/cli 既有先例 EXE = build/cli/astrocs (Linux GCC; Windows 侧 WIN-* 实机)。
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")

MODULES_LIST_SCHEMA = os.path.join(
    REPO, "contracts", "config", "cli_modules_list.schema.json")
SELFTEST_SCHEMA = os.path.join(
    REPO, "contracts", "config", "cli_selftest.schema.json")


def built():
    if not os.path.isfile(EXE):
        raise AssertionError("先构建 CLI: cmake -S cli -B build/cli && cmake --build build/cli")
    return EXE


def run(*args, cwd=None, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    return subprocess.run([built(), *args], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=90, cwd=cwd, env=e)


def validate_schema(obj, schema):
    """最小 draft-07/2020-12 子集校验(与 tests/version 同式, 不引入第三方依赖)。"""
    def ok_node(v, s, path):
        t = s.get("type")
        if isinstance(t, list):
            if not any(ok_node(v, {**s, "type": one}, path) is None for one in t):
                return f"{path}: type 违例 {type(v).__name__}"
            return None
        if t == "object":
            if not isinstance(v, dict):
                return f"{path}: 非对象"
            for k in s.get("required", []):
                if k not in v:
                    return f"{path}: 缺字段 {k}"
            if s.get("additionalProperties") is False:
                extra = set(v) - set(s.get("properties", {}))
                if extra:
                    return f"{path}: 多余字段 {extra}"
            for k, sub in s.get("properties", {}).items():
                if k in v:
                    err = ok_node(v[k], sub, f"{path}.{k}")
                    if err:
                        return err
        elif t == "string":
            if not isinstance(v, str):
                return f"{path}: 非字符串"
            if "const" in s and v != s["const"]:
                return f"{path}: const 违例 {v!r}"
            if "enum" in s and v not in s["enum"]:
                return f"{path}: enum 违例 {v!r}"
            if "pattern" in s and s["pattern"] and not __import__("re").match(s["pattern"], v):
                return f"{path}: pattern 违例 {v!r}"
        elif t == "integer":
            if not isinstance(v, int) or isinstance(v, bool):
                return f"{path}: 非整数"
            if "const" in s and v != s["const"]:
                return f"{path}: const 违例 {v!r}"
            if "enum" in s and v not in s["enum"]:
                return f"{path}: enum 违例 {v!r}"
        elif t == "boolean":
            if not isinstance(v, bool):
                return f"{path}: 非布尔"
        elif t == "array":
            if not isinstance(v, list):
                return f"{path}: 非数组"
            for i, item in enumerate(v):
                err = ok_node(item, s.get("items", {}), f"{path}[{i}]")
                if err:
                    return err
        elif t == "null":
            if v is not None:
                return f"{path}: 非 null"
        return None
    return ok_node(obj, schema, "$")


def load_schema(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


class TestVersionSurface(unittest.TestCase):
    """version 子命令: 稳定输出 + JSON schema + 与 --version 一致。"""

    @classmethod
    def setUpClass(cls):
        built()

    def test_01_version_plain(self):
        r = run("version")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertRegex(r.stdout.strip(), r"^astrocs 0\.11\.0-alpha\.1\+g[0-9a-f]{12}")
        self.assertEqual(r.stderr, "")

    def test_02_version_json_single_document(self):
        r = run("version", "--json")
        self.assertEqual(r.returncode, 0)
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertEqual(len(lines), 1, "stdout 恰一个 JSON 文档(无日志混杂)")
        doc = json.loads(lines[0])
        self.assertEqual(doc["name"], "astrocs")
        self.assertEqual(doc["schema_version"], "1")
        self.assertRegex(doc["version"], r"^0\.11\.0-alpha\.1\+g[0-9a-f]{12}")

    def test_03_version_matches_dash_version(self):
        a = run("--version", "--json")
        b = run("version", "--json")
        self.assertEqual(a.returncode, 0)
        self.assertEqual(b.returncode, 0)
        self.assertEqual(json.loads(a.stdout), json.loads(b.stdout),
                         "version 子命令必须与 --version 输出一致(单事实源)")

    def test_04_version_rejects_unknown_flag(self):
        r = run("version", "--bogus")
        self.assertEqual(r.returncode, 2)
        self.assertIn("astrocs:", r.stderr)
        self.assertEqual(r.stdout, "")


class TestModulesSurface(unittest.TestCase):
    """modules list|verify: manifest 扫描 + 缺 DLL 明确非零。"""

    @classmethod
    def setUpClass(cls):
        built()
        cls.tmp = tempfile.mkdtemp(prefix="cli001_mod_")
        # 构造安装树 fixture: astrocs + manifest; 模块目录按需放/不放文件
        cls.tree_ok = os.path.join(cls.tmp, "tree_ok")
        cls.tree_missing = os.path.join(cls.tmp, "tree_missing")
        cls.tree_badjson = os.path.join(cls.tmp, "tree_badjson")
        for t in (cls.tree_ok, cls.tree_missing, cls.tree_badjson):
            os.makedirs(os.path.join(t, "modules"))
            os.makedirs(os.path.join(t, "providers"))
            shutil.copy(EXE, os.path.join(t, "astrocs"))
        manifest = {
            "schema_version": 1,
            "product_version": "0.11.0-alpha.1",
            "source_commit": "0" * 40,
            "platform": "linux-amd64",
            "note": "CLI-001 test fixture",
            "units": [
                {"unit_id": "PLATFORM-CLI", "kind": "exe",
                 "rel_path": "astrocs", "abi_version": 1,
                 "module_id": None, "sha256": None, "status": "SKELETON"},
                {"unit_id": "MOD-NOOP", "kind": "module",
                 "rel_path": "modules/astrocs_noop.so", "abi_version": 1,
                 "module_id": "astrocs.conformance.noop", "sha256": None,
                 "status": "SKELETON"},
                {"unit_id": "PROV-CPU-BASELINE", "kind": "provider",
                 "rel_path": "providers/astrocs_cpu_baseline.so", "abi_version": 1,
                 "module_id": None, "sha256": None, "status": "SKELETON"},
            ],
        }
        for t in (cls.tree_ok, cls.tree_missing):
            with open(os.path.join(t, "astrocs.product.json"), "w",
                      encoding="utf-8") as f:
                json.dump(manifest, f)
        with open(os.path.join(cls.tree_badjson, "astrocs.product.json"), "w",
                  encoding="utf-8") as f:
            f.write("{ not json")
        # tree_ok: 全部文件存在; tree_missing: 缺 modules/astrocs_noop.so
        with open(os.path.join(cls.tree_ok, "modules", "astrocs_noop.so"),
                  "wb") as f:
            f.write(b"fake-noop")
        with open(os.path.join(cls.tree_ok, "providers",
                               "astrocs_cpu_baseline.so"), "wb") as f:
            f.write(b"fake-provider")
        # 纯开发树(无 manifest): cwd 无 manifest, exe 旁也无 → modules list FAIL(5)
        cls.tree_bare = os.path.join(cls.tmp, "tree_bare")
        os.makedirs(cls.tree_bare)
        shutil.copy(EXE, os.path.join(cls.tree_bare, "astrocs"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_no_manifest_is_backend_error(self):
        # 纯开发单 exe(无 product manifest)→ 装配查询失败 = 5(缺装配事实源)
        r = run("modules", "list", "--json", cwd=self.tree_bare)
        self.assertEqual(r.returncode, 5, "无 product manifest → 5")
        doc = json.loads(r.stdout)
        self.assertEqual(doc["manifest_present"], False)
        self.assertEqual(doc["verdict"], "FAIL")
        self.assertEqual(r.stderr, "")

    def test_02_full_tree_list_pass(self):
        r = run("modules", "list", "--json", cwd=self.tree_ok)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["kind"], "astrocs_modules_list")
        self.assertEqual(doc["verdict"], "PASS")
        self.assertEqual(doc["manifest_present"], True)
        self.assertEqual(len(doc["units"]), 3)
        noop = next(u for u in doc["units"] if u["unit_id"] == "MOD-NOOP")
        self.assertTrue(noop["present"])
        self.assertEqual(noop["module_id"], "astrocs.conformance.noop")
        # schema 校验(最小 draft 子集)
        schema = load_schema(MODULES_LIST_SCHEMA)
        err = validate_schema(doc, schema)
        self.assertIsNone(err, f"modules list 输出违反 schema: {err}")

    def test_03_missing_dll_verify_fails_5(self):
        # tree_missing 缺 modules/astrocs_noop.so → verify 必须非零(5), issues 明细
        r = run("modules", "verify", "--json", cwd=self.tree_missing)
        self.assertEqual(r.returncode, 5, "缺 DLL → 5")
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "FAIL")
        kinds = [i["kind"] for i in doc["issues"]]
        self.assertIn("missing_unit_file", kinds)
        noop = next(u for u in doc["units"] if u["unit_id"] == "MOD-NOOP")
        self.assertFalse(noop["present"])
        # list 同样报 5(缺文件为装配缺陷)
        r2 = run("modules", "list", "--json", cwd=self.tree_missing)
        self.assertEqual(r2.returncode, 5)

    def test_04_full_tree_verify_pass(self):
        r = run("modules", "verify", "--json", cwd=self.tree_ok)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "PASS")
        self.assertEqual(doc["issues"], [])

    def test_05_bad_manifest_backend_error(self):
        r = run("modules", "verify", "--json", cwd=self.tree_badjson)
        self.assertEqual(r.returncode, 5, "坏 manifest → 5")
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "FAIL")
        self.assertIn("manifest", [i["kind"] for i in doc["issues"]])

    def test_06_unknown_command_nonzero(self):
        r = run("modules", "bogus")
        self.assertEqual(r.returncode, 2)
        self.assertIn("astrocs:", r.stderr)
        self.assertEqual(r.stdout, "")

    def test_07_stdout_purity(self):
        # --json 输出必须是恰一个 JSON 文档: 整体可 parse 且首字符为 '{'
        # (无日志/进度文字前缀混杂; 日志纪律要求诊断进 stderr)。
        for args, cwd in [(("modules", "verify", "--json"), self.tree_ok),
                          (("modules", "list", "--json"), self.tree_ok),
                          (("modules", "verify", "--json"), self.tree_missing)]:
            r = run(*args, cwd=cwd)
            stripped = r.stdout.lstrip()
            self.assertTrue(stripped.startswith("{"),
                            f"{args} stdout 必须以 JSON 对象开头(无混杂文字)")
            doc = json.loads(r.stdout)   # 整体可 parse → 无混杂非 JSON 行
            self.assertIn("schema_version", doc)

    def test_08_utf8_install_tree_path(self):
        # UTF-8 path(含非 ASCII 目录名)下 modules list/verify 必须正常工作:
        # manifest 发现/unit 存在性判定走 u8path, 不依赖 locale。
        uni = os.path.join(self.tmp, "安装树_β")
        shutil.copytree(self.tree_ok, uni)
        r = run("modules", "list", "--json", cwd=uni)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "PASS")
        self.assertEqual(len(doc["units"]), 3)
        r2 = run("modules", "verify", "--json", cwd=uni)
        self.assertEqual(r2.returncode, 0, r2.stderr)
        self.assertEqual(json.loads(r2.stdout)["verdict"], "PASS")
        # 定向 selftest 在 UTF-8 树内也正常
        r3 = run("selftest", "--json", "--module", "astrocs.conformance.noop",
                 cwd=uni)
        self.assertEqual(r3.returncode, 0, r3.stderr)


class TestSelftestSurface(unittest.TestCase):
    """selftest: 宿主自检 + 定向模块装配校验。"""

    @classmethod
    def setUpClass(cls):
        built()
        cls.tmp = tempfile.mkdtemp(prefix="cli001_st_")
        cls.tree_ok = os.path.join(cls.tmp, "tree_ok")
        cls.tree_bare = os.path.join(cls.tmp, "tree_bare")
        for t in (cls.tree_ok, cls.tree_bare):
            os.makedirs(t)
            shutil.copy(EXE, os.path.join(t, "astrocs"))
        manifest = {
            "schema_version": 1, "product_version": "0.11.0-alpha.1",
            "source_commit": "0" * 40, "platform": "linux-amd64",
            "note": "CLI-001 selftest fixture",
            "units": [
                {"unit_id": "MOD-NOOP", "kind": "module",
                 "rel_path": "modules/astrocs_noop.so", "abi_version": 1,
                 "module_id": "astrocs.conformance.noop", "sha256": None,
                 "status": "SKELETON"},
                {"unit_id": "PROV-CPU-BASELINE", "kind": "provider",
                 "rel_path": "providers/astrocs_cpu_baseline.so", "abi_version": 1,
                 "module_id": None, "sha256": None, "status": "SKELETON"},
            ],
        }
        os.makedirs(os.path.join(cls.tree_ok, "modules"))
        os.makedirs(os.path.join(cls.tree_ok, "providers"))
        with open(os.path.join(cls.tree_ok, "astrocs.product.json"), "w",
                  encoding="utf-8") as f:
            json.dump(manifest, f)
        with open(os.path.join(cls.tree_ok, "modules", "astrocs_noop.so"),
                  "wb") as f:
            f.write(b"fake-noop")
        with open(os.path.join(cls.tree_ok, "providers",
                               "astrocs_cpu_baseline.so"), "wb") as f:
            f.write(b"fake-provider")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_selftest_bare_passes(self):
        # 纯开发单 exe: 宿主自检过; manifest 缺席 → skipped(非 fail)
        r = run("selftest", "--json", cwd=self.tree_bare)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["kind"], "astrocs_selftest")
        self.assertEqual(doc["verdict"], "PASS")
        names = [c["name"] for c in doc["checks"]]
        self.assertIn("host_baseline", names)
        self.assertIn("exit_code_table", names)
        schema = load_schema(SELFTEST_SCHEMA)
        err = validate_schema(doc, schema)
        self.assertIsNone(err, f"selftest 输出违反 schema: {err}")

    def test_02_selftest_module_declared_pass(self):
        r = run("selftest", "--json", "--module", "astrocs.conformance.noop",
                cwd=self.tree_ok)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "PASS")

    def test_03_selftest_module_undeclared_fails(self):
        r = run("selftest", "--json", "--module", "astrocs.conformance.echo",
                cwd=self.tree_ok)
        self.assertEqual(r.returncode, 5, "未登记模块 → 5")
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "FAIL")

    def test_04_provider_declared_present_passes(self):
        r = run("selftest", "--json", "--provider", "PROV-CPU-BASELINE",
                cwd=self.tree_ok)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "PASS")
        names = [c["name"] for c in doc["checks"]]
        self.assertIn("provider_assembly:PROV-CPU-BASELINE", names)

    def test_05_provider_undeclared_fails(self):
        # 回归: 旧实现 ukind=="provider" 兜底导致未登记 provider 也 PASS → 必须非零
        r = run("selftest", "--json", "--provider", "PROV-CPU-AVX512",
                cwd=self.tree_ok)
        self.assertEqual(r.returncode, 5, "未登记 provider → 5(禁止兜底 PASS)")
        doc = json.loads(r.stdout)
        self.assertEqual(doc["verdict"], "FAIL")
        self.assertIn("provider_assembly:PROV-CPU-AVX512",
                      [c["name"] for c in doc["checks"]])
        # 机器输出无混杂进度文字: stdout 恰一个 JSON 文档(首字符 '{', 整体可 parse)
        self.assertTrue(r.stdout.lstrip().startswith("{"))
        json.loads(r.stdout)

    def test_06_selftest_stdout_pure(self):
        r = run("selftest", "--json", cwd=self.tree_bare)
        stripped = r.stdout.lstrip()
        self.assertTrue(stripped.startswith("{"), "stdout 以 JSON 对象开头")
        json.loads(r.stdout)   # 整体可 parse

    def test_07_selftest_rejects_unknown_flag(self):
        r = run("selftest", "--bogus")
        self.assertEqual(r.returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
