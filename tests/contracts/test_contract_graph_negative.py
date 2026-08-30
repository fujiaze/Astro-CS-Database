#!/usr/bin/env python3
"""DOC-001 negative fixtures: 校验器必须抓到悬空引用/重复 ID/ACTIVE依赖OBSOLETE。"""
import importlib.util, os, pathlib, tempfile, unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("ccg", REPO / "tools" / "check_contract_graph.py")
ccg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ccg)

BASE = """schema: astrocs.contract-index/v1
version: 1.0.0
contracts:
  - id: SCI-A-001
    type: SCI
    status: ACTIVE
    owner: x
    version: 1.0.0
    path: a.md
    upstream: []
    downstream: []
"""

def run(content):
    with tempfile.TemporaryDirectory() as td:
        d = pathlib.Path(td)
        (d/"a.md").write_text("ok")
        idx = d / "INDEX.yaml"; idx.write_text(content)
        return ccg.validate(d, idx)

class TestNegative(unittest.TestCase):
    def test_dangling_ref(self):
        errs = run(BASE.replace("upstream: []", "upstream: [SCI-NOPE-999]", 1))
        self.assertTrue(any("悬空" in e for e in errs), errs)

    def test_duplicate_id(self):
        dup = BASE + BASE.split("contracts:\n",1)[1]
        errs = run(dup)
        self.assertTrue(any("重复" in e for e in errs), errs)

    def test_active_depends_obsolete(self):
        content = BASE.replace("upstream: []", "upstream: [SCI-O-001]", 1) + """  - id: SCI-O-001
    type: SCI
    status: OBSOLETE
    owner: x
    version: 1.0.0
    path: a.md
    upstream: []
    downstream: [SCI-A-001]
"""
        errs = run(content)
        self.assertTrue(any("OBSOLETE" in e for e in errs), errs)

    def test_bad_id_format(self):
        errs = run(BASE.replace("SCI-A-001", "SCI-A_001!"))
        self.assertTrue(any("非法 ID" in e for e in errs), errs)

    def test_missing_path(self):
        errs = run(BASE.replace("path: a.md", "path: missing.md"))
        self.assertTrue(any("路径不存在" in e for e in errs), errs)

if __name__ == "__main__":
    unittest.main(verbosity=2)
