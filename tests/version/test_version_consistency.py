#!/usr/bin/env python3
"""VER-001 测试: 版本源合同 + 一致性 checker 的 mutation 试金石。stdlib only。"""
import json, os, re, shutil, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools"))
import gen_version  # noqa: E402
import importlib.util

def load_checker():
    spec = importlib.util.spec_from_file_location(
        "cvc", os.path.join(REPO, "tools", "check_version_consistency.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m

def validate_schema(obj, schema):
    """最小 draft-07 子集校验: type/required/additionalProperties/const/pattern。"""
    def ok_node(v, s, path):
        t = s.get("type")
        if t == "object":
            if not isinstance(v, dict): return f"{path}: 非对象"
            for k in s.get("required", []):
                if k not in v: return f"{path}: 缺字段 {k}"
            if s.get("additionalProperties") is False:
                extra = set(v) - set(s.get("properties", {}))
                if extra: return f"{path}: 多余字段 {extra}"
            for k, sub in s.get("properties", {}).items():
                if k in v:
                    e = ok_node(v[k], sub, f"{path}.{k}")
                    if e: return e
        elif t == "string":
            if not isinstance(v, str): return f"{path}: 非字符串"
            if "const" in s and v != s["const"]: return f"{path}: const 违例 {v!r}"
            if "pattern" in s and not re.match(s["pattern"], v): return f"{path}: pattern 违例 {v!r}"
        elif t == "boolean":
            if not isinstance(v, bool): return f"{path}: 非布尔"
        return None
    return ok_node(obj, schema, "$")

class TestVersionContract(unittest.TestCase):
    def test_01_base_format_and_report_schema(self):
        base = gen_version.read_base_version()
        self.assertRegex(base, r"^\d+\.\d+\.\d+-alpha\.\d+$")
        schema = json.load(open(os.path.join(REPO, "schemas", "version.schema.json"), encoding="utf-8"))
        rep = gen_version.build_report(commit="0123456789ab" * 3, dirty=False)
        self.assertIsNone(validate_schema(rep, schema), "gen_version 输出必须符合 version.schema.json")
        self.assertTrue(rep["version"].startswith("0.10.0-alpha.1+g0123456789ab"))
        self.assertNotIn(".dirty", rep["version"])

    def test_02_dirty_suffix(self):
        rep = gen_version.build_report(commit="0123456789ab" * 3, dirty=True)
        self.assertTrue(rep["version"].endswith(".dirty"))
        self.assertEqual(rep["build_id"], "g0123456789ab.dirty")

    def test_03_reject_forbidden_prerelease(self):
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write("1.0.0-rc.1\n"); bad = f.name
        with self.assertRaises(SystemExit):
            gen_version.read_base_version(bad)
        os.unlink(bad)

    def test_04_real_repo_checker_pass(self):
        r = subprocess.run([sys.executable, os.path.join(REPO, "tools", "check_version_consistency.py")],
                           capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 0, f"真实仓库一致性必须 PASS:\n{r.stdout}{r.stderr}")
        self.assertIn("VERSION_CONSISTENCY_PASS", r.stdout)

    def test_05_mutation_forged_literal_must_fail(self):
        """mutation 合同: 任何一处伪造版本字面量必须被抓。"""
        m = load_checker()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            forged = os.path.join(td, "docs", "FORGED.md")
            open(forged, "w").write("发布版本: 1.2.3 正式版\n")
            errs = []
            m.check_file(forged, "0.10.0", 1, errs)
            self.assertTrue(any("1.2.3" in e for e in errs), f"伪造版本必须被抓: {errs}")
            # 对照: 当前唯一源不被误报
            ok_doc = os.path.join(td, "docs", "OK.md")
            open(ok_doc, "w").write("当前版本 0.10.0-alpha.1\n")
            errs2 = []
            m.check_file(ok_doc, "0.10.0", 1, errs2)
            self.assertEqual(errs2, [])

if __name__ == "__main__":
    unittest.main(verbosity=2)
