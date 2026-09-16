#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-ROOT-CLEAN 的能绿能红证据（ROOT-002 验收门）。

1 正例 + 3 负例：
  * test_positive_clean_tree_is_green                —— 合规根目录 rc=0；
  * test_negative_unregistered_entry_is_red          —— 多出未登记条目 rc!=0；
  * test_negative_runtime_product_at_root_is_red     —— astrocs_run_* 落根 rc!=0；
  * test_negative_missing_required_entry_is_red      —— §7 要求存在的条目缺失 rc!=0。

以及清单保真度证据：
  * test_manifest_matches_engineering_spec_section7  —— ci/root_manifest.json 的
    allowed_files/allowed_dirs 与 ENGINEERING_SPEC.md §7 逐条对照（不比文档更宽）。
"""
from __future__ import annotations

import json
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
CHECKER = REPO / "tools" / "quality" / "check_root_cleanliness.py"
MANIFEST = REPO / "ci" / "root_manifest.json"


def load_manifest() -> dict:
    with open(MANIFEST, encoding="utf-8") as fh:
        return json.load(fh)


def build_tree(root: pathlib.Path, manifest: dict, extra=(), drop=()) -> None:
    for name in manifest["required_files"]:
        if name in drop:
            continue
        (root / name).write_text("", encoding="utf-8")
    for name in manifest["required_dirs"]:
        if name in drop:
            continue
        (root / name).mkdir(parents=True, exist_ok=True)
    for name in extra:
        p = root / name
        if name.endswith("/"):
            p.mkdir(parents=True, exist_ok=True)
        else:
            p.write_text("", encoding="utf-8")


def run_checker(root: pathlib.Path):
    out = root.parent / (root.name + "-report.json")
    proc = subprocess.run(
        [sys.executable, str(CHECKER), "--root", str(root),
         "--manifest", str(MANIFEST), "--json-out", str(out), "--quiet"],
        capture_output=True, text=True, timeout=300)
    report = json.loads(out.read_text(encoding="utf-8"))
    return proc.returncode, report


def reasons(report: dict) -> set:
    return {v["reason"] for v in report["violations"]}


class RootCleanlinessPositiveTest(unittest.TestCase):
    def test_positive_clean_tree_is_green(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "clean"
            root.mkdir()
            build_tree(root, load_manifest())
            rc, report = run_checker(root)
            self.assertEqual(rc, 0, report)
            self.assertEqual(report["verdict"], "PASS")
            self.assertEqual(report["violations"], [])
            self.assertEqual(report["counts"]["top_level_entries"], 35)


class RootCleanlinessNegativeTest(unittest.TestCase):
    def test_negative_unregistered_entry_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "extra"
            root.mkdir()
            build_tree(root, load_manifest(), extra=["stray_probe.txt"])
            rc, report = run_checker(root)
            self.assertNotEqual(rc, 0)
            self.assertEqual(report["verdict"], "FAIL")
            self.assertIn("unregistered_root_entry", reasons(report))
            self.assertIn("stray_probe.txt", [v["path"] for v in report["violations"]])

    def test_negative_runtime_product_at_root_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "runtime"
            root.mkdir()
            build_tree(root, load_manifest(),
                       extra=["astrocs_run_deadbeef1234.json", "run_context.json"])
            rc, report = run_checker(root)
            self.assertNotEqual(rc, 0)
            self.assertIn("runtime_product_at_root", reasons(report))
            bad = {v["path"] for v in report["violations"]
                   if v["reason"] == "runtime_product_at_root"}
            self.assertEqual(bad, {"astrocs_run_deadbeef1234.json", "run_context.json"})

    def test_negative_missing_required_entry_is_red(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td) / "missing"
            root.mkdir()
            build_tree(root, load_manifest(), drop={"README.md"})
            rc, report = run_checker(root)
            self.assertNotEqual(rc, 0)
            self.assertIn("missing_required_file", reasons(report))
            self.assertIn("README.md", [v["path"] for v in report["violations"]])


class RootManifestFidelityTest(unittest.TestCase):
    """清单不得比 ENGINEERING_SPEC §7 更宽。"""

    def test_manifest_matches_engineering_spec_section7(self):
        spec = (REPO / "ENGINEERING_SPEC.md").read_text(encoding="utf-8")
        block = re.search(r"仓库根固定条目：\n(.*?)\n\nlib/", spec, re.S)
        self.assertIsNotNone(block, "未能在 ENGINEERING_SPEC.md 定位 §7 根固定条目代码块")
        tokens = [x for x in re.split(r"[\s/]+", block.group(1)) if x and x != "text"]
        doc_files = set(tokens) - {".github"}   # 文档中 .github/ 为目录
        manifest = load_manifest()
        self.assertEqual(set(manifest["allowed_files"]), doc_files,
                         "allowed_files 与 §7 文档不一致（多出或缺失）")
        self.assertIn(".github", manifest["allowed_dirs"])

        tail = "其他固定目录："
        dirs_para = re.search(tail + r"(.*?)" + chr(96) * 3, spec, re.S)
        self.assertIsNotNone(dirs_para, "未能在 §7 定位「其他固定目录」")
        cleaned = re.sub(r"（[^）]*）", "", dirs_para.group(1))
        doc_dirs = {d for d in re.findall(r"(\S+?)/", cleaned) if d and not d.endswith("：")}
        manifest_dirs = set(manifest["allowed_dirs"])
        self.assertEqual(manifest_dirs, doc_dirs | {"lib", ".github"},
                         "清单与 §7 目录不一致；缺=" + repr(sorted((doc_dirs | {"lib", ".github"}) - manifest_dirs))
                         + " 多=" + repr(sorted(manifest_dirs - doc_dirs - {"lib", ".github"})))


if __name__ == "__main__":
    unittest.main()
