#!/usr/bin/env python3
"""DATA-001 negative fixtures: 校验器必须抓重复 schema_id / 缺字段 / 未登记 DATA ID。"""
import importlib.util, pathlib, tempfile, unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("cda", REPO / "tools" / "check_data_artifacts.py")
cda = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cda)

TBL = """| schema_id | 内容 | scalar | shape/axis | unit | coordinate | invalid | ownership | serialization |
|---|---|---|---|---|---|---|---|---|
| DATA-IMG-RAW-001 | raw | f32 | [H,W] | ADU | pixel | NaN | unique | FITS |
| DATA-IMG-CAL-001 | cal | f32 | [H,W] | ADU | pixel | NaN | unique | FITS |
"""

class TestDataArtifactsNegative(unittest.TestCase):
    def _run(self, artifacts_text, semantics_text="", extra_lines=""):
        with tempfile.TemporaryDirectory() as td:
            d = pathlib.Path(td)
            (d / "docs").mkdir(); (d / "docs" / "contracts").mkdir()
            (d / "docs" / "contracts" / "DATA_ARTIFACTS.md").write_text(artifacts_text + extra_lines)
            (d / "docs" / "contracts" / "DATA_SEMANTICS.md").write_text(semantics_text)
            return cda.validate(d)

    def test_duplicate_id(self):
        dup = TBL + "| DATA-IMG-RAW-001 | dup | f32 | [H,W] | ADU | pixel | NaN | unique | FITS |\n"
        errs = self._run(dup)
        self.assertTrue(any("重复" in e for e in errs), errs)

    def test_missing_column(self):
        bad = TBL.replace("| f32 | [H,W] | ADU | pixel | NaN | unique | FITS |", "| f32 | [H,W] | ADU | pixel | NaN | unique |  |\n", 1)
        errs = self._run(bad)
        self.assertTrue(any("缺字段" in e for e in errs), errs)

    def test_unregistered_data_id(self):
        errs = self._run(TBL, semantics_text="DATA-IMG-NOPE-001 声明未登记")
        self.assertTrue(any("未登记" in e for e in errs), errs)

    def test_bad_id(self):
        bad = TBL.replace("DATA-IMG-RAW-001", "DATA-IMG-RAW")
        errs = self._run(bad)
        self.assertTrue(any("非法 schema_id" in e for e in errs), errs)

if __name__ == "__main__":
    unittest.main(verbosity=2)
