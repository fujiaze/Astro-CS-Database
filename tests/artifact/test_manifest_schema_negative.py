#!/usr/bin/env python3
"""DATA-001 schema 负测：严格拒绝缺字段 / NaN / 未知 type / 重复 producer / 非法 digest。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-001):
  - JSON Schema 严格拒绝缺字段 → test_missing_field / test_missing_required_each
  - JSON Schema 严格拒绝 NaN   → test_nan_literal / test_nan_string_size / test_inf_size
  - JSON Schema 严格拒绝未知 type → test_unknown_type
  - 重复 producer → test_duplicate_producer_key / test_duplicate_input
  - 附加结构拒绝: 非法 digest hex、status 非法、type↔schema_version 不匹配、storage_uri 裸路径
"""
from __future__ import annotations

import copy
import json
import pathlib
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "artifact_store"))

from artifact_manifest_validator import Validator, load_strict_json  # noqa: E402

EX = REPO / "contracts" / "data" / "examples" / "frame_hips_manifest.example.json"


def base() -> dict:
    return json.loads(EX.read_text(encoding="utf-8"))


def validate(doc) -> tuple[bool, list[str]]:
    return Validator().validate_doc(doc)


class TestSchemaNegative(unittest.TestCase):
    def test_missing_field(self):
        for field in ["manifest_schema", "artifact_id", "type_id", "schema_version",
                      "storage_uri", "content_digest", "size", "producer", "run",
                      "node", "input_digests", "config_digest", "status", "created_utc"]:
            d = base()
            del d[field]
            ok, errs = validate(d)
            self.assertFalse(ok, f"缺字段 {field} 应被拒")
            self.assertTrue(errs, f"{field} 应有错误信息")

    def test_extra_field(self):
        d = base()
        d["sneaky_extra"] = 1
        ok, errs = validate(d)
        self.assertFalse(ok, "多余字段应被拒 (additionalProperties=false)")
        self.assertTrue(any("extra" in e or "sneaky_extra" in e for e in errs), errs)

    def test_nan_literal(self):
        # JSON 标准不允许 NaN; 宽松解析也必须被拒
        text = EX.read_text(encoding="utf-8")
        bad = text.replace('"size": 104857600', '"size": NaN')
        ok, errs = Validator().validate_text(bad)
        self.assertFalse(ok, "NaN 字面量应被拒")
        self.assertTrue(any("parse" in e for e in errs), errs)

    def test_nan_string_size(self):
        d = base()
        d["size"] = "NaN"
        ok, errs = validate(d)
        self.assertFalse(ok, "size='NaN' 字符串应被拒")
        self.assertTrue(any("not of type" in e or "NaN" in e for e in errs), errs)

    def test_inf_size(self):
        d = base()
        d["size"] = 1e999  # python json 序列化为 Infinity → 字符串; 语义上拒绝
        text = json.dumps(d).replace("1e+999", "Infinity")
        ok, errs = Validator().validate_text(text)
        self.assertFalse(ok, "Infinity size 应被拒")

    def test_unknown_type(self):
        d = base()
        d["type_id"] = "astrocs.phase9.mystery.v1"
        ok, errs = validate(d)
        self.assertFalse(ok, "未知 type_id 应被拒")
        self.assertTrue(any("unknown type_id" in e for e in errs), errs)

    def test_type_schema_version_mismatch(self):
        d = base()
        d["schema_version"] = 2  # registry 中为 1
        ok, errs = validate(d)
        self.assertFalse(ok, "type_id↔schema_version 不匹配应被拒")
        self.assertTrue(any("mismatch" in e for e in errs), errs)

    def test_duplicate_producer_key(self):
        # JSON 对象重复 key (producer 出现两次) → 严格解析拒绝
        text = EX.read_text(encoding="utf-8")
        # 在 producer 对象后注入重复 producer 顶层 key
        ins = text.find('"run": {')
        dup = '  "producer": {"module_id": "astrocs.evil", "module_build_id": "x"},\n'
        bad = text[:ins] + dup + text[ins:]
        ok, errs = Validator().validate_text(bad)
        self.assertFalse(ok, "重复 producer 顶层 key 应被拒")
        self.assertTrue(any("duplicate" in e for e in errs), errs)

    def test_duplicate_input_artifact(self):
        d = base()
        d["input_digests"].append(copy.deepcopy(d["input_digests"][0]))
        ok, errs = validate(d)
        self.assertFalse(ok, "重复 input artifact_id 应被拒")
        self.assertTrue(any("duplicate input artifact_id" in e for e in errs), errs)

    def test_bad_digest(self):
        d = base()
        d["content_digest"]["hex"] = "xyz"
        ok, errs = validate(d)
        self.assertFalse(ok, "非法 content_digest hex 应被拒")
        d = base()
        d["config_digest"]["hex"] = "abc"
        ok, errs = validate(d)
        self.assertFalse(ok, "非法 config_digest hex 应被拒")

    def test_bad_status(self):
        d = base()
        d["status"] = "DONE"
        ok, errs = validate(d)
        self.assertFalse(ok, "非法 status 应被拒")

    def test_bare_path_storage_uri(self):
        d = base()
        d["storage_uri"] = "/home/user/data/out.fits"
        ok, errs = validate(d)
        self.assertFalse(ok, "裸绝对路径 storage_uri 应被拒 (禁路径字符串)")
        d = base()
        d["storage_uri"] = "C:\\Users\\x\\out.fits"
        ok, errs = validate(d)
        self.assertFalse(ok, "Windows 裸路径 storage_uri 应被拒")

    def test_negative_size(self):
        d = base()
        d["size"] = -5
        ok, errs = validate(d)
        self.assertFalse(ok, "负 size 应被拒")

    def test_missing_subfield(self):
        d = base()
        del d["producer"]["module_id"]
        ok, errs = validate(d)
        self.assertFalse(ok, "producer 缺 module_id 应被拒")
        d = base()
        del d["run"]["phase"]
        ok, errs = validate(d)
        self.assertFalse(ok, "run 缺 phase 应被拒")


if __name__ == "__main__":
    unittest.main(verbosity=2)
