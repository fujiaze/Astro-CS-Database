#!/usr/bin/env python3
"""VER-001 测试: 版本源合同（gen_version / VERSION 单源）。stdlib only。

判据对象 check_version_consistency.py 随 G08-01 门禁删除一并移除，依赖它的
一致性 checker 试金石（test_04/05、TestStandardClauseExclusion、
TestLifecycleBoundary、TestExternalToolVersionExemption、
TestSectionNumberNotAVersion、TestCheckerMutationGuard）随之移除；
版本源合同本身（test_01/02/03 走活的 gen_version）保留。
"""
import json, os, re, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(REPO, "eng", "tools"))
import gen_version  # noqa: E402

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
        schema = json.load(open(os.path.join(REPO, "eng", "contracts", "schemas", "version.schema.json"), encoding="utf-8"))
        rep = gen_version.build_report(commit="0123456789ab" * 3, dirty=False)
        self.assertIsNone(validate_schema(rep, schema), "gen_version 输出必须符合 version.schema.json")
        # 单源语义修正 (V8-CI-012 R6.5): 旧断言硬编码 0.10.0-alpha.2 过期字面量，
        # 与 read_base_version() 单源设计自相矛盾（版本推进即挂）。改由单源派生。
        self.assertTrue(rep["version"].startswith(base + "+g0123456789ab"))
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
